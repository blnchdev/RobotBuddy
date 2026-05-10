#include "Active.h"

#include "Globals/Globals.h"

namespace Components::Operation
{
	asio::awaitable<void> Active( const Command* Data )
	{
		const auto  ActiveAccount = Globals::LeagueAPI->GetActiveAccount( Data->ChannelName );
		std::string Message       = std::format( "{} is not currently in-game!", Data->ChannelName );

		if ( !ActiveAccount )
		{
			Globals::TwitchAPI->ReplyTo( Data->Context, Message );
			co_return;
		}

		const auto ActiveGame = ActiveAccount->CurrentGame.get();

		if ( ActiveGame )
		{
			Message = std::format( "{} is playing {} in a game where the average elo is {}", Data->ChannelName, ActiveGame->Champion, ActiveGame->AverageElo.Formatted() );
		}

		Globals::TwitchAPI->ReplyTo( Data->Context, Message );
		co_return;
	}
}
