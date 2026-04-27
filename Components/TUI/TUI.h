#pragma once
#include <format>
#include <string>
#include <string_view>

namespace TUI
{
	struct Config
	{
		bool  Timestamps = true;
		bool  Labels     = true;
		bool  Color      = true;
		FILE* Out        = stdout;
		FILE* ErrOut     = stderr;
	};

	namespace ANSI
	{
		constexpr std::string_view Reset = "\033[0m";
		constexpr std::string_view Bold  = "\033[1m";
		constexpr std::string_view Dim   = "\033[2m";

		constexpr std::string_view Lavender = "\033[38;5;183m";
		constexpr std::string_view Mint     = "\033[38;5;121m";
		constexpr std::string_view Peach    = "\033[38;5;216m";
		constexpr std::string_view Rose     = "\033[38;5;211m";
		constexpr std::string_view Sky      = "\033[38;5;153m";
		constexpr std::string_view Lemon    = "\033[38;5;228m";
		constexpr std::string_view Lilac    = "\033[38;5;219m";
		constexpr std::string_view Frost    = "\033[38;5;159m";
		constexpr std::string_view Mist     = "\033[38;5;250m";

		constexpr std::string_view BgLavender = "\033[48;5;183m";
		constexpr std::string_view BgMint     = "\033[48;5;121m";
		constexpr std::string_view BgPeach    = "\033[48;5;216m";
		constexpr std::string_view BgRose     = "\033[48;5;211m";
		constexpr std::string_view BgSky      = "\033[48;5;153m";
	}

	namespace Internal
	{
		std::string Timestamp();
		void        Emit( FILE* Destination, std::string_view TimestampColor, std::string_view LabelColor, std::string_view Label, std::string_view MessageColor, std::string_view Message );
	}

	Config& GlobalConfig();
	void    PrintSection( std::string_view Title = "" );

	template <typename... Args>
	void PrintOk( std::format_string<Args...> Format, Args&&... Arguments )
	{
		const auto Message = std::format( Format, std::forward<Args>( Arguments )... );
		Internal::Emit( GlobalConfig().Out, ANSI::Mint, ANSI::Mint, "[ ok ]", ANSI::Mint, Message );
	}

	template <typename... Args>
	void PrintError( std::format_string<Args...> Format, Args&&... Arguments )
	{
		const auto Message = std::format( Format, std::forward<Args>( Arguments )... );
		Internal::Emit( GlobalConfig().ErrOut, ANSI::Rose, ANSI::Rose, "[ !! ]", ANSI::Rose, Message );
	}

	template <typename... Args>
	void PrintWarn( std::format_string<Args...> Format, Args&&... Arguments )
	{
		const auto Message = std::format( Format, std::forward<Args>( Arguments )... );
		Internal::Emit( GlobalConfig().Out, ANSI::Lemon, ANSI::Lemon, "[ ~~ ]", ANSI::Lemon, Message );
	}

	template <typename... Args>
	void PrintInfo( std::format_string<Args...> Format, Args&&... Arguments )
	{
		const auto Message = std::format( Format, std::forward<Args>( Arguments )... );
		Internal::Emit( GlobalConfig().Out, ANSI::Sky, ANSI::Sky, "[ >> ]", ANSI::Sky, Message );
	}

	template <typename... Args>
	void PrintDebug( std::format_string<Args...> Format, Args&&... Arguments )
	{
		const auto Message = std::format( Format, std::forward<Args>( Arguments )... );
		Internal::Emit( GlobalConfig().Out, ANSI::Lavender, ANSI::Lavender, "[ ## ]", ANSI::Lavender, Message );
	}

	template <typename... Args>
	void PrintTrace( std::format_string<Args...> Format, Args&&... Arguments )
	{
		const auto Message = std::format( Format, std::forward<Args>( Arguments )... );
		Internal::Emit( GlobalConfig().Out, ANSI::Mist, ANSI::Mist, "[ .. ]", ANSI::Mist, Message );
	}

	template <typename... Args>
	void PrintNote( std::format_string<Args...> Format, Args&&... Arguments )
	{
		const auto Message = std::format( Format, std::forward<Args>( Arguments )... );
		Internal::Emit( GlobalConfig().Out, ANSI::Lilac, ANSI::Lilac, "[ ** ]", ANSI::Lilac, Message );
	}

	template <typename... Args>
	void PrintSuccess( std::format_string<Args...> Format, Args&&... Arguments )
	{
		const auto Message = std::format( Format, std::forward<Args>( Arguments )... );
		Internal::Emit( GlobalConfig().Out, ANSI::Mint, ANSI::Mint, "[ ✓  ]", ANSI::Mint, Message );
	}

	template <typename... Args>
	void PrintFail( std::format_string<Args...> Format, Args&&... Arguments )
	{
		const auto Message = std::format( Format, std::forward<Args>( Arguments )... );
		Internal::Emit( GlobalConfig().ErrOut, ANSI::Rose, ANSI::Rose, "[ ✗  ]", ANSI::Rose, Message );
	}
}

using namespace TUI;
