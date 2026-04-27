#include "Riot.h"

#include <format>
#include <string>
#include <unordered_set>

#include "Components/TUI/TUI.h"
#include "Components/TwitchBot/TwitchBot.h"
#include "Globals/Globals.h"

namespace Components
{
	using namespace boost::urls;

	namespace
	{
		std::string_view RegionalHost( const std::string_view Region )
		{
			using namespace std::string_view_literals;

			if ( Region == "eun1" || Region == "euw1" || Region == "ru" || Region == "tr1" || Region == "me1" ) return "europe.api.riotgames.com"sv;
			if ( Region == "na1" || Region == "br1" || Region == "la1" || Region == "la2" ) return "americas.api.riotgames.com"sv;
			if ( Region == "jp1" || Region == "kr" ) return "asia.api.riotgames.com"sv;

			return "sea.api.riotgames.com"sv;
		}

		uint64_t ExtractGameID( std::string_view Input )
		{
			const size_t Index = Input.find( '_' );
			if ( Index == std::string_view::npos ) return 0;

			uint64_t   Value{};
			const auto Start = Input.data() + Index + 1;
			const auto End   = Input.data() + Input.size();

			auto [ Ptr, Ec ] = std::from_chars( Start, End, Value );
			if ( Ec != std::errc() ) return 0;

			return Value;
		}

		std::string GetLastGameID( const RiotAccount& Account )
		{
			url U;
			U.set_path( "/lol/match/v5/matches/by-puuid" );
			U.segments().push_back( Account.PUUID );
			U.segments().push_back( "ids?queue=420&start=0&count=1" );

			const auto Target           = U.encoded_path();
			auto       [ Status, Body ] = Globals::LeagueAPI->GET( RegionalHost( Account.Region ), Target );

			if ( Status != 200 ) return {};

			const auto J = json::parse( Body );
			return J.begin().value().get<std::string>();
		}

		std::vector<std::string> GetLastGameIDs( const RiotAccount& Account, const size_t Count )
		{
			url U;
			U.set_path( "/lol/match/v5/matches/by-puuid" );
			U.segments().push_back( Account.PUUID );
			U.segments().push_back( "ids" );
			U.params().append( { "queue", "420" } );
			U.params().append( { "start", "0" } );
			U.params().append( { "count", std::to_string( Count ) } );

			const auto Target = U.encoded_target();
			PrintDebug( "GetLastGameIDs URL: {}", std::string( Target ) );
			auto [ Status, Body ] = Globals::LeagueAPI->GET( RegionalHost( Account.Region ), Target );

			if ( Status != 200 ) return {};

			const auto J = json::parse( Body );
			return J.get<std::vector<std::string>>();
		}

		std::optional<GameSummary> GetGameSummary( const std::string_view PUUID, const std::string_view Region, const std::string_view GameID )
		{
			url U;
			U.set_path( "/lol/match/v5/matches" );
			U.segments().push_back( GameID );

			const auto Target           = U.encoded_path();
			auto       [ Status, Body ] = Globals::LeagueAPI->GET( RegionalHost( Region ), Target );

			if ( Status != 200 ) return std::nullopt;

			auto J    = json::parse( Body );
			auto Info = J[ "info" ];

			auto& Participants = Info[ "participants" ];

			auto TryMatch = [&] ( const json& Data )
			{
				return Data[ "puuid" ].get<std::string>() == PUUID;
			};

			const auto Iterator = std::ranges::find_if( Participants, TryMatch );

			if ( Iterator == Participants.end() ) return std::nullopt;

			auto& PlayerData = *Iterator;

			const uint64_t GameStart    = Info[ "gameStartTimestamp" ].get<uint64_t>();
			const uint64_t GameDuration = Info[ "gameDuration" ].get<uint64_t>();

			int32_t K, D, A;
			PlayerData[ "kills" ].get_to( K );
			PlayerData[ "deaths" ].get_to( D );
			PlayerData[ "assists" ].get_to( A );

			const float KDA = ( static_cast<float>( K ) + static_cast<float>( A ) ) / static_cast<float>( D );

			return GameSummary
			{
				.GameID = ExtractGameID( GameID ),
				.Champion = PlayerData[ "championName" ],
				.Role = PlayerData[ "individualPosition" ],
				.Win = PlayerData[ "win" ].get<bool>(),
				.Kills = K,
				.Deaths = D,
				.Assists = A,
				.KDA = KDA,
				.Duration = GameDuration,
				.GameEnd = GameStart + GameDuration * 1000ULL,
				.CreepScore = PlayerData[ "totalMinionsKilled" ].get<int32_t>() + PlayerData[ "neutralMinionsKilled" ].get<int32_t>(),
				.VisionScore = PlayerData[ "visionScore" ].get<double>()
			};
		}

		std::optional<GameSummary> GetLastGame( RiotAccount& Account )
		{
			const std::string StringID = GetLastGameID( Account );
			if ( StringID.empty() ) return std::nullopt;

			const uint64_t GameID = ExtractGameID( StringID );
			if ( GameID == 0 ) return std::nullopt;

			auto TryMatch = [&] ( const GameSummary& Summary )
			{
				return Summary.GameID == GameID;
			};

			const auto Iterator = std::ranges::find_if( Account.Summaries, TryMatch );

			if ( Iterator == Account.Summaries.end() )
			{
				const auto Summary = GetGameSummary( Account.PUUID, Account.Region, StringID );
				if ( !Summary ) return std::nullopt;

				return Account.Summaries.emplace_back( *Summary );
			}

			return *Iterator;
		}

		void RefreshSession( RiotAccount& Account )
		{
			if ( Account.Summaries.empty() )
			{
				// First pass, we grab the last 20 GameIDs
				const auto Strings       = GetLastGameIDs( Account, 20 );
				uint64_t   LastGameStart = 0;

				for ( const auto& StringID : Strings )
				{
					const auto Summary = GetGameSummary( Account.PUUID, Account.Region, StringID );

					if ( Summary.has_value() )
					{
						const uint64_t CurrentGameStart = Summary->GameEnd - Summary->Duration * 1000ULL;

						if ( LastGameStart != 0 )
						{
							const auto Delta = LastGameStart - Summary->GameEnd;
							PrintDebug( "Delta = {} (aka {} seconds)", Delta, Delta / 1000ULL );

							if ( Delta >= 2ULL * 60ULL * 60ULL * 1000ULL ) break;
						}

						LastGameStart = CurrentGameStart;
						Account.Summaries.push_back( *Summary );
					}
				}
			}
			else
			{
				auto StringID = GetLastGameID( Account );
				auto LastID   = ExtractGameID( StringID );
				if ( LastID == Account.Summaries.front().GameID ) return; // Last Fetched == Last Saved; no new game.

				auto NewSummary = GetGameSummary( Account.PUUID, Account.Region, StringID );

				if ( NewSummary.has_value() )
				{
					Account.Summaries.push_back( *NewSummary );
				}
			}
		}

		void Work()
		{
			auto GetLastGameTimestamp = [&] ( RiotAccount& Account )
			{
				const auto LastGame = GetLastGame( Account );

				return LastGame.has_value() ? LastGame->GameEnd : 0;
			};

			auto PopulateLastSession = [&] ( const Database::Streamer& Streamer )
			{
				auto& RiotData = Globals::LeagueAPI->GetData( Streamer.StreamerID );

				if ( RiotData.Accounts.empty() ) return;

				const auto LatestActive = std::ranges::max_element( RiotData.Accounts, {}, GetLastGameTimestamp );

				if ( LatestActive == RiotData.Accounts.end() ) return;

				auto& MostActiveAccount = *LatestActive;

				RefreshSession( MostActiveAccount );
			};

			const auto Streamers = Globals::DB->GetStreamers();

			std::ranges::for_each( Streamers, PopulateLastSession );

			while ( true )
			{
			}
		}
	}

	RiotAccount::RiotAccount( std::string PUUID ) : PUUID( std::move( PUUID ) )
	{
		if ( !Globals::LeagueAPI ) throw std::runtime_error( "No RiotAPI instance, can not create RiotAccounts!" );

		url U;

		{
			U.set_path( "/riot/account/v1/accounts/by-puuid" );
			U.segments().push_back( this->PUUID );

			const auto Target           = U.encoded_path();
			auto       [ Status, Body ] = Globals::LeagueAPI->GET( Target );
			if ( Status != 200 ) return;

			const auto J = json::parse( Body, nullptr, false );
			if ( J.is_discarded() ) return;

			J[ "gameName" ].get_to( this->SummonerName );
			J[ "tagLine" ].get_to( this->TagLine );
		}

		{
			U.set_path( "/riot/account/v1/region/by-game/lol/by-puuid" );
			U.segments().push_back( this->PUUID );

			const auto Target           = U.encoded_path();
			auto       [ Status, Body ] = Globals::LeagueAPI->GET( Target );
			if ( Status != 200 ) return;

			const auto J = json::parse( Body, nullptr, false );
			if ( J.is_discarded() ) return;

			J[ "region" ].get_to( this->Region );
		}

		this->Valid = true;
	}

	RiotData::RiotData( std::string Channel ) : TwitchChannel( std::move( Channel ) )
	{
	}

	Riot::Riot( std::string Key ) : API( std::move( Key ) ), SSLC( ssl::context::tlsv12_client )
	{
		SSLC.set_default_verify_paths();
	}

	void Riot::Connect( const std::vector<Database::Streamer>& Streamers )
	{
		auto Populate = [&] ( const Database::Streamer& Streamer )
		{
			RiotData Buffer{ Streamer.StreamerID };

			std::ranges::for_each( Streamer.PUUIDs, [&] ( const std::string_view PUUID ) { Buffer.Accounts.emplace_back( std::string( PUUID ) ); } );
			this->Data[ Buffer.TwitchChannel ] = std::move( Buffer );
		};

		std::ranges::for_each( Streamers, Populate );

		this->WorkerThread = std::jthread( Work );
	}

	Riot::Response Riot::GET( std::string_view Host, std::string_view Target )
	{
		ssl::stream<beast::tcp_stream> Stream{ IOC, SSLC };

		if ( !SSL_set_tlsext_host_name( Stream.native_handle(), std::string{ Host }.c_str() ) ) throw std::runtime_error( "SNI failed" );

		tcp::resolver Resolver{ IOC };
		auto const    Results = Resolver.resolve( Host, "443" );
		beast::get_lowest_layer( Stream ).connect( Results );
		Stream.handshake( ssl::stream_base::client );

		http::request<http::empty_body> Request{ http::verb::get, Target, 11 };
		Request.set( http::field::host, Host );
		Request.set( http::field::user_agent, "lol-sessions/1.0" );
		Request.set( "X-Riot-Token", API );

		http::write( Stream, Request );

		beast::flat_buffer                Buffer;
		http::response<http::string_body> Result;
		http::read( Stream, Buffer, Result );

		return { .Status = ( Result.result_int() ), .Body = std::move( Result.body() ) };
	}

	Riot::Response Riot::GET( const std::string_view Target )
	{
		return this->GET( REGIONAL_HOST, Target );
	}

	std::optional<RiotAccount> Riot::GetActiveAccount( const std::string_view StreamerID )
	{
		const auto& Data     = this->Data[ std::string( StreamerID ) ];
		const auto  Iterator = std::ranges::find_if( Data.Accounts, [] ( const RiotAccount& Account ) { return !Account.Summaries.empty(); } );

		if ( Iterator == Data.Accounts.end() ) return std::nullopt;

		return *Iterator;
	}

	std::optional<std::string> Riot::GetPUUID( const std::string_view SummonerName, const std::string_view TagLine )
	{
		url U;
		U.set_path( "/riot/account/v1/accounts/by-riot-id" );
		U.segments().push_back( SummonerName );
		U.segments().push_back( TagLine );

		const auto Target           = U.encoded_path();
		auto       [ Status, Body ] = this->GET( Target );

		if ( Status != 200 ) return std::nullopt;

		const auto J = json::parse( Body );
		return J[ "puuid" ].get<std::string>();
	}

	RiotData& Riot::GetData( const std::string_view StreamerID )
	{
		return this->Data[ std::string( StreamerID ) ];
	}

	bool Riot::AddAccount( const std::string_view StreamerID, const std::string_view PUUID )
	{
		auto&       Streamer = this->Data[ std::string( StreamerID ) ];
		const auto& Account  = Streamer.Accounts.emplace_back( std::string( PUUID ) );

		if ( !Account.Valid ) return false; // PUUID did not result into a valid RiotAccount

		( void )Globals::DB->AddAccount( StreamerID, PUUID );
		return true;
	}

	bool Riot::RemoveAccount( const std::string_view StreamerID, const std::string_view SummonerName, const std::string_view TagLine )
	{
		const auto Iterator = this->Data.find( std::string( StreamerID ) );

		if ( Iterator == this->Data.end() ) return false;

		const auto TryMatch = [&] ( const RiotAccount& Account )
		{
			return Account.SummonerName == SummonerName && Account.TagLine == TagLine;
		};

		const auto Result = std::ranges::remove_if( Iterator->second.Accounts, TryMatch );

		if ( Result.begin() == Result.end() ) return false; // No match

		Iterator->second.Accounts.erase( Result.begin(), Result.end() );

		{
			const auto PUUID = Globals::LeagueAPI->GetPUUID( SummonerName, TagLine );

			if ( PUUID.has_value() )
			{
				( void )Globals::DB->RemoveAccount( StreamerID, PUUID.value() );
			}
		}

		return true;
	}
}
