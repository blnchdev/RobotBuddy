#pragma once
#include <memory>

#include "Components/API/API.h"
#include "Components/Database/Database.h"
#include "Components/Riot/Riot.h"
#include "Components/TwitchBot/TwitchBot.h"

// If you're not based in EU (aka closer to NA/SEA/ASIA), comment/uncomment one of these (only 1 should be active!)
// #define REGIONAL_HOST "americas.api.riotgames.com"
// #define REGIONAL_HOST "asia.api.riotgames.com"
#define REGIONAL_HOST "europe.api.riotgames.com"
// #define REGIONAL_HOST "sea.api.riotgames.com"

namespace Globals
{
	inline std::unique_ptr<Components::TwitchBot> TwitchAPI = nullptr;
	inline std::unique_ptr<Components::Riot>      LeagueAPI = nullptr;
	inline std::unique_ptr<Components::Database>  DB        = nullptr;
	inline std::unique_ptr<Components::API>       BotAPI    = nullptr;

	// Network Stuff
	inline boost::asio::io_context          IOC = {};
	inline boost::asio::executor_work_guard Work{ make_work_guard( IOC ) };
}
