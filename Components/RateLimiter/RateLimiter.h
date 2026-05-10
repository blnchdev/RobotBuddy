#pragma once
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
		asio::strand<asio::io_context::executor_type> Strand;
		asio::steady_timer                            Timer;
		uint32_t                                      TokensShort,    TokensLong;
		uint32_t                                      MaxShort,       MaxLong;
		std::chrono::steady_clock::time_point         ShortWindowEnd, LongWindowEnd;
	};
}
