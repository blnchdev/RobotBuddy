#include "API.h"

namespace Components
{
	namespace
	{
		asio::awaitable<void> HandleClient( tcp::socket socket )
		{
			beast::flat_buffer              buf;
			http::request<http::empty_body> req;

			co_await http::async_read( socket, buf, req, asio::use_awaitable );

			http::response<http::string_body> Response{ http::status::ok, req.version() };
			Response.set( http::field::content_type, "text/plain" );
			Response.body() = "OK";
			Response.prepare_payload();

			co_await http::async_write( socket, Response, asio::use_awaitable );
		}

		asio::awaitable<void> Listener( tcp::acceptor& Acceptor )
		{
			while ( true )
			{
				auto [ EC, Socket ] = co_await Acceptor.async_accept( asio::as_tuple( asio::use_awaitable ) );
				if ( EC.failed() ) continue;
				co_spawn( Acceptor.get_executor(), HandleClient( std::move( Socket ) ), asio::detached );
			}
		}
	}

	API::API() : Acceptor( IOC, { tcp::v4(), PORT } )
	{
		co_spawn( IOC, Listener( this->Acceptor ), asio::detached );

		this->Worker = std::jthread( [&] { IOC.run(); } );
	}
}
