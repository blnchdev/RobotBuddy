#include "Today.h"

#include "Components/Riot/Riot.h"
#include "Components/TUI/TUI.h"
#include "Globals/Globals.h"

namespace Components::Operation
{
	namespace
	{
		std::string GetResponse( std::string_view StreamerID )
		{
			const auto Data          = Globals::DB->GetStreamer( StreamerID );
			const auto ActiveAccount = Globals::LeagueAPI->GetActiveAccount( StreamerID );

			if ( !Data.has_value() || !ActiveAccount )
			{
				return std::format( "No account linked to {}", StreamerID );
			}

			PrintDebug( "ActiveAccount: {}", ActiveAccount->Info.SummonerName );

			const auto  GameData = ActiveAccount->GetData( ActiveAccount->LastGameModePlayed );
			const auto  Wins     = static_cast<uint32_t>( std::ranges::count( GameData->Games, true, &GameSummary::Win ) );
			const auto  Losses   = static_cast<uint32_t>( GameData->Games.size() ) - Wins;
			const float WinRate  = Wins + Losses > 0 ? static_cast<float>( Wins ) / static_cast<float>( Wins + Losses ) * 100.f : 0.f;

			std::string Response = std::format( "{}W/{}L - {:.0f}% Winrate ", Wins, Losses, WinRate );

			if ( GameData->Rank.TotalDeltaLP != 0 )
			{
				Response += std::format( "{:+} LP ", GameData->Rank.TotalDeltaLP );
			}

			for ( const auto& Summary : std::views::reverse( GameData->Games ) )
			{
				Response += Summary.Win ? Data->WinEmoji : Data->LoseEmoji;
			}

			return Response;
		}
	}

	asio::awaitable<void> Today( const Command* Data )
	{
		const std::string Response = GetResponse( Data->ChannelName );

		Globals::TwitchAPI->ReplyTo( Data->Context, Response );
		co_return;
	}
}
