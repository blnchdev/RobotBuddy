#include "TUI.h"

#include <chrono>
#include <print>
#include <windows.h>

namespace TUI
{
	std::string Internal::Timestamp()
	{
		const auto Now          = std::chrono::system_clock::now();
		const auto Time         = std::chrono::system_clock::to_time_t( Now );
		const auto Milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>( Now.time_since_epoch() ) % 1000;

		std::tm TM{};
		( void )localtime_s( &TM, &Time );
		return std::format( "{:02}:{:02}:{:02}.{:03}", TM.tm_hour, TM.tm_min, TM.tm_sec, Milliseconds.count() );
	}

	void Internal::Emit( FILE* Destination, const std::string_view TimestampColor, const std::string_view LabelColor, const std::string_view Label, const std::string_view MessageColor, const std::string_view Message )
	{
		const auto& Config = GlobalConfig();

		if ( Config.Color )
		{
			if ( Config.Timestamps )
			{
				std::print( Destination, "{}{}[{}]{} ", ANSI::Dim.data(), TimestampColor.data(), Timestamp(), ANSI::Reset.data() );
			}

			if ( Config.Labels && !Label.empty() )
			{
				std::print( Destination, "{}{}{}{} ", ANSI::Bold.data(), LabelColor.data(), Label.data(), ANSI::Reset.data() );
			}

			std::println( Destination, "{}{}{}", MessageColor.data(), Message.data(), ANSI::Reset.data() );
		}
		else
		{
			if ( Config.Timestamps ) std::print( Destination, "[{}] ", Timestamp() );
			if ( Config.Labels && !Label.empty() ) std::print( Destination, "{} ", Label.data() );
			std::println( Destination, "{}", Message.data() );
		}
	}

	Config& GlobalConfig()
	{
		static Config Config{};
		return Config;
	}

	void PrintSection( const std::string_view Title )
	{
		static bool     Initialized = false;
		static uint32_t Columns;

		const auto&       Config = GlobalConfig();
		const std::string line( 48, '─' );

		if ( !Initialized )
		{
			Initialized = true;
			CONSOLE_SCREEN_BUFFER_INFO CSBI;
			GetConsoleScreenBufferInfo( GetStdHandle( STD_OUTPUT_HANDLE ), &CSBI );
			Columns = CSBI.srWindow.Right - CSBI.srWindow.Left - 6;
		}

		if ( Config.Color )
		{
			if ( Title.empty() )
			{
				std::println( "{}{}{}", ANSI::Lavender, line, ANSI::Reset );
			}
			else
			{
				const auto Padding = ( Columns - std::min<std::size_t>( Title.size(), Columns ) ) / 2;

				std::string Left( Padding, ' ' );
				std::string Right( Padding, ' ' );

				if ( Title.size() % 2 != 0 )
				{
					Right.push_back( ' ' );
				}

				std::println( "{}{}--- {}{}{} ---{}", ANSI::Bold, ANSI::Lavender, Left, Title, Right, ANSI::Reset );
			}
		}
		else
		{
			if ( Title.empty() )
			{
				std::println( "{}", line );
			}
			else
			{
				const auto Padding = ( Columns - std::min<std::size_t>( Title.size(), Columns ) ) / 2;

				std::string Left( Padding, ' ' );
				std::string Right( Padding, ' ' );

				if ( Title.size() % 2 != 0 )
				{
					Right.push_back( ' ' );
				}

				std::println( "--- {}{}{} ---", Left, Title, Right );
			}
		}
	}
}
