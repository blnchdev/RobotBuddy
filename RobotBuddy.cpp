#include <functional>
#include <iostream>
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
	struct ParsedCommand
	{
		std::string_view Command;
		std::string_view Argument1;
		std::string_view Argument2;
	};

	struct ParsedRiotAccount
	{
		std::string_view Region;
		std::string_view Name;
		std::string_view Tag;
	};

	ParsedCommand ParseCommand( std::string_view Message )
	{
		ParsedCommand Result{};

		size_t Position = Message.find( ' ' );
		if ( Position == std::string_view::npos )
		{
			Result.Command = Message;
			return Result;
		}

		Result.Command = Message.substr( 0, Position );
		Message.remove_prefix( Position + 1 );

		Position = Message.find( ' ' );
		if ( Position == std::string_view::npos )
		{
			Result.Argument1 = Message;
			return Result;
		}

		Result.Argument1 = Message.substr( 0, Position );
		Message.remove_prefix( Position + 1 );

		Result.Argument2 = Message;

		return Result;
	}

	ParsedRiotAccount ParseAccount( std::string_view Argument )
	{
		const size_t Delimiter = Argument.find( ':' );
		const size_t Hash      = Argument.find( '#' );

		const std::string_view Region = Argument.substr( 0, Delimiter );
		const std::string_view User   = Argument.substr( Delimiter + 1, Hash - ( Delimiter + 1 ) );
		const std::string_view Tag    = Argument.substr( Hash + 1 );

		return { Region, User, Tag };
	}

	void OnMessage( const Components::TwitchMessage& Message )
	{
		if ( Message.Text.empty() || Message.Text[ 0 ] != '!' ) return;

		Components::Dispatch( Message );
	}
}

int main()
{
	using namespace Components;

	PrintSection( "RobotBuddy Pre-Alpha!" );

	Environment::Load();
	PrintDebug( "Loaded Environment" );

	Globals::DB = std::make_unique<Database>( "Accounts.db" );
	PrintDebug( "Loaded Database" );

	( void )Globals::DB->UpdateStreamer( "milliemeowss", true, "🟦", "🟥" );

	const auto Streamers = Globals::DB->GetStreamers();
	PrintInfo( "DB: {} streamer(s)", Streamers.size() );

	TokenManager Tokens( Environment::Get( "TWITCH_CLIENT_ID" ), Environment::Get( "TWITCH_CLIENT_SECRET" ), Environment::Get( "TWITCH_ACCESS_TOKEN" ), Environment::Get( "TWITCH_REFRESH_TOKEN" ) );
	PrintDebug( "Loaded Twitch TokenManager" );

	std::vector<std::string_view> ToJoin;
	std::ranges::for_each( Streamers, [&] ( const Database::Streamer& S ) { if ( S.IsJoinEnabled ) ToJoin.emplace_back( S.StreamerID ); } );

	Globals::Bot = std::make_unique<TwitchBot>( Tokens, Environment::Get( "TWITCH_BOT_NICK" ) );
	Globals::Bot->Connect();
	Globals::Bot->Login( ToJoin );

	Globals::LeagueAPI = std::make_unique<Riot>( Environment::Get( "RIOT_API_KEY" ) );
	Globals::LeagueAPI->Connect( Streamers );
	PrintDebug( "Loaded LeagueAPI" );

	Globals::Bot->Run( OnMessage );

	return 0;
}
