#include "Last.h"

#include "Components/Riot/Riot.h"
#include "Globals/Globals.h"

namespace Components::Operation
{
	namespace
	{
		std::string GetResponse( const Command* Data, const GameSummary* Summary )
		{
			std::string Response;

			Response += std::format( "{} {} a game on {}", Data->ChannelName, Summary->Win ? "won" : "lost", Summary->Streamer.Champion );

			if ( Summary->Opponent.has_value() )
			{
				Response += std::format( " against {}", Summary->Opponent->Champion );
			}

			Response += std::format( " by going {}/{}/{} ", Summary->Streamer.KDA.Kills, Summary->Streamer.KDA.Deaths, Summary->Streamer.KDA.Assists );

			if ( Summary->Streamer.KDA.IsPerfect() ) [[unlikely]] // No offense guys
			{
				Response += "(Perfect)";
			}
			else
			{
				Response += std::format( "({:.2f})", Summary->Streamer.KDA.Ratio );
			}

			if ( Summary->DeltaLP != 0 )
			{
				Response += std::format( " they {} {} LP", Summary->Win ? "won" : "lost", abs( Summary->DeltaLP ) );
			}

			return Response;
		}
	}

	asio::awaitable<void> Last( const Command* Data )
	{
		const auto ActiveAccount = Globals::LeagueAPI->GetActiveAccount( Data->ChannelName );

		if ( !ActiveAccount )
		{
			Globals::TwitchAPI->ReplyTo( Data->Context, std::format( "{} has not played yet", Data->ChannelName ) );
			co_return;
		}

		const auto& Games = ActiveAccount->GetData( ActiveAccount->LastGameModePlayed )->Games;

		if ( Games.empty() )
		{
			Globals::TwitchAPI->ReplyTo( Data->Context, std::format( "{} has not played yet", Data->ChannelName ) );
			co_return;
		}

		const auto* LastSummary = &Games.back();

		Globals::TwitchAPI->ReplyTo( Data->Context, GetResponse( Data, LastSummary ) );
	}
}
