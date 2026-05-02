#pragma once
#include <string_view>

#include "Components/TwitchBot/TwitchBot.h"

namespace Components
{
	struct Command
	{
		TwitchMessage Context;

		std::string ChannelName;
		std::string Operation;
		std::string Argument1;
		std::string Argument2;
	};

	asio::awaitable<void> Dispatch( const TwitchMessage& Message );
}
