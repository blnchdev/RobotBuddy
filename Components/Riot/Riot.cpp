#include "Riot.h"

#include <format>
#include <string>
#include <unordered_set>

#include "Components/Events/OnEndGame/OnEndGame.h"
#include "Components/TUI/TUI.h"
#include "Components/TwitchBot/TwitchBot.h"
#include "Globals/Globals.h"

namespace Components
{
	using namespace boost::urls;

	namespace
	{
		constexpr int32_t ApexLP = 2800;

		std::unordered_map<int32_t, std::string> ChampionMap = {};
		std::unordered_map<std::string, int32_t> TierBase    =
		{
			{ "Iron", 0 }, { "Bronze", 400 }, { "Silver", 800 }, { "Gold", 1200 },
			{ "Platinum", 1600 }, { "Emerald", 2000 }, { "Diamond", 2400 },
			{ "Master", ApexLP }, { "Grandmaster", ApexLP }, { "Challenger", ApexLP }
		};
		std::unordered_map<std::string, int32_t> DivisionBase =
		{
			{ "IV", 0 }, { "III", 100 }, { "II", 200 }, { "I", 300 }
		};

		int32_t GetAverageLP( const std::span<RankEntry> Entries )
		{
			if ( Entries.empty() ) return 0;

			int32_t Sum = 0;
			for ( auto& Entry : Entries ) Sum += RankToLP( Entry );

			return Sum / static_cast<int>( Entries.size() );
		}

		RoleEnum ToRoleEnum( const std::string_view String )
		{
			static const std::unordered_map<std::string_view, RoleEnum> Map =
			{
				{ "TOP", TOP },
				{ "JUNGLE", JUNGLE },
				{ "MIDDLE", MIDDLE },
				{ "BOTTOM", ADC },
				{ "UTILITY", SUPPORT }
			};

			if ( const auto Iterator = Map.find( String ); Iterator != Map.end() ) return Iterator->second;

			return NONE;
		}

		GameType ToGameType( const uint32_t ID )
		{
			// According to https://static.developer.riotgames.com/docs/lol/queues.json

			switch ( ID )
			{
			case 400: // 5v5 Draft Pick games
			case 430: // 5v5 Blind Pick games
			case 480: // Swiftplay Games
			case 490: // Normal (Quickplay)
				return UNRANKED;
			case 420: // 5v5 Ranked Solo games
				return SOLOQ;
			case 440: // 5v5 Ranked Flex games
				return FLEX;
			case 450:  // 5v5 ARAM games
			case 2400: // ARAM: Mayhem
				return ARAM;
			case 700: // Summoner's Rift Clash games
			case 720: // ARAM Clash games
				return CLASH;
			case 1710: // 16 player lobby
				return ARENA;
			}

			return ROTATING; // This should probably be a case, and default should be a 'UNKNOWN' type, iunno...
		}

		std::string_view RegionalHost( const std::string_view Region )
		{
			using namespace std::string_view_literals;

			if ( Region == "eun1" || Region == "euw1" || Region == "ru" || Region == "tr1" || Region == "me1" ) return "europe.api.riotgames.com"sv;
			if ( Region == "na1" || Region == "br1" || Region == "la1" || Region == "la2" ) return "americas.api.riotgames.com"sv;
			if ( Region == "jp1" || Region == "kr" ) return "asia.api.riotgames.com"sv;

			return "sea.api.riotgames.com"sv;
		}

		std::string ServerHost( const std::string_view Region )
		{
			return std::string( Region ) + ".api.riotgames.com";
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

		asio::awaitable<std::string> GetLastGameID( const RiotAccount& Account )
		{
			url U;
			U.set_path( "/lol/match/v5/matches/by-puuid" );
			U.segments().push_back( Account.PUUID );
			U.segments().push_back( "ids" );
			U.params().append( { "queue", "420" } );
			U.params().append( { "start", "0" } );
			U.params().append( { "count", "1" } );

			const auto Target           = U.encoded_target();
			auto       [ Status, Body ] = co_await Globals::LeagueAPI->GET( RegionalHost( Account.Region ), Target );

			if ( Status != 200 ) co_return std::string();

			const auto J = json::parse( Body );
			co_return J.begin().value().get<std::string>();
		}

		asio::awaitable<std::vector<std::string>> GetLastGameIDs( const RiotAccount& Account, const size_t Count )
		{
			url U;
			U.set_path( "/lol/match/v5/matches/by-puuid" );
			U.segments().push_back( Account.PUUID );
			U.segments().push_back( "ids" );
			U.params().append( { "queue", "420" } );
			U.params().append( { "start", "0" } );
			U.params().append( { "count", std::to_string( Count ) } );

			const auto Target           = U.encoded_target();
			auto       [ Status, Body ] = co_await Globals::LeagueAPI->GET( RegionalHost( Account.Region ), Target );

			if ( Status != 200 ) co_return std::vector<std::string>();

			const auto J = json::parse( Body );
			co_return J.get<std::vector<std::string>>();
		}

		asio::awaitable<std::optional<GameSummary>> GetGameSummary( const std::string_view PUUID, const std::string_view Region, const std::string_view GameID )
		{
			url U;
			U.set_path( "/lol/match/v5/matches" );
			U.segments().push_back( GameID );

			const auto Target           = U.encoded_path();
			auto       [ Status, Body ] = co_await Globals::LeagueAPI->GET( RegionalHost( Region ), Target );

			if ( Status != 200 ) co_return std::nullopt;

			auto J    = json::parse( Body );
			auto Info = J[ "info" ];

			auto& Participants = Info[ "participants" ];

			auto TryMatch = [&] ( const json& Data )
			{
				return Data[ "puuid" ].get<std::string>() == PUUID;
			};

			const auto Iterator = std::ranges::find_if( Participants, TryMatch );

			if ( Iterator == Participants.end() ) co_return std::nullopt;

			auto& PlayerData = *Iterator;

			const uint64_t GameStart    = Info[ "gameStartTimestamp" ].get<uint64_t>();
			const uint64_t GameDuration = Info[ "gameDuration" ].get<uint64_t>();
			const auto     RoleString   = Info[ "individualPosition" ].is_string() ? Info[ "individualPosition" ].get<std::string>() : std::string( "NONE" );
			uint32_t       QueueID      = Info[ "queueId" ];

			int32_t K, D, A;
			PlayerData[ "kills" ].get_to( K );
			PlayerData[ "deaths" ].get_to( D );
			PlayerData[ "assists" ].get_to( A );

			const float Ratio = D == 0 ? -1.f : ( static_cast<float>( K ) + A ) / D;

			co_return GameSummary
			{
				.GameID = ExtractGameID( GameID ),
				.Champion = PlayerData[ "championName" ],
				.Role = ToRoleEnum( RoleString ),
				.Type = ToGameType( QueueID ),
				.Win = PlayerData[ "win" ].get<bool>(),
				.KDA =
				{
					.Kills = K,
					.Deaths = D,
					.Assists = A,
					.Ratio = Ratio
				},
				.Duration = GameDuration,
				.GameEnd = GameStart + GameDuration * 1000ULL,
				.CreepScore = PlayerData[ "totalMinionsKilled" ].get<int32_t>() + PlayerData[ "neutralMinionsKilled" ].get<int32_t>(),
				.VisionScore = PlayerData[ "visionScore" ].get<double>()
			};
		}

		asio::awaitable<std::optional<GameSummary>> GetLastGame( RiotAccount& Account )
		{
			const std::string StringID = co_await GetLastGameID( Account );
			if ( StringID.empty() ) co_return std::nullopt;

			const uint64_t GameID = ExtractGameID( StringID );
			if ( GameID == 0 ) co_return std::nullopt;

			auto TryMatch = [&] ( const GameSummary& Summary )
			{
				return Summary.GameID == GameID;
			};

			const auto Iterator = std::ranges::find_if( Account.Summaries, TryMatch );

			if ( Iterator == Account.Summaries.end() )
			{
				const auto Summary = co_await GetGameSummary( Account.PUUID, Account.Region, StringID );
				if ( !Summary ) co_return std::nullopt;

				co_return Account.Summaries.emplace_back( *Summary );
			}

			co_return *Iterator;
		}

		asio::awaitable<void> RefreshSession( std::string_view ChannelID, RiotAccount& Account )
		{
			if ( Account.Summaries.empty() )
			{
				// First pass, we grab the last 20 GameIDs
				const auto Strings       = co_await GetLastGameIDs( Account, 20 );
				uint64_t   LastGameStart = 0;

				std::vector<GameSummary> TemporaryList = {};

				for ( const auto& StringID : Strings )
				{
					const auto Summary = co_await GetGameSummary( Account.PUUID, Account.Region, StringID );

					if ( Summary.has_value() )
					{
						const uint64_t CurrentGameStart = Summary->GameEnd - Summary->Duration * 1000ULL;

						if ( LastGameStart != 0 )
						{
							const auto Delta = LastGameStart - Summary->GameEnd;

							if ( Delta >= 2ULL * 60ULL * 60ULL * 1000ULL ) break;
						}

						LastGameStart = CurrentGameStart;
						TemporaryList.push_back( *Summary );
					}
				}

				std::ranges::reverse( TemporaryList );
				Account.Summaries.swap( TemporaryList );
			}
			else
			{
				const auto StringID = co_await GetLastGameID( Account );
				const auto LastID   = ExtractGameID( StringID );

				if ( LastID == Account.Summaries.back().GameID ) co_return; // Last Fetched == Last Saved; no new game.

				const auto NewSummary = co_await GetGameSummary( Account.PUUID, Account.Region, StringID );

				if ( NewSummary.has_value() )
				{
					GameSummary Summary     = NewSummary.value();
					const auto  CurrentRank = co_await Globals::LeagueAPI->GetLeagueRank( Account );

					if ( Account.LastKnownRank.has_value() && CurrentRank.has_value() )
					{
						int32_t OldLP = RankToLP( Account.LastKnownRank.value() );
						int32_t NewLP = RankToLP( CurrentRank.value() );
						int32_t Delta = NewLP - OldLP;

						if ( Delta != 0 )
						{
							Summary.DeltaLP = Delta;
						}

						Account.LastKnownRank = CurrentRank;
					}

					Event::OnEndGame::Trigger( ChannelID, Summary );
					Account.Summaries.push_back( Summary );
				}
			}
		}

		void Capitalize( std::string& String )
		{
			if ( String.empty() ) return;

			std::ranges::transform( String, String.begin(), [] ( const unsigned char c ) { return std::tolower( c ); } );

			String[ 0 ] = std::toupper( String[ 0 ] );
		}

		asio::awaitable<void> Work()
		{
			auto GetLastGameTimestamp = [&] ( RiotAccount& Account ) -> asio::awaitable<int64_t>
			{
				const auto LastGame = co_await GetLastGame( Account );
				co_return LastGame.has_value() ? LastGame->GameEnd : 0;
			};

			auto PopulateLastSession = [&GetLastGameTimestamp] ( const Database::Streamer& Streamer ) -> asio::awaitable<void>
			{
				auto& RiotData = Globals::LeagueAPI->GetData( Streamer.StreamerID );
				PrintDebug( "Streamer {} has {} accounts", Streamer.StreamerID, RiotData.Accounts.size() );
				if ( RiotData.Accounts.empty() ) co_return;

				const auto ActiveGame = co_await Globals::LeagueAPI->GetCurrentGame( Streamer.StreamerID );

				if ( ActiveGame.has_value() )
				{
					if ( RiotData.ActivePUUID != ActiveGame->PUUID ) RiotData.ActivePUUID = ActiveGame->PUUID;
				}
				else
				{
					const RiotAccount* LatestActive    = nullptr;
					int64_t            LatestTimestamp = 0;

					for ( auto& Account : RiotData.Accounts )
					{
						const auto Timestamp = co_await GetLastGameTimestamp( Account );

						if ( Timestamp > LatestTimestamp )
						{
							LatestTimestamp = Timestamp;
							LatestActive    = &Account;
						}
					}

					if ( !LatestActive )
					{
						PrintDebug( "No LatestActive for {}", Streamer.StreamerID );
						co_return;
					}

					RiotData.ActivePUUID = LatestActive->PUUID;
				}

				const auto Iterator = std::ranges::find_if( RiotData.Accounts, [&] ( const RiotAccount& Account )
				{
					return Account.PUUID == RiotData.ActivePUUID;
				} );

				if ( Iterator != RiotData.Accounts.end() ) co_await RefreshSession( Streamer.StreamerID, *Iterator );
			};

			PrintDebug( "First Pass" );
			// First pass
			{
				const auto Streamers = Globals::DB->GetStreamers();
				for ( const auto& Streamer : Streamers ) co_await PopulateLastSession( Streamer );
			}

			asio::steady_timer Timer{ co_await asio::this_coro::executor };

			PrintDebug( "Work Loop" );
			while ( true )
			{
				const auto Streamers = Globals::DB->GetStreamers();
				PrintDebug( "Streamers::Size = {}", Streamers.size() );

				for ( const auto& Streamer : Streamers )
				{
					PrintDebug( "Working on {}...", Streamer.StreamerID );
					const auto ActiveAccount = Globals::LeagueAPI->GetActiveAccount( Streamer.StreamerID );
					if ( ActiveAccount )
					{
						PrintDebug( "ActiveAccount for {}: {}#{}", Streamer.StreamerID, ActiveAccount->SummonerName, ActiveAccount->TagLine );
						co_await RefreshSession( Streamer.StreamerID, *ActiveAccount );
					}
				}

				Timer.expires_after( std::chrono::seconds( 15 ) );
				co_await Timer.async_wait( asio::use_awaitable );
			}
		}
	}

	int32_t RankToLP( const RankEntry& Entry )
	{
		const auto TierIterator = TierBase.find( Entry.Rank );
		if ( TierIterator == TierBase.end() ) return 0;

		const bool IsApexRank = Entry.Rank == "Master" || Entry.Rank == "Grandmaster" || Entry.Rank == "Challenger";
		if ( IsApexRank ) return TierIterator->second + Entry.LP;

		const auto DivisionIterator = DivisionBase.find( Entry.Division );
		if ( DivisionIterator == DivisionBase.end() ) return 0;

		return TierIterator->second + DivisionIterator->second + Entry.LP;
	}

	RankEntry LPToRank( const int32_t LP )
	{
		static constexpr std::array Ranks =
		{
			std::pair{ "Iron", 0 }, std::pair{ "Bronze", 400 },
			std::pair{ "Silver", 800 }, std::pair{ "Gold", 1200 },
			std::pair{ "Platinum", 1600 }, std::pair{ "Emerald", 2000 },
			std::pair{ "Diamond", 2400 }, std::pair{ "Master", 2800 }
		};

		static constexpr std::array Divisions =
		{
			std::pair{ "IV", 0 }, std::pair{ "III", 100 },
			std::pair{ "II", 200 }, std::pair{ "I", 300 }
		};

		if ( LP >= 2800 ) return { .Rank = "MASTER", .Division = {}, .LP = LP - 2800, .IsApex = true };

		int32_t     RankBase = 0;
		std::string TierName;
		for ( auto& [ Name, Base ] : Ranks | std::views::reverse )
		{
			if ( LP >= Base )
			{
				TierName = Name;
				RankBase = Base;
				break;
			}
		}

		int32_t     Remainder = LP - RankBase;
		std::string DivisionName;
		for ( auto& [ Name, Base ] : Divisions | std::views::reverse )
		{
			if ( Remainder >= Base )
			{
				DivisionName = Name;
				Remainder -= Base;
				break;
			}
		}

		return { .Rank = TierName, .Division = DivisionName, .LP = Remainder, .IsApex = false };
	}

	asio::awaitable<std::optional<RiotAccount>> RiotAccount::Create( std::string PUUID )
	{
		if ( !Globals::LeagueAPI ) throw std::runtime_error( "No RiotAPI instance, can not create RiotAccounts!" );

		RiotAccount Account;
		Account.PUUID = std::move( PUUID );

		url U;

		{
			U.set_path( "/riot/account/v1/accounts/by-puuid" );
			U.segments().push_back( Account.PUUID );

			auto [ Status, Body ] = co_await Globals::LeagueAPI->GET( U.encoded_path() );
			if ( Status != 200 ) co_return std::nullopt;

			const auto J = json::parse( Body, nullptr, false );
			if ( J.is_discarded() ) co_return std::nullopt;

			J[ "gameName" ].get_to( Account.SummonerName );
			J[ "tagLine" ].get_to( Account.TagLine );
		}

		{
			U.set_path( "/riot/account/v1/region/by-game/lol/by-puuid" );
			U.segments().push_back( Account.PUUID );

			auto [ Status, Body ] = co_await Globals::LeagueAPI->GET( U.encoded_path() );
			if ( Status != 200 ) co_return std::nullopt;

			const auto J = json::parse( Body, nullptr, false );
			if ( J.is_discarded() ) co_return std::nullopt;

			J[ "region" ].get_to( Account.Region );
		}

		Account.LastKnownRank = co_await Globals::LeagueAPI->GetLeagueRank( Account );
		Account.Valid         = true;

		co_return Account;
	}

	RiotData::RiotData( std::string Channel ) : TwitchChannel( std::move( Channel ) )
	{
	}

	Riot::Riot( std::string Key ) : API( std::move( Key ) ), SSLC( ssl::context::tlsv12_client )
	{
		SSLC.set_default_verify_paths();
	}

	void Riot::InitializeDataDragon()
	{
		std::string LastVersion = "16.1.1";
		url         U;

		{
			U.set_path( "/api/versions.json" );

			const auto Target           = U.encoded_path();
			auto       [ Status, Body ] = this->GETSync( "ddragon.leagueoflegends.com", Target, false );

			if ( Status != 200 )
			{
				PrintError( "Riot::DataDragon couldn't be reached; Status {}", Status );
				throw std::runtime_error( "Couldn't reach DataDragon" );
			}

			auto J      = json::parse( Body );
			LastVersion = J.begin().value().get<std::string>();
		}

		PrintDebug( "Using DataDragon Version {}", LastVersion );

		{
			U.set_path( "/cdn" );
			U.segments().push_back( LastVersion );
			U.segments().push_back( "data/en_US/champion.json" );

			const auto Target           = U.encoded_path();
			auto       [ Status, Body ] = this->GETSync( "ddragon.leagueoflegends.com", Target, false );

			auto  J         = json::parse( Body );
			auto& Champions = J[ "data" ];

			ChampionMap.reserve( Champions.size() );

			for ( auto& [ _, ChampData ] : Champions.items() )
			{
				auto ID          = std::stoi( ChampData[ "key" ].get<std::string>() );
				auto DisplayName = ChampData[ "name" ].get<std::string>();

				ChampionMap[ ID ] = std::move( DisplayName );
			}
		}

		PrintDebug( "Loaded {} champions", ChampionMap.size() );
	}

	void Riot::Connect( const std::vector<Database::Streamer>& Streamers )
	{
		for ( const auto& Streamer : Streamers ) this->Data[ Streamer.StreamerID ] = RiotData{ Streamer.StreamerID };

		std::atomic<size_t> Pending = 0;
		std::promise<void>  Ready;

		for ( const auto& Streamer : Streamers )
		{
			for ( const auto& PUUID : Streamer.PUUIDs )
			{
				++Pending;
				asio::co_spawn( Globals::IOC, [&, PUUID = std::string( PUUID ), StreamerID = Streamer.StreamerID]() -> asio::awaitable<void>
				{
					const auto Account = co_await RiotAccount::Create( PUUID );
					if ( Account ) this->Data[ StreamerID ].Accounts.push_back( *Account );

					if ( --Pending == 0 ) Ready.set_value();
				}, asio::detached );
			}
		}

		if ( Pending > 0 ) Ready.get_future().wait();

		asio::co_spawn( Globals::IOC, Work(), [] ( std::exception_ptr E )
		{
			if ( E )
			{
				try { std::rethrow_exception( E ); }
				catch ( const std::exception& Ex ) { PrintError( "Work() threw: {}", Ex.what() ); }
			}
		} );
	}

	asio::awaitable<Riot::Response> Riot::GET( std::string_view Host, std::string_view Target, bool NeedsAPI )
	{
		auto                           Executor = co_await asio::this_coro::executor;
		ssl::stream<beast::tcp_stream> Stream{ Executor, SSLC };

		if ( !SSL_set_tlsext_host_name( Stream.native_handle(), std::string{ Host }.c_str() ) ) throw std::runtime_error( "SNI failed" );

		tcp::resolver Resolver{ Executor };

		auto const [ EC, Results ] = co_await Resolver.async_resolve( Host, "443", asio::as_tuple( asio::use_awaitable ) );
		if ( EC.failed() ) co_return Riot::Response{};
		co_await beast::get_lowest_layer( Stream ).async_connect( Results, asio::use_awaitable );
		co_await Stream.async_handshake( ssl::stream_base::client, asio::use_awaitable );

		http::request<http::empty_body> Request{ http::verb::get, Target, 11 };
		Request.set( http::field::host, Host );
		Request.set( http::field::user_agent, "lol-sessions/1.0" );
		if ( NeedsAPI ) Request.set( "X-Riot-Token", API );

		co_await http::async_write( Stream, Request, asio::use_awaitable );

		beast::flat_buffer                Buffer;
		http::response<http::string_body> Result;
		co_await http::async_read( Stream, Buffer, Result, asio::use_awaitable );

		co_return Response{ .Status = Result.result_int(), .Body = std::move( Result.body() ) };
	}

	Riot::Response Riot::GETSync( std::string_view Host, std::string_view Target, bool NeedsAPI )
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
		if ( NeedsAPI ) Request.set( "X-Riot-Token", API );

		http::write( Stream, Request );

		beast::flat_buffer                Buffer;
		http::response<http::string_body> Result;
		http::read( Stream, Buffer, Result );

		return { .Status = ( Result.result_int() ), .Body = std::move( Result.body() ) };
	}

	asio::awaitable<Riot::Response> Riot::GET( std::string_view Target )
	{
		co_return co_await GET( REGIONAL_HOST, Target );
	}

	RiotAccount* Riot::GetActiveAccount( const std::string_view StreamerID )
	{
		auto&      StreamerData = this->Data[ std::string( StreamerID ) ];
		const auto Iterator     = std::ranges::find_if( StreamerData.Accounts, [] ( const RiotAccount& Account ) { return !Account.Summaries.empty(); } );

		if ( Iterator == StreamerData.Accounts.end() ) return nullptr;

		return &*Iterator;
	}

	asio::awaitable<std::optional<ActiveGame>> Riot::GetCurrentGame( std::string_view StreamerID )
	{
		auto& StreamerData = this->Data[ std::string( StreamerID ) ];

		const RiotAccount* ActiveAccount = nullptr;
		for ( const auto& Account : StreamerData.Accounts )
		{
			url U;
			U.set_path( "/lol/spectator/v5/active-games/by-summoner" );
			U.segments().push_back( Account.PUUID );

			auto [ Status, Body ] = co_await GET( ServerHost( Account.Region ), U.encoded_path() );

			if ( Status == 200 )
			{
				ActiveAccount = &Account;
				break;
			}
		}

		if ( !ActiveAccount ) co_return std::nullopt;

		url U;
		U.set_path( "/lol/spectator/v5/active-games/by-summoner" );
		U.segments().push_back( ActiveAccount->PUUID );

		auto [ Status, Body ] = co_await GET( ServerHost( ActiveAccount->Region ), U.encoded_path() );

		if ( Status != 200 ) co_return std::nullopt;

		const auto J = json::parse( Body );

		int32_t ChampionID = 0;

		std::vector<std::string> PUUIDs;
		for ( const auto& Participant : J[ "participants" ] )
		{
			if ( !Participant.contains( "puuid" ) || Participant[ "puuid" ].is_null() ) continue;

			const auto PUUID = Participant[ "puuid" ].get<std::string>();
			if ( PUUID == ActiveAccount->PUUID ) ChampionID = Participant[ "championId" ];

			PUUIDs.push_back( PUUID );
		}

		auto CoFetch = [&] ( const std::string& PUUID )-> asio::awaitable<std::optional<RankEntry>>
		{
			url RankURL;
			RankURL.set_path( "/lol/league/v4/entries/by-puuid" );
			RankURL.segments().push_back( PUUID );

			auto [ _Status, _RankBody ] = co_await GET( ServerHost( ActiveAccount->Region ), RankURL.encoded_path() );
			if ( _Status != 200 ) co_return std::nullopt;

			const auto Entries = json::parse( _RankBody );
			const auto SoloQ   = std::ranges::find_if( Entries, [] ( const json& E )
			{
				return E[ "queueType" ].get<std::string>() == "RANKED_SOLO_5x5";
			} );

			if ( SoloQ == Entries.end() ) co_return std::nullopt;

			std::string Rank = ( *SoloQ )[ "tier" ];
			Capitalize( Rank );

			co_return RankEntry{ .Rank = Rank, .Division = ( *SoloQ )[ "rank" ], .LP = ( *SoloQ )[ "leaguePoints" ] };
		};

		std::vector<asio::awaitable<std::optional<RankEntry>>> Tasks;
		Tasks.reserve( PUUIDs.size() );
		for ( const auto& PUUID : PUUIDs )
		{
			Tasks.push_back( CoFetch( PUUID ) );
		}

		std::vector<RankEntry> Ranks;
		for ( auto& Task : Tasks )
		{
			auto Result = co_await std::move( Task );
			if ( Result.has_value() ) Ranks.push_back( Result.value() );
		}

		auto [ Rank, Division, LP, IsApex ] = LPToRank( GetAverageLP( Ranks ) );
		Capitalize( Rank );

		const auto AverageEloFormatted = IsApex ? std::format( "average LP is {} LP", LP ) : std::format( "average rank is {} {} {} LP", Rank, Division, LP );

		co_return ActiveGame{ .PUUID = ActiveAccount->PUUID, .Champion = ChampionMap[ ChampionID ], .AverageElo = AverageEloFormatted };
	}

	asio::awaitable<std::optional<RankEntry>> Riot::GetLeagueRank( const RiotAccount& Account )
	{
		url RankURL;
		RankURL.set_path( "/lol/league/v4/entries/by-puuid" );
		RankURL.segments().push_back( Account.PUUID );

		auto [ Status, RankBody ] = co_await this->GET( ServerHost( Account.Region ), RankURL.encoded_path() );

		if ( Status != 200 )
		{
			co_return std::nullopt;
		}

		const auto Entries = json::parse( RankBody );
		const auto SoloQ   = std::ranges::find_if( Entries, [] ( const json& E )
		{
			return E[ "queueType" ].get<std::string>() == "RANKED_SOLO_5x5";
		} );

		if ( SoloQ == Entries.end() ) co_return std::nullopt;

		auto              Rank     = ( *SoloQ )[ "tier" ].get<std::string>();
		const std::string Division = ( *SoloQ )[ "rank" ];
		const int32_t     LP       = ( *SoloQ )[ "leaguePoints" ];
		const bool        IsApex   = Rank == "MASTER" || Rank == "GRANDMASTER" || Rank == "CHALLENGER";

		Capitalize( Rank );

		co_return RankEntry{ .Rank = Rank, .Division = Division, .LP = LP, .IsApex = IsApex };
	}

	asio::awaitable<std::optional<std::string>> Riot::GetLeagueRankFormatted( std::string_view StreamerID )
	{
		const auto ActiveAccount = this->GetActiveAccount( StreamerID );

		if ( !ActiveAccount ) co_return std::nullopt;

		url RankURL;
		RankURL.set_path( "/lol/league/v4/entries/by-puuid" );
		RankURL.segments().push_back( ActiveAccount->PUUID );

		auto [ Status, RankBody ] = co_await Globals::LeagueAPI->GET( ServerHost( ActiveAccount->Region ), RankURL.encoded_path() );

		if ( Status != 200 )
		{
			co_return std::nullopt;
		}

		const auto Entries = json::parse( RankBody );
		const auto SoloQ   = std::ranges::find_if( Entries, [] ( const json& E )
		{
			return E[ "queueType" ].get<std::string>() == "RANKED_SOLO_5x5";
		} );

		if ( SoloQ == Entries.end() ) co_return std::nullopt;

		std::string       Rank     = ( *SoloQ )[ "tier" ];
		const std::string Division = ( *SoloQ )[ "rank" ];
		const int32_t     LP       = ( *SoloQ )[ "leaguePoints" ];
		const bool        IsApex   = Rank == "MASTER" || Rank == "GRANDMASTER" || Rank == "CHALLENGER";

		Capitalize( Rank );

		if ( IsApex )
		{
			co_return std::format( "{} is currently {} {} LP", StreamerID, Rank, LP );
		}

		co_return std::format( "{} is currently {} {} {} LP", StreamerID, Rank, Division, LP );
	}

	asio::awaitable<std::optional<std::string>> Riot::GetPUUID( const std::string_view SummonerName, const std::string_view TagLine )
	{
		url U;
		U.set_path( "/riot/account/v1/accounts/by-riot-id" );
		U.segments().push_back( SummonerName );
		U.segments().push_back( TagLine );

		const auto Target           = U.encoded_path();
		auto       [ Status, Body ] = co_await this->GET( Target );

		if ( Status != 200 ) co_return std::nullopt;

		const auto J = json::parse( Body );
		co_return J[ "puuid" ].get<std::string>();
	}

	RiotData& Riot::GetData( const std::string_view StreamerID )
	{
		return this->Data[ std::string( StreamerID ) ];
	}

	asio::awaitable<bool> Riot::AddAccount( const std::string_view StreamerID, const std::string_view PUUID )
	{
		auto& Streamer = this->Data[ std::string( StreamerID ) ];
		auto  Account  = co_await RiotAccount::Create( std::string( PUUID ) );

		if ( !Account.has_value() || !Account->Valid ) co_return false;

		Streamer.Accounts.push_back( Account.value() );

		( void )Globals::DB->AddAccount( StreamerID, PUUID );
		co_return true;
	}

	asio::awaitable<bool> Riot::RemoveAccount( const std::string_view StreamerID, const std::string_view SummonerName, const std::string_view TagLine )
	{
		const auto Iterator = this->Data.find( std::string( StreamerID ) );

		if ( Iterator == this->Data.end() ) co_return false;

		const auto TryMatch = [&] ( const RiotAccount& Account )
		{
			return Account.SummonerName == SummonerName && Account.TagLine == TagLine;
		};

		const auto Result = std::ranges::remove_if( Iterator->second.Accounts, TryMatch );

		if ( Result.begin() == Result.end() ) co_return false; // No match

		Iterator->second.Accounts.erase( Result.begin(), Result.end() );

		{
			const auto PUUID = co_await Globals::LeagueAPI->GetPUUID( SummonerName, TagLine );

			if ( PUUID.has_value() )
			{
				( void )Globals::DB->RemoveAccount( StreamerID, PUUID.value() );
			}
		}

		co_return true;
	}
}
