#include <functional>
#include <string>
#include <string_view>

#pragma comment(lib, "Crypt32.lib")

// #define ROBOTBUDDY_DBG 1

#include "Components/Commands/Commands.h"
#include "Components/Database/Database.h"
#include "Components/Environment/Environment.h"
#include "Components/GUI/GUI.h"
#include "Components/Riot/Riot.h"
#include "Components/TokenManager/TokenManager.h"
#include "Components/TUI/TUI.h"
#include "Components/TwitchBot/TwitchBot.h"
#include "Globals/Globals.h"

namespace
{
	enum class ModuleID : uint8_t
	{
		GUI,
		Environment,
		API,
		Database,
		TokenManager,
		TwitchAPI,
		LeagueAPI,

		__SIZE
	};

	std::array<std::chrono::milliseconds, std::to_underlying( ModuleID::__SIZE )> InitTimes = {};

	void OnMessage( const Components::TwitchMessage& Message )
	{
		if ( Message.Text.empty() || Message.Text[ 0 ] != '!' ) return;

		co_spawn( Globals::IOC, Dispatch( Message ), boost::asio::detached );
	}
}

// #define ROBOTBUDDY_DEBUG_GUI 1

int main()
{
	using namespace Components;
	SetConsoleOutputCP( CP_UTF8 );

	std::string ConsoleTitle = "RobotBuddy " + std::string( ROBOTBUDDY_VERSION );
	SetConsoleTitleA( ConsoleTitle.c_str() );

	auto A = std::chrono::high_resolution_clock::now();
	auto B = A;

	auto StartTimer = [&]()
	{
		A = std::chrono::high_resolution_clock::now();
	};

	auto EndTimer = [&] ( const ModuleID Module )
	{
		B                                         = std::chrono::high_resolution_clock::now();
		const auto TimeMS                         = std::chrono::duration_cast<std::chrono::milliseconds>( B - A );
		InitTimes[ std::to_underlying( Module ) ] = TimeMS;
	};

	PrintSection( "RobotBuddy" );
	PrintInfo( "Version {}", ROBOTBUDDY_VERSION );

#ifdef ROBOTBUDDY_DEBUG_GUI
	StartTimer();
	GUI::Initialize();
	EndTimer( ModuleID::GUI );
#endif

	StartTimer();
	Environment::Load();
	EndTimer( ModuleID::Environment );
	PrintDebug( "Loaded Environment" );

	StartTimer();
	Globals::BotAPI = std::make_unique<API>();
	EndTimer( ModuleID::API );
	PrintSuccess( "API listening on port {}", PORT );

	std::string ConnectionString = "host=127.0.0.1 port=5432 dbname=RobotBuddy user=robotbuddy password=" + Environment::Get( "POSTGRES_DB_PASSWORD" );
	StartTimer();
	Globals::DB = std::make_unique<Database>( ConnectionString );
	EndTimer( ModuleID::Database );
	PrintDebug( "Loaded Database" );

	auto Streamers = Globals::DB->GetStreamers();

	StartTimer();
	TokenManager Tokens( Environment::Get( "TWITCH_CLIENT_ID" ), Environment::Get( "TWITCH_CLIENT_SECRET" ), Environment::Get( "TWITCH_ACCESS_TOKEN" ), Environment::Get( "TWITCH_REFRESH_TOKEN" ) );
	EndTimer( ModuleID::TokenManager );
	PrintDebug( "Loaded Twitch TokenManager" );

	std::vector<std::string_view> ToJoin;
	std::ranges::for_each( Streamers, [&] ( const Database::Streamer& S ) { if ( S.Settings.IsJoinEnabled ) ToJoin.emplace_back( S.ChannelName ); } );

	StartTimer();
	Globals::TwitchAPI = std::make_unique<TwitchBot>( Tokens, Environment::Get( "TWITCH_BOT_NICK" ) );
	Globals::TwitchAPI->Connect();
	Globals::TwitchAPI->Login( ToJoin );
	EndTimer( ModuleID::TwitchAPI );
	PrintDebug( "Loaded TwitchAPI" );

	constexpr size_t         THREAD_COUNT = 10;
	std::vector<std::thread> Pool         = {};
	for ( auto i{ 0ULL }; i < THREAD_COUNT; ++i )
	{
		Pool.emplace_back( [] { Globals::IOC.run(); } );
	}

	StartTimer();
	Globals::LeagueAPI = std::make_unique<Riot>( Environment::Get( "RIOT_API_KEY" ) );
	Globals::LeagueAPI->InitializeDataDragon();
	Globals::LeagueAPI->Connect( Streamers );
	EndTimer( ModuleID::LeagueAPI );
	PrintDebug( "Loaded LeagueAPI" );

	auto GetMilliseconds = [] ( ModuleID ID ) { return InitTimes[ std::to_underlying( ID ) ].count(); };

	PrintSection( "Load Times" );
#ifdef ROBOTBUDDY_DEBUG_GUI
	PrintInfo( "GUI in {} ms", GetMilliseconds( ModuleID::GUI ) );
#endif
	PrintOk( "Environment in {} ms / API in {} ms / Database in {} ms", GetMilliseconds( ModuleID::Environment ), GetMilliseconds( ModuleID::API ), GetMilliseconds( ModuleID::Database ) );
	PrintOk( "TokenManager in {} ms / TwitchAPI in {} ms / LeagueAPI in {} ms", GetMilliseconds( ModuleID::TokenManager ), GetMilliseconds( ModuleID::TwitchAPI ), GetMilliseconds( ModuleID::LeagueAPI ) );

	asio::co_spawn( Globals::IOC, []() -> asio::awaitable<void>
	{
		co_await Globals::TwitchAPI->Run( OnMessage );
	}, asio::detached );

	asio::signal_set Signals{ Globals::IOC, SIGINT, SIGTERM };
	Signals.async_wait( [&] ( auto, auto )
	{
		Globals::Work.reset();
	} );

#ifdef ROBOTBUDDY_DEBUG_GUI
	GUI::Execute();
#endif

	for ( auto& T : Pool ) T.join();
}
