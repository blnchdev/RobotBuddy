#include "Commands.h"

#include "Accounts/Accounts.h"
#include "Active/Active.h"
#include "Today/Today.h"
#include "Components/TUI/TUI.h"
#include "Elo/Elo.h"
#include "Globals/Globals.h"
#include "KDA/KDA.h"
#include "OPGG/OPGG.h"

namespace Components
{
	namespace
	{
		struct ParseResult
		{
			std::string_view A, B, C;
		};

		ParseResult ParseCommand( std::string_view Message )
		{
			ParseResult Result{};

			size_t Position = Message.find( ' ' );
			if ( Position == std::string_view::npos )
			{
				Result.A = Message;
				return Result;
			}

			Result.A = Message.substr( 0, Position );
			Message.remove_prefix( Position + 1 );

			Position = Message.find( ' ' );
			if ( Position == std::string_view::npos )
			{
				Result.B = Message;
				return Result;
			}

			Result.B = Message.substr( 0, Position );
			Message.remove_prefix( Position + 1 );

			Result.C = Message;

			return Result;
		}
	}

	asio::awaitable<void> Dispatch( const TwitchMessage& Message )
	{
		const size_t Hash = Message.Channel.find( '#' );

		if ( Hash == std::string::npos || Hash + 1 >= Message.Channel.size() )
		{
			PrintError( "Malformed Channel name! '{}' does not contain a hash", Message.Channel );
			co_return;
		}

		Command SentCommand{ .Context = Message };

		auto [ Operation, Argument1, Argument2 ] = ParseCommand( SentCommand.Context.Text );

		SentCommand.ChannelName = std::string( SentCommand.Context.Channel ).substr( Hash + 1 );
		SentCommand.Operation   = std::string( Operation );
		SentCommand.Argument1   = std::string( Argument1 );
		SentCommand.Argument2   = std::string( Argument2 );

		PrintDebug( "Operation: '{}', Arg1: '{}', Arg2: '{}'", SentCommand.Operation, SentCommand.Argument1, SentCommand.Argument2 );

		static const std::unordered_map<std::string_view, asio::awaitable<void>(*)( const Command* )> Handlers =
		{
			{ "!accounts", Operation::Accounts },
			{ "!account", Operation::Accounts },
			{ "!today", Operation::Today },
			{ "!score", Operation::Today },
			{ "!opgg", Operation::OPGG },
			{ "!active", Operation::Active },
			{ "!current", Operation::Active },
			{ "!elo", Operation::Elo },
			{ "!kda", Operation::KDA },
		};

		const auto Iterator = Handlers.find( SentCommand.Operation );
		if ( Iterator == Handlers.end() ) co_return;

		asio::co_spawn( Globals::IOC, [Command = std::move( SentCommand ), Method = Iterator->second]() mutable -> asio::awaitable<void>
		{
			co_await Method( &Command );
		}, asio::detached );
	}
}
