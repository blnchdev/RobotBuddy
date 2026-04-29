#include "OnEndGame.h"

#include "Components/TUI/TUI.h"
#include "Components/TwitchBot/TwitchBot.h"
#include "Globals/Globals.h"

namespace Components::Event
{
	void OnEndGame::Trigger( std::string_view ChannelID, const GameSummary& Summary )
	{
		std::string GameResult = Summary.Win ? "👑 Win" : "💀 Loss";

		if ( Summary.DeltaLP.has_value() )
		{
			GameResult += std::format( " ({:+} LP)", Summary.DeltaLP.value() );
		}

		std::string Ratio = Summary.KDA.Deaths == 0 ? "Perfect" : std::format( "{:.2f}", Summary.KDA.Ratio );

		const std::string Response = std::format( "{} game result: {}. Finished {}/{}/{} ({}) on {}", ChannelID, GameResult, Summary.KDA.Kills, Summary.KDA.Deaths, Summary.KDA.Assists, Ratio, Summary.Champion );

		Globals::TwitchAPI->SendChat( ChannelID, Response );
	}
}
