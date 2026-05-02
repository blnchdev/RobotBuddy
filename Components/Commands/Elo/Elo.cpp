#include "Elo.h"

#include "Globals/Globals.h"

namespace Components::Operation
{
	asio::awaitable<void> Elo( const Command* Data )
	{
		const auto Response = co_await Globals::LeagueAPI->GetLeagueRankFormatted( Data->ChannelName );

		if ( Response.has_value() )
		{
			Globals::TwitchAPI->ReplyTo( Data->Context, Response.value() );
		}
		else
		{
			Globals::TwitchAPI->ReplyTo( Data->Context, std::format( "{} is currently not ranked", Data->ChannelName ) );
		}

		co_return;
	}
}
