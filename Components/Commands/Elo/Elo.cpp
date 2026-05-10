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

		std::optional<RankEntry> Response   = std::nullopt;
		bool                     IsUnranked = true;

		if ( ActiveAccount->LastGameModePlayed != GameType::SOLOQ && ActiveAccount->LastGameModePlayed != GameType::FLEX )
		{
			const auto& SoloRank = ActiveAccount->GetData( GameType::SOLOQ )->Rank;
			const auto& FlexRank = ActiveAccount->GetData( GameType::FLEX )->Rank;

			if ( !SoloRank.IsUnranked && SoloRank.LastKnown )
			{
				IsUnranked = false;
				Response   = *SoloRank.LastKnown;
			}
			else if ( !FlexRank.IsUnranked && FlexRank.LastKnown )
			{
				IsUnranked = false;
				Response   = *FlexRank.LastKnown;
			}
		}
		else
		{
			const auto& Rank = ActiveAccount->GetData( ActiveAccount->LastGameModePlayed )->Rank;

			if ( !Rank.IsUnranked && Rank.LastKnown )
			{
				IsUnranked = false;
				Response   = *Rank.LastKnown;
			}
		}

		if ( IsUnranked || !Response.has_value() )
		{
			Globals::TwitchAPI->ReplyTo( Data->Context, std::format( "{} is currently not ranked", Data->ChannelName ) );
			co_return;
		}

		Globals::TwitchAPI->ReplyTo( Data->Context, std::format( "{} is currently {}", Data->ChannelName, Response->Formatted() ) );
	}
}
