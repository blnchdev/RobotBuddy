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

		std::string GetLastGameID( const RiotAccount& Account )
		{
			url U;
			U.set_path( "/lol/match/v5/matches/by-puuid" );
			U.segments().push_back( Account.PUUID );
			U.segments().push_back( "ids" );
			U.params().append( { "queue", "420" } );
			U.params().append( { "start", "0" } );
			U.params().append( { "count", "1" } );

			const auto Target           = U.encoded_target();
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

			const auto Target           = U.encoded_target();
			auto       [ Status, Body ] = Globals::LeagueAPI->GET( RegionalHost( Account.Region ), Target );

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

			const float Ratio = D == 0 ? -1.f : ( static_cast<float>( K ) + A ) / D;

			return GameSummary
			{
				.GameID = ExtractGameID( GameID ),
				.Champion = PlayerData[ "championName" ],
				.Role = PlayerData[ "individualPosition" ],
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

		void RefreshSession( std::string_view ChannelID, RiotAccount& Account )
		{
			if ( Account.Summaries.empty() )
			{
				// First pass, we grab the last 20 GameIDs
				const auto Strings       = GetLastGameIDs( Account, 20 );
				uint64_t   LastGameStart = 0;

				std::vector<GameSummary> TemporaryList = {};

				for ( const auto& StringID : Strings )
				{
					const auto Summary = GetGameSummary( Account.PUUID, Account.Region, StringID );

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
				const auto StringID = GetLastGameID( Account );
				const auto LastID   = ExtractGameID( StringID );

				if ( LastID == Account.Summaries.back().GameID ) return; // Last Fetched == Last Saved; no new game.

				PrintDebug( "Found new game! '{}' (LastID was {})", StringID, LastID );

				const auto NewSummary = GetGameSummary( Account.PUUID, Account.Region, StringID );

				if ( NewSummary.has_value() )
				{
					GameSummary Summary     = NewSummary.value();
					const auto  CurrentRank = Globals::LeagueAPI->GetLeagueRank( Account );

					if ( Account.LastKnownRank.has_value() && CurrentRank.has_value() )
					{
						int32_t OldLP = RankToLP( Account.LastKnownRank.value() );
						int32_t NewLP = RankToLP( CurrentRank.value() );
						int32_t Delta = NewLP - OldLP;

						if ( Delta != 0 )
						{
							Summary.DeltaLP = Delta;
						}

						PrintDebug( "Delta {} (OldRank: {} {} {} LP / NewRank: {} {} {} LP)", Delta,
						            Account.LastKnownRank->Rank, Account.LastKnownRank->Division, Account.LastKnownRank->LP,
						            CurrentRank->Rank, CurrentRank->Division, CurrentRank->LP
						          );

						Account.LastKnownRank = CurrentRank;
					}
					else
					{
						PrintWarn( "{}#{} LastKnownRank? {} / CurrentRank?", Account.SummonerName, Account.TagLine, Account.LastKnownRank.has_value(), CurrentRank.has_value() );
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

				const auto ActiveGame = Globals::LeagueAPI->GetCurrentGame( Streamer.StreamerID );

				if ( ActiveGame.has_value() )
				{
					if ( RiotData.ActivePUUID != ActiveGame->PUUID )
					{
						RiotData.ActivePUUID = ActiveGame->PUUID;
					}
				}
				else
				{
					const auto LatestActive = std::ranges::max_element( RiotData.Accounts, {}, GetLastGameTimestamp );

					if ( LatestActive == RiotData.Accounts.end() ) return;

					RiotData.ActivePUUID = LatestActive->PUUID;
				}

				const auto Iterator = std::ranges::find_if( RiotData.Accounts, [&] ( const RiotAccount& Account ) { return Account.PUUID == RiotData.ActivePUUID; } );

				if ( Iterator != RiotData.Accounts.end() )
				{
					RefreshSession( Streamer.StreamerID, *Iterator );
				}
			};

			// First pass
			{
				const auto Streamers = Globals::DB->GetStreamers();
				std::ranges::for_each( Streamers, PopulateLastSession );
			}

			while ( true )
			{
				const auto Streamers = Globals::DB->GetStreamers();

				auto Refresh = [&] ( const Database::Streamer& Streamer )
				{
					const auto ActiveAccount = Globals::LeagueAPI->GetActiveAccount( Streamer.StreamerID );

					if ( ActiveAccount )
					{
						RefreshSession( Streamer.StreamerID, *ActiveAccount );
					}
				};

				std::ranges::for_each( Streamers, Refresh );
				std::this_thread::sleep_for( std::chrono::seconds( 15 ) );
			}
		}
	}

	int32_t RankToLP( const RankEntry& Entry )
	{
		const auto TierIterator = TierBase.find( Entry.Rank );
		if ( TierIterator == TierBase.end() ) return 0;

		const bool IsApexRank = Entry.Rank == "MASTER" || Entry.Rank == "GRANDMASTER" || Entry.Rank == "CHALLENGER";
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

		this->LastKnownRank = Globals::LeagueAPI->GetLeagueRank( *this );
		this->Valid         = true;
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
			auto       [ Status, Body ] = this->GET( "ddragon.leagueoflegends.com", Target, false );

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
			auto       [ Status, Body ] = this->GET( "ddragon.leagueoflegends.com", Target, false );

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
		auto Populate = [&] ( const Database::Streamer& Streamer )
		{
			RiotData Buffer{ Streamer.StreamerID };

			std::ranges::for_each( Streamer.PUUIDs, [&] ( const std::string_view PUUID ) { Buffer.Accounts.emplace_back( std::string( PUUID ) ); } );
			this->Data[ Buffer.TwitchChannel ] = std::move( Buffer );
		};

		std::ranges::for_each( Streamers, Populate );

		this->WorkerThread = std::jthread( Work );
	}

	Riot::Response Riot::GET( std::string_view Host, std::string_view Target, bool NeedsAPI )
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

	Riot::Response Riot::GET( const std::string_view Target )
	{
		return this->GET( REGIONAL_HOST, Target );
	}

	RiotAccount* Riot::GetActiveAccount( const std::string_view StreamerID )
	{
		auto&      StreamerData = this->Data[ std::string( StreamerID ) ];
		const auto Iterator     = std::ranges::find_if( StreamerData.Accounts, [] ( const RiotAccount& Account ) { return !Account.Summaries.empty(); } );

		if ( Iterator == StreamerData.Accounts.end() ) return nullptr;

		return &*Iterator;
	}

	std::optional<ActiveGame> Riot::GetCurrentGame( std::string_view StreamerID )
	{
		auto& StreamerData = this->Data[ std::string( StreamerID ) ];

		const auto* ActiveAccount = [&]() -> const RiotAccount*
		{
			for ( const auto& Account : StreamerData.Accounts )
			{
				url U;
				U.set_path( "/lol/spectator/v5/active-games/by-summoner" );
				U.segments().push_back( Account.PUUID );

				auto Target        = U.encoded_path();
				auto [ Status, _ ] = Globals::LeagueAPI->GET( ServerHost( Account.Region ), Target );
				if ( Status == 200 ) return &Account;
			}

			return nullptr;
		}();

		if ( !ActiveAccount ) return std::nullopt;

		url U;
		U.set_path( "/lol/spectator/v5/active-games/by-summoner" );
		U.segments().push_back( ActiveAccount->PUUID );

		auto       [ _, Body ] = Globals::LeagueAPI->GET( ServerHost( ActiveAccount->Region ), U.encoded_path() );
		const auto J           = json::parse( Body );

		int32_t                ChampionID = 0;
		std::vector<RankEntry> Ranks;

		for ( const auto& Participant : J[ "participants" ] )
		{
			if ( !Participant.contains( "puuid" ) || Participant[ "puuid" ].is_null() ) continue;

			const auto PUUID = Participant[ "puuid" ].get<std::string>();

			if ( PUUID == ActiveAccount->PUUID ) ChampionID = Participant[ "championId" ];

			url RankURL;
			RankURL.set_path( "/lol/league/v4/entries/by-puuid" );
			RankURL.segments().push_back( PUUID );

			auto [ Status, RankBody ] = Globals::LeagueAPI->GET( ServerHost( ActiveAccount->Region ), RankURL.encoded_path() );

			if ( Status != 200 )
			{
				continue;
			}

			const auto Entries = json::parse( RankBody );
			const auto SoloQ   = std::ranges::find_if( Entries, [] ( const json& E )
			{
				return E[ "queueType" ].get<std::string>() == "RANKED_SOLO_5x5";
			} );

			if ( SoloQ != Entries.end() )
			{
				Ranks.emplace_back( ( *SoloQ )[ "tier" ], ( *SoloQ )[ "rank" ], ( *SoloQ )[ "leaguePoints" ] );
			}
		}

		auto [ Rank, Division, LP, IsApex ] = LPToRank( GetAverageLP( Ranks ) );

		Capitalize( Rank );

		const auto AverageEloFormatted = IsApex ? std::format( "average LP is {} LP", LP ) : std::format( "average rank is {} {} {} LP", Rank, Division, LP );

		return ActiveGame{ .PUUID = ActiveAccount->PUUID, .Champion = ChampionMap[ ChampionID ], .AverageElo = AverageEloFormatted };
	}

	std::optional<RankEntry> Riot::GetLeagueRank( const RiotAccount& Account )
	{
		url RankURL;
		RankURL.set_path( "/lol/league/v4/entries/by-puuid" );
		RankURL.segments().push_back( Account.PUUID );

		auto [ Status, RankBody ] = this->GET( ServerHost( Account.Region ), RankURL.encoded_path() );

		if ( Status != 200 )
		{
			return std::nullopt;
		}

		const auto Entries = json::parse( RankBody );
		const auto SoloQ   = std::ranges::find_if( Entries, [] ( const json& E )
		{
			return E[ "queueType" ].get<std::string>() == "RANKED_SOLO_5x5";
		} );

		auto              Rank     = ( *SoloQ )[ "tier" ].get<std::string>();
		const std::string Division = ( *SoloQ )[ "rank" ];
		const int32_t     LP       = ( *SoloQ )[ "leaguePoints" ];
		const bool        IsApex   = Rank == "MASTER" || Rank == "GRANDMASTER" || Rank == "CHALLENGER";

		Capitalize( Rank );

		return RankEntry{ .Rank = Rank, .Division = Division, .LP = LP, .IsApex = IsApex };
	}

	std::optional<std::string> Riot::GetLeagueRankFormatted( std::string_view StreamerID )
	{
		const auto ActiveAccount = this->GetActiveAccount( StreamerID );

		if ( !ActiveAccount ) return std::nullopt;

		url RankURL;
		RankURL.set_path( "/lol/league/v4/entries/by-puuid" );
		RankURL.segments().push_back( ActiveAccount->PUUID );

		auto [ Status, RankBody ] = Globals::LeagueAPI->GET( ServerHost( ActiveAccount->Region ), RankURL.encoded_path() );

		if ( Status != 200 )
		{
			return std::nullopt;
		}

		const auto Entries = json::parse( RankBody );
		const auto SoloQ   = std::ranges::find_if( Entries, [] ( const json& E )
		{
			return E[ "queueType" ].get<std::string>() == "RANKED_SOLO_5x5";
		} );

		std::string       Rank     = ( *SoloQ )[ "tier" ].get<std::string>();
		const std::string Division = ( *SoloQ )[ "rank" ];
		const int32_t     LP       = ( *SoloQ )[ "leaguePoints" ];
		const bool        IsApex   = Rank == "MASTER" || Rank == "GRANDMASTER" || Rank == "CHALLENGER";

		Capitalize( Rank );

		if ( IsApex )
		{
			return std::format( "{} is currently {} {} LP", StreamerID, Rank, LP );
		}

		return std::format( "{} is currently {} {} {} LP", StreamerID, Rank, Division, LP );
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
