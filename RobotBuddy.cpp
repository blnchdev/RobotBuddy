#include <functional>
#include <string>
#include <string_view>

#pragma comment(lib, "Crypt32.lib")

#include "Components/Commands/Commands.h"
#include "Components/Database/Database.h"
#include "Components/Environment/Environment.h"
#include "Components/Riot/Riot.h"
#include "Components/TokenManager/TokenManager.h"
#include "Components/TUI/TUI.h"
#include "Components/TwitchBot/TwitchBot.h"
#include "Globals/Globals.h"

namespace
{
	void OnMessage( const Components::TwitchMessage& Message )
	{
		if ( Message.Text.empty() || Message.Text[ 0 ] != '!' ) return;

		co_spawn( Globals::IOC, Dispatch( Message ), boost::asio::detached );
	}
}

int main()
{
	using namespace Components;
	SetConsoleOutputCP( CP_UTF8 );

	PrintSection( "RobotBuddy Pre-Alpha!" );

	Environment::Load();
	PrintDebug( "Loaded Environment" );

	Globals::BotAPI = std::make_unique<API>();
	PrintDebug( "Started API, listening on port {}", PORT );

	Globals::DB = std::make_unique<Database>( "Accounts.db" );
	PrintDebug( "Loaded Database" );

	auto Streamers = Globals::DB->GetStreamers();

	TokenManager Tokens( Environment::Get( "TWITCH_CLIENT_ID" ), Environment::Get( "TWITCH_CLIENT_SECRET" ), Environment::Get( "TWITCH_ACCESS_TOKEN" ), Environment::Get( "TWITCH_REFRESH_TOKEN" ) );
	PrintDebug( "Loaded Twitch TokenManager" );

	std::vector<std::string_view> ToJoin;
	std::ranges::for_each( Streamers, [&] ( const Database::Streamer& S ) { if ( S.IsJoinEnabled ) ToJoin.emplace_back( S.StreamerID ); } );

	Globals::TwitchAPI = std::make_unique<TwitchBot>( Tokens, Environment::Get( "TWITCH_BOT_NICK" ) );
	Globals::TwitchAPI->Connect();
	Globals::TwitchAPI->Login( ToJoin );
	PrintDebug( "Loaded TwitchAPI" );

	constexpr size_t         THREAD_COUNT = 10;
	std::vector<std::thread> Pool         = {};
	for ( auto i{ 0ULL }; i < THREAD_COUNT; ++i )
	{
		Pool.emplace_back( [] { Globals::IOC.run(); } );
	}

	Globals::LeagueAPI = std::make_unique<Riot>( Environment::Get( "RIOT_API_KEY" ) );
	Globals::LeagueAPI->InitializeDataDragon();
	Globals::LeagueAPI->Connect( Streamers );
	PrintDebug( "Loaded LeagueAPI" );

	asio::co_spawn( Globals::IOC, [&]() -> asio::awaitable<void>
	{
		co_await Globals::TwitchAPI->Run( OnMessage );
	}, asio::detached );

	asio::signal_set Signals{ Globals::IOC, SIGINT, SIGTERM };
	Signals.async_wait( [&] ( auto, auto )
	{
		Globals::Work.reset();
	} );

	for ( auto& T : Pool ) T.join();
}
