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

			PrintDebug( "ActiveAccount: {}", ActiveAccount->Info.SummonerName );

			const auto  GameData = ActiveAccount->GetData( ActiveAccount->LastGameModePlayed );
			const auto  Wins     = static_cast<uint32_t>( std::ranges::count( GameData->Games, true, &GameSummary::Win ) );
			const auto  Losses   = static_cast<uint32_t>( GameData->Games.size() ) - Wins;
			const float WinRate  = Wins + Losses > 0 ? static_cast<float>( Wins ) / static_cast<float>( Wins + Losses ) * 100.f : 0.f;

			std::string Response = std::format( "{}W/{}L - {:.0f}% Winrate ", Wins, Losses, WinRate );

			const auto WinEmoji  = Globals::DB->GetSetting<std::string>( ActiveAccount->Info.Owner->ID, SettingIDs::WinEmoji, "🟦" );
			const auto LoseEmoji = Globals::DB->GetSetting<std::string>( ActiveAccount->Info.Owner->ID, SettingIDs::LoseEmoji, "🟥" );

			if ( GameData->Rank.SessionDeltaLP != 0 )
			{
				Response += std::format( "{:+} LP ", GameData->Rank.SessionDeltaLP );
			}

			for ( const auto& Summary : std::views::reverse( GameData->Games ) )
			{
				Response += Summary.Win ? WinEmoji : LoseEmoji;
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
