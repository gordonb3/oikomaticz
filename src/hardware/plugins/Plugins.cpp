#include "stdafx.h"

//
//	Domoticz Plugin System - Dnpwwo, 2016
//
#ifdef ENABLE_PYTHON

#include "Plugins.h"
#include "PluginMessages.h"
#include "PluginProtocols.h"
#include "PluginTransports.h"
#include "PythonObjects.h"

#include "main/Helper.h"
#include "main/Logger.h"
#include "main/SQLHelper.h"
#include "main/mainworker.h"
#include "main/localtime_r.h"
#include "tinyxpath/tinyxml.h"

#include "notifications/NotificationHelper.h"

#define ADD_STRING_TO_DICT(pPlugin, pDict, key, value)                                                                                                                                                          \
	{                                                                                                                                                                                              \
		PyNewRef	pObj = Py_BuildValue("s", value.c_str());                                                                                                                                    \
		if (PyDict_SetItemString(pDict, key, pObj) == -1)                                                                                                                                      \
			pPlugin->Log(LOG_ERROR, "(%s) failed to add key '%s', value '%s' to dictionary.", m_PluginKey.c_str(), key, value.c_str());                                                        \
	}

#define GETSTATE(m) ((struct module_state *)PyModule_GetState(m))

extern std::string szWWWFolder;
extern std::string szStartupFolder;
extern std::string szUserDataFolder;
extern std::string szWebRoot;
extern std::string dbasefile;
extern std::string szAppVersion;
extern std::string szAppHash;
extern std::string szAppDate;
extern MainWorker m_mainworker;

namespace Plugins
{
	std::mutex PythonMutex; // controls access to Python

	void LogPythonException(CPlugin *pPlugin, const std::string &sHandler)
	{
		PyTracebackObject *pTraceback;
		PyNewRef			pExcept;
		PyNewRef			pValue;
		PyTypeObject *TypeName;
		PyBytesObject *pErrBytes = nullptr;
		const char *pTypeText = nullptr;
		std::string Name = "Unknown";

		if (pPlugin)
			Name = pPlugin->m_Name;

		PyErr_Fetch(&pExcept, &pValue, (PyObject **)&pTraceback);

		if (pExcept)
		{
			TypeName = (PyTypeObject *)pExcept;
			pTypeText = TypeName->tp_name;
		}
		if (pValue)
		{
			pErrBytes = (PyBytesObject *)PyUnicode_AsASCIIString(pValue);
		}
		if (pTypeText && pErrBytes)
		{
			if (pPlugin)
				pPlugin->Log(LOG_ERROR, "(%s) '%s' failed '%s':'%s'.", Name.c_str(), sHandler.c_str(), pTypeText, pErrBytes->ob_sval);
			else
				_log.Log(LOG_ERROR, "(%s) '%s' failed '%s':'%s'.", Name.c_str(), sHandler.c_str(), pTypeText, pErrBytes->ob_sval);
		}
		if (pTypeText && !pErrBytes)
		{
			if (pPlugin)
				pPlugin->Log(LOG_ERROR, "(%s) '%s' failed '%s'.", Name.c_str(), sHandler.c_str(), pTypeText);
			else
				_log.Log(LOG_ERROR, "(%s) '%s' failed '%s'.", Name.c_str(), sHandler.c_str(), pTypeText);
		}
		if (!pTypeText && pErrBytes)
		{
			if (pPlugin)
				pPlugin->Log(LOG_ERROR, "(%s) '%s' failed '%s'.", Name.c_str(), sHandler.c_str(), pErrBytes->ob_sval);
			else
				_log.Log(LOG_ERROR, "(%s) '%s' failed '%s'.", Name.c_str(), sHandler.c_str(), pErrBytes->ob_sval);
		}
		if (!pTypeText && !pErrBytes)
		{
			if (pPlugin)
				pPlugin->Log(LOG_ERROR, "(%s) '%s' failed, unable to determine error.", Name.c_str(), sHandler.c_str());
			else
				_log.Log(LOG_ERROR, "(%s) '%s' failed, unable to determine error.", Name.c_str(), sHandler.c_str());
		}
		if (pErrBytes)
			Py_XDECREF(pErrBytes);

		// Log a stack trace if there is one
		PyTracebackObject *pTraceFrame = pTraceback;
		while (pTraceFrame)
		{
			PyFrameObject *frame = pTraceFrame->tb_frame;
			if (frame)
			{
				int lineno = PyFrame_GetLineNumber(frame);
				PyCodeObject *pCode = frame->f_code;
				PyNewRef	pFileBytes = PyUnicode_AsASCIIString(pCode->co_filename);
				PyNewRef	pFuncBytes = PyUnicode_AsASCIIString(pCode->co_name);
				if (pPlugin)
					pPlugin->Log(LOG_ERROR, "(%s) ----> Line %d in %s, function %s", Name.c_str(), lineno, ((PyBytesObject*)pFileBytes)->ob_sval, ((PyBytesObject*)pFuncBytes)->ob_sval);
				else
					_log.Log(LOG_ERROR, "(%s) ----> Line %d in %s, function %s", Name.c_str(), lineno, ((PyBytesObject*)pFileBytes)->ob_sval, ((PyBytesObject*)pFuncBytes)->ob_sval);
			}
			pTraceFrame = pTraceFrame->tb_next;
		}

		if (!pExcept && !pValue && !pTraceback)
		{
			if (pPlugin)
				pPlugin->Log(LOG_ERROR, "(%s) Call to message handler '%s' failed, unable to decode exception.", Name.c_str(), sHandler.c_str());
			else
				_log.Log(LOG_ERROR, "(%s) Call to message handler '%s' failed, unable to decode exception.", Name.c_str(), sHandler.c_str());
		}

		if (pTraceback)
			Py_XDECREF(pTraceback);
	}

	int PyDomoticz_ProfileFunc(PyObject *self, PyFrameObject *frame, int what, PyObject *arg)
	{
		module_state *pModState = ((struct module_state *)PyModule_GetState(self));
		if (!pModState)
		{
			_log.Log(LOG_ERROR, "CPlugin:%s, unable to obtain module state.", __func__);
		}
		else if (!pModState->pPlugin)
		{
			_log.Log(LOG_ERROR, "CPlugin:%s, illegal operation, Plugin has not started yet.", __func__);
		}
		else
		{
			int lineno = PyFrame_GetLineNumber(frame);
			std::string sFuncName = "Unknown";
			PyCodeObject *pCode = frame->f_code;
			if (pCode && pCode->co_filename)
			{
				PyNewRef pFileBytes = PyUnicode_AsASCIIString(pCode->co_filename);
				sFuncName = ((PyBytesObject *)pFileBytes)->ob_sval;
			}
			if (pCode && pCode->co_name)
			{
				if (!sFuncName.empty())
					sFuncName += "\\";
				PyNewRef pFuncBytes = PyUnicode_AsASCIIString(pCode->co_name);
				sFuncName += ((PyBytesObject *)pFuncBytes)->ob_sval;
			}

			switch (what)
			{
				case PyTrace_CALL:
					pModState->pPlugin->Log(LOG_NORM, "(%s) Calling function at line %d in '%s'", pModState->pPlugin->m_Name.c_str(), lineno, sFuncName.c_str());
					break;
				case PyTrace_RETURN:
					pModState->pPlugin->Log(LOG_NORM, "(%s) Returning from line %d in '%s'", pModState->pPlugin->m_Name.c_str(), lineno, sFuncName.c_str());
					break;
				case PyTrace_EXCEPTION:
					pModState->pPlugin->Log(LOG_NORM, "(%s) Exception at line %d in '%s'", pModState->pPlugin->m_Name.c_str(), lineno, sFuncName.c_str());
					break;
			}
		}

		return 0;
	}

	int PyDomoticz_TraceFunc(PyObject *self, PyFrameObject *frame, int what, PyObject *arg)
	{
		module_state *pModState = ((struct module_state *)PyModule_GetState(self));
		if (!pModState)
		{
			_log.Log(LOG_ERROR, "CPlugin:%s, unable to obtain module state.", __func__);
		}
		else if (!pModState->pPlugin)
		{
			_log.Log(LOG_ERROR, "CPlugin:%s, illegal operation, Plugin has not started yet.", __func__);
		}
		else
		{
			int lineno = PyFrame_GetLineNumber(frame);
			std::string sFuncName = "Unknown";
			PyCodeObject *pCode = frame->f_code;
			if (pCode && pCode->co_filename)
			{
				PyNewRef	pFileBytes = PyUnicode_AsASCIIString(pCode->co_filename);
				sFuncName = ((PyBytesObject *)pFileBytes)->ob_sval;
			}
			if (pCode && pCode->co_name)
			{
				if (!sFuncName.empty())
					sFuncName += "\\";
				PyNewRef	pFuncBytes = PyUnicode_AsASCIIString(pCode->co_name);
				sFuncName += ((PyBytesObject *)pFuncBytes)->ob_sval;
			}

			switch (what)
			{
				case PyTrace_CALL:
					pModState->pPlugin->Log(LOG_NORM, "(%s) Calling function at line %d in '%s'", pModState->pPlugin->m_Name.c_str(), lineno, sFuncName.c_str());
					break;
				case PyTrace_LINE:
					pModState->pPlugin->Log(LOG_NORM, "(%s) Executing line %d in '%s'", pModState->pPlugin->m_Name.c_str(), lineno, sFuncName.c_str());
					break;
				case PyTrace_EXCEPTION:
					pModState->pPlugin->Log(LOG_NORM, "(%s) Exception at line %d in '%s'", pModState->pPlugin->m_Name.c_str(), lineno, sFuncName.c_str());
					break;
			}
		}

		return 0;
	}

	static PyObject *PyDomoticz_Debug(PyObject *self, PyObject *args)
	{
		module_state *pModState = ((struct module_state *)PyModule_GetState(self));
		if (!pModState)
		{
			_log.Log(LOG_ERROR, "CPlugin:PyDomoticz_Debug, unable to obtain module state.");
		}
		else if (!pModState->pPlugin)
		{
			_log.Log(LOG_ERROR, "CPlugin:PyDomoticz_Debug, illegal operation, Plugin has not started yet.");
		}
		else
		{
			if (pModState->pPlugin->m_bDebug & PDM_PYTHON)
			{
				char *msg;
				if (!PyArg_ParseTuple(args, "s", &msg))
				{
					// TODO: Dump data to aid debugging
					pModState->pPlugin->Log(LOG_ERROR, "(%s) PyDomoticz_Debug failed to parse parameters: string expected.", pModState->pPlugin->m_Name.c_str());
					LogPythonException(pModState->pPlugin, std::string(__func__));
				}
				else
				{
					std::string message = "(" + pModState->pPlugin->m_Name + ") " + msg;
					pModState->pPlugin->Log((_eLogLevel)LOG_NORM, message);
				}
			}
		}

		Py_RETURN_NONE;
	}

	static PyObject *PyDomoticz_Log(PyObject *self, PyObject *args)
	{
		module_state *pModState = ((struct module_state *)PyModule_GetState(self));
		if (!pModState)
		{
			_log.Log(LOG_ERROR, "CPlugin:PyDomoticz_Log, unable to obtain module state.");
		}
		else if (!pModState->pPlugin)
		{
			_log.Log(LOG_ERROR, "CPlugin:PyDomoticz_Log, illegal operation, Plugin has not started yet.");
		}
		else
		{
			char *msg;
			if (!PyArg_ParseTuple(args, "s", &msg))
			{
				pModState->pPlugin->Log(LOG_ERROR, "(%s) PyDomoticz_Log failed to parse parameters: string expected.", pModState->pPlugin->m_Name.c_str());
				LogPythonException(pModState->pPlugin, std::string(__func__));
			}
			else
			{
				std::string message = "(" + pModState->pPlugin->m_Name + ") " + msg;
				pModState->pPlugin->Log((_eLogLevel)LOG_NORM, message);
			}
		}

		Py_RETURN_NONE;
	}

	static PyObject *PyDomoticz_Status(PyObject *self, PyObject *args)
	{
		module_state *pModState = ((struct module_state *)PyModule_GetState(self));
		if (!pModState)
		{
			_log.Log(LOG_ERROR, "CPlugin:%s, unable to obtain module state.", std::string(__func__).c_str());
		}
		else if (!pModState->pPlugin)
		{
			_log.Log(LOG_ERROR, "CPlugin:%s, illegal operation, Plugin has not started yet.", std::string(__func__).c_str());
		}
		else
		{
			char *msg;
			if (!PyArg_ParseTuple(args, "s", &msg))
			{
				pModState->pPlugin->Log(LOG_ERROR, "(%s) %s failed to parse parameters: string expected.", pModState->pPlugin->m_Name.c_str(), std::string(__func__).c_str());
				LogPythonException(pModState->pPlugin, std::string(__func__));
			}
			else
			{
				std::string message = "(" + pModState->pPlugin->m_Name + ") " + msg;
				pModState->pPlugin->Log((_eLogLevel)LOG_STATUS, message);
			}
		}

		Py_RETURN_NONE;
	}

	static PyObject *PyDomoticz_Error(PyObject *self, PyObject *args)
	{
		module_state *pModState = ((struct module_state *)PyModule_GetState(self));
		if (!pModState)
		{
			_log.Log(LOG_ERROR, "CPlugin:PyDomoticz_Error, unable to obtain module state.");
		}
		else if (!pModState->pPlugin)
		{
			_log.Log(LOG_ERROR, "CPlugin:PyDomoticz_Error, illegal operation, Plugin has not started yet.");
		}
		else
		{
			char *msg;
			if (!PyArg_ParseTuple(args, "s", &msg))
			{
				// TODO: Dump data to aid debugging
				pModState->pPlugin->Log(LOG_ERROR, "(%s) PyDomoticz_Error failed to parse parameters: string expected.", pModState->pPlugin->m_Name.c_str());
				LogPythonException(pModState->pPlugin, std::string(__func__));
			}
			else
			{
				std::string message = "(" + pModState->pPlugin->m_Name + ") " + msg;
				pModState->pPlugin->Log((_eLogLevel)LOG_ERROR, message);
			}
		}

		Py_RETURN_NONE;
	}

	static PyObject *PyDomoticz_Debugging(PyObject *self, PyObject *args)
	{
		module_state *pModState = ((struct module_state *)PyModule_GetState(self));
		if (!pModState)
		{
			_log.Log(LOG_ERROR, "CPlugin:PyDomoticz_Debugging, unable to obtain module state.");
		}
		else if (!pModState->pPlugin)
		{
			_log.Log(LOG_ERROR, "CPlugin:PyDomoticz_Debugging, illegal operation, Plugin has not started yet.");
		}
		else
		{
			unsigned int type;
			if (!PyArg_ParseTuple(args, "i", &type))
			{
				pModState->pPlugin->Log(LOG_ERROR, "(%s) failed to parse parameters, integer expected.", pModState->pPlugin->m_Name.c_str());
				LogPythonException(pModState->pPlugin, std::string(__func__));
			}
			else
			{
				// Maintain backwards compatibility
				if (type == 1)
					type = PDM_ALL;

				pModState->pPlugin->m_bDebug = (PluginDebugMask)type;
				pModState->pPlugin->Log(LOG_NORM, "(%s) Debug logging mask set to: %s%s%s%s%s%s%s%s%s", pModState->pPlugin->m_Name.c_str(), (type == PDM_NONE ? "NONE" : ""),
					 (type & PDM_PYTHON ? "PYTHON " : ""), (type & PDM_PLUGIN ? "PLUGIN " : ""), (type & PDM_QUEUE ? "QUEUE " : ""), (type & PDM_IMAGE ? "IMAGE " : ""),
					 (type & PDM_DEVICE ? "DEVICE " : ""), (type & PDM_CONNECTION ? "CONNECTION " : ""), (type & PDM_MESSAGE ? "MESSAGE " : ""), (type == PDM_ALL ? "ALL" : ""));
			}
		}

		Py_RETURN_NONE;
	}

	static PyObject *PyDomoticz_Heartbeat(PyObject *self, PyObject *args)
	{
		module_state *pModState = ((struct module_state *)PyModule_GetState(self));
		if (!pModState)
		{
			_log.Log(LOG_ERROR, "CPlugin:PyDomoticz_Heartbeat, unable to obtain module state.");
		}
		else if (!pModState->pPlugin)
		{
			_log.Log(LOG_ERROR, "CPlugin:PyDomoticz_Heartbeat, illegal operation, Plugin has not started yet.");
		}
		else
		{
			int iPollinterval;
			if (!PyArg_ParseTuple(args, "i", &iPollinterval))
			{
				pModState->pPlugin->Log(LOG_ERROR, "(%s) failed to parse parameters, integer expected.", pModState->pPlugin->m_Name.c_str());
				LogPythonException(pModState->pPlugin, std::string(__func__));
			}
			else
			{
				//	Add heartbeat command to message queue
				pModState->pPlugin->MessagePlugin(new PollIntervalDirective(pModState->pPlugin, iPollinterval));
			}
		}

		Py_RETURN_NONE;
	}

	static PyObject *PyDomoticz_Notifier(PyObject *self, PyObject *args)
	{
		module_state *pModState = ((struct module_state *)PyModule_GetState(self));
		if (!pModState)
		{
			_log.Log(LOG_ERROR, "CPlugin:%s, unable to obtain module state.", __func__);
		}
		else if (!pModState->pPlugin)
		{
			_log.Log(LOG_ERROR, "CPlugin:%s, illegal operation, Plugin has not started yet.", __func__);
		}
		else
		{
			char *szNotifier;
			if (!PyArg_ParseTuple(args, "s", &szNotifier))
			{
				pModState->pPlugin->Log(LOG_ERROR, "(%s) failed to parse parameters, Notifier Name expected.", pModState->pPlugin->m_Name.c_str());
				LogPythonException(pModState->pPlugin, std::string(__func__));
			}
			else
			{
				std::string sNotifierName = szNotifier;
				if ((sNotifierName.empty()) || (sNotifierName.find_first_of(' ') != std::string::npos))
				{
					pModState->pPlugin->Log(LOG_ERROR, "(%s) failed to parse parameters, valid Notifier Name expected, received '%s'.", pModState->pPlugin->m_Name.c_str(),
								szNotifier);
				}
				else
				{
					//	Add notifier command to message queue
					pModState->pPlugin->MessagePlugin(new NotifierDirective(pModState->pPlugin, szNotifier));
				}
			}
		}

		Py_RETURN_NONE;
	}

	static PyObject *PyDomoticz_Trace(PyObject *self, PyObject *args)
	{
		module_state *pModState = ((struct module_state *)PyModule_GetState(self));
		if (!pModState)
		{
			_log.Log(LOG_ERROR, "CPlugin:%s, unable to obtain module state.", __func__);
		}
		else if (!pModState->pPlugin)
		{
			_log.Log(LOG_ERROR, "CPlugin:%s, illegal operation, Plugin has not started yet.", __func__);
		}
		else
		{
			int bTrace = 0;
			if (!PyArg_ParseTuple(args, "p", &bTrace))
			{
				pModState->pPlugin->Log(LOG_ERROR, "(%s) failed to parse parameter, True/False expected.", pModState->pPlugin->m_Name.c_str());
				LogPythonException(pModState->pPlugin, std::string(__func__));
			}
			else
			{
				pModState->pPlugin->m_bTracing = (bool)bTrace;
				pModState->pPlugin->Log(LOG_NORM, "(%s) Low level Python tracing %s.", pModState->pPlugin->m_Name.c_str(), (pModState->pPlugin->m_bTracing ? "ENABLED" : "DISABLED"));

				if (pModState->pPlugin->m_bTracing)
				{
					PyEval_SetProfile(PyDomoticz_ProfileFunc, self);
					PyEval_SetTrace(PyDomoticz_TraceFunc, self);
				}
				else
				{
					PyEval_SetProfile(nullptr, nullptr);
					PyEval_SetTrace(nullptr, nullptr);
				}
			}
		}

		Py_RETURN_NONE;
	}

	static PyObject *PyDomoticz_Configuration(PyObject *self, PyObject *args, PyObject *kwds)
	{
		PyObject *pConfig = Py_None;
		std::string sConfig;
		std::vector<std::vector<std::string>> result;

		Py_INCREF(Py_None);

		module_state *pModState = ((struct module_state *)PyModule_GetState(self));
		if (!pModState)
		{
			_log.Log(LOG_ERROR, "CPlugin:%s, unable to obtain module state.", __func__);
		}
		else if (!pModState->pPlugin)
		{
			_log.Log(LOG_ERROR, "CPlugin:%s, illegal operation, Plugin has not started yet.", __func__);
		}
		else
		{
			CPluginProtocolJSON jsonProtocol;
			PyObject *pNewConfig = nullptr;
			static char *kwlist[] = { "Config", nullptr };
			if (PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &pNewConfig))
			{
				// Python object supplied if it is not a dictionary
				if (!PyDict_Check(pNewConfig))
				{
					pModState->pPlugin->Log(LOG_ERROR, "CPlugin:%s, Function expects no parameter or a Dictionary.", __func__);
					return pConfig;
				}
				//  Convert to JSON and store
				sConfig = jsonProtocol.PythontoJSON(pNewConfig);

				// Update database
				m_sql.safe_query("UPDATE Hardware SET Configuration='%q' WHERE (ID == %d)", sConfig.c_str(), pModState->pPlugin->m_HwdID);
			}
			PyErr_Clear();

			// Read the configuration
			result = m_sql.safe_query("SELECT Configuration FROM Hardware WHERE (ID==%d)", pModState->pPlugin->m_HwdID);
			if (result.empty())
			{
				pModState->pPlugin->Log(LOG_ERROR, "CPlugin:%s, Hardware ID not found in database '%d'.", __func__, pModState->pPlugin->m_HwdID);
				return pConfig;
			}

			// Build a Python structure to return
			sConfig = result[0][0];
			if (sConfig.empty())
				sConfig = "{}";
			pConfig = jsonProtocol.JSONtoPython(sConfig);
			Py_DECREF(Py_None);
		}

		return pConfig;
	}

	static PyObject *PyDomoticz_Register(PyObject *self, PyObject *args, PyObject *kwds)
	{
		static char *kwlist[] = { "Device", "Unit", NULL };
		module_state *pModState = ((struct module_state *)PyModule_GetState(self));
		if (!pModState)
		{
			_log.Log(LOG_ERROR, "CPlugin:PyDomoticz_Log, unable to obtain module state.");
		}
		else
		{
			PyTypeObject *pDeviceClass = NULL;
			PyTypeObject *pUnitClass = NULL;
			if (!PyArg_ParseTupleAndKeywords(args, kwds, "O|O", kwlist, &pDeviceClass, &pUnitClass))
			{
				_log.Log(LOG_ERROR, "(%s) PyDomoticz_Register failed to parse parameters: Python class name expected.", pModState->pPlugin->m_Name.c_str());
				LogPythonException(pModState->pPlugin, std::string(__func__));
			}
			else
			{
				if (pDeviceClass)
				{
					PyTypeObject *pBaseClass = pDeviceClass->tp_base;
					while (pBaseClass)
					{
						if (pBaseClass->tp_name == pModState->pDeviceClass->tp_name)
						{
							_log.Log((_eLogLevel)LOG_NORM, "Class '%s' registered to override '%s'.", pDeviceClass->tp_name, pModState->pDeviceClass->tp_name);
							pModState->pDeviceClass = pDeviceClass;
							break;
						}
						pBaseClass = pBaseClass->tp_base;
					}
					if (pDeviceClass->tp_name != pModState->pDeviceClass->tp_name)
					{
						_log.Log((_eLogLevel)LOG_ERROR, "Class '%s' registration failed, Device is not derived from '%s'", pDeviceClass->tp_name, pModState->pDeviceClass->tp_name);
					}
				}
				if (pUnitClass)
				{
					PyTypeObject *pBaseClass = pUnitClass->tp_base;
					while (pBaseClass)
					{
						if (pBaseClass->tp_name == pModState->pUnitClass->tp_name)
						{
							_log.Log((_eLogLevel)LOG_NORM, "Class '%s' registered to override '%s'.", pDeviceClass->tp_name, pModState->pUnitClass->tp_name);
							pModState->pUnitClass = pUnitClass;
							break;
						}
						pBaseClass = pBaseClass->tp_base;
					}
					if (pUnitClass->tp_name != pModState->pUnitClass->tp_name)
					{
						_log.Log((_eLogLevel)LOG_ERROR, "Class '%s' registration failed, Unit is not derived from '%s'", pDeviceClass->tp_name, pModState->pDeviceClass->tp_name);
					}
				}
			}
		}

		Py_RETURN_NONE;
	}

	static PyMethodDef DomoticzMethods[] = { { "Debug", PyDomoticz_Debug, METH_VARARGS, "Write a message to Oikomaticz log only if verbose logging is turned on." },
						 { "Log", PyDomoticz_Log, METH_VARARGS, "Write a message to Oikomaticz log." },
						 { "Status", PyDomoticz_Status, METH_VARARGS, "Write a status message to Oikomaticz log." },
						 { "Error", PyDomoticz_Error, METH_VARARGS, "Write an error message to Oikomaticz log." },
						 { "Debugging", PyDomoticz_Debugging, METH_VARARGS, "Set logging level. 1 set verbose logging, all other values use default level" },
						 { "Heartbeat", PyDomoticz_Heartbeat, METH_VARARGS, "Set the heartbeat interval, default 10 seconds." },
						 { "Notifier", PyDomoticz_Notifier, METH_VARARGS, "Enable notification handling with supplied name." },
						 { "Trace", PyDomoticz_Trace, METH_VARARGS, "Enable/Disable line level Python tracing." },
						 { "Configuration", (PyCFunction)PyDomoticz_Configuration, METH_VARARGS | METH_KEYWORDS, "Retrieve and Store structured plugin configuration." },
						 { "Register", (PyCFunction)PyDomoticz_Register, METH_VARARGS | METH_KEYWORDS, "Register Device override class." },
						 { nullptr, nullptr, 0, nullptr } };

	static int DomoticzTraverse(PyObject *m, visitproc visit, void *arg)
	{
		Py_VISIT(GETSTATE(m)->error);
		return 0;
	}

	static int DomoticzClear(PyObject *m)
	{
		Py_CLEAR(GETSTATE(m)->error);
		return 0;
	}

	struct PyModuleDef DomoticzModuleDef = { PyModuleDef_HEAD_INIT, "Domoticz", nullptr, sizeof(struct module_state), DomoticzMethods, nullptr, DomoticzTraverse, DomoticzClear, nullptr };

	PyMODINIT_FUNC PyInit_Domoticz(void)
	{

		// This is called during the import of the plugin module
		// triggered by the "import Domoticz" statement
		PyObject *pModule = PyModule_Create2(&DomoticzModuleDef, PYTHON_API_VERSION);
		module_state *pModState = ((struct module_state *)PyModule_GetState(pModule));

		if (PyType_Ready(&CDeviceType) < 0)
		{
			_log.Log(LOG_ERROR, "%s, Device Type not ready.", __func__);
			return pModule;
		}
		Py_INCREF((PyObject *)&CDeviceType);
		PyModule_AddObject(pModule, "Device", (PyObject *)&CDeviceType);
		pModState->pDeviceClass = &CDeviceType;
		pModState->pUnitClass = nullptr;

		if (PyType_Ready(&CConnectionType) < 0)
		{
			_log.Log(LOG_ERROR, "%s, Connection Type not ready.", __func__);
			return pModule;
		}
		Py_INCREF((PyObject *)&CConnectionType);
		PyModule_AddObject(pModule, "Connection", (PyObject *)&CConnectionType);

		if (PyType_Ready(&CImageType) < 0)
		{
			_log.Log(LOG_ERROR, "%s, Image Type not ready.", __func__);
			return pModule;
		}
		Py_INCREF((PyObject *)&CImageType);
		PyModule_AddObject(pModule, "Image", (PyObject *)&CImageType);

		return pModule;
	}

	struct PyModuleDef DomoticzExModuleDef = { PyModuleDef_HEAD_INIT, "DomoticzEx", nullptr, sizeof(struct module_state), DomoticzMethods, nullptr, DomoticzTraverse, DomoticzClear, nullptr };

	PyMODINIT_FUNC PyInit_DomoticzEx(void)
	{

		// This is called during the import of the plugin module
		// triggered by the "import Domoticz" statement
		PyObject *pModule = PyModule_Create2(&DomoticzExModuleDef, PYTHON_API_VERSION);
		module_state *pModState = ((struct module_state *)PyModule_GetState(pModule));

		if (PyType_Ready(&CDeviceExType) < 0)
		{
			_log.Log(LOG_ERROR, "%s, Device Type not ready.", __func__);
			return pModule;
		}
		Py_INCREF((PyObject *)&CDeviceExType);
		PyModule_AddObject(pModule, "Device", (PyObject *)&CDeviceExType);
		pModState->pDeviceClass = &CDeviceExType;

		if (PyType_Ready(&CUnitExType) < 0)
		{
			_log.Log(LOG_ERROR, "%s, Unit Type not ready.", __func__);
			return pModule;
		}
		Py_INCREF((PyObject *)&CUnitExType);
		PyModule_AddObject(pModule, "Unit", (PyObject *)&CUnitExType);
		pModState->pUnitClass = &CUnitExType;

		if (PyType_Ready(&CConnectionType) < 0)
		{
			_log.Log(LOG_ERROR, "%s, Connection Type not ready.", __func__);
			return pModule;
		}
		Py_INCREF((PyObject *)&CConnectionType);
		PyModule_AddObject(pModule, "Connection", (PyObject *)&CConnectionType);

		if (PyType_Ready(&CImageType) < 0)
		{
			_log.Log(LOG_ERROR, "%s, Image Type not ready.", __func__);
			return pModule;
		}
		Py_INCREF((PyObject *)&CImageType);
		PyModule_AddObject(pModule, "Image", (PyObject *)&CImageType);

		return pModule;
	}

	CPlugin::CPlugin(const int HwdID, const std::string &sName, const std::string &sPluginKey)
		: m_iPollInterval(10)
		, m_PyInterpreter(nullptr)
		, m_PyModule(nullptr)
		, m_Notifier(nullptr)
		, m_PluginKey(sPluginKey)
		, m_DeviceDict(nullptr)
		, m_ImageDict(nullptr)
		, m_SettingsDict(nullptr)
		, m_bDebug(PDM_NONE)
	{
		m_HwdID = HwdID;
		m_Name = sName;
		m_bIsStarted = false;
		m_bIsStarting = false;
		m_bTracing = false;
	}

	CPlugin::~CPlugin()
	{
		m_bIsStarted = false;
	}

	void CPlugin::LogPythonException()
	{
		PyTracebackObject *pTraceback;
		PyNewRef	pExcept;
		PyNewRef	pValue;

		PyErr_Fetch(&pExcept, &pValue, (PyObject **)&pTraceback);
		PyErr_NormalizeException(&pExcept, &pValue, (PyObject **)&pTraceback);
		PyErr_Clear();

		if (pExcept)
		{
			Log(LOG_ERROR, "(%s) Module Import failed, exception: '%s'", m_Name.c_str(), ((PyTypeObject *)pExcept)->tp_name);
		}
		if (pValue)
		{
			std::string sError;
			PyNewRef	pErrBytes = PyUnicode_AsASCIIString(pValue); // Won't normally return text for Import related errors
			if (!pErrBytes)
			{
				// ImportError has name and path attributes
				PyErr_Clear();
				if (PyObject_HasAttrString(pValue, "path"))
				{
					PyNewRef	pString = PyObject_GetAttrString(pValue, "path");
					PyNewRef	pBytes = PyUnicode_AsASCIIString(pString);
					if (pBytes)
					{
						sError += "Path: ";
						sError += ((PyBytesObject *)pBytes)->ob_sval;
					}
				}
				PyErr_Clear();
				if (PyObject_HasAttrString(pValue, "name"))
				{
					PyNewRef	pString = PyObject_GetAttrString(pValue, "name");
					PyNewRef	pBytes = PyUnicode_AsASCIIString(pString);
					if (pBytes)
					{
						sError += " Name: ";
						sError += ((PyBytesObject *)pBytes)->ob_sval;
					}
				}
				if (!sError.empty())
				{
					Log(LOG_ERROR, "(%s) Module Import failed: '%s'", m_Name.c_str(), sError.c_str());
					sError = "";
				}

				// SyntaxError, IndentationError & TabError have filename, lineno, offset and text attributes
				PyErr_Clear();
				if (PyObject_HasAttrString(pValue, "filename"))
				{
					PyNewRef pString = PyObject_GetAttrString(pValue, "filename");
					PyNewRef pBytes = PyUnicode_AsASCIIString(pString);
					sError += "File: ";
					sError += ((PyBytesObject *)pBytes)->ob_sval;
				}
				long long lineno = -1;
				long long offset = -1;
				PyErr_Clear();
				if (PyObject_HasAttrString(pValue, "lineno"))
				{
					PyNewRef pString = PyObject_GetAttrString(pValue, "lineno");
					lineno = PyLong_AsLongLong(pString);
				}
				PyErr_Clear();
				if (PyObject_HasAttrString(pValue, "offset"))
				{
					PyNewRef pString = PyObject_GetAttrString(pValue, "offset");
					offset = PyLong_AsLongLong(pString);
				}

				if (!sError.empty())
				{
					if ((lineno > 0) && (lineno < 1000))
					{
						Log(LOG_ERROR, "(%s) Import detail: %s, Line: %lld, offset: %lld", m_Name.c_str(), sError.c_str(), lineno, offset);
					}
					else
					{
						Log(LOG_ERROR, "(%s) Import detail: %s, Line: %lld", m_Name.c_str(), sError.c_str(), offset);
					}
					sError = "";
				}

				PyErr_Clear();
				if (PyObject_HasAttrString(pValue, "text"))
				{
					PyNewRef pText = PyObject_GetAttrString(pValue, "text");
					PyNewRef pString = PyObject_Str(pText);
					std::string sUTF = PyUnicode_AsUTF8(pString);
					Log(LOG_ERROR, "(%s) Error Line '%s'", m_Name.c_str(), sUTF.c_str());
				}
				else
				{
					Log(LOG_ERROR, "(%s) Error Line details not available.", m_Name.c_str());
				}

				if (!sError.empty())
				{
					Log(LOG_ERROR, "(%s) Import detail: %s", m_Name.c_str(), sError.c_str());
				}
			}
			else
				Log(LOG_ERROR, "(%s) Module Import failed '%s'", m_Name.c_str(), ((PyBytesObject *)pErrBytes)->ob_sval);
		}

		if (!pExcept && !pValue && !pTraceback)
		{
			Log(LOG_ERROR, "(%s) Call to import module failed, unable to decode exception.", m_Name.c_str());
		}

		if (pTraceback)
			Py_XDECREF(pTraceback);
	}

	void CPlugin::LogPythonException(const std::string &sHandler)
	{
		PyTracebackObject *pTraceback;
		PyNewRef	pExcept;
		PyNewRef	pValue;
		PyTypeObject *TypeName;
		PyNewRef	pErrBytes = nullptr;
		const char *pTypeText = nullptr;

		PyErr_Fetch(&pExcept, &pValue, (PyObject **)&pTraceback);

		if (pExcept)
		{
			TypeName = (PyTypeObject *)pExcept;
			pTypeText = TypeName->tp_name;
		}
		if (pValue)
		{
			pErrBytes = PyUnicode_AsASCIIString(pValue);
		}
		if (pTypeText && pErrBytes)
		{
			Log(LOG_ERROR, "(%s) '%s' failed '%s':'%s'.", m_Name.c_str(), sHandler.c_str(), pTypeText, ((PyBytesObject *)pErrBytes)->ob_sval);
		}
		if (pTypeText && !pErrBytes)
		{
			Log(LOG_ERROR, "(%s) '%s' failed '%s'.", m_Name.c_str(), sHandler.c_str(), pTypeText);
		}
		if (!pTypeText && pErrBytes)
		{
			Log(LOG_ERROR, "(%s) '%s' failed '%s'.", m_Name.c_str(), sHandler.c_str(), ((PyBytesObject *)pErrBytes)->ob_sval);
		}
		if (!pTypeText && !pErrBytes)
		{
			Log(LOG_ERROR, "(%s) '%s' failed, unable to determine error.", m_Name.c_str(), sHandler.c_str());
		}

		// Log a stack trace if there is one
		PyTracebackObject *pTraceFrame = pTraceback;
		while (pTraceFrame)
		{
			PyFrameObject *frame = pTraceFrame->tb_frame;
			if (frame)
			{
				int lineno = PyFrame_GetLineNumber(frame);
				PyCodeObject *pCode = frame->f_code;
				std::string FileName;
				if (pCode->co_filename)
				{
					PyNewRef pFileBytes = PyUnicode_AsASCIIString(pCode->co_filename);
					FileName = ((PyBytesObject*)pFileBytes)->ob_sval;
				}
				std::string FuncName = "Unknown";
				if (pCode->co_name)
				{
					PyNewRef pFuncBytes = PyUnicode_AsASCIIString(pCode->co_name);
					FuncName = ((PyBytesObject*)pFuncBytes)->ob_sval;
				}
				if (!FileName.empty())
					Log(LOG_ERROR, "(%s) ----> Line %d in '%s', function %s", m_Name.c_str(), lineno, FileName.c_str(), FuncName.c_str());
				else
					Log(LOG_ERROR, "(%s) ----> Line %d in '%s'", m_Name.c_str(), lineno, FuncName.c_str());
			}
			pTraceFrame = pTraceFrame->tb_next;
		}

		if (!pExcept && !pValue && !pTraceback)
		{
			Log(LOG_ERROR, "(%s) Call to message handler '%s' failed, unable to decode exception.", m_Name.c_str(), sHandler.c_str());
		}

		if (pTraceback)
			Py_XDECREF(pTraceback);
	}

	int CPlugin::PollInterval(int Interval)
	{
		if (Interval > 0)
			m_iPollInterval = Interval;
		if (m_bDebug & PDM_PLUGIN)
			Log(LOG_NORM, "(%s) Heartbeat interval set to: %d.", m_Name.c_str(), m_iPollInterval);
		return m_iPollInterval;
	}

	void CPlugin::Notifier(const std::string &Notifier)
	{
		delete m_Notifier;
		m_Notifier = nullptr;
		if (m_bDebug & PDM_PLUGIN)
			Log(LOG_NORM, "(%s) Notifier Name set to: %s.", m_Name.c_str(), Notifier.c_str());
		m_Notifier = new CPluginNotifier(this, Notifier);
	}

	void CPlugin::AddConnection(CPluginTransport *pTransport)
	{
		std::lock_guard<std::mutex> l(m_TransportsMutex);
		m_Transports.push_back(pTransport);
	}

	void CPlugin::RemoveConnection(CPluginTransport *pTransport)
	{
		std::lock_guard<std::mutex> l(m_TransportsMutex);
		for (auto itt = m_Transports.begin(); itt != m_Transports.end(); itt++)
		{
			CPluginTransport *pPluginTransport = *itt;
			if (pTransport == pPluginTransport)
			{
				m_Transports.erase(itt);
				break;
			}
		}
	}

	bool CPlugin::StartHardware()
	{
		if (m_bIsStarted)
			StopHardware();

		RequestStart();

		// Flush the message queue (should already be empty)
		{
			std::lock_guard<std::mutex> l(m_QueueMutex);
			while (!m_MessageQueue.empty())
			{
				m_MessageQueue.pop_front();
			}
		}

		// Start worker thread
		try
		{
			std::lock_guard<std::mutex> l(m_QueueMutex);
			m_thread = std::make_shared<std::thread>(&CPlugin::Do_Work, this);
			if (!m_thread)
			{
				_log.Log(LOG_ERROR, "Failed start interface worker thread.");
			}
			else
			{
				SetThreadName(m_thread->native_handle(), m_Name.c_str());
				_log.Log(LOG_NORM, "%s hardware started.", m_Name.c_str());
			}
		}
		catch (...)
		{
			_log.Log(LOG_ERROR, "Exception caught in '%s'.", __func__);
		}

		//	Add start command to message queue
		m_bIsStarting = true;
		MessagePlugin(new InitializeMessage(this));

		Log(LOG_STATUS, "(%s) Started.", m_Name.c_str());

		return true;
	}

	bool CPlugin::StopHardware()
	{
		try
		{
			Log(LOG_STATUS, "(%s) Stop directive received.", m_Name.c_str());

			// loop on plugin to finish startup
			while (m_bIsStarting)
			{
				sleep_milliseconds(100);
			}

			RequestStop();

			if (m_bIsStarted)
			{
				// If we have connections queue disconnects
				if (!m_Transports.empty())
				{
					std::lock_guard<std::mutex> lPython(PythonMutex); // Take mutex to guard access to CPluginTransport::m_pConnection
											  // TODO: Must take before m_TransportsMutex to avoid deadlock, try to improve to allow only taking when needed
					std::lock_guard<std::mutex> lTransports(m_TransportsMutex);
					for (const auto &pPluginTransport : m_Transports)
					{
						// Tell transport to disconnect if required
						if (pPluginTransport)
						{
							// std::lock_guard<std::mutex> l(PythonMutex); // Take mutex to guard access to CPluginTransport::m_pConnection
							MessagePlugin(new DisconnectDirective(this, pPluginTransport->Connection()));
						}
					}
				}
				else
				{
					// otherwise just signal stop
					MessagePlugin(new onStopCallback(this));
				}

				// loop on stop to be processed
				while (m_bIsStarted)
				{
					sleep_milliseconds(100);
				}
			}

			Log(LOG_STATUS, "(%s) Stopping threads.", m_Name.c_str());

			if (m_thread)
			{
				m_thread->join();
				m_thread.reset();
			}

			if (m_Notifier)
			{
				delete m_Notifier;
				m_Notifier = nullptr;
			}

			if (m_PyInterpreter) {
				Log(LOG_STATUS, "(%s) Stopping python interpreter.", m_Name.c_str());
				RestoreThread();

				Py_EndInterpreter((PyThreadState *)m_PyInterpreter);
				m_PyInterpreter = nullptr;

				CPluginSystem pManager;
				PyThreadState_Swap((PyThreadState *)pManager.PythonThread());
				PyEval_ReleaseLock();
			}
		}
		catch (...)
		{
			// Don't throw from a Stop command
		}

		Log(LOG_STATUS, "(%s) Stopped.", m_Name.c_str());

		return true;
	}

	void CPlugin::Do_Work()
	{
		Log(LOG_STATUS, "(%s) Entering work loop.", m_Name.c_str());
		m_LastHeartbeat = mytime(nullptr);
		while (!IsStopRequested(50) || !m_bIsStopped)
		{
			time_t Now = time(nullptr);
			bool bProcessed = true;
			while (bProcessed)
			{
				CPluginMessageBase *Message = nullptr;
				bProcessed = false;

				// Cycle once through the queue looking for the 1st message that is ready to process
				{
					std::lock_guard<std::mutex> l(m_QueueMutex);
					for (size_t i = 0; i < m_MessageQueue.size(); i++)
					{
						CPluginMessageBase *FrontMessage = m_MessageQueue.front();
						m_MessageQueue.pop_front();
						if (!FrontMessage->m_Delay || FrontMessage->m_When <= Now)
						{
							// Message is ready now or was already ready (this is the case for almost all messages)
							Message = FrontMessage;
							break;
						}
						// Message is for sometime in the future so requeue it (this happens when the 'Delay' parameter is used on a Send)
						m_MessageQueue.push_back(FrontMessage);
					}
				}

				if (Message)
				{
					bProcessed = true;
					try
					{
						const CPlugin *pPlugin = Message->Plugin();
						if (pPlugin && (pPlugin->m_bDebug & PDM_QUEUE))
						{
							_log.Log(LOG_NORM, "(" + pPlugin->m_Name + ") Processing '" + std::string(Message->Name()) + "' message");
						}
						Message->Process();
					}
					catch (...)
					{
						_log.Log(LOG_ERROR, "PluginSystem: Exception processing message.");
					}
				}
				// Free the memory for the message
				if (Message)
				{
					std::lock_guard<std::mutex> l(PythonMutex); // Take mutex to guard access to CPluginTransport::m_pConnection inside the message
					CPlugin *pPlugin = (CPlugin *)Message->Plugin();
					pPlugin->RestoreThread();
					delete Message;
					pPlugin->ReleaseThread();
				}
			}

			if (Now >= (m_LastHeartbeat + m_iPollInterval))
			{
				//	Add heartbeat to message queue
				MessagePlugin(new onHeartbeatCallback(this));
				m_LastHeartbeat = mytime(nullptr);
			}

			// Check all connections are still valid, vector could be affected by a disconnect on another thread
			try
			{
				std::lock_guard<std::mutex> lPython(PythonMutex); // Take mutex to guard access to CPluginTransport::m_pConnection
										  // TODO: Must take before m_TransportsMutex to avoid deadlock, try to improve to allow only taking when needed
				std::lock_guard<std::mutex> lTransports(m_TransportsMutex);
				if (!m_Transports.empty())
				{
					for (const auto &pPluginTransport : m_Transports)
					{
						// std::lock_guard<std::mutex> l(PythonMutex); // Take mutex to guard access to CPluginTransport::m_pConnection
						pPluginTransport->VerifyConnection();
					}
				}
			}
			catch (...)
			{
				Log(LOG_NORM, "(%s) Transport vector changed during %s loop, continuing.", m_Name.c_str(), __func__);
			}
		}

		Log(LOG_STATUS, "(%s) Exiting work loop.", m_Name.c_str());
	}

	bool CPlugin::Initialise()
	{
		m_bIsStarted = false;

		try
		{
			PyEval_RestoreThread((PyThreadState *)m_mainworker.m_pluginsystem.PythonThread());
			m_PyInterpreter = Py_NewInterpreter();
			if (!m_PyInterpreter)
			{
				Log(LOG_ERROR, "(%s) failed to create interpreter.", m_PluginKey.c_str());
				goto Error;
			}

			// Prepend plugin directory to path so that python will search it early when importing
#ifdef WIN32
			std::wstring sSeparator = L";";
#else
			std::wstring sSeparator = L":";
#endif
			std::wstringstream ssPath;
			std::string sFind = "key=\"" + m_PluginKey + "\"";
			CPluginSystem Plugins;
			std::map<std::string, std::string> *mPluginXml = Plugins.GetManifest();
			std::string sPluginXML;
			for (const auto &type : *mPluginXml)
			{
				if (type.second.find(sFind) != std::string::npos)
				{
					m_HomeFolder = type.first;
					ssPath << m_HomeFolder.c_str();
					sPluginXML = type.second;
					break;
				}
			}

			std::wstring sPath = ssPath.str() + sSeparator;
			sPath += Py_GetPath();

			try
			{
				//
				//	Python loads the 'site' module automatically and adds extra search directories for module loading
				//	This code makes the plugin framework function the same way
				//
				void *pSiteModule = PyImport_ImportModule("site");
				if (!pSiteModule)
				{
					Log(LOG_ERROR, "(%s) failed to load 'site' module, continuing.", m_PluginKey.c_str());
				}
				else
				{
					PyNewRef	pFunc = PyObject_GetAttrString((PyObject *)pSiteModule, "getsitepackages");
					if (pFunc && PyCallable_Check(pFunc))
					{
						PyNewRef	pSites = PyObject_CallObject(pFunc, nullptr);
						if (!pSites)
						{
							LogPythonException("getsitepackages");
						}
						else
							for (Py_ssize_t i = 0; i < PyList_Size(pSites); i++)
							{
								PyBorrowedRef	pSite = PyList_GetItem(pSites, i);
								if (pSite && PyUnicode_Check(pSite))
								{
									std::wstringstream ssPath;
									ssPath << PyUnicode_AsUTF8(pSite);
									sPath += sSeparator + ssPath.str();
								}
							}
					}
				}
			}
			catch (...)
			{
				Log(LOG_ERROR, "(%s) exception loading 'site' module, continuing.", m_PluginKey.c_str());
				PyErr_Clear();
			}

			// Update the path itself
			PySys_SetPath((wchar_t *)sPath.c_str());

			try
			{
				//
				//	Load the 'faulthandler' module to get a python stackdump during a segfault
				//
				void *pFaultModule = PyImport_ImportModule("faulthandler");
				if (!pFaultModule)
				{
					Log(LOG_ERROR, "(%s) failed to load 'faulthandler' module, continuing.", m_PluginKey.c_str());
				}
				else
				{
					PyNewRef	pFunc = PyObject_GetAttrString((PyObject *)pFaultModule, "enable");
					if (pFunc && PyCallable_Check(pFunc))
					{
						PyNewRef pRetObj = PyObject_CallObject(pFunc, nullptr);
					}
				}
			}
			catch (...)
			{
				Log(LOG_ERROR, "(%s) exception loading 'faulthandler' module, continuing.", m_PluginKey.c_str());
				PyErr_Clear();
			}

			try
			{
				m_PyModule = PyImport_ImportModule("plugin");
				if (!m_PyModule)
				{
					Log(LOG_ERROR, "(%s) failed to load 'plugin.py', Python Path used was '%S'.", m_PluginKey.c_str(), sPath.c_str());
					LogPythonException();
					goto Error;
				}
			}
			catch (...)
			{
				Log(LOG_ERROR, "(%s) exception loading 'plugin.py', Python Path used was '%S'.", m_PluginKey.c_str(), sPath.c_str());
				PyErr_Clear();
			}

			// Oikomaticz callbacks need state so they know which plugin to act on
			PyBorrowedRef	pMod = PyState_FindModule(&DomoticzModuleDef);
			if (!pMod)
			{
				pMod = PyState_FindModule(&DomoticzExModuleDef);
				if (!pMod)
				{
					Log(LOG_ERROR, "(%s) %s failed, Domoticz/DomoticzEx modules not found in interpreter.", __func__, m_PluginKey.c_str());
					goto Error;
				}
			}
			module_state *pModState = ((struct module_state *)PyModule_GetState(pMod));
			pModState->pPlugin = this;

			//	Add start command to message queue
			MessagePlugin(new onStartCallback(this));

			std::string sExtraDetail;
			TiXmlDocument XmlDoc;
			XmlDoc.Parse(sPluginXML.c_str());
			if (XmlDoc.Error())
			{
				Log(LOG_ERROR, "%s: Error '%s' at line %d column %d in XML '%s'.", __func__, XmlDoc.ErrorDesc(), XmlDoc.ErrorRow(), XmlDoc.ErrorCol(), sPluginXML.c_str());
			}
			else
			{
				TiXmlNode *pXmlNode = XmlDoc.FirstChild("plugin");
				for (pXmlNode; pXmlNode; pXmlNode = pXmlNode->NextSiblingElement())
				{
					TiXmlElement *pXmlEle = pXmlNode->ToElement();
					if (pXmlEle)
					{
						const char *pAttributeValue = pXmlEle->Attribute("version");
						if (pAttributeValue)
						{
							m_Version = pAttributeValue;
							sExtraDetail += "version ";
							sExtraDetail += pAttributeValue;
						}
						pAttributeValue = pXmlEle->Attribute("author");
						if (pAttributeValue)
						{
							m_Author = pAttributeValue;
							if (!sExtraDetail.empty())
								sExtraDetail += ", ";
							sExtraDetail += "author '";
							sExtraDetail += pAttributeValue;
							sExtraDetail += "'";
						}
					}
				}
			}
			Log(LOG_STATUS, "(%s) Initialized %s", m_Name.c_str(), sExtraDetail.c_str());

			PyEval_SaveThread();
			return true;
		}
		catch (...)
		{
			Log(LOG_ERROR, "(%s) exception caught in '%s'.", m_PluginKey.c_str(), __func__);
		}

	Error:
		PyEval_SaveThread();
		m_bIsStarting = false;
		return false;
	}

	bool CPlugin::Start()
	{
		try
		{
			PyBorrowedRef	pModuleDict = PyModule_GetDict((PyObject *)m_PyModule); // returns a borrowed referece to the __dict__ object for the module
			PyNewRef		pParamsDict = PyDict_New();
			if (PyDict_SetItemString(pModuleDict, "Parameters", pParamsDict) == -1)
			{
				Log(LOG_ERROR, "(%s) failed to add Parameters dictionary.", m_PluginKey.c_str());
				goto Error;
			}

			PyNewRef pObj = Py_BuildValue("i", m_HwdID);
			if (PyDict_SetItemString(pParamsDict, "HardwareID", pObj) == -1)
			{
				Log(LOG_ERROR, "(%s) failed to add key 'HardwareID', value '%d' to dictionary.", m_PluginKey.c_str(), m_HwdID);
				goto Error;
			}

			std::string sLanguage = "en";
			m_sql.GetPreferencesVar("Language", sLanguage);

			std::vector<std::vector<std::string>> result;
			result = m_sql.safe_query("SELECT Name, Address, Port, SerialPort, Username, Password, Extra, Mode1, Mode2, Mode3, Mode4, Mode5, Mode6 FROM Hardware WHERE (ID==%d)", m_HwdID);
			if (!result.empty())
			{
				for (const auto &sd : result)
				{
					ADD_STRING_TO_DICT(this, pParamsDict, "HomeFolder", m_HomeFolder);
					ADD_STRING_TO_DICT(this, pParamsDict, "StartupFolder", szStartupFolder);
					ADD_STRING_TO_DICT(this, pParamsDict, "UserDataFolder", szUserDataFolder);
					ADD_STRING_TO_DICT(this, pParamsDict, "WebRoot", szWebRoot);
					ADD_STRING_TO_DICT(this, pParamsDict, "Database", dbasefile);
					ADD_STRING_TO_DICT(this, pParamsDict, "Language", sLanguage);
					ADD_STRING_TO_DICT(this, pParamsDict, "Version", m_Version);
					ADD_STRING_TO_DICT(this, pParamsDict, "Author", m_Author);
					ADD_STRING_TO_DICT(this, pParamsDict, "Name", sd[0]);
					ADD_STRING_TO_DICT(this, pParamsDict, "Address", sd[1]);
					ADD_STRING_TO_DICT(this, pParamsDict, "Port", sd[2]);
					ADD_STRING_TO_DICT(this, pParamsDict, "SerialPort", sd[3]);
					ADD_STRING_TO_DICT(this, pParamsDict, "Username", sd[4]);
					ADD_STRING_TO_DICT(this, pParamsDict, "Password", sd[5]);
					ADD_STRING_TO_DICT(this, pParamsDict, "Key", sd[6]);
					ADD_STRING_TO_DICT(this, pParamsDict, "Mode1", sd[7]);
					ADD_STRING_TO_DICT(this, pParamsDict, "Mode2", sd[8]);
					ADD_STRING_TO_DICT(this, pParamsDict, "Mode3", sd[9]);
					ADD_STRING_TO_DICT(this, pParamsDict, "Mode4", sd[10]);
					ADD_STRING_TO_DICT(this, pParamsDict, "Mode5", sd[11]);
					ADD_STRING_TO_DICT(this, pParamsDict, "Mode6", sd[12]);

					ADD_STRING_TO_DICT(this, pParamsDict, "DomoticzVersion", szAppVersion);
					ADD_STRING_TO_DICT(this, pParamsDict, "DomoticzHash", szAppHash);
					ADD_STRING_TO_DICT(this, pParamsDict, "DomoticzBuildTime", szAppDate);
				}
			}

			m_DeviceDict = (PyDictObject*)PyDict_New();
			if (PyDict_SetItemString(pModuleDict, "Devices", (PyObject *)m_DeviceDict) == -1)
			{
				Log(LOG_ERROR, "(%s) failed to add Device dictionary.", m_PluginKey.c_str());
				goto Error;
			}

			std::string tupleStr = "(si)";
			PyBorrowedRef brModule = PyState_FindModule(&DomoticzModuleDef);
			PyBorrowedRef brModuleEx = PyState_FindModule(&DomoticzExModuleDef);

			// Check author has not loaded both Oikomaticz modules
			if ((brModule) && (brModuleEx))
			{
				Log(LOG_ERROR, "(%s) %s failed, Oikomaticz and DomoticzEx modules both found in interpreter, use one or the other.", __func__, m_PluginKey.c_str());
				goto Error;
			}

			if (brModule)
			{
				result = m_sql.safe_query("SELECT '', Unit FROM DeviceStatus WHERE (HardwareID==%d) ORDER BY Unit ASC", m_HwdID);
			}
			else
			{
				brModule = brModuleEx;
				if (!brModule)
				{
					Log(LOG_ERROR, "(%s) %s failed, Domoticz/DomoticzEx modules not found in interpreter.", __func__, m_PluginKey.c_str());
					goto Error;
				}
				result = m_sql.safe_query("SELECT DISTINCT DeviceID, '-1' FROM DeviceStatus WHERE (HardwareID==%d) ORDER BY Unit ASC", m_HwdID);
				tupleStr = "(s)";
			}

			module_state *pModState = ((struct module_state *)PyModule_GetState(brModule));
			if (!pModState)
			{
				Log(LOG_ERROR, "CPlugin:%s, unable to obtain module state.", __func__);
				goto Error;
			}

			// load associated devices to make them available to python
			if (!result.empty())
			{
				PyType_Ready(pModState->pDeviceClass);
				// Add device objects into the device dictionary with Unit as the key
				for (const auto &sd : result)
				{
					// Build argument list
					PyNewRef nrArgList = Py_BuildValue(tupleStr.c_str(), sd[0].c_str(), atoi(sd[1].c_str()));
					if (!nrArgList)
					{
						Log(LOG_ERROR, "Building device argument list failed for key %s/%s.", sd[0].c_str(), sd[1].c_str());
						goto Error;
					}
					PyNewRef pDevice = PyObject_CallObject((PyObject *)pModState->pDeviceClass, nrArgList);
					if (!pDevice)
					{
						Log(LOG_ERROR, "Device object creation failed for key %s/%s.", sd[0].c_str(), sd[1].c_str());
						goto Error;
					}

					// Add the object to the dictionary
					PyNewRef pKey = PyObject_GetAttrString(pDevice, "Key");
					if (!PyDict_Contains((PyObject*)m_DeviceDict, pKey))
					{
						if (PyDict_SetItem((PyObject*)m_DeviceDict, pKey, pDevice) == -1)
						{
							PyNewRef pString = PyObject_Str(pKey);
							std::string sUTF = PyUnicode_AsUTF8(pString);
							Log(LOG_ERROR, "(%s) failed to add key '%s' to device dictionary.", m_PluginKey.c_str(), sUTF.c_str());
							goto Error;
						}
					}

					// Force the object to refresh from the database
					PyNewRef	pRefresh = PyObject_GetAttrString(pDevice, "Refresh");
					PyNewRef	pObj = PyObject_CallNoArgs(pRefresh);
				}
			}

			m_ImageDict = (PyDictObject *)PyDict_New();
			if (PyDict_SetItemString(pModuleDict, "Images", (PyObject *)m_ImageDict) == -1)
			{
				Log(LOG_ERROR, "(%s) failed to add Image dictionary.", m_PluginKey.c_str());
				goto Error;
			}

			// load associated custom images to make them available to python
			result = m_sql.safe_query("SELECT ID, Base, Name, Description FROM CustomImages WHERE Base LIKE '%q%%' ORDER BY ID ASC", m_PluginKey.c_str());
			if (!result.empty())
			{
				PyType_Ready(&CImageType);
				// Add image objects into the image dictionary with ID as the key
				for (const auto &sd : result)
				{
					CImage *pImage = (CImage *)CImage_new(&CImageType, (PyObject *)nullptr, (PyObject *)nullptr);

					PyNewRef	pKey = PyUnicode_FromString(sd[1].c_str());
					if (PyDict_SetItem((PyObject *)m_ImageDict, pKey, (PyObject *)pImage) == -1)
					{
						Log(LOG_ERROR, "(%s) failed to add ID '%s' to image dictionary.", m_PluginKey.c_str(), sd[0].c_str());
						goto Error;
					}
					pImage->ImageID = atoi(sd[0].c_str()) + 100;
					pImage->Base = PyUnicode_FromString(sd[1].c_str());
					pImage->Name = PyUnicode_FromString(sd[2].c_str());
					pImage->Description = PyUnicode_FromString(sd[3].c_str());
					Py_DECREF(pImage);
				}
			}

			LoadSettings();

			m_bIsStarted = true;
			m_bIsStarting = false;
			m_bIsStopped = false;
			return true;
		}
		catch (...)
		{
			Log(LOG_ERROR, "(%s) exception caught in '%s'.", m_PluginKey.c_str(), __func__);
		}

	Error:
		m_bIsStarting = false;
		return false;
	}

	void CPlugin::ConnectionProtocol(CDirectiveBase *pMess)
	{
		ProtocolDirective *pMessage = (ProtocolDirective *)pMess;
		CConnection *pConnection = pMessage->m_pConnection;
		if (m_Notifier)
		{
			delete pConnection->pProtocol;
			pConnection->pProtocol = nullptr;
		}
		std::string sProtocol = PyUnicode_AsUTF8(pConnection->Protocol);
		pConnection->pProtocol = CPluginProtocol::Create(sProtocol);
		if (m_bDebug & PDM_CONNECTION)
			Log(LOG_NORM, "(%s) Protocol set to: '%s'.", m_Name.c_str(), sProtocol.c_str());
	}

	void CPlugin::ConnectionConnect(CDirectiveBase *pMess)
	{
		ConnectDirective *pMessage = (ConnectDirective *)pMess;
		CConnection *pConnection = pMessage->m_pConnection;

		if (pConnection->pTransport && pConnection->pTransport->IsConnected())
		{
			Log(LOG_ERROR, "(%s) Current transport is still connected, directive ignored.", m_Name.c_str());
			return;
		}

		if (!pConnection->pProtocol)
		{
			if (m_bDebug & PDM_CONNECTION)
			{
				std::string sConnection = PyUnicode_AsUTF8(pConnection->Name);
				Log(LOG_NORM, "(%s) Protocol for '%s' not specified, 'None' assumed.", m_Name.c_str(), sConnection.c_str());
			}
			pConnection->pProtocol = new CPluginProtocol();
		}

		std::string sTransport = PyUnicode_AsUTF8(pConnection->Transport);
		std::string sAddress = PyUnicode_AsUTF8(pConnection->Address);
		if ((sTransport == "TCP/IP") || (sTransport == "TLS/IP"))
		{
			std::string sPort = PyUnicode_AsUTF8(pConnection->Port);
			if (m_bDebug & PDM_CONNECTION)
				Log(LOG_NORM, "(%s) Transport set to: '%s', %s:%s.", m_Name.c_str(), sTransport.c_str(), sAddress.c_str(), sPort.c_str());
			if (sPort.empty())
			{
				Log(LOG_ERROR, "(%s) No port number specified for %s connection to: '%s'.", m_Name.c_str(), sTransport.c_str(), sAddress.c_str());
				return;
			}
			if ((sTransport == "TLS/IP") || pConnection->pProtocol->Secure())
				pConnection->pTransport = (CPluginTransport *)new CPluginTransportTCPSecure(m_HwdID, pConnection, sAddress, sPort);
			else
				pConnection->pTransport = (CPluginTransport *)new CPluginTransportTCP(m_HwdID, pConnection, sAddress, sPort);
		}
		else if (sTransport == "Serial")
		{
			if (pConnection->pProtocol->Secure())
				Log(LOG_ERROR, "(%s) Transport '%s' does not support secure connections.", m_Name.c_str(), sTransport.c_str());
			if (m_bDebug & PDM_CONNECTION)
				Log(LOG_NORM, "(%s) Transport set to: '%s', '%s', %d.", m_Name.c_str(), sTransport.c_str(), sAddress.c_str(), pConnection->Baud);
			pConnection->pTransport = (CPluginTransport *)new CPluginTransportSerial(m_HwdID, pConnection, sAddress, pConnection->Baud);
		}
		else
		{
			Log(LOG_ERROR, "(%s) Invalid transport type for connecting specified: '%s', valid types are TCP/IP and Serial.", m_Name.c_str(), sTransport.c_str());
			return;
		}
		if (pConnection->pTransport)
		{
			AddConnection(pConnection->pTransport);
		}
		if (pConnection->pTransport->handleConnect())
		{
			if (m_bDebug & PDM_CONNECTION)
				Log(LOG_NORM, "(%s) Connect directive received, action initiated successfully.", m_Name.c_str());
		}
		else
		{
			Log(LOG_NORM, "(%s) Connect directive received, action initiation failed.", m_Name.c_str());
			RemoveConnection(pConnection->pTransport);
		}
	}

	void CPlugin::ConnectionListen(CDirectiveBase *pMess)
	{
		ListenDirective *pMessage = (ListenDirective *)pMess;
		CConnection *pConnection = pMessage->m_pConnection;

		if (pConnection->pTransport && pConnection->pTransport->IsConnected())
		{
			Log(LOG_ERROR, "(%s) Current transport is still connected, directive ignored.", m_Name.c_str());
			return;
		}

		if (!pConnection->pProtocol)
		{
			if (m_bDebug & PDM_CONNECTION)
			{
				std::string sConnection = PyUnicode_AsUTF8(pConnection->Name);
				Log(LOG_NORM, "(%s) Protocol for '%s' not specified, 'None' assumed.", m_Name.c_str(), sConnection.c_str());
			}
			pConnection->pProtocol = new CPluginProtocol();
		}

		std::string sTransport = PyUnicode_AsUTF8(pConnection->Transport);
		std::string sAddress = PyUnicode_AsUTF8(pConnection->Address);
		if (sTransport == "TCP/IP")
		{
			std::string sPort = PyUnicode_AsUTF8(pConnection->Port);
			if (m_bDebug & PDM_CONNECTION)
				Log(LOG_NORM, "(%s) Transport set to: '%s', %s:%s.", m_Name.c_str(), sTransport.c_str(), sAddress.c_str(), sPort.c_str());
			if (!pConnection->pProtocol->Secure())
				pConnection->pTransport = (CPluginTransport *)new CPluginTransportTCP(m_HwdID, pConnection, "", sPort);
			else
				pConnection->pTransport = (CPluginTransport *)new CPluginTransportTCPSecure(m_HwdID, pConnection, "", sPort);
		}
		else if (sTransport == "UDP/IP")
		{
			std::string sPort = PyUnicode_AsUTF8(pConnection->Port);
			if (pConnection->pProtocol->Secure())
				Log(LOG_ERROR, "(%s) Transport '%s' does not support secure connections.", m_Name.c_str(), sTransport.c_str());
			if (m_bDebug & PDM_CONNECTION)
				Log(LOG_NORM, "(%s) Transport set to: '%s', %s:%s.", m_Name.c_str(), sTransport.c_str(), sAddress.c_str(), sPort.c_str());
			pConnection->pTransport = (CPluginTransport *)new CPluginTransportUDP(m_HwdID, pConnection, sAddress, sPort);
		}
		else if (sTransport == "ICMP/IP")
		{
			std::string sPort = PyUnicode_AsUTF8(pConnection->Port);
			if (pConnection->pProtocol->Secure())
				Log(LOG_ERROR, "(%s) Transport '%s' does not support secure connections.", m_Name.c_str(), sTransport.c_str());
			if (m_bDebug & PDM_CONNECTION)
				Log(LOG_NORM, "(%s) Transport set to: '%s', %s.", m_Name.c_str(), sTransport.c_str(), sAddress.c_str());
			pConnection->pTransport = (CPluginTransport *)new CPluginTransportICMP(m_HwdID, pConnection, sAddress, sPort);
		}
		else
		{
			Log(LOG_ERROR, "(%s) Invalid transport type for listening specified: '%s', valid types are TCP/IP, UDP/IP and ICMP/IP.", m_Name.c_str(), sTransport.c_str());
			return;
		}
		if (pConnection->pTransport)
		{
			AddConnection(pConnection->pTransport);
		}
		if (pConnection->pTransport->handleListen())
		{
			if (m_bDebug & PDM_CONNECTION)
				Log(LOG_NORM, "(%s) Listen directive received, action initiated successfully.", m_Name.c_str());
		}
		else
		{
			Log(LOG_NORM, "(%s) Listen directive received, action initiation failed.", m_Name.c_str());
			RemoveConnection(pConnection->pTransport);
		}
	}

	void CPlugin::ConnectionRead(CPluginMessageBase *pMess)
	{
		ReadEvent *pMessage = (ReadEvent *)pMess;
		CConnection *pConnection = pMessage->m_pConnection;

		pConnection->pProtocol->ProcessInbound(pMessage);
	}

	void CPlugin::ConnectionWrite(CDirectiveBase *pMess)
	{
		WriteDirective *pMessage = (WriteDirective *)pMess;
		CConnection *pConnection = pMessage->m_pConnection;
		std::string sTransport = PyUnicode_AsUTF8(pConnection->Transport);
		std::string sConnection = PyUnicode_AsUTF8(pConnection->Name);
		if (pConnection->pTransport)
		{
			if (sTransport == "UDP/IP")
			{
				Log(LOG_ERROR, "(%s) Connectionless Transport is listening, write directive to '%s' ignored.", m_Name.c_str(), sConnection.c_str());
				return;
			}

			if ((sTransport != "ICMP/IP") && (!pConnection->pTransport->IsConnected()))
			{
				Log(LOG_ERROR, "(%s) Transport is not connected, write directive to '%s' ignored.", m_Name.c_str(), sConnection.c_str());
				return;
			}
		}

		if (!pConnection->pTransport)
		{
			// UDP is connectionless so create a temporary transport and write to it
			if (sTransport == "UDP/IP")
			{
				std::string sAddress = PyUnicode_AsUTF8(pConnection->Address);
				std::string sPort = PyUnicode_AsUTF8(pConnection->Port);
				if (m_bDebug & PDM_CONNECTION)
				{
					if (!sPort.empty())
						Log(LOG_NORM, "(%s) Transport set to: '%s', %s:%s for '%s'.", m_Name.c_str(), sTransport.c_str(), sAddress.c_str(), sPort.c_str(),
							 sConnection.c_str());
					else
						Log(LOG_NORM, "(%s) Transport set to: '%s', %s for '%s'.", m_Name.c_str(), sTransport.c_str(), sAddress.c_str(), sConnection.c_str());
				}
				pConnection->pTransport = (CPluginTransport *)new CPluginTransportUDP(m_HwdID, pConnection, sAddress, sPort);
			}
			else
			{
				Log(LOG_ERROR, "(%s) No transport, write directive to '%s' ignored.", m_Name.c_str(), sConnection.c_str());
				return;
			}
		}

		// Make sure there is a protocol to encode the data
		if (!pConnection->pProtocol)
		{
			pConnection->pProtocol = new CPluginProtocol();
		}

		std::vector<byte> vWriteData = pConnection->pProtocol->ProcessOutbound(pMessage);
		WriteDebugBuffer(vWriteData, false);

		pConnection->pTransport->handleWrite(vWriteData);

		// UDP is connectionless so remove the transport after write
		if (pConnection->pTransport && (sTransport == "UDP/IP"))
		{
			delete pConnection->pTransport;
			pConnection->pTransport = nullptr;
		}
	}

	void CPlugin::ConnectionDisconnect(CDirectiveBase *pMess)
	{
		DisconnectDirective *pMessage = (DisconnectDirective *)pMess;
		CConnection *pConnection = pMessage->m_pConnection;

		// Return any partial data to plugin
		if (pConnection->pProtocol)
		{
			pConnection->pProtocol->Flush(pMessage->m_pPlugin, pConnection);
		}

		if (pConnection->pTransport)
		{
			if (m_bDebug & PDM_CONNECTION)
			{
				std::string sTransport = PyUnicode_AsUTF8(pConnection->Transport);
				std::string sAddress = PyUnicode_AsUTF8(pConnection->Address);
				std::string sPort = PyUnicode_AsUTF8(pConnection->Port);
				if ((sTransport == "Serial") || (sPort.empty()))
					Log(LOG_NORM, "(%s) Disconnect directive received for '%s'.", m_Name.c_str(), sAddress.c_str());
				else
					Log(LOG_NORM, "(%s) Disconnect directive received for '%s:%s'.", m_Name.c_str(), sAddress.c_str(), sPort.c_str());
			}

			// If transport is not going to disconnect asynchronously tidy it up here
			if (!pConnection->pTransport->AsyncDisconnect())
			{
				pConnection->pTransport->handleDisconnect();
				RemoveConnection(pConnection->pTransport);
				delete pConnection->pTransport;
				pConnection->pTransport = nullptr;

				// Plugin exiting and all connections have disconnect messages queued
				if (IsStopRequested(0) && m_Transports.empty())
				{
					MessagePlugin(new onStopCallback(this));
				}
			}
			else
			{
				pConnection->pTransport->handleDisconnect();
			}
		}
	}

	void CPlugin::onDeviceAdded(const std::string DeviceID, int Unit)
	{
		PyBorrowedRef pObject;
		PyBorrowedRef pModule = PyState_FindModule(&DomoticzExModuleDef);
		if (pModule)
		{
			module_state *pModState = ((struct module_state *)PyModule_GetState(pModule));
			if (!pModState)
			{
				_log.Log(LOG_ERROR, "(%s) unable to obtain module state.", __func__);
				return;
			}

			if (!pModState->pPlugin)
			{
				PyBorrowedRef pyDevice = FindDevice(DeviceID);
				if (!pyDevice)
				{
					// Create the device object if not found
					PyNewRef nrArgList = Py_BuildValue("(s)", DeviceID.c_str());
					if (!nrArgList)
					{
						Log(LOG_ERROR, "Building device argument list failed for key %s.", DeviceID.c_str());
						return;
					}
					PyNewRef pDevice = PyObject_CallObject((PyObject *)pModState->pDeviceClass, nrArgList);
					if (!pDevice)
					{
						Log(LOG_ERROR, "Device object creation failed for key %s.", DeviceID.c_str());
						return;
					}

					// Add the object to the dictionary
					PyNewRef pKey = PyObject_GetAttrString(pDevice, "Key");
					if (!PyDict_Contains((PyObject *)m_DeviceDict, pKey))
					{
						if (PyDict_SetItem((PyObject *)m_DeviceDict, pKey, pDevice) == -1)
						{
							PyNewRef pString = PyObject_Str(pKey);
							std::string sUTF = PyUnicode_AsUTF8(pString);
							Log(LOG_ERROR, "(%s) failed to add key '%s' to device dictionary.", m_PluginKey.c_str(), sUTF.c_str());
							return;
						}
					}

					// now find it
					pyDevice = FindDevice(DeviceID);
				}

				// Create unit object
				PyNewRef nrArgList = Py_BuildValue("(ssi)", "", DeviceID.c_str(), Unit);
				if (!nrArgList)
				{
					pModState->pPlugin->Log(LOG_ERROR, "Building device argument list failed for key %s/%d.", DeviceID.c_str(), Unit);
					return;
				}
				PyNewRef pUnit = PyObject_CallObject((PyObject *)pModState->pUnitClass, nrArgList);
				if (!pUnit)
				{
					pModState->pPlugin->Log(LOG_ERROR, "Unit object creation failed for key %d.", Unit);
					return;
				}

				// and add it to the parent directory
				CDeviceEx *pDevice = pyDevice;
				PyNewRef pKey = PyLong_FromLong(Unit);
				if (PyDict_SetItem((PyObject *)pDevice->m_UnitDict, pKey, pUnit) == -1)
				{
					PyNewRef pString = PyObject_Str(pKey);
					std::string sUTF = PyUnicode_AsUTF8(pString);
					pModState->pPlugin->Log(LOG_ERROR, "Failed to add key '%s' to Unit dictionary.", sUTF.c_str());
					return;
				}

				// Force the Unit object to refresh from the database
				PyNewRef pRefresh = PyObject_GetAttrString(pUnit, "Refresh");
				PyNewRef pObj = PyObject_CallNoArgs(pRefresh);
			}
		}
		else
		{
			CDevice *pDevice = (CDevice *)CDevice_new(&CDeviceType, (PyObject *)nullptr, (PyObject *)nullptr);

			PyNewRef pKey = PyLong_FromLong(Unit);
			if (PyDict_SetItem((PyObject *)m_DeviceDict, pKey, (PyObject *)pDevice) == -1)
			{
				Log(LOG_ERROR, "(%s) failed to add unit '%d' to device dictionary.", m_PluginKey.c_str(), Unit);
				return;
			}
			pDevice->pPlugin = this;
			pDevice->PluginKey = PyUnicode_FromString(m_PluginKey.c_str());
			pDevice->HwdID = m_HwdID;
			pDevice->Unit = Unit;
			CDevice_refresh(pDevice);
			Py_DECREF(pDevice);
		}
	}

	void CPlugin::onDeviceModified(const std::string DeviceID, int Unit)
	{
		PyBorrowedRef pObject;
		PyBorrowedRef pModule = PyState_FindModule(&DomoticzExModuleDef);
		if (pModule)
		{
			pObject = FindUnitInDevice(DeviceID, Unit);
		}
		else
		{
			PyNewRef pKey = PyLong_FromLong(Unit);
			pObject = PyDict_GetItem((PyObject *)m_DeviceDict, pKey);
		}

		if (!pObject)
		{
			Log(LOG_ERROR, "(%s) failed to refresh unit '%u' in device dictionary.", m_PluginKey.c_str(), Unit);
			return;
		}

		// Force the object to refresh from the database
		if (PyObject_HasAttrString(pObject, "Refresh"))
		{
			PyNewRef pRefresh = PyObject_GetAttrString(pObject, "Refresh");
			PyNewRef pObj = PyObject_CallNoArgs(pRefresh);
		}
	}

	void CPlugin::onDeviceRemoved(const std::string DeviceID, int Unit)
	{
		PyNewRef pKey = PyLong_FromLong(Unit);
		PyBorrowedRef pModule = PyState_FindModule(&DomoticzExModuleDef);
		if (pModule)
		{
			PyBorrowedRef pObject = FindDevice(DeviceID.c_str());
			if (pObject)
			{
				CDeviceEx *pDevice = (CDeviceEx *)pObject;
				if (PyDict_DelItem((PyObject *)pDevice->m_UnitDict, pKey) == -1)
				{
					Log(LOG_ERROR, "(%s) failed to remove Unit '%u' from Unit dictionary of '%s'.", m_PluginKey.c_str(), Unit, DeviceID.c_str());
				}
			}
		}
		else
		{
			if (PyDict_DelItem((PyObject *)m_DeviceDict, pKey) == -1)
			{
				Log(LOG_ERROR, "(%s) failed to remove Unit '%u' from Device dictionary.", m_PluginKey.c_str(), Unit);
			}
		}
	}

	void CPlugin::MessagePlugin(CPluginMessageBase *pMessage)
	{
		if (m_bDebug & PDM_QUEUE)
		{
			Log(LOG_NORM, "(" + m_Name + ") Pushing '" + std::string(pMessage->Name()) + "' on to queue");
		}

		// Add message to queue
		std::lock_guard<std::mutex> l(m_QueueMutex);
		m_MessageQueue.push_back(pMessage);
	}

	void CPlugin::DeviceAdded(const std::string DeviceID, int Unit)
	{
		CPluginMessageBase *pMessage = new onDeviceAddedCallback(this, DeviceID, Unit);
		MessagePlugin(pMessage);
	}

	void CPlugin::DeviceModified(const std::string DeviceID, int Unit)
	{
		CPluginMessageBase *pMessage = new onDeviceModifiedCallback(this, DeviceID, Unit);
		MessagePlugin(pMessage);
	}

	void CPlugin::DeviceRemoved(const std::string DeviceID, int Unit)
	{
		CPluginMessageBase *pMessage = new onDeviceRemovedCallback(this, DeviceID, Unit);
		MessagePlugin(pMessage);
	}

	void CPlugin::DisconnectEvent(CEventBase *pMess)
	{
		DisconnectedEvent *pMessage = (DisconnectedEvent *)pMess;
		CConnection *pConnection = (CConnection *)pMessage->m_pConnection;

		// Return any partial data to plugin
		if (pConnection->pProtocol)
		{
			pConnection->pProtocol->Flush(pMessage->m_pPlugin, pConnection);
		}

		if (pConnection->pTransport)
		{
			if (m_bDebug & PDM_CONNECTION)
			{
				std::string sTransport = PyUnicode_AsUTF8(pConnection->Transport);
				std::string sAddress = PyUnicode_AsUTF8(pConnection->Address);
				std::string sPort = PyUnicode_AsUTF8(pConnection->Port);
				if ((sTransport == "Serial") || (sPort.empty()))
					Log(LOG_NORM, "(%s) Disconnect event received for '%s'.", m_Name.c_str(), sAddress.c_str());
				else
					Log(LOG_NORM, "(%s) Disconnect event received for '%s:%s'.", m_Name.c_str(), sAddress.c_str(), sPort.c_str());
			}

			RemoveConnection(pConnection->pTransport);
			delete pConnection->pTransport;
			pConnection->pTransport = nullptr;

			// inform the plugin if transport is connection based
			if (pMessage->bNotifyPlugin)
			{
				MessagePlugin(new onDisconnectCallback(this, pConnection));
			}

			// Plugin exiting and all connections have disconnect messages queued
			if (IsStopRequested(0) && m_Transports.empty())
			{
				MessagePlugin(new onStopCallback(this));
			}
		}
	}

	void CPlugin::RestoreThread()
	{
		if (m_PyInterpreter)
			PyEval_RestoreThread((PyThreadState *)m_PyInterpreter);
	}

	void CPlugin::ReleaseThread()
	{
		if (m_PyInterpreter)
			PyEval_SaveThread();
	}

	void CPlugin::Callback(PyObject *pTarget, const std::string &sHandler, PyObject *pParams)
	{
		try
		{
			// Callbacks MUST already have taken the PythonMutex lock otherwise bad things will happen
			if (pTarget && !sHandler.empty())
			{
				if (PyErr_Occurred())
				{
					PyErr_Clear();
					Log(LOG_NORM, "(%s) Python exception set prior to callback '%s'", m_Name.c_str(), sHandler.c_str());
				}

				PyNewRef pFunc = PyObject_GetAttrString(pTarget, sHandler.c_str());
				if (pFunc && PyCallable_Check(pFunc))
				{
					if (m_bDebug & PDM_QUEUE)
						Log(LOG_NORM, "(%s) Calling message handler '%s' on '%s' type object.", m_Name.c_str(), sHandler.c_str(), pTarget->ob_type->tp_name);

					PyErr_Clear();
					PyNewRef	pReturnValue = PyObject_CallObject(pFunc, pParams);
					if (!pReturnValue || PyErr_Occurred())
					{
						LogPythonException(sHandler);
						{
							PyErr_Clear();
						}
						if (m_bDebug & PDM_PLUGIN)
						{
							// See if additional information is available
							PyNewRef pLocals = PyObject_Dir(pTarget);
							if (PyList_Check(pLocals))
							{
								Log(LOG_NORM, "(%s) Local context:", m_Name.c_str());
								PyNewRef pIter = PyObject_GetIter(pLocals);
								PyNewRef pItem = PyIter_Next(pIter);
								while (pItem)
								{
									std::string sAttrName = PyUnicode_AsUTF8(pItem);
									if (sAttrName.substr(0, 2) != "__") // ignore system stuff
									{
										if (PyObject_HasAttrString(pTarget, sAttrName.c_str()))
										{
											PyNewRef pValue = PyObject_GetAttrString(pTarget, sAttrName.c_str());
											if (!PyCallable_Check(pValue)) // Filter out methods
											{
												PyNewRef nrString = PyObject_Str(pValue);
												if (nrString)
												{
													std::string sUTF = PyUnicode_AsUTF8(nrString);
													std::string sBlank((sAttrName.length() < 20) ? 20 - sAttrName.length() : 0, ' ');
													Log(LOG_NORM, "(%s) ----> '%s'%s '%s'", m_Name.c_str(), sAttrName.c_str(), sBlank.c_str(),
													    sUTF.c_str());
												}
											}
										}
									}
									pItem = PyIter_Next(pIter);
								}
							}
						}
					}
				}
				else
				{
					if (m_bDebug & PDM_QUEUE)
					{
						Log(LOG_NORM, "(%s) Message handler '%s' not callable, ignored.", m_Name.c_str(), sHandler.c_str());
					}
					if (PyErr_Occurred())
					{
						PyErr_Clear();
					}
				}
			}
		}
		catch (std::exception *e)
		{
			Log(LOG_ERROR, "%s: Execption thrown: %s", __func__, e->what());
		}
		catch (...)
		{
			Log(LOG_ERROR, "%s: Unknown execption thrown", __func__);
		}
	}

	void CPlugin::Stop()
	{
		try
		{
			PyErr_Clear();

			// Validate Device dictionary prior to shutdown
			if (m_DeviceDict)
			{
				PyBorrowedRef brModule = PyState_FindModule(&DomoticzModuleDef);
				if (!brModule)
				{
					brModule = PyState_FindModule(&DomoticzExModuleDef);
					if (!brModule)
					{
						Log(LOG_ERROR, "(%s) %s failed, Domoticz/DomoticzEx modules not found in interpreter.", __func__, m_PluginKey.c_str());
						return;
					}
				}

				module_state *pModState = ((struct module_state *)PyModule_GetState(brModule));
				if (!pModState)
				{
					Log(LOG_ERROR, "CPlugin:%s, unable to obtain module state.", __func__);
					return;
				}

				PyBorrowedRef	key;
				PyBorrowedRef	pDevice;
				Py_ssize_t pos = 0;
				// Sanity check to make sure the reference counting is all good.
				while (PyDict_Next((PyObject*)m_DeviceDict, &pos, &key, &pDevice))
				{
					// Dictionary should be full of Devices but Python script can make this assumption false, log warning if this has happened
					int isDevice = PyObject_IsInstance(pDevice, (PyObject *)pModState->pDeviceClass);
					if (isDevice == -1)
					{
						LogPythonException("Error determining type of Python object during dealloc");
					}
					else if (isDevice == 0)
					{
						Log(LOG_NORM, "%s: Device dictionary contained non-Device entry '%s'.", __func__, pDevice->ob_type->tp_name);
					}
					else
					{
						PyNewRef pUnits = PyObject_GetAttrString(pDevice, "Units");	// Free any Units if the object has them
						if (pUnits)
						{
							PyBorrowedRef key;
							PyBorrowedRef pUnit;
							Py_ssize_t	pos = 0;
							// Sanity check to make sure the reference counting is all good.
							while (PyDict_Next(pUnits, &pos, &key, &pUnit))
							{
								// Dictionary should be full of Units but Python script can make this assumption false, log warning if this has happened
								int isValue = PyObject_IsInstance(pUnit, (PyObject *)pModState->pUnitClass);
								if (isValue == -1)
								{
									_log.Log(LOG_ERROR, "Error determining type of Python object during dealloc");
								}
								else if (isValue == 0)
								{
									_log.Log(LOG_NORM, "%s: Unit dictionary contained non-Unit entry '%s'.", __func__, pUnit->ob_type->tp_name);
								}
								else
								{
									if (pUnit->ob_refcnt > 1)
									{
										PyNewRef pName = PyObject_GetAttrString(pDevice, "Name");
										std::string sName = PyUnicode_AsUTF8(pName);
										_log.Log(LOG_ERROR, "%s: Unit '%s' Reference Count not one: %d.", __func__, sName.c_str(), pUnit->ob_refcnt);
									}
									else if (pUnit->ob_refcnt < 1)
									{
										_log.Log(LOG_ERROR, "%s: Unit Reference Count not one: %d.", __func__, pUnit->ob_refcnt);
									}
								}
							}
							PyDict_Clear(pUnits);
						}
						else
						{
							PyErr_Clear();
						}

						if (pDevice->ob_refcnt > 1)
						{
							PyNewRef pName = PyObject_GetAttrString(pDevice, "Name");
							if (!pName)
							{
								PyErr_Clear();
								pName = PyObject_GetAttrString(pDevice, "DeviceID");
							}
							PyNewRef pString = PyObject_Str(pName);
							std::string sName = PyUnicode_AsUTF8(pString);
							Log(LOG_ERROR, "%s: Device '%s' Reference Count not correct, expected %d found %d.", __func__, sName.c_str(), 1, (int) pDevice->ob_refcnt);
						}
						else if (pDevice->ob_refcnt < 1)
						{
							Log(LOG_ERROR, "%s: Device Reference Count is less than one: %d.", __func__, (int)pDevice->ob_refcnt);
						}
					}
				}
				PyDict_Clear((PyObject*)m_DeviceDict);
			}

			// Stop Python
			Py_XDECREF(m_PyModule);
			Py_XDECREF(m_DeviceDict);
			if (m_ImageDict)
				Py_XDECREF(m_ImageDict);
			if (m_SettingsDict)
				Py_XDECREF(m_SettingsDict);
			if (m_PyInterpreter)
				Py_EndInterpreter((PyThreadState *)m_PyInterpreter);
			// To release the GIL there must be a valid thread state so use
			// the one created during start up of the plugin system because it will always exist
			CPluginSystem pManager;
			PyThreadState_Swap((PyThreadState *)pManager.PythonThread());
			PyEval_ReleaseLock();
		}
		catch (std::exception *e)
		{
			Log(LOG_ERROR, "%s: Execption thrown releasing Interpreter: %s", __func__, e->what());
		}
		catch (...)
		{
			Log(LOG_ERROR, "%s: Unknown execption thrown releasing Interpreter", __func__);
		}

		m_PyModule = nullptr;
		m_DeviceDict = nullptr;
		m_ImageDict = nullptr;
		m_SettingsDict = nullptr;
		m_PyInterpreter = nullptr;
		m_bIsStarted = false;

		// Flush the message queue (should already be empty)
		{
			std::lock_guard<std::mutex> l(m_QueueMutex);
			while (!m_MessageQueue.empty())
			{
				m_MessageQueue.pop_front();
			}
		}

		m_bIsStopped = true;
	}

	bool CPlugin::LoadSettings()
	{
		PyObject *pModuleDict = PyModule_GetDict((PyObject *)m_PyModule); // returns a borrowed referece to the __dict__ object for the module
		if (m_SettingsDict)
			Py_XDECREF(m_SettingsDict);
		m_SettingsDict = (PyDictObject *)PyDict_New();
		if (PyDict_SetItemString(pModuleDict, "Settings", (PyObject *)m_SettingsDict) == -1)
		{
			Log(LOG_ERROR, "(%s) failed to add Settings dictionary.", m_PluginKey.c_str());
			return false;
		}

		// load associated settings to make them available to python
		std::vector<std::vector<std::string>> result;
		result = m_sql.safe_query("SELECT Key, nValue, sValue FROM Preferences");
		if (!result.empty())
		{
			PyType_Ready(&CDeviceType);
			// Add settings strings into the settings dictionary with Unit as the key
			for (const auto &sd : result)
			{
				PyObject *pKey = PyUnicode_FromString(sd[0].c_str());
				PyObject *pValue = nullptr;
				if (!sd[2].empty())
				{
					pValue = PyUnicode_FromString(sd[2].c_str());
				}
				else
				{
					pValue = PyUnicode_FromString(sd[1].c_str());
				}
				if (PyDict_SetItem((PyObject *)m_SettingsDict, pKey, pValue))
				{
					Log(LOG_ERROR, "(%s) failed to add setting '%s' to settings dictionary.", m_PluginKey.c_str(), sd[0].c_str());
					return false;
				}
				Py_XDECREF(pValue);
				Py_XDECREF(pKey);
			}
		}

		return true;
	}

#define DZ_BYTES_PER_LINE 20
	void CPlugin::WriteDebugBuffer(const std::vector<byte> &Buffer, bool Incoming)
	{
		if (m_bDebug & (PDM_CONNECTION | PDM_MESSAGE))
		{
			if (Incoming)
				Log(LOG_NORM, "(%s) Received %d bytes of data", m_Name.c_str(), (int)Buffer.size());
			else
				Log(LOG_NORM, "(%s) Sending %d bytes of data", m_Name.c_str(), (int)Buffer.size());
		}

		if (m_bDebug & PDM_MESSAGE)
		{
			for (int i = 0; i < (int)Buffer.size(); i = i + DZ_BYTES_PER_LINE)
			{
				std::stringstream ssHex;
				std::string sChars;
				for (int j = 0; j < DZ_BYTES_PER_LINE; j++)
				{
					if (i + j < (int)Buffer.size())
					{
						if (Buffer[i + j] < 16)
							ssHex << '0' << std::hex << (int)Buffer[i + j] << " ";
						else
							ssHex << std::hex << (int)Buffer[i + j] << " ";
						if ((int)Buffer[i + j] > 32)
							sChars += Buffer[i + j];
						else
							sChars += ".";
					}
					else
						ssHex << ".. ";
				}
				Log(LOG_NORM, "(%s)     %s    %s", m_Name.c_str(), ssHex.str().c_str(), sChars.c_str());
			}
		}
	}

	bool CPlugin::WriteToHardware(const char *pdata, const unsigned char length)
	{
		return true;
	}

	void CPlugin::SendCommand(const std::string &DeviceID, const int Unit, const std::string &command, const int level, const _tColor color)
	{
		//	Add command to message queue
		std::string JSONColor = color.toJSONString();
		MessagePlugin(new onCommandCallback(this, DeviceID, Unit, command, level, JSONColor));
	}

	void CPlugin::SendCommand(const std::string &DeviceID, const int Unit, const std::string &command, const float level)
	{
		//	Add command to message queue
		MessagePlugin(new onCommandCallback(this, DeviceID, Unit, command, level));
	}

	bool CPlugin::HasNodeFailed(const std::string DeviceID, const int Unit)
	{
		if (!m_DeviceDict)
			return true;

		PyObject *key, *value;
		Py_ssize_t pos = 0;
		while (PyDict_Next((PyObject *)m_DeviceDict, &pos, &key, &value))
		{
			// Handle different Device dictionaries types
			if (PyUnicode_Check(key))
			{
				// Version 2+ of the framework, keyed by DeviceID
				std::string sKey = PyUnicode_AsUTF8(key);
				if (sKey == DeviceID)
				{
					CDeviceEx *pDevice = (CDeviceEx *)value;
					return (pDevice->TimedOut != 0);
				}
			}
			else
			{
				// Version 1 of the framework, keyed by Unit
				long iKey = PyLong_AsLong(key);
				if (iKey == -1 && PyErr_Occurred())
				{
					PyErr_Clear();
					return false;
				}

				if (iKey == Unit)
				{
					CDevice *pDevice = (CDevice *)value;
					return (pDevice->TimedOut != 0);
				}
			}
		}

		return false;
	}

	PyBorrowedRef CPlugin::FindDevice(const std::string &Key)
	{
		return PyDict_GetItemString((PyObject *)m_DeviceDict, Key.c_str());
	}

	PyBorrowedRef	CPlugin::FindUnitInDevice(const std::string &deviceKey, const int unitKey)
	{
		CDeviceEx *pDevice = this->FindDevice(deviceKey);

		if (pDevice)
		{
			PyNewRef pKey = PyLong_FromLong(unitKey);
			return PyBorrowedRef(PyDict_GetItem((PyObject *)pDevice->m_UnitDict, pKey));
		}

		return nullptr;
	}

	CPluginNotifier::CPluginNotifier(CPlugin *pPlugin, const std::string &NotifierName)
		: CNotificationBase(NotifierName, OPTIONS_NONE)
		, m_pPlugin(pPlugin)
	{
		m_notifications.AddNotifier(this);
	}

	CPluginNotifier::~CPluginNotifier()
	{
		m_notifications.RemoveNotifier(this);
	}

	bool CPluginNotifier::IsConfigured()
	{
		return true;
	}

	std::string CPluginNotifier::GetCustomIcon(std::string &szCustom)
	{
		int iIconLine = atoi(szCustom.c_str());
		std::string szRetVal = "Light48";
		if (iIconLine < 100) // default set of custom icons
		{
			std::string sLine;
			std::ifstream infile;
			std::string switchlightsfile = szWWWFolder + "/switch_icons.txt";
			infile.open(switchlightsfile.c_str());
			if (infile.is_open())
			{
				int index = 0;
				while (!infile.eof())
				{
					getline(infile, sLine);
					if ((!sLine.empty()) && (index++ == iIconLine))
					{
						std::vector<std::string> results;
						StringSplit(sLine, ";", results);
						if (results.size() == 3)
						{
							szRetVal = results[0] + "48";
							break;
						}
					}
				}
				infile.close();
			}
		}
		else // Uploaded icons
		{
			std::vector<std::vector<std::string>> result;
			result = m_sql.safe_query("SELECT Base FROM CustomImages WHERE ID = %d", iIconLine - 100);
			if (result.size() == 1)
			{
				std::string sBase = result[0][0];
				return sBase;
			}
		}

		return szRetVal;
	}

	std::string CPluginNotifier::GetIconFile(const std::string &ExtraData)
	{
		std::string szImageFile;
#ifdef WIN32
		std::string szImageFolder = szWWWFolder + "\\images\\";
#else
		std::string szImageFolder = szWWWFolder + "/images/";
#endif

		std::string szStatus = "Off";
		int posStatus = (int)ExtraData.find("|Status=");
		if (posStatus >= 0)
		{
			posStatus += 8;
			szStatus = ExtraData.substr(posStatus, ExtraData.find('|', posStatus) - posStatus);
			if (szStatus != "Off")
				szStatus = "On";
		}

		// Use image is specified
		int posImage = (int)ExtraData.find("|Image=");
		if (posImage >= 0)
		{
			posImage += 7;
			szImageFile = szImageFolder + ExtraData.substr(posImage, ExtraData.find('|', posImage) - posImage) + ".png";
			if (file_exist(szImageFile.c_str()))
			{
				return szImageFile;
			}
		}

		// Use uploaded and custom images
		int posCustom = (int)ExtraData.find("|CustomImage=");
		if (posCustom >= 0)
		{
			posCustom += 13;
			std::string szCustom = ExtraData.substr(posCustom, ExtraData.find('|', posCustom) - posCustom);
			int iCustom = atoi(szCustom.c_str());
			if (iCustom)
			{
				szImageFile = szImageFolder + GetCustomIcon(szCustom) + "_" + szStatus + ".png";
				if (file_exist(szImageFile.c_str()))
				{
					return szImageFile;
				}
				szImageFile = szImageFolder + GetCustomIcon(szCustom) + "48_" + szStatus + ".png";
				if (file_exist(szImageFile.c_str()))
				{
					return szImageFile;
				}
				szImageFile = szImageFolder + GetCustomIcon(szCustom) + ".png";
				if (file_exist(szImageFile.c_str()))
				{
					return szImageFile;
				}
			}
		}

		// if a switch type was supplied try and work out the image
		int posType = (int)ExtraData.find("|SwitchType=");
		if (posType >= 0)
		{
			posType += 12;
			std::string szType = ExtraData.substr(posType, ExtraData.find('|', posType) - posType);
			std::string szTypeImage;
			device::tswitch::type::value switchtype = (device::tswitch::type::value)atoi(szType.c_str());
			switch (switchtype)
			{
				case device::tswitch::type::OnOff:
					if (posCustom >= 0)
					{
						std::string szCustom = ExtraData.substr(posCustom, ExtraData.find('|', posCustom) - posCustom);
						szTypeImage = GetCustomIcon(szCustom);
					}
					else
						szTypeImage = "Light48";
					break;
				case device::tswitch::type::Doorbell:
					szTypeImage = "doorbell48";
					break;
				case device::tswitch::type::Contact:
					szTypeImage = "Contact48";
					break;
				case device::tswitch::type::Blinds:
				case device::tswitch::type::BlindsPercentage:
				case device::tswitch::type::VenetianBlindsUS:
				case device::tswitch::type::VenetianBlindsEU:
				case device::tswitch::type::BlindsPercentageInverted:
				case device::tswitch::type::BlindsInverted:
					szTypeImage = "blinds48";
					break;
				case device::tswitch::type::X10Siren:
					szTypeImage = "siren";
					break;
				case device::tswitch::type::SMOKEDETECTOR:
					szTypeImage = "smoke48";
					break;
				case device::tswitch::type::Dimmer:
					szTypeImage = "Dimmer48";
					break;
				case device::tswitch::type::Motion:
					szTypeImage = "motion48";
					break;
				case device::tswitch::type::PushOn:
					szTypeImage = "Push48";
					break;
				case device::tswitch::type::PushOff:
					szTypeImage = "Push48";
					break;
				case device::tswitch::type::DoorContact:
					szTypeImage = "Door48";
					break;
				case device::tswitch::type::DoorLock:
					szTypeImage = "Door48";
					break;
				case device::tswitch::type::DoorLockInverted:
					szTypeImage = "Door48";
					break;
				case device::tswitch::type::Media:
					if (posCustom >= 0)
					{
						std::string szCustom = ExtraData.substr(posCustom, ExtraData.find('|', posCustom) - posCustom);
						szTypeImage = GetCustomIcon(szCustom);
					}
					else
						szTypeImage = "Media48";
					break;
				default:
					szTypeImage = "logo";
			}
			szImageFile = szImageFolder + szTypeImage + "_" + szStatus + ".png";
			if (file_exist(szImageFile.c_str()))
			{
				return szImageFile;
			}

			szImageFile = szImageFolder + szTypeImage + ((szStatus == "Off") ? "-off" : "-on") + ".png";
			if (file_exist(szImageFile.c_str()))
			{
				return szImageFile;
			}

			szImageFile = szImageFolder + szTypeImage + ((szStatus == "Off") ? "off" : "on") + ".png";
			if (file_exist(szImageFile.c_str()))
			{
				return szImageFile;
			}

			szImageFile = szImageFolder + szTypeImage + ".png";
			if (file_exist(szImageFile.c_str()))
			{
				return szImageFile;
			}
		}

		// Image of last resort is the logo
		szImageFile = szImageFolder + "logo.png";
		if (!file_exist(szImageFile.c_str()))
		{
			m_pPlugin->Log(LOG_ERROR, "Logo image file does not exist: %s", szImageFile.c_str());
			szImageFile = "";
		}
		return szImageFile;
	}

	bool CPluginNotifier::SendMessageImplementation(const uint64_t Idx, const std::string &Name, const std::string &Subject, const std::string &Text, const std::string &ExtraData,
							const int Priority, const std::string &Sound, const bool bFromNotification)
	{
		// ExtraData = |Name=Test|SwitchType=9|CustomImage=0|Status=On|

		std::string sIconFile = GetIconFile(ExtraData);
		std::string sName = "Unknown";
		int posName = (int)ExtraData.find("|Name=");
		if (posName >= 0)
		{
			posName += 6;
			sName = ExtraData.substr(posName, ExtraData.find('|', posName) - posName);
		}

		std::string sStatus = "Unknown";
		int posStatus = (int)ExtraData.find("|Status=");
		if (posStatus >= 0)
		{
			posStatus += 8;
			sStatus = ExtraData.substr(posStatus, ExtraData.find('|', posStatus) - posStatus);
		}

		//	Add command to message queue for every plugin
		m_pPlugin->MessagePlugin(new onNotificationCallback(m_pPlugin, Subject, Text, sName, sStatus, Priority, Sound, sIconFile));

		return true;
	}
} // namespace Plugins
#endif
