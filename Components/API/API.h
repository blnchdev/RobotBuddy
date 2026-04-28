#pragma once
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/url.hpp>

#include <nlohmann/json.hpp>

namespace Components
{
	namespace asio = boost::asio;
	namespace beast = boost::beast;
	namespace http = beast::http;
	namespace ssl = asio::ssl;
	using tcp  = asio::ip::tcp;
	using json = nlohmann::json;

	constexpr int32_t PORT = 19723;

	class API
	{
	public:
		API();

	private:
		asio::io_context IOC{ 1 };
		tcp::acceptor    Acceptor;
		std::jthread     Worker;
		std::stop_source StopSource = {};
	};
}
