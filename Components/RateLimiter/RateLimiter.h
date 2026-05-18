#pragma once
#include <deque>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/url.hpp>

#include <nlohmann/json_fwd.hpp>

namespace Components
{
	namespace asio = boost::asio;
	namespace beast = boost::beast;
	namespace http = beast::http;
	using tcp  = asio::ip::tcp;
	using json = nlohmann::json;

	struct RateLimiter
	{
		explicit RateLimiter( uint32_t PerSecond = 18, uint32_t Per120Seconds = 95 );

		asio::awaitable<void> Acquire();
		void                  UpdateFromHeaders( const http::response<http::string_body>& Response );

	private:
		void Cleanup( const std::chrono::high_resolution_clock::time_point& Now );

		asio::strand<asio::io_context::executor_type> Strand;
		uint32_t                                      PerSecond, Per120Seconds;

		std::deque<std::chrono::high_resolution_clock::time_point> Recent1s;
		std::deque<std::chrono::high_resolution_clock::time_point> Recent120s;
	};
}
