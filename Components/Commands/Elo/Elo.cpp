#include "Elo.h"

#include "Globals/Globals.h"

namespace Components::Operation
{
	void Elo( const Command* Data )
	{
		const auto Response = Globals::LeagueAPI->GetLeagueRankFormatted( Data->ChannelID );

		if ( Response.has_value() )
		{
			Globals::Bot->ReplyTo( *Data->Context, Response.value() );
		}
		else
		{
			Globals::Bot->ReplyTo( *Data->Context, std::format( "{} is currently not ranked", Data->ChannelID ) );
		}
	}
}
