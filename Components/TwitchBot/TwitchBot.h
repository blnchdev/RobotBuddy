#pragma once
#include "Components/TokenManager/TokenManager.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

#include <functional>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>

namespace Components
{
	namespace asio = boost::asio;
	namespace beast = boost::beast;
	namespace websocket = beast::websocket;
	namespace ssl = asio::ssl;
	using tcp = asio::ip::tcp;

	struct TwitchMessage
	{
		std::string ID;
		std::string Username;
		std::string Channel;
		std::string Text;
		bool        IsModerator;
		bool        IsSubscriber;
		bool        IsOwner;
	};

	class TwitchBot
	{
	public:
		using MessageHandler = std::function<void( const TwitchMessage& )>;

		TwitchBot( TokenManager& Tokens, std::string_view Nick );

		void Connect();
		void Login( std::span<const std::string_view> ChannelsToJoin );
		void Join( std::string_view Channel );
		void Part( std::string_view Channel );
		void Run( const MessageHandler& OnMessage );
		void SendChat( std::string_view Channel, std::string_view Message );
		void ReplyTo( const TwitchMessage& Parent, std::string_view Message );

	private:
		void SendRaw( const std::string& Line );

		TokenManager&                               Tokens;
		std::string                                 Nick;
		std::unordered_set<std::string>             Channels;
		asio::io_context                            IOC;
		ssl::context                                SSLC;
		websocket::stream<ssl::stream<tcp::socket>> WebSocket;
	};
}
