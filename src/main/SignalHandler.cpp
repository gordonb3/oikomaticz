#include "stdafx.h"
#include "mainworker.h"
#include <sys/types.h>
#include <signal.h>
#include "Logger.h"
#include "Helper.h"
#include "localtime_r.h"

#if defined WIN32
//	#include "msbuild/WindowsHelper.h"
//	#include <Shlobj.h>
#else
	#include <sys/stat.h>
	#include <sys/types.h>
//	#include <unistd.h>
	#include <syslog.h>
//	#include <errno.h>
	#include <fcntl.h>
	#include <string.h>
#if defined(__linux__)
	#include <sys/prctl.h>
	#include <sys/syscall.h>
	#include <sys/wait.h>
#endif
#endif

#include "SignalHandler.h"

#define Heartbeat_Timeout 300 //5 minutes

extern MainWorker m_mainworker;

extern std::string logfile;
extern bool g_bStopApplication;
extern bool g_bUseSyslog;
extern bool g_bRunAsDaemon;

extern time_t m_LastHeartbeat;

#if defined(__linux__)
static void printRegInfo(siginfo_t * info, ucontext_t * ucontext)
{
#if defined(REG_RIP) //x86_64
	_log.Log(LOG_ERROR, "siginfo address=%p, address=%p", info->si_addr, (void*)((ucontext_t *)ucontext)->uc_mcontext.gregs[REG_RIP]);
#elif defined(REG_EIP) //x86
	_log.Log(LOG_ERROR, "siginfo address=%p, address=%p", info->si_addr, (void*)((ucontext_t *)ucontext)->uc_mcontext.gregs[REG_EIP]);
#elif defined(__aarch64__) //arm64 (aarch64 according to gnu)
	_log.Log(LOG_ERROR, "siginfo address=%p, address=%p", info->si_addr, (void*)((ucontext_t *)ucontext)->uc_mcontext.regs[30]);
#elif defined(__arm__) //arm32
	_log.Log(LOG_ERROR, "siginfo address=%p, address=%p", info->si_addr, (void*)((ucontext_t *)ucontext)->uc_mcontext.arm_lr);
#else // unknown
	_log.Log(LOG_ERROR, "siginfo address=%p", info->si_addr);
#endif
}
/*
static void printSingleThreadInfo(FILE* f, const char* pattern, bool& foundThread, bool& gdbSuccess)
{
	char * line = nullptr;
	size_t len = 0;
	ssize_t read;
	rewind(f);
	while (!foundThread && (read = getline(&line, &len, f)) != -1) {
		if (strstr(line, pattern) != nullptr)
		{
			foundThread = true;
			if (line[strlen(line) - 1] == '\n') line[strlen(line) - 1] = '\0';
			_log.Log(LOG_ERROR, "%s", line);
		}
		if (strstr(line, "Thread") == line) // End of thread info
		{
			free(line);
			break;
		}
		if (strstr(line, "No stack.") == line)
		{
			gdbSuccess = false;
			if (line[strlen(line) - 1] == '\n') line[strlen(line) - 1] = '\0';
			_log.Log(LOG_ERROR, "gdb failed to get stacktrace:\n > %s", line);
		}
		free(line);
		line = nullptr;
	}
}
*/
static void printSingleCallStack(FILE* f, const char* pattern, bool& foundThread, bool& gdbSuccess)
{
	char * line = nullptr;
	size_t len = 0;
	ssize_t read;
	rewind(f);
	while ((read = getline(&line, &len, f)) != -1) {
		if (foundThread)
		{
			if (strstr(line, "#") != line) break; // No '#' means full stack trace for thread printed
			if (line[strlen(line) - 1] == '\n') line[strlen(line) - 1] = '\0';
			_log.Log(LOG_ERROR, "%s", line);
		}
		else
		{
			if (strstr(line, pattern) != nullptr)
			{
				foundThread = true;
				if (line[strlen(line) - 1] == '\n') line[strlen(line) - 1] = '\0';
				_log.Log(LOG_ERROR, "%s", line);
			}
			if (strstr(line, "No stack.") == line)
			{
				gdbSuccess = false;
				if (line[strlen(line) - 1] == '\n') line[strlen(line) - 1] = '\0';
				_log.Log(LOG_ERROR, "gdb failed to get stacktrace:\n > %s", line);
			}
		}
		free(line);
		line = nullptr;
	}
}

// Try to attach gdb to print stack trace (Linux only).
// The main purpose is to improve the very poor stack traces generated by backtrace() on ARM platforms
static bool dumpstack_gdb(bool printAllThreads) {
	//pid_t parent = getpid();
	char pid_buf[30];
	sprintf(pid_buf, "%d", getpid());
	char thread_buf[30];
	sprintf(thread_buf, "(LWP %ld))", syscall(__NR_gettid));
	char thread_buf2[30];
	sprintf(thread_buf2, "(LWP %ld) ", syscall(__NR_gettid));
	char name_buf[512];
	name_buf[readlink("/proc/self/exe", name_buf, 511)]=0;

	if (IsDebuggerPresent())
	{
		return false;
	}

	// Allow us to be traced
	// Note: Does not currently work in WSL: https://github.com/Microsoft/WSL/issues/3053 (Fixed in Windows 10 build 17723)
#ifdef PR_SET_PTRACER
	prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);
#endif

	sigset_t signal_set;
	sigemptyset(&signal_set);

	sigaddset(&signal_set, SIGTERM);
	sigaddset(&signal_set, SIGINT);
	sigaddset(&signal_set, SIGSEGV);
	sigaddset(&signal_set, SIGABRT);
	sigaddset(&signal_set, SIGILL);
	sigaddset(&signal_set, SIGUSR1);
	sigaddset(&signal_set, SIGHUP);

	// Block signals to child processes
	sigprocmask(SIG_BLOCK, &signal_set, nullptr);

	// Spawn helper process which will keep running when gdb is attached to main Oikomaticz process
	pid_t intermediate_pid = fork();
	if (intermediate_pid == -1)
	{
		return false;
	}

	if (!intermediate_pid) {
		// Wathchdog 1: Used to kill sub processes to gdb which may hang
		pid_t timeout_pid1 = fork();
		if (timeout_pid1 == -1)
		{
			_Exit(1);
		}
		if (timeout_pid1 == 0) {
			int timeout = 30;
			sleep_seconds(timeout);
			_Exit(1);
		}

		// Wathchdog 2: Give up on gdb, if it still does not finish even after killing its sub processes
		pid_t timeout_pid2 = fork();
		if (timeout_pid2 == -1)
		{
			kill(timeout_pid1, SIGKILL);
			_Exit(1);
		}
		if (timeout_pid2 == 0) {
			int timeout = 60;
			sleep_seconds(timeout);
			_Exit(1);
		}

		// Worker: Spawns gdb
		pid_t worker_pid = fork();
		if (worker_pid == -1)
		{
			kill(timeout_pid1, SIGKILL);
			kill(timeout_pid2, SIGKILL);
			_Exit(1);
		}
		if (worker_pid == 0) {
			(void) remove(/*szUserDataFolder+*/"oikomaticz_crash.log");
			int fd = open(/*szUserDataFolder+*/"oikomaticz_crash.log", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
			if (fd == -1) _Exit(1);
			if (dup2(fd, STDOUT_FILENO) == -1) _Exit(1);
			if (dup2(fd, STDERR_FILENO) == -1) _Exit(1);
			execlp("gdb", "gdb", "--batch", "-n", "-ex", "info threads", "-ex", "thread apply all bt", "-ex", "echo \nMain thread:\n", "-ex", "bt", "-ex", "detach", name_buf, pid_buf, nullptr);

			// If gdb failed to start, signal back
			close(fd);
			_Exit(1);
		}

		int result = 1;

		// Wait for all children to die
		while (worker_pid || timeout_pid1 || timeout_pid2)
		{
			int status;
			pid_t exited_pid = wait(&status);
			//printf("pid %d exited, worker_pid: %d, timeout_pid1: %d, timeout_pid2: %d\n", exited_pid, worker_pid, timeout_pid1, timeout_pid2);
			if (exited_pid == worker_pid) {
				if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
				{
					result = 0; // Success
				}
				else
				{
					result = 2; // Failed to start gdb
				}
				worker_pid = 0;
				//printf("Status: %x, wifexited: %u, wexitstatus: %u\n", status, WIFEXITED(status), WEXITSTATUS(status));
				//printf("Sending SIGKILL to timeout_pid1\n");
				if (timeout_pid1) kill(timeout_pid1, SIGKILL);
				if (timeout_pid2) kill(timeout_pid2, SIGKILL);
			} else if (exited_pid == timeout_pid1) {
				// Watchdog 1 timed out, attempt to recover by killing all gdb's child processes
				char tmp[128];
				timeout_pid1 = 0;
				//printf("Sending SIGKILL to worker_pid's children\n");
				if (worker_pid)
				{
					sprintf(tmp, "pkill -KILL -P %d", worker_pid);
					if (!system(tmp))
					{
						result = 0;
					}
				}
			} else if (exited_pid == timeout_pid2) {
				// Watchdog 2 timed out, give up
				timeout_pid2 = 0;
				//printf("Sending SIGKILL to worker_pid\n");
				if (worker_pid) kill(worker_pid, SIGKILL);
				if (timeout_pid1) kill(timeout_pid1, SIGKILL);
			}
		}
		_Exit(result); // Or some more informative status
	} else {
		char * line = nullptr;
		size_t len = 0;
		ssize_t read;
		int status;

		// Unblock signals to main process
		sigprocmask(SIG_UNBLOCK, &signal_set, nullptr);
		pid_t res = 0;
		res = waitpid(intermediate_pid, &status, 0);

		if (res == -1 || res == 0) return false;
		if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
		{
			FILE* f = fopen("oikomaticz_crash.log", "r");
			if (f)
			{
				bool foundThread = false;
				bool gdbSuccess = true;
				//if (!printAllThreads) printSingleThreadInfo(f, thread_buf2, foundThread, gdbSuccess);
				//foundThread = false;
				//gdbSuccess = true;
				if (!printAllThreads) printSingleCallStack(f, thread_buf, foundThread, gdbSuccess);

				if (!foundThread)
				{
					if (!printAllThreads) _log.Log(LOG_ERROR, "Did not find stack frame for thread %s, printing full gdb output:\n", thread_buf);
					else _log.Log(LOG_ERROR, "Stack frame for all threads:\n");
					rewind(f);
					while ((read = getline(&line, &len, f)) != -1) {
						if (line[strlen(line) - 1] == '\n') line[strlen(line) - 1] = '\0';
						_log.Log(LOG_ERROR, "> %s", line);
						free(line);
						line = nullptr;
					}
				}
				fclose(f);
				return gdbSuccess;
			}
		}
		else
		{
			_log.Log(LOG_ERROR, "Failed to start gdb, will use backtrace() for printing stack frame\n");
		}
	}
	return false;
}
#else
static bool dumpstack_gdb(bool printAllThreads) {
	return false;
}
#endif

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
static void dumpstack_backtrace(void *info, void *ucontext) {
	// Notes :
	// The following code does needs -rdynamic compile option.not print full backtrace.
	// To have a full backtrace you need to :
	// - compile with -g -rdynamic options
	// - active core dump using "ulimit -c unlimited" before starting daemon
	// - use gdb to analyze the core dump
	void *addrs[128];
	int count = backtrace(addrs, 128);
	char** symbols = backtrace_symbols(addrs, count);

	// skip first stack frame (points here)
	for (int i = 0; i < count && symbols != nullptr; ++i)
	{
		char *mangled_name = 0, *offset_begin = 0, *offset_end = 0;

		// find parentheses and +address offset surrounding mangled name
		for (char *p = symbols[i]; *p; ++p)
		{
			if (*p == '(')
			{
				mangled_name = p;
			}
			else if (*p == '+')
			{
				offset_begin = p;
			}
			else if (*p == ')')
			{
				offset_end = p;
				break;
			}
		}

		// if the line could be processed, attempt to demangle the symbol
		if (mangled_name && offset_begin && offset_end &&
			mangled_name < offset_begin)
		{
			*mangled_name++ = '\0';
			*offset_begin++ = '\0';
			*offset_end++ = '\0';

			int status;
			char * real_name = abi::__cxa_demangle(mangled_name, 0, 0, &status);

			// if demangling is successful, output the demangled function name
			if (status == 0)
			{
				_log.Log(LOG_ERROR, "#%-2d %s : %s + %s%s", i, symbols[i], real_name, offset_begin, offset_end);
			}
			// otherwise, output the mangled function name
			else
			{
				_log.Log(LOG_ERROR, "#%-2d %s : %s + %s%s", i, symbols[i], mangled_name, offset_begin, offset_end);
			}
			free(real_name);
		}
		// otherwise, print the whole line
		else
		{
			_log.Log(LOG_ERROR, "#%-2d %s", i, symbols[i]);
		}
	}
	free(symbols);
}
#else
static void dumpstack_backtrace(void *info, void *ucontext) {
}
#endif

static void dumpstack(void *info, void *ucontext) {
	bool result = false;

	result = dumpstack_gdb(false);
	if (!result) dumpstack_backtrace(info, ucontext);
}

int fatal_handling = 0;
#ifndef WIN32
pthread_t fatal_handling_thread;
#endif

void signal_handler(int sig_num
#ifndef WIN32
, siginfo_t * info, void * ucontext
#endif
)
{
#ifdef WIN32
	void *info = nullptr;
	void *ucontext = nullptr;
#endif
	long tid = 0;
	char thread_name[16] = {'-', '\0'};

	switch(sig_num)
	{
#ifndef WIN32
	case SIGHUP:
		if (!logfile.empty())
			_log.SetOutputFile(logfile.c_str());
		break;
#endif
	case SIGINT:
	case SIGTERM:
#ifndef WIN32
		if ((g_bRunAsDaemon)||(g_bUseSyslog))
			syslog(LOG_INFO, "Oikomaticz is exiting...");
#endif
		g_bStopApplication = true;
		break;
	case SIGSEGV:
	case SIGILL:
	case SIGABRT:
	case SIGFPE:
#if defined(__linux__)
#if defined(__GLIBC__)
		pthread_getname_np(pthread_self(), thread_name, sizeof(thread_name));
#endif
		tid = syscall(__NR_gettid);
#endif
		if (fatal_handling) {
#if defined(__GLIBC__)
			_log.Log(LOG_ERROR, "Oikomaticz(pid:%d, tid:%ld('%s')) received fatal signal %d (%s) while backtracing", getpid(), tid, thread_name, sig_num
#else
			_log.Log(LOG_ERROR, "Oikomaticz(pid:%d, tid:%ld) received fatal signal %d (%s) while backtracing", getpid(), tid, sig_num
#endif
#ifndef WIN32
				, strsignal(sig_num));
#else
				, "-");
#endif
#if defined(__linux__)
			printRegInfo(info, ((ucontext_t *)ucontext));
#endif
#ifndef WIN32
			if (!pthread_equal(fatal_handling_thread, pthread_self()))
			{
				// fatal error in other thread, wait for dump handler to finish
				// If using WSL, may be caused by https://github.com/Microsoft/WSL/issues/1731 (Fixed in Windows 10 build 17728)
				// TODO: Replace sleep with read from FIFO
				sleep(120);
			}
#endif
			dumpstack_backtrace(info, ucontext);
			// re-raise signal to enforce core dump
			signal(sig_num, SIG_DFL);
			raise(sig_num);
		}
		fatal_handling = 1;
#ifndef WIN32
		fatal_handling_thread = pthread_self();
#endif
		_log.Log(LOG_ERROR, "Oikomaticz(pid:%d, tid:%ld('%s')) received fatal signal %d (%s)", getpid(), tid, thread_name, sig_num
#ifndef WIN32
			, strsignal(sig_num));
#else
			, "-");
#endif
#if defined(__linux__)
		printRegInfo(info, ((ucontext_t *)ucontext));
#endif
		dumpstack(info, ucontext);
		// re-raise signal to enforce core dump
		signal(sig_num, SIG_DFL);
		raise(sig_num);
		break;
#ifndef WIN32
	case SIGUSR1:
		fatal_handling = 1;
		fatal_handling_thread = pthread_self();
		_log.Log(LOG_ERROR, "Oikomaticz(%d) is exiting due to watchdog triggered...", getpid());
		// Print call stack of all threads to aid debugging of deadlock
		dumpstack_gdb(true);
		g_bStopApplication = true;
		// Give main thread a few seconds to shut down
		sleep_milliseconds(5000);
		// re-raise signal to enforce core dump
		signal(sig_num, SIG_DFL);
		raise(sig_num);
		break;
#endif
	}
}

static void heartbeat_check()
{
	time_t now;
	mytime(&now);

	double diff = difftime(now, m_mainworker.m_LastHeartbeat);
	if (diff > Heartbeat_Timeout)
	{
		_log.Log(LOG_ERROR, "mainworker seems to have ended or hung unexpectedly (last update %f seconds ago)", diff);
		if (!IsDebuggerPresent())
		{
#ifdef WIN32
			abort();
#else
			raise(SIGUSR1);
#endif
		}
	}

	diff = difftime(now, m_LastHeartbeat);
	if (diff > Heartbeat_Timeout)
	{
		_log.Log(LOG_ERROR, "main thread seems to have ended or hung unexpectedly (last update %f seconds ago)", diff);
		if (!IsDebuggerPresent())
		{
#ifdef WIN32
			abort();
#else
			raise(SIGUSR1);
#endif
		}
	}
}

bool g_stop_watchdog = false;

void Do_Watchdog_Work()
{
	while(!g_stop_watchdog)
	{
		sleep_milliseconds(1000);
		heartbeat_check();
	}
}

