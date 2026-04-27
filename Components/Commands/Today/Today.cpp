#include "Today.h"

#include "Components/Riot/Riot.h"
#include "Globals/Globals.h"

namespace Components::Operation
{
	namespace
	{
		std::string GetResponse( std::string_view StreamerID )
		{
			auto Data          = Globals::DB->GetStreamer( StreamerID );
			auto ActiveAccount = Globals::LeagueAPI->GetActiveAccount( StreamerID );

			if ( !Data.has_value() || !ActiveAccount.has_value() )
			{
				return std::format( "No account linked to {}", StreamerID );
			}

			const auto Wins = static_cast<uint32_t>( std::ranges::count
				(
				 ActiveAccount->Summaries, true, &GameSummary::Win
				) );

			const auto Losses = static_cast<uint32_t>( ActiveAccount->Summaries.size() ) - Wins;

			const float WinRate = Wins + Losses > 0 ? static_cast<float>( Wins ) / static_cast<float>( Wins + Losses ) * 100.f : 0.f;

			std::string Response = std::format( "{}W/{}L Winrate {:.0f}% ", Wins, Losses, WinRate );

			for ( const auto& Summary : ActiveAccount->Summaries )
			{
				Response += Summary.Win ? Data->WinEmoji : Data->LoseEmoji;
			}

			return Response;
		}
	}

	void Today( const Command* Data )
	{
		std::string Response = GetResponse( Data->ChannelID );

		Globals::Bot->ReplyTo( *Data->Context, Response );
	}
}
