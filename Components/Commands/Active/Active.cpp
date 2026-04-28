#include "Active.h"

#include "Globals/Globals.h"

namespace Components::Operation
{
	void Active( const Command* Data )
	{
		auto ActiveGame = Globals::LeagueAPI->GetCurrentGame( Data->ChannelID );

		std::string Message;

		if ( ActiveGame.has_value() )
		{
			Message = std::format( "{}'s is playing {} in a game where the {}", Data->ChannelID, ActiveGame->Champion, ActiveGame->AverageElo );
		}
		else
		{
			Message = std::format( "{} is not currently in-game!", Data->ChannelID );
		}

		Globals::Bot->ReplyTo( *Data->Context, Message );
	}
}
