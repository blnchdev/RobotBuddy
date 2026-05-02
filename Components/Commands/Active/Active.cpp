#include "Active.h"

#include "Globals/Globals.h"

namespace Components::Operation
{
	asio::awaitable<void> Active( const Command* Data )
	{
		auto ActiveGame = co_await Globals::LeagueAPI->GetCurrentGame( Data->ChannelName );

		std::string Message;

		if ( ActiveGame.has_value() )
		{
			Message = std::format( "{} is playing {} in a game where the {}", Data->ChannelName, ActiveGame->Champion, ActiveGame->AverageElo );
		}
		else
		{
			Message = std::format( "{} is not currently in-game!", Data->ChannelName );
		}

		Globals::TwitchAPI->ReplyTo( Data->Context, Message );
		co_return;
	}
}
