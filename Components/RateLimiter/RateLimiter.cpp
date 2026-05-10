#include "RateLimiter.h"

#include "Globals/Globals.h"

namespace Components
{
	RateLimiter::RateLimiter( const uint32_t PerSecond, const uint32_t Per120Seconds ) : Strand( asio::make_strand( Globals::IOC ) ), Timer( Globals::IOC ),
	                                                                                     TokensShort( PerSecond ), TokensLong( Per120Seconds ),
	                                                                                     MaxShort( PerSecond ), MaxLong( Per120Seconds ),
	                                                                                     ShortWindowEnd( std::chrono::steady_clock::now() + std::chrono::seconds( 1 ) ), LongWindowEnd( std::chrono::steady_clock::now() + std::chrono::seconds( 120 ) )
	{
	}

	asio::awaitable<void> RateLimiter::Acquire()
	{
		co_await asio::dispatch( Strand, asio::use_awaitable );

		while ( TokensShort == 0 || TokensLong == 0 )
		{
			const auto Now = std::chrono::steady_clock::now();

			if ( TokensShort == 0 && Now >= ShortWindowEnd )
			{
				TokensShort    = MaxShort;
				ShortWindowEnd = Now + std::chrono::seconds( 1 );
			}

			if ( TokensLong == 0 && Now >= LongWindowEnd )
			{
				TokensLong    = MaxLong;
				LongWindowEnd = Now + std::chrono::seconds( 120 );
			}

			if ( TokensShort == 0 || TokensLong == 0 )
			{
				const auto WaitUntil = ( TokensShort == 0 ) ? ShortWindowEnd : LongWindowEnd;
				Timer.expires_at( WaitUntil );
				co_await Timer.async_wait( asio::use_awaitable );
			}
		}

		--TokensShort;
		--TokensLong;
	}

	void RateLimiter::UpdateFromHeaders( const http::response<http::string_body>& Response )
	{
		asio::dispatch( Strand, [this, &Response]
		{
			if ( const auto Iterator = Response.find( "X-App-Rate-Limit-Count" ); Iterator != Response.end() )
			{
				std::string_view Value{ Iterator->value() };

				auto ParseCount = [&] ( const std::string_view Bucket )
				{
					uint32_t Count = 0;
					std::from_chars( Bucket.data(), Bucket.data() + Bucket.size(), Count );
					return Count;
				};

				if ( const auto Comma = Value.find( ',' ); Comma != std::string_view::npos )
				{
					const auto ShortCount = ParseCount( Value.substr( 0, Value.find( ':' ) ) );
					const auto LongCount  = ParseCount( Value.substr( Comma + 1, Value.rfind( ':' ) - Comma - 1 ) );

					TokensShort = ShortCount >= MaxShort ? 0 : MaxShort - ShortCount;
					TokensLong  = LongCount >= MaxLong ? 0 : MaxLong - LongCount;
				}
			}
		} );
	}
}
