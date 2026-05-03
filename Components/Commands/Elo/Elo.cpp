#include "Elo.h"

#include "Globals/Globals.h"

namespace Components::Operation
{
	asio::awaitable<void> Elo( const Command* Data )
	{
		const auto ActiveAccount = Globals::LeagueAPI->GetActiveAccount( Data->ChannelName );

		if ( !ActiveAccount )
		{
			Globals::TwitchAPI->ReplyTo( Data->Context, std::format( "{} is currently not ranked", Data->ChannelName ) );
			co_return;
		}

		auto Response = co_await Globals::LeagueAPI->GetLeagueRank( *ActiveAccount, ActiveAccount->LastGameModePlayed );

		if ( Response.has_value() )
		{
			Globals::TwitchAPI->ReplyTo( Data->Context, std::format( "{} is currently {}", Data->ChannelName, Response->Formatted() ) );
		}
		else
		{
			Globals::TwitchAPI->ReplyTo( Data->Context, std::format( "{} is currently not ranked", Data->ChannelName ) );
		}

		co_return;
	}
}
