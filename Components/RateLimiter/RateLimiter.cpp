#include "RateLimiter.h"

#include "Globals/Globals.h"

namespace Components
{
	RateLimiter::RateLimiter( const uint32_t PerSecond, const uint32_t Per120Seconds ) : Strand( asio::make_strand( Globals::IOC ) ), PerSecond( PerSecond ), Per120Seconds( Per120Seconds )
	{
	}

	asio::awaitable<void> RateLimiter::Acquire()
	{
		co_await asio::dispatch( Strand, asio::use_awaitable );

		while ( true )
		{
			const auto Now = std::chrono::high_resolution_clock::now();
			Cleanup( Now );
			auto Earliest = Now;

			if ( Recent1s.size() > PerSecond )
			{
				Earliest = std::max( Earliest, Recent1s.front() + std::chrono::seconds( 1 ) );
			}

			if ( Recent120s.size() > Per120Seconds )
			{
				Earliest = std::max( Earliest, Recent120s.front() + std::chrono::minutes( 2 ) );
			}

			if ( Earliest <= Now )
			{
				Recent1s.push_back( Now );
				Recent120s.push_back( Now );
				co_return;
			}

			asio::steady_timer Timer( Strand );
			Timer.expires_at( Earliest );
			co_await Timer.async_wait( asio::use_awaitable );
		}
	}

	void RateLimiter::UpdateFromHeaders( const http::response<http::string_body>& Response )
	{
		// Unimplemented
	}

	void RateLimiter::Cleanup( const std::chrono::high_resolution_clock::time_point& Now )
	{
		while ( !Recent1s.empty() && Now - Recent1s.front() >= std::chrono::seconds( 1 ) )
		{
			Recent1s.pop_front();
		}

		while ( !Recent120s.empty() && Now - Recent120s.front() >= std::chrono::seconds( 120 ) )
		{
			Recent120s.pop_front();
		}
	}
}
