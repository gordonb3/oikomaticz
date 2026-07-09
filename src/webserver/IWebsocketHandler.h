#pragma once
#include <string>
#include <functional>
#include <memory>
#include "session.h"

namespace http {
namespace server {

	class cWebem;

	class IWebsocketHandler {
	public:
		virtual ~IWebsocketHandler() = default;

		// Called when a complete WebSocket message is received
		virtual bool Handle(const std::string& packet_data, bool outbound) = 0;

		// Lifecycle
		virtual void Start() = 0;
		virtual void Stop() = 0;
	};

	// Factory: given a webem pointer, two writer functions (text and binary), and the
	// authenticated session from the HTTP upgrade handshake, create a handler.
	// The session is fully resolved by CheckAuthentication() before the upgrade completes,
	// so no cookie parsing is required inside the handler.
	using WebsocketHandlerFactory = std::function<
		std::shared_ptr<IWebsocketHandler>(
			cWebem*                                   webem,
			std::function<void(const std::string&)>   text_writer,
			std::function<void(const std::string&)>   binary_writer,
			const WebEmSession&                       session
		)
	>;

} // namespace server
} // namespace http
