#pragma once
#include <string_view>

#include "Components/Riot/Riot.h"

namespace Components::Event
{
	class OnEndGame
	{
	public:
		static void Trigger( std::string_view ChannelID, GameSummary& Summary );
	};
}
