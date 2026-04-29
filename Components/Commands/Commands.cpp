#include "Commands.h"

#include "Accounts/Accounts.h"
#include "Active/Active.h"
#include "Today/Today.h"
#include "Components/TUI/TUI.h"
#include "Elo/Elo.h"
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

	void Dispatch( const TwitchMessage& Message )
	{
		const size_t Hash = Message.Channel.find( '#' );

		if ( Hash == std::string::npos || Hash + 1 >= Message.Channel.size() )
		{
			PrintError( "Malformed Channel name! '{}' does not contain a hash", Message.Channel );
			return;
		}

		const std::string_view ChannelID = std::string_view( Message.Channel ).substr( Hash + 1 );

		auto [ Operation, Argument1, Argument2 ] = ParseCommand( Message.Text );

		const Command SentCommand = { .ChannelID = ChannelID, .Operation = Operation, .Argument1 = Argument1, .Argument2 = Argument2, .Context = &Message };

		if ( Operation == "!accounts" || Operation == "!account" )
		{
			Operation::Accounts( &SentCommand );
			return;
		}

		if ( Operation == "!today" || Operation == "!score" )
		{
			Operation::Today( &SentCommand );
			return;
		}

		if ( Operation == "!opgg" )
		{
			Operation::OPGG( &SentCommand );
			return;
		}

		if ( Operation == "!active" || Operation == "!current" )
		{
			Operation::Active( &SentCommand );
			return;
		}

		if ( Operation == "!elo" )
		{
			Operation::Elo( &SentCommand );
			return;
		}

		if ( Operation == "!kda" )
		{
			Operation::KDA( &SentCommand );
		}
	}
}
