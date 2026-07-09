//
// connection.cpp
// ~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2008 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "webem_stdafx.h"
#include "webserver/connection.h"
#include <boost/algorithm/string.hpp>
#include <iomanip>
#include <sstream>
#include "webserver/connection_manager.h"
#include "webserver/request_handler.h"
#include "mime_types.h"
#include "webserver/cWebem.h"
#include "webserver/webem_utils.h"

namespace http {
	namespace server {
		// this is the constructor for plain connections
		connection::connection(boost::asio::io_context &io_context, connection_manager &manager, request_handler &handler, int read_timeout, WebServerLogger logger)
			: m_logger(std::move(logger))
			, send_buffer_(nullptr)
			, read_timeout_(read_timeout)
			, read_timer_(io_context, std::chrono::seconds(read_timeout))
			, default_abandoned_timeout_(20 * 60)
			// 20mn before stopping abandoned connection
			, abandoned_timer_(io_context, std::chrono::seconds(default_abandoned_timeout_))
			, ws_session_renewal_timer_(io_context)
			, connection_manager_(manager)
			, request_handler_(handler)
			, status_(INITIALIZING)
			, default_max_requests_(20)
			, websocket_parser([this](auto &&r) { MyWrite(r); }, [this](auto &&r) { WS_Write(r); })
		{
			secure_ = false;
			keepalive_ = false;
			write_in_progress = false;
			connection_type = ConnectionType::connection_http;
			socket_ = std::make_unique<boost::asio::ip::tcp::socket>(io_context);
		}

#ifdef WWW_ENABLE_SSL
		// this is the constructor for secure connections
		connection::connection(boost::asio::io_context &io_context, connection_manager &manager, request_handler &handler, int read_timeout, boost::asio::ssl::context &context, WebServerLogger logger)
			: m_logger(std::move(logger))
			, send_buffer_(nullptr)
			, read_timeout_(read_timeout)
			, read_timer_(io_context, std::chrono::seconds(read_timeout))
			, default_abandoned_timeout_(20 * 60)
			// 20mn before stopping abandoned connection
			, abandoned_timer_(io_context, std::chrono::seconds(default_abandoned_timeout_))
			, ws_session_renewal_timer_(io_context)
			, connection_manager_(manager)
			, request_handler_(handler)
			, status_(INITIALIZING)
			, default_max_requests_(20)
			, websocket_parser([this](auto &&r) { MyWrite(r); }, [this](auto &&r) { WS_Write(r); })
		{
			secure_ = true;
			keepalive_ = false;
			write_in_progress = false;
			connection_type = ConnectionType::connection_http;
			socket_ = nullptr;
			sslsocket_ = std::make_unique<ssl_socket>(io_context, context);
		}
#endif

#ifdef WWW_ENABLE_SSL
		// get the attached client socket of this connection
		ssl_socket::lowest_layer_type& connection::socket()
		{
			if (secure_) {
				return sslsocket_->lowest_layer();
			}
			return socket_->lowest_layer();
		}
#else
		// alternative: get the attached client socket of this connection if ssl is not compiled in
		boost::asio::ip::tcp::socket& connection::socket()
		{
			return *socket_;
		}
#endif

		void connection::start()
		{
			boost::system::error_code ec;
			boost::asio::ip::tcp::endpoint remote_endpoint = socket().remote_endpoint(ec);
			if (ec) {
				// Prevent the exception to be thrown to run to avoid the server to be locked (still listening but no more connection or stop).
				// If the exception returns to WebServer to also create a exception loop.
				if (m_logger) m_logger->Log(LogLevel::Error, "Getting error '%s' while getting remote_endpoint in connection::start", ec.message().c_str());
				connection_manager_.stop(shared_from_this());
				return;
			}
			host_remote_endpoint_address_ = remote_endpoint.address().to_string();
			host_remote_endpoint_port_ = std::to_string(remote_endpoint.port());

			boost::asio::ip::tcp::endpoint local_endpoint = socket().local_endpoint(ec);
			if (ec) {
				// Prevent the exception to be thrown to run to avoid the server to be locked (still listening but no more connection or stop).
				// If the exception returns to WebServer to also create a exception loop.
				if (m_logger) m_logger->Log(LogLevel::Error, "Getting error '%s' while getting local_endpoint in connection::start", ec.message().c_str());
				connection_manager_.stop(shared_from_this());
				return;
			}
			host_local_endpoint_address_ = local_endpoint.address().to_string();
			host_local_endpoint_port_ = std::to_string(local_endpoint.port());

			set_abandoned_timeout();

			if (secure_) {
#ifdef WWW_ENABLE_SSL
				status_ = WAITING_HANDSHAKE;
				// with ssl, we first need to complete the handshake before reading
				sslsocket_->async_handshake(boost::asio::ssl::stream_base::server, [self = shared_from_this()](auto &&err) { self->handle_handshake(err); });
#endif
			}
			else {
				// start reading data
				read_more();
			}
		}

		void connection::stop()
		{
			switch (connection_type) {
			case ConnectionType::connection_websocket:
			case ConnectionType::connection_websocket_closing:
			{
				auto handler = websocket_parser.DetachHandler();
				if (handler) {
					auto* webem = request_handler_.Get_myWebem();
					if (webem) {
						webem->ScheduleHandlerCleanup(std::move(handler));
					} else {
						if (m_logger) m_logger->Log(LogLevel::Error, "WebSocket: webem unavailable, falling back to inline handler cleanup");
						try { handler->Stop(); } catch (...) {}
					}
				}
				break;
			}
			}
			// Cancel timers
			cancel_ws_session_renewal();
			cancel_abandoned_timeout();
			cancel_read_timeout();

			// Initiate graceful connection closure.
			boost::system::error_code ignored_ec;
			socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec); // @note For portable behaviour with respect to graceful closure of a
																						// connected socket, call shutdown() before closing the socket.
			socket().close();
		}

		void connection::handle_timeout(const boost::system::error_code& error)
		{
			if (error != boost::asio::error::operation_aborted) {
				switch (connection_type) {
				case ConnectionType::connection_http:
					// Timers should be cancelled before stopping to remove tasks from the io_context.
					// The io_context will stop naturally when every tasks are removed.
					// If timers are not cancelled, the exception ERROR_ABANDONED_WAIT_0 is thrown up to the io_context::run() caller.
					cancel_abandoned_timeout();
					cancel_read_timeout();

					try {
						// Initiate graceful connection closure.
						boost::system::error_code ignored_ec;
						socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored_ec); // @note For portable behaviour with respect to graceful closure of a
																									// connected socket, call shutdown() before closing the socket.
						socket().close(ignored_ec);
					}
					catch (...) {
						if (m_logger) m_logger->Log(LogLevel::Error, "%s -> exception thrown while stopping connection", host_remote_endpoint_address_.c_str());
					}
					break;
				case ConnectionType::connection_websocket:
					websocket_parser.SendPing();
					break;
				}
			}
		}

#ifdef WWW_ENABLE_SSL
		void connection::handle_handshake(const boost::system::error_code& error)
		{
			status_ = ENDING_HANDSHAKE;
			if (secure_) { // assert
				if (!error)
				{
					// handshake completed, start reading
					read_more();
				}
				else
				{
					if (m_logger) m_logger->Debug(DebugCategory::WebServer, "connection::handle_handshake Error: %s", error.message().c_str());
					connection_manager_.stop(shared_from_this());
				}
			}
		}
#endif

		void connection::read_more()
		{
			status_ = WAITING_READ;

			// read chunks of max 4 KB
			boost::asio::streambuf::mutable_buffers_type buf = _buf.prepare(4096);

			// set timeout timer
			reset_read_timeout();

			if (secure_) {
#ifdef WWW_ENABLE_SSL
				// Perform secure read
				sslsocket_->async_read_some(buf, [self = shared_from_this()](auto &&err, auto bytes) { self->handle_read(err, bytes); });
#endif
			}
			else {
				// Perform plain read
				socket_->async_read_some(buf, [self = shared_from_this()](auto &&err, auto bytes) { self->handle_read(err, bytes); });
			}
		}

		void connection::SocketWrite(const std::string& buf)
		{
			// do not call directly, use MyWrite()
			if (write_in_progress) {
				// something went wrong, this shouldnt happen
			}
			write_in_progress = true;
			write_buffer = buf;
			if (secure_) {
#ifdef WWW_ENABLE_SSL
				boost::asio::async_write(*sslsocket_, boost::asio::buffer(write_buffer), [self = shared_from_this()](auto &&err, auto bytes) { self->handle_write(err, bytes); });
#endif
			}
			else {
				boost::asio::async_write(*socket_, boost::asio::buffer(write_buffer), [self = shared_from_this()](auto &&err, auto bytes) { self->handle_write(err, bytes); });
			}

		}

		void connection::WS_Write(const std::string& resp)
		{
			if (connection_type == ConnectionType::connection_websocket) {
				MyWrite(CWebsocketFrame::Create(opcode_text, resp, false));
			}
			else {
				// socket connection not set up yet, add to queue
				std::unique_lock<std::mutex> lock(writeMutex);
				writeQ.push_back(CWebsocketFrame::Create(opcode_text, resp, false));
			}
		}

		void connection::WS_WriteBinary(const std::string& data)
		{
			if (connection_type == ConnectionType::connection_websocket) {
				MyWrite(CWebsocketFrame::Create(opcode_binary, data, false));
			}
			else {
				// socket connection not set up yet, add to queue
				std::unique_lock<std::mutex> lock(writeMutex);
				writeQ.push_back(CWebsocketFrame::Create(opcode_binary, data, false));
			}
		}

		void connection::MyWrite(const std::string& buf)
		{
			switch (connection_type) {
			case ConnectionType::connection_http:
			case ConnectionType::connection_websocket:
				// we dont send data anymore in websocket closing state
				std::unique_lock<std::mutex> lock(writeMutex);
				if (write_in_progress) {
					// write in progress, add to queue
					writeQ.push_back(buf);
				}
				else {
					SocketWrite(buf);
				}
				break;
			}
		}

		void connection::handle_write_file(const boost::system::error_code& error, size_t bytes_transferred)
		{
			if (!error && sendfile_.is_open() && !sendfile_.eof())
			{
				if (!send_buffer_)
					send_buffer_ = std::make_unique<std::array<uint8_t, FILE_SEND_BUFFER_SIZE>>();
				size_t bread = static_cast<size_t>(sendfile_.read((char *)send_buffer_->data(), FILE_SEND_BUFFER_SIZE).gcount());
				if (bread <= 0)
				{
					//Error reading file!
					return;
				};
				if (secure_) {
#ifdef WWW_ENABLE_SSL
					boost::asio::async_write(*sslsocket_, boost::asio::buffer(*send_buffer_, bread),
								 [self = shared_from_this()](auto &&err, auto bytes) { self->handle_write_file(err, bytes); });
#endif
				}
				else {
					boost::asio::async_write(*socket_, boost::asio::buffer(*send_buffer_, bread),
								 [self = shared_from_this()](auto &&err, auto bytes) { self->handle_write_file(err, bytes); });
				}
				return;
			}

			if (sendfile_.is_open())
				sendfile_.close();

			send_buffer_.release();
			connection_manager_.stop(shared_from_this());
		}

		bool connection::send_file(const std::string& filename, std::string& attachment_name, reply& rep)
		{
			boost::system::error_code write_error;

			rep = reply::stock_reply(reply::ok);

			sendfile_.open(filename.c_str(), std::ios::in | std::ios::binary); //we open this file
			if (!sendfile_.is_open())
			{
				//File not found!
				rep = reply::stock_reply(reply::not_found);
				return false;
			}
			time_t ftime = last_write_time(filename, m_logger);

			sendfile_.seekg(0, std::ios::end);
			std::streamsize total_size = sendfile_.tellg();
			sendfile_.seekg(0, std::ios::beg);

			reply::add_header(&rep, "Cache-Control", "max-age=0, private");
			reply::add_header(&rep, "Accept-Ranges", "bytes");
			reply::add_header(&rep, "Date", utils::make_web_time(time(nullptr)));
			reply::add_header(&rep, "Last-Modified", utils::make_web_time(ftime));
			// Use the configured server name if set, otherwise omit the Server header.
			{
				auto* webem = request_handler_.Get_myWebem();
				if (webem && !webem->m_settings.server_name.empty())
					reply::add_header(&rep, "Server", webem->m_settings.server_name);
			}

			std::size_t last_dot_pos = filename.find_last_of('.');
			if (last_dot_pos != std::string::npos) {
				std::string file_extension = filename.substr(last_dot_pos + 1);
				std::string mime_type = mime_types::extension_to_type(file_extension);
				reply::add_header_content_type(&rep, mime_type);
			}
			reply::add_header_attachment(&rep, attachment_name);
			reply::add_header(&rep, "Content-Length", std::to_string(total_size));

			//write headers
			std::string headers = rep.to_string("GET");
			write_buffer = headers;

			if (secure_) {
#ifdef WWW_ENABLE_SSL
				boost::asio::async_write(*sslsocket_, boost::asio::buffer(write_buffer), [self = shared_from_this()](auto &&err, auto bytes) { self->handle_write_file(err, bytes); });
#endif
			}
			else {
				boost::asio::async_write(*socket_, boost::asio::buffer(write_buffer), [self = shared_from_this()](auto &&err, auto bytes) { self->handle_write_file(err, bytes); });
			}
			return true;
		}

		void connection::handle_read(const boost::system::error_code& error, std::size_t bytes_transferred)
		{
			status_ = READING;

			// data read, no need for timeouts (RK, note: race condition)
			cancel_read_timeout();

			if (!error && bytes_transferred > 0)
			{
				// ensure written bytes in the buffer
				_buf.commit(bytes_transferred);
				boost::tribool result;

				// http variables
				/// The incoming request.
				request request_;
				/// our response
				reply reply_;
				const char* begin;
				// websocket variables
				size_t bytes_consumed;

				switch (connection_type)
				{
				case ConnectionType::connection_http:
					begin = static_cast<const char*>(_buf.data().data());
					try
					{
						request_parser_.reset();
						boost::tie(result, boost::tuples::ignore) = request_parser_.parse(
							request_, begin, begin + _buf.size());
					}
					catch (...)
					{
						if (m_logger) m_logger->Log(LogLevel::Error, "Exception parsing HTTP. Address: %s", host_remote_endpoint_address_.c_str());
					}

					if (result) {
						struct timeval tv;
						std::time_t newt;

						if(m_logger && m_logger->IsAccessLogEnabled())
						{
							// Record timestamp (with milliseconds) before starting to process
						#ifdef CLOCK_REALTIME
							struct timespec ts;
							if (!clock_gettime(CLOCK_REALTIME, &ts))
							{
								tv.tv_sec = ts.tv_sec;
								tv.tv_usec = ts.tv_nsec / 1000;
							}
							else
						#endif
								utils::get_timeofday(&tv);
							newt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
						}

						size_t sizeread = begin - static_cast<const char*>(_buf.data().data());
						_buf.consume(sizeread);
						reply_.reset();
						const char* pConnection = request_.get_req_header(&request_, "Connection");
						keepalive_ = pConnection != nullptr && boost::iequals(pConnection, "Keep-Alive");
						request_.keep_alive = keepalive_;
						request_.host_remote_address = host_remote_endpoint_address_;
						request_.host_local_address = host_local_endpoint_address_;
						if (request_.host_remote_address.substr(0, 7) == "::ffff:") {
							request_.host_remote_address = request_.host_remote_address.substr(7);
						}
						if (request_.host_local_address.substr(0, 7) == "::ffff:") {
							request_.host_local_address = request_.host_local_address.substr(7);
						}
						request_.host_remote_port = host_remote_endpoint_port_;
						request_.host_local_port = host_local_endpoint_port_;
						host_last_request_uri_ = request_.uri;
						request_handler_.handle_request(request_, reply_);

						if(m_logger && m_logger->IsAccessLogEnabled())	// Only do this if we are gonna use it, otherwise don't spend the compute power
						{
							// Generate webserver logentry
							std::string wlHost = (reply_.originHost.empty()) ? request_.host_remote_address : reply_.originHost;
							std::string wlUser = "-";	// Maybe we can fill this sometime? Or maybe not so we don't expose sensitive data?
							std::string wlReqUri = request_.method + " " + request_.uri + " HTTP/" + std::to_string(request_.http_version_major) + (request_.http_version_minor ? "." + std::to_string(request_.http_version_minor): "");
							std::string wlReqRef = "-";
							if (request_.get_req_header(&request_, "Referer") != nullptr)
							{
								std::string shdr = request_.get_req_header(&request_, "Referer");
								wlReqRef = "\"" + shdr + "\"";
							}
							std::string wlBrowser = "-";
							if (request_.get_req_header(&request_, "User-Agent") != nullptr)
							{
								std::string shdr = request_.get_req_header(&request_, "User-Agent");
								wlBrowser = "\"" + shdr + "\"";
							}
							int wlResCode = (int)reply_.status;
							int wlContentSize = (int)reply_.content.length();

							std::stringstream sstr;
							sstr << std::setw(3) << std::setfill('0') << ((int)tv.tv_usec / 1000);
							std::string wlReqTimeMs = sstr.str();

							char wlReqTime[32];
							struct tm ltm{};
							utils::safe_localtime(&newt, &ltm);
							std::strftime(wlReqTime, sizeof(wlReqTime), "%d/%b/%Y:%H:%M:%S", &ltm);
							wlReqTime[sizeof(wlReqTime) - 1] = '\0';

							char wlReqTimeZone[16];
							std::strftime(wlReqTimeZone, sizeof(wlReqTimeZone), "%z", &ltm);
							wlReqTimeZone[sizeof(wlReqTimeZone) - 1] = '\0';

							if (m_logger) m_logger->AccessLog("%s - %s [%s.%s %s] \"%s\" %d %d %s %s", wlHost.c_str(), wlUser.c_str(), wlReqTime, wlReqTimeMs.c_str(), wlReqTimeZone, wlReqUri.c_str(), wlResCode, wlContentSize, wlReqRef.c_str(), wlBrowser.c_str());
						}

						if (reply_.status == reply::switching_protocols) {
							// this was an upgrade request
							// Do NOT set connection_type = connection_websocket here.
							// The handler's Start() may call WS_Write/WS_WriteBinary to send data
							// immediately. Those writes must be queued (writeQ) and delivered only
							// after the HTTP 101 response below. Setting connection_type to
							// connection_websocket here causes WS_Write/WS_WriteBinary to write
							// directly to the socket before the 101, corrupting the handshake.
							// connection_type is set to connection_websocket after MyWrite() below.
							// from now on we are a persistant connection
							keepalive_ = true;
							// Create the handler via factory for this request path
							{
								auto* webem = request_handler_.Get_myWebem();
								if (webem)
								{
									std::string req_path = webem->ExtractRequestPath(request_.uri);
									auto factory = webem->GetWebsocketFactory(req_path);
									if (factory)
									{
										// Capture weak_ptr instead of raw this so the writer
										// lambdas safely no-op after the connection is destroyed.
										// This prevents use-after-free when async handler cleanup
										// runs after the connection has already been torn down.
										std::weak_ptr<connection> weak_self = shared_from_this();
										auto ws_handler = factory(
											webem,
											[weak_self](const std::string& data) {
												if (auto self = weak_self.lock())
													self->WS_Write(data);
											},
											[weak_self](const std::string& data) {
												if (auto self = weak_self.lock())
													self->WS_WriteBinary(data);
											},
											reply_.ws_session);
										websocket_parser.SetHandler(ws_handler);
										webem->RegisterWebsocketHandler(ws_handler);
									}
								}
							}
							m_ws_session_id = reply_.ws_session.id;
							start_ws_session_renewal();
							websocket_parser.Start();
							// todo: check if multiple connection from the same client in CONNECTING state?
						}
						else if (reply_.status == reply::download_file) {
							std::string filename_attachment = reply_.content;
							size_t npos = filename_attachment.find("\r\n");
							if (npos == std::string::npos)
							{
								reply_ = reply::stock_reply(reply::internal_server_error);
							}
							else
							{
								std::string filename = filename_attachment.substr(0, npos);
								std::string attachment = filename_attachment.substr(npos + 2);
								if (send_file(filename, attachment, reply_))
									return;
							}
						}

						if (request_.keep_alive && ((reply_.status == reply::ok) || (reply_.status == reply::no_content) || (reply_.status == reply::not_modified))) {
							// Allows request handler to override the header (but it should not)
							reply::add_header_if_absent(&reply_, "Connection", "Keep-Alive");
							std::stringstream ss;
							ss << "max=" << default_max_requests_ << ", timeout=" << read_timeout_;
							reply::add_header_if_absent(&reply_, "Keep-Alive", ss.str());
						}

						MyWrite(reply_.to_string(request_.method));
						if (reply_.status == reply::switching_protocols) {
							// this was an upgrade request, set this value after MyWrite to allow the 101 response to go out
							connection_type = ConnectionType::connection_websocket;
						}

						if (keepalive_) {
							read_more();
						}
						status_ = WAITING_WRITE;
					}
					else if (!result)
					{
						if (m_logger) m_logger->Log(LogLevel::Error, "Error parsing http request address: %s", host_remote_endpoint_address_.c_str());
						keepalive_ = false;
						reply_ = reply::stock_reply(reply::bad_request);
						MyWrite(reply_.to_string(request_.method));
						if (keepalive_) {
							read_more();
						}
					}
					else
					{
						read_more();
					}
					break;
				case ConnectionType::connection_websocket:
				case ConnectionType::connection_websocket_closing:
					begin = static_cast<const char*>(_buf.data().data());
					result = websocket_parser.parse((const unsigned char*)begin, _buf.size(), bytes_consumed, keepalive_);
					_buf.consume(bytes_consumed);
					if (result) {
						// we received a complete packet (that was handled already)
						if (keepalive_) {
							read_more();
						}
						else {
							// a connection close control packet was received
							// todo: wait for writeQ to flush?
							connection_type = ConnectionType::connection_websocket_closing;
						}
					}
					else // if (!result)
					{
						read_more();
					}
					break;
				}
			}
			else if (error == boost::asio::error::eof)
			{
				connection_manager_.stop(shared_from_this());
			}
			else if (error != boost::asio::error::operation_aborted)
			{
				connection_manager_.stop(shared_from_this());
			}
		}

		void connection::handle_write(const boost::system::error_code& error, size_t bytes_transferred)
		{
			std::unique_lock<std::mutex> lock(writeMutex);
			write_buffer.clear();
			write_in_progress = false;
			bool stopConnection = false;
			if (!error && !writeQ.empty())
			{
				std::string buf = writeQ.front();
				writeQ.pop_front();
				SocketWrite(buf);
				if (keepalive_)
				{
					reset_abandoned_timeout();
				}
				return;
			}

			// Stop needs to be outside the lock to avoid potential deadlocks.
			lock.unlock();

			if (error == boost::asio::error::operation_aborted)
			{
				connection_manager_.stop(shared_from_this());
			}
			else if (error)
			{
					connection_manager_.stop(shared_from_this());
			}
			else if (keepalive_)
			{
				status_ = ENDING_WRITE;
				reset_abandoned_timeout();
			}
			else
			{
				//Everything has been send. Closing connection.
				connection_manager_.stop(shared_from_this());
			}
		}

		// schedule read timeout timer
		void connection::set_read_timeout() {
			read_timer_.expires_after(std::chrono::seconds(read_timeout_));
			read_timer_.async_wait([self = shared_from_this()](auto &&err) { self->handle_read_timeout(err); });
		}

		/// simply cancel read timeout timer
		void connection::cancel_read_timeout() {
			try {
				read_timer_.cancel();
			}
			catch (...) {
				if (m_logger) m_logger->Log(LogLevel::Error, "%s -> exception thrown while canceling read timeout", host_remote_endpoint_address_.c_str());
			}
		}

		/// reschedule read timeout timer
		void connection::reset_read_timeout() {
			cancel_read_timeout();
			set_read_timeout();
		}

		/// stop connection on read timeout
		void connection::handle_read_timeout(const boost::system::error_code& error) {
			if (!error && keepalive_ && (connection_type == ConnectionType::connection_websocket)) {
				// For WebSockets that requested keep-alive, use a Server side Ping
				websocket_parser.SendPing();
			}
			else if (!error)
			{
				connection_manager_.stop(shared_from_this());
			}
			else if (error != boost::asio::error::operation_aborted)
			{
				if (m_logger) m_logger->Log(LogLevel::Error, "connection::handle_read_timeout Error: %s", error.message().c_str());
				connection_manager_.stop(shared_from_this());
			}
		}

		/// schedule abandoned timeout timer
		void connection::set_abandoned_timeout() {
			abandoned_timer_.expires_after(std::chrono::seconds(default_abandoned_timeout_));
			abandoned_timer_.async_wait([self = shared_from_this()](auto &&err) { self->handle_abandoned_timeout(err); });
		}

		/// simply cancel abandoned timeout timer
		void connection::cancel_abandoned_timeout() {
			try {
				abandoned_timer_.cancel();
			}
			catch (...) {
				if (m_logger) m_logger->Log(LogLevel::Error, "%s -> exception thrown while canceling abandoned timeout", host_remote_endpoint_address_.c_str());
			}
		}

		/// reschedule abandoned timeout timer
		void connection::reset_abandoned_timeout() {
			cancel_abandoned_timeout();
			set_abandoned_timeout();
		}

		/// stop connection on abandoned timeout
		void connection::handle_abandoned_timeout(const boost::system::error_code& error) {
			if (error != boost::asio::error::operation_aborted) {
				if (m_logger) m_logger->Log(LogLevel::Status, "%s -> handle abandoned timeout (status=%d)", host_remote_endpoint_address_.c_str(), status_);
				connection_manager_.stop(shared_from_this());
			}
		}

		// Interval at which the WebSocket session renewal timer fires.
		// Must be shorter than SHORT_SESSION_TIMEOUT/2 (defined in cWebem.cpp) so that
		// RenewSessionIfNeeded() reliably catches the renewal window before expiry.
		static constexpr int kWsSessionRenewalInterval = 60; // seconds

		/// Start periodic session renewal timer for an authenticated WebSocket connection.
		/// Fires every kWsSessionRenewalInterval seconds so the session stays alive
		/// even when the client sends no HTTP requests (e.g. passive dashboard pages).
		void connection::start_ws_session_renewal() {
			if (m_ws_session_id.empty())
				return;
			ws_session_renewal_timer_.expires_after(std::chrono::seconds(kWsSessionRenewalInterval));
			ws_session_renewal_timer_.async_wait([self = shared_from_this()](const boost::system::error_code& err) {
				self->handle_ws_session_renewal(err);
			});
		}

		void connection::cancel_ws_session_renewal() {
			try {
				ws_session_renewal_timer_.cancel();
			}
			catch (...) {}
		}

		void connection::handle_ws_session_renewal(const boost::system::error_code& error) {
			if (error == boost::asio::error::operation_aborted)
				return;
			if (connection_type != ConnectionType::connection_websocket)
				return;
			auto* webem = request_handler_.Get_myWebem();
			if (webem && !m_ws_session_id.empty())
				webem->RenewSessionIfNeeded(m_ws_session_id);
			start_ws_session_renewal();
		}

	} // namespace server
} // namespace http
