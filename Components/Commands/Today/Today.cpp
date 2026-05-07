#include "Today.h"

#include "Components/Riot/Riot.h"
#include "Components/TUI/TUI.h"
#include "Globals/Globals.h"

namespace Components::Operation
{
	namespace
	{
		std::string GetResponse( std::string_view ChannelName )
		{
			const auto ActiveAccount = Globals::LeagueAPI->GetActiveAccount( ChannelName );

			if ( !ActiveAccount )
			{
				return std::format( "No account linked to {}", ChannelName );
			}

			// Bit of scaffolding to get there
			const auto StreamerID = ActiveAccount->Info.Owner->ID;
			// const auto StreamerData = Globals::DB->GetSetting( StreamerID, );

			PrintDebug( "ActiveAccount: {}", ActiveAccount->Info.SummonerName );

			const auto  GameData = ActiveAccount->GetData( ActiveAccount->LastGameModePlayed );
			const auto  Wins     = static_cast<uint32_t>( std::ranges::count( GameData->Games, true, &GameSummary::Win ) );
			const auto  Losses   = static_cast<uint32_t>( GameData->Games.size() ) - Wins;
			const float WinRate  = Wins + Losses > 0 ? static_cast<float>( Wins ) / static_cast<float>( Wins + Losses ) * 100.f : 0.f;

			/*std::string Response = std::format( "{}W/{}L - {:.0f}% Winrate ", Wins, Losses, WinRate );

			if ( GameData->Rank.TotalDeltaLP != 0 )
			{
				Response += std::format( "{:+} LP ", GameData->Rank.TotalDeltaLP );
			}

			for ( const auto& Summary : std::views::reverse( GameData->Games ) )
			{
				Response += Summary.Win ? StreamerData. : Data->LoseEmoji;
			}*/

			return "Maintenance";
		}
	}

	asio::awaitable<void> Today( const Command* Data )
	{
		const std::string Response = GetResponse( Data->ChannelName );

		Globals::TwitchAPI->ReplyTo( Data->Context, Response );
		co_return;
	}
}
