#include "OnEndGame.h"

#include "Components/TUI/TUI.h"
#include "Components/TwitchBot/TwitchBot.h"
#include "Globals/Globals.h"

namespace Components::Event
{
	void OnEndGame::Trigger( std::string_view ChannelID, const GameSummary& Summary )
	{
		PrintOk( "OnEndGame::Trigger -> {} ({} finished {}/{}/{})", ChannelID, Summary.Champion, Summary.Kills, Summary.Deaths, Summary.Assists );

		std::string GameResult = Summary.Win ? "👑 Win" : "💀 Loss";

		if ( Summary.DeltaLP.has_value() )
		{
			GameResult += std::format( " ({:+} LP)", Summary.DeltaLP.value() );
		}

		const std::string Response = std::format( "{} game result: {}. Finished {}/{}/{} ({:.2f}) on {}", ChannelID, GameResult, Summary.Kills, Summary.Deaths, Summary.Assists, Summary.KDA, Summary.Champion );

		Globals::TwitchAPI->SendChat( ChannelID, Response );
	}
}
