#include "OnEndGame.h"

#include "Components/TUI/TUI.h"
#include "Components/TwitchBot/TwitchBot.h"
#include "Globals/Globals.h"

namespace Components::Event
{
	std::string ToDisplayName( const GameType Type )
	{
		switch ( Type )
		{
		case GameType::UNRANKED:
			return "Normal";
		case GameType::SOLOQ:
			return "SoloQ";
		case GameType::FLEX:
			return "Flex";
		case GameType::ARAM:
			return "ARAM";
		case GameType::ARENA:
			return "Arena";
		case GameType::CLASH:
			return "Clash";
		default:
			break;
		}

		return "";
	}

	void OnEndGame::Trigger( std::string_view ChannelID, const GameSummary& Summary )
	{
		std::string GameResult = Summary.Win ? "👑 Win" : "💀 Loss";

		if ( Summary.DeltaLP != 0 )
		{
			GameResult += std::format( " ({:+} LP)", Summary.DeltaLP );
		}

		std::string Ratio = Summary.KDA.IsPerfect() ? "Perfect" : std::format( "{:.2f}", Summary.KDA.Ratio );

		const std::string Response = std::format( "{} {} game result: {}. Finished {}/{}/{} ({}) on {}", ChannelID, ToDisplayName( Summary.Type ), GameResult, Summary.KDA.Kills, Summary.KDA.Deaths, Summary.KDA.Assists, Ratio, Summary.Champion );

		PrintOk( "OnGameEnd::Trigger({}): {}", ChannelID, Response );
		Globals::TwitchAPI->SendChat( ChannelID, Response );
	}
}
