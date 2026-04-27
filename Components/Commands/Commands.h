#pragma once
#include <string_view>

#include "Components/TwitchBot/TwitchBot.h"

namespace Components
{
	struct Command
	{
		std::string_view ChannelID;

		std::string_view Operation;
		std::string_view Argument1;
		std::string_view Argument2;

		const TwitchMessage* Context;
	};

	void Dispatch( const TwitchMessage& Message );
}
