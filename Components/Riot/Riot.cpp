#include "Riot.h"

#include <algorithm>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include "Components/Events/OnEndGame/OnEndGame.h"
#include "Components/TUI/TUI.h"
#include "Globals/Globals.h"

namespace Components
{
	namespace
	{
		std::unordered_map<uint32_t, std::string> ChampionMap = {};

		std::string_view RegionalHost( const std::string_view Region )
		{
			using namespace std::string_view_literals;

			if ( Region == "eun1" || Region == "euw1" || Region == "ru" || Region == "tr1" || Region == "me1" ) return "europe.api.riotgames.com"sv;
			if ( Region == "na1" || Region == "br1" || Region == "la1" || Region == "la2" ) return "americas.api.riotgames.com"sv;
			if ( Region == "jp1" || Region == "kr" ) return "asia.api.riotgames.com"sv;

			return "sea.api.riotgames.com"sv;
		}

		bool IsInCurrentDay( const std::chrono::system_clock::time_point& Now, const uint64_t UnixTimeMS )
		{
			using namespace std::chrono;

			const auto TimePoint = system_clock::time_point{ milliseconds( UnixTimeMS ) };

			const auto TimePointDay = std::chrono::floor<days>( zoned_time{ current_zone(), TimePoint }.get_local_time() );
			const auto NowDay       = std::chrono::floor<days>( zoned_time{ current_zone(), Now }.get_local_time() );

			return TimePointDay == NowDay;
		}

		// Takes a unix time in milliseconds
		int64_t MinutesBetweenTimestamps( const uint64_t A, const uint64_t B )
		{
			const auto _A = std::chrono::system_clock::time_point{ std::chrono::milliseconds( A ) };
			const auto _B = std::chrono::system_clock::time_point{ std::chrono::milliseconds( B ) };

			return abs( duration_cast<std::chrono::minutes>( _A - _B ).count() );
		}

		int64_t MinutesSinceTimestamp( const std::chrono::system_clock::time_point& Now, const uint64_t UnixTimeMS )
		{
			const auto TimePoint = std::chrono::system_clock::time_point{ std::chrono::milliseconds{ UnixTimeMS } };
			return duration_cast<std::chrono::minutes>( Now - TimePoint ).count();
		}

		uint64_t ExtractGameID( std::string_view Input )
		{
			const size_t Index = Input.find( '_' );
			if ( Index == std::string_view::npos ) return 0;

			uint64_t   Value{};
			const auto Start = Input.data() + Index + 1;
			const auto End   = Input.data() + Input.size();

			auto [ Pointer, EC ] = std::from_chars( Start, End, Value );
			if ( EC != std::errc() ) return 0;

			return Value;
		}

		std::string ServerHost( const std::string_view Region )
		{
			return std::string( Region ) + ".api.riotgames.com";
		}

		int32_t GetAverageLP( const std::span<RankEntry> Entries )
		{
			if ( Entries.empty() ) return 0;

			int32_t Sum = 0;
			for ( auto& Entry : Entries ) Sum += RankToLP( Entry );

			return Sum / static_cast<int>( Entries.size() );
		}

		asio::awaitable<std::vector<std::string>> GetLastGameIDs( const RiotAccount& Account, const size_t Count )
		{
			url U;
			U.set_path( "/lol/match/v5/matches/by-puuid" );
			U.segments().push_back( Account.Info.PUUID );
			U.segments().push_back( "ids" );
			U.params().append( { "start", "0" } );
			U.params().append( { "count", std::to_string( Count ) } );

			const auto Target           = U.encoded_target();
			auto       [ Status, Body ] = co_await Globals::LeagueAPI->GET( RegionalHost( Account.Info.Region ), Target );

			if ( Status != 200 ) co_return std::vector<std::string>();

			const auto J = json::parse( Body );
			co_return J.get<std::vector<std::string>>();
		}

		asio::awaitable<std::optional<GameSummary>> GetGameSummary( std::string PUUID, std::string Region, std::string GameID )
		{
			url U;
			U.set_path( "/lol/match/v5/matches" );
			U.segments().push_back( GameID );

			const auto Target           = U.encoded_path();
			auto       [ Status, Body ] = co_await Globals::LeagueAPI->GET( RegionalHost( Region ), Target );

			if ( Status != 200 || Body.empty() )
			{
				PrintWarn( "GetGameSummary() failed with Status {}", Status );
				co_return std::nullopt;
			}

			auto J    = json::parse( Body );
			auto Info = J[ "info" ];

			auto& Participants = Info[ "participants" ];

			auto TryMatch = [&] ( const json& Data )
			{
				return Data[ "puuid" ].get<std::string>() == PUUID;
			};

			const auto Iterator = std::ranges::find_if( Participants, TryMatch );

			if ( Iterator == Participants.end() )
			{
				PrintWarn( "GetGameSummary(): Could not find Participant" );
				co_return std::nullopt;
			}

			auto& PlayerData = *Iterator;

			int8_t Number = 0;

			const uint64_t GameStart    = Info[ "gameStartTimestamp" ].get<uint64_t>();
			const uint64_t GameDuration = Info[ "gameDuration" ].get<uint64_t>();
			const auto     RoleString   = PlayerData[ "individualPosition" ].is_string() ? PlayerData[ "individualPosition" ].get<std::string>() : std::string( "NONE" );
			uint32_t       QueueID      = Info[ "queueId" ];

			Number++;

			int32_t K, D, A;
			PlayerData[ "kills" ].get_to( K );
			PlayerData[ "deaths" ].get_to( D );
			PlayerData[ "assists" ].get_to( A );

			Number++;

			co_return GameSummary
			{
				.GameID = ExtractGameID( GameID ),
				.Champion = PlayerData[ "championName" ],
				.Role = DisplayToRole( RoleString ),
				.Type = QueueIDToType( QueueID ),
				.Win = PlayerData[ "win" ].get<bool>(),
				.KDA = KDA( K, D, A ),
				.Duration = GameDuration,
				.GameEnd = GameStart + GameDuration * 1000ULL,
				.CreepScore = PlayerData[ "totalMinionsKilled" ].get<int32_t>() + PlayerData[ "neutralMinionsKilled" ].get<int32_t>(),
				.VisionScore = PlayerData[ "visionScore" ].get<double>()
			};
		}
	}

	asio::awaitable<void> RiotAccount::Populate()
	{
		this->WasPopulated = true;

		const auto Now = std::chrono::system_clock::now();
		const auto IDs = co_await GetLastGameIDs( *this, 20 );

		std::optional<uint64_t> PreviousGameEnd = std::nullopt;

		for ( const auto& ID : IDs )
		{
			auto Summary = co_await GetGameSummary( this->Info.PUUID, this->Info.Region, ID );

			if ( !Summary.has_value() )
			{
				PrintWarn( "GetSummary failed" );
				continue;
			}

			if ( PreviousGameEnd.has_value() )
			{
				const auto DeltaMinutes = MinutesBetweenTimestamps( Summary->GameEnd, *PreviousGameEnd );
				if ( DeltaMinutes > 120 ) break;
			}
			else
			{
				if ( !IsInCurrentDay( Now, Summary->GameEnd ) ) break;
			}

			PreviousGameEnd = Summary->GameEnd;

			auto* Data = this->GetData( Summary->Type );
			if ( Data ) Data->Games.push_back( *Summary );
		}

		std::ranges::sort( this->Normal.Games, std::less{}, &GameSummary::GameEnd );
		std::ranges::sort( this->ARAM.Games, std::less{}, &GameSummary::GameEnd );
		std::ranges::sort( this->SoloQ.Games, std::less{}, &GameSummary::GameEnd );
		std::ranges::sort( this->FlexQ.Games, std::less{}, &GameSummary::GameEnd );
		std::ranges::sort( this->Arena.Games, std::less{}, &GameSummary::GameEnd );
	}

	asio::awaitable<void> RiotAccount::Refresh()
	{
		if ( !WasPopulated )
		{
			co_await this->Populate();
			co_return;
		}

		const auto SpectatorData = co_await Globals::LeagueAPI->GetCurrentGame( this );

		if ( SpectatorData.has_value() )
		{
			if ( !this->CurrentGame )
			{
				this->CurrentGame = std::make_unique<ActiveGame>( *SpectatorData );
			}
		}
		else
		{
			// No longer in-game, aka game ended!
			if ( this->CurrentGame )
			{
				const auto LastID = co_await GetLastGameIDs( *this, 1 );

				if ( LastID.empty() )
				{
					this->CurrentGame = nullptr;
					co_return;
				}

				const uint64_t ExtractedID = ExtractGameID( LastID.front() );

				auto& Games = this->GetData( this->CurrentGame->Type )->Games;

				if ( std::ranges::find( Games, ExtractedID, &GameSummary::GameID ) != Games.end() )
				{
					this->CurrentGame = nullptr;
					co_return;
				}

				const auto NewSummary = co_await GetGameSummary( this->Info.PUUID, this->Info.Region, LastID.front() );

				if ( !NewSummary.has_value() )
				{
					this->CurrentGame = nullptr;
					co_return;
				}

				GameSummary Summary  = NewSummary.value();
				bool        IsRanked = Summary.Type == GameType::SOLOQ || Summary.Type == GameType::FLEX;

				if ( IsRanked )
				{
					const auto NewRank = co_await Globals::LeagueAPI->GetLeagueRank( *this, NewSummary->Type );

					if ( NewRank.has_value() )
					{
						auto&         ToEdit  = NewSummary->Type == GameType::SOLOQ ? this->SoloQ : this->FlexQ;
						const int32_t DeltaLP = ToEdit.Push( NewRank.value() );

						if ( DeltaLP != 0 )
						{
							Summary.DeltaLP = DeltaLP;
						}
					}
				}

				this->LastGameModePlayed = this->CurrentGame->Type;
				// Event::OnEndGame::Trigger( this->Info.Owner->Channel, Summary );
				Games.push_back( Summary );
				this->CurrentGame = nullptr;
			}
		}
	}

	asio::awaitable<std::shared_ptr<RiotAccount>> RiotAccount::Create( StreamerData* Owner, std::string PUUID )
	{
		auto Account        = std::make_unique<RiotAccount>();
		Account->Info.PUUID = std::move( PUUID );
		Account->Info.Owner = Owner;

		url U;

		// Get Summoner Name and Tag Line
		{
			U.set_path( "/riot/account/v1/accounts/by-puuid" );
			U.segments().push_back( Account->Info.PUUID );

			auto [ Status, Body ] = co_await Globals::LeagueAPI->GET( U.encoded_path() );
			if ( Status != 200 ) co_return nullptr;

			const auto J = json::parse( Body, nullptr, false );
			if ( J.is_discarded() ) co_return nullptr;

			J[ "gameName" ].get_to( Account->Info.SummonerName );
			J[ "tagLine" ].get_to( Account->Info.TagLine );
		}

		// Get Region
		{
			U.set_path( "/riot/account/v1/region/by-game/lol/by-puuid" );
			U.segments().push_back( Account->Info.PUUID );

			auto [ Status, Body ] = co_await Globals::LeagueAPI->GET( U.encoded_path() );
			if ( Status != 200 ) co_return nullptr;

			const auto J = json::parse( Body, nullptr, false );
			if ( J.is_discarded() ) co_return nullptr;

			J[ "region" ].get_to( Account->Info.Region );
		}

		{
			auto SoloQ = co_await Globals::LeagueAPI->GetLeagueRank( *Account, GameType::SOLOQ );
			auto FlexQ = co_await Globals::LeagueAPI->GetLeagueRank( *Account, GameType::FLEX );

			if ( SoloQ.has_value() )
			{
				Account->SoloQ.Populate( SoloQ.value() );
			}

			if ( FlexQ.has_value() )
			{
				Account->FlexQ.Populate( FlexQ.value() );
			}
		}

		co_return Account;
	}

	asio::awaitable<void> StreamerData::Refresh()
	{
		for ( const auto& Account : this->Accounts )
		{
			if ( Account ) co_await Account->Refresh();
		}

		for ( const auto& Account : this->Accounts )
		{
			if ( Account->CurrentGame )
			{
				this->Active = Account;
				co_return;
			}
		}

		std::shared_ptr<RiotAccount> Latest      = nullptr;
		uint64_t                     LatestStamp = 0;

		for ( const auto& Account : this->Accounts )
		{
			uint64_t AccountLatest = 0;

			for ( const auto* Mode : { &Account->SoloQ, &Account->FlexQ, &Account->Normal, &Account->ARAM, &Account->Arena, &Account->Other } )
			{
				if ( !Mode->Games.empty() )
				{
					const uint64_t Last = Mode->Games.back().GameEnd;
					AccountLatest       = std::max( Last, AccountLatest );
				}
			}

			if ( AccountLatest > LatestStamp )
			{
				LatestStamp = AccountLatest;
				Latest      = Account;
			}
		}

		this->Active = Latest;
	}

	StreamerData::StreamerData( std::string Channel ) : Channel( std::move( Channel ) )
	{
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

	StreamerData* Riot::GetData( const std::string_view ChannelName )
	{
		const uint32_t Hash     = Globals::FNV1a( ChannelName );
		const auto     Iterator = this->Streamers.find( Hash );

		if ( Iterator == this->Streamers.end() ) return nullptr;

		return Iterator->second.get();
	}

	asio::awaitable<std::optional<RankEntry>> Riot::GetLeagueRank( const RiotAccount& Account, const GameType Type )
	{
		std::string_view ToMatch = Type == GameType::SOLOQ ? "RANKED_SOLO_5x5" : "RANKED_FLEX_SR";

		url RankURL;
		RankURL.set_path( "/lol/league/v4/entries/by-puuid" );
		RankURL.segments().push_back( Account.Info.PUUID );

		auto [ Status, RankBody ] = co_await this->GET( ServerHost( Account.Info.Region ), RankURL.encoded_path() );

		if ( Status != 200 )
		{
			co_return std::nullopt;
		}

		const auto Entries      = json::parse( RankBody );
		const auto MatchingRank = std::ranges::find_if( Entries, [ToMatch] ( const json& E )
		{
			return E[ "queueType" ].get<std::string>() == ToMatch;
		} );

		if ( MatchingRank == Entries.end() ) co_return std::nullopt;

		const std::string Rank     = ( *MatchingRank )[ "tier" ];
		const std::string Division = ( *MatchingRank )[ "rank" ];
		const int16_t     LP       = ( *MatchingRank )[ "leaguePoints" ];

		co_return RankEntry( DisplayToRank( Rank ), RomanToByte( Division ), LP );
	}

	asio::awaitable<std::optional<ActiveGame>> Riot::GetCurrentGame( std::string_view ChannelName )
	{
		const uint32_t Hash     = Globals::FNV1a( ChannelName );
		const auto     Iterator = this->Streamers.find( Hash );

		if ( Iterator == this->Streamers.end() ) co_return std::nullopt;

		auto* Data = Iterator->second.get();

		std::shared_ptr<RiotAccount> ActiveAccount = nullptr;
		std::string                  CachedBody;

		for ( const auto& Account : Data->Accounts )
		{
			url U;
			U.set_path( "/lol/spectator/v5/active-games/by-summoner" );
			U.segments().push_back( Account->Info.PUUID );

			auto [ Status, Body ] = co_await GET( ServerHost( Account->Info.Region ), U.encoded_path() );

			if ( Status == 200 )
			{
				ActiveAccount = Account;
				CachedBody    = Body;
				break;
			}
		}

		if ( !ActiveAccount || CachedBody.empty() ) co_return std::nullopt;

		const auto J = json::parse( CachedBody );

		int32_t ChampionID = 0;
		int32_t QueueID    = J[ "gameQueueConfigId" ];

		std::vector<std::string> PUUIDs;

		for ( const auto& Participant : J[ "participants" ] )
		{
			if ( !Participant.contains( "puuid" ) || Participant[ "puuid" ].is_null() ) continue;

			const auto PUUID = Participant[ "puuid" ].get<std::string>();
			if ( PUUID == ActiveAccount->Info.PUUID ) ChampionID = Participant[ "championId" ];

			PUUIDs.push_back( PUUID );
		}

		std::vector<asio::awaitable<std::optional<RankEntry>>> Tasks;
		Tasks.reserve( PUUIDs.size() );

		auto FetchRank = [&] ( const std::string& PUUID, const std::string_view Queue )-> asio::awaitable<std::optional<RankEntry>>
		{
			url RankURL;
			RankURL.set_path( "/lol/league/v4/entries/by-puuid" );
			RankURL.segments().push_back( PUUID );

			auto [ _Status, _RankBody ] = co_await GET( ServerHost( ActiveAccount->Info.Region ), RankURL.encoded_path() );
			if ( _Status != 200 ) co_return std::nullopt;

			const auto Entries      = json::parse( _RankBody );
			const auto MatchingRank = std::ranges::find_if( Entries, [&] ( const json& E )
			{
				return E[ "queueType" ].get<std::string>() == Queue;
			} );

			if ( MatchingRank == Entries.end() ) co_return std::nullopt;

			const std::string Rank     = ( *MatchingRank )[ "tier" ];
			const std::string Division = ( *MatchingRank )[ "rank" ];
			const int16_t     LP       = ( *MatchingRank )[ "leaguePoints" ];

			co_return RankEntry( DisplayToRank( Rank ), RomanToByte( Division ), LP );
		};

		const std::string Queue = QueueID == 440 ? "RANKED_FLEX_SR" : "RANKED_SOLO_5x5";

		for ( const auto& PUUID : PUUIDs )
		{
			Tasks.push_back( FetchRank( PUUID, Queue ) );
		}

		std::vector<RankEntry> Ranks;

		for ( auto& Task : Tasks )
		{
			auto Result = co_await std::move( Task );
			if ( Result.has_value() ) Ranks.push_back( Result.value() );
		}

		RankEntry AverageElo = LPToRank( GetAverageLP( Ranks ) );
		GameType  Type       = QueueIDToType( QueueID );

		co_return ActiveGame( ActiveAccount.get(), ChampionMap[ ChampionID ], AverageElo, QueueID, Type );
	}

	asio::awaitable<std::optional<ActiveGame>> Riot::GetCurrentGame( RiotAccount* Account )
	{
		url U;
		U.set_path( "/lol/spectator/v5/active-games/by-summoner" );
		U.segments().push_back( Account->Info.PUUID );

		auto [ Status, Body ] = co_await GET( ServerHost( Account->Info.Region ), U.encoded_path() );

		if ( Status != 200 )
		{
			co_return std::nullopt;
		}

		if ( Body.empty() ) co_return std::nullopt;

		const auto J = json::parse( Body );

		int32_t ChampionID = 0;
		int32_t QueueID    = J[ "gameQueueConfigId" ];

		std::vector<std::string> PUUIDs;

		for ( const auto& Participant : J[ "participants" ] )
		{
			if ( !Participant.contains( "puuid" ) || Participant[ "puuid" ].is_null() ) continue;

			const auto PUUID = Participant[ "puuid" ].get<std::string>();
			if ( PUUID == Account->Info.PUUID ) ChampionID = Participant[ "championId" ];

			PUUIDs.push_back( PUUID );
		}

		std::vector<asio::awaitable<std::optional<RankEntry>>> Tasks;
		Tasks.reserve( PUUIDs.size() );

		auto FetchRank = [&] ( const std::string& PUUID, const std::string_view Queue )-> asio::awaitable<std::optional<RankEntry>>
		{
			url RankURL;
			RankURL.set_path( "/lol/league/v4/entries/by-puuid" );
			RankURL.segments().push_back( PUUID );

			auto [ _Status, _RankBody ] = co_await GET( ServerHost( Account->Info.Region ), RankURL.encoded_path() );
			if ( _Status != 200 ) co_return std::nullopt;

			const auto Entries      = json::parse( _RankBody );
			const auto MatchingRank = std::ranges::find_if( Entries, [&] ( const json& E )
			{
				return E[ "queueType" ].get<std::string>() == Queue;
			} );

			if ( MatchingRank == Entries.end() ) co_return std::nullopt;

			const std::string Rank     = ( *MatchingRank )[ "tier" ];
			const std::string Division = ( *MatchingRank )[ "rank" ];
			const int16_t     LP       = ( *MatchingRank )[ "leaguePoints" ];

			co_return RankEntry( DisplayToRank( Rank ), RomanToByte( Division ), LP );
		};

		const std::string Queue = QueueID == 440 ? "RANKED_FLEX_SR" : "RANKED_SOLO_5x5";

		for ( const auto& PUUID : PUUIDs )
		{
			Tasks.push_back( FetchRank( PUUID, Queue ) );
		}

		std::vector<RankEntry> Ranks;

		for ( auto& Task : Tasks )
		{
			auto Result = co_await std::move( Task );
			if ( Result.has_value() ) Ranks.push_back( Result.value() );
		}

		RankEntry AverageElo = LPToRank( GetAverageLP( Ranks ) );
		GameType  Type       = QueueIDToType( QueueID );

		co_return ActiveGame( Account, ChampionMap[ ChampionID ], AverageElo, QueueID, Type );
	}

	RiotAccount* Riot::GetActiveAccount( const std::string_view ChannelName )
	{
		const uint32_t Hash     = Globals::FNV1a( ChannelName );
		const auto     Iterator = this->Streamers.find( Hash );

		if ( Iterator == this->Streamers.end() ) return nullptr;

		return Iterator->second->Active ? Iterator->second->Active.get() : nullptr;
	}

	Riot::Response Riot::GETSync( std::string_view Host, std::string_view Target, bool NeedsAPI )
	{
		ssl::stream<beast::tcp_stream> Stream{ Globals::IOC, SSLC };

		if ( !SSL_set_tlsext_host_name( Stream.native_handle(), std::string{ Host }.c_str() ) ) throw std::runtime_error( "SNI failed" );

		tcp::resolver Resolver{ Globals::IOC };
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

	asio::awaitable<Riot::Response> Riot::GET( std::string_view Host, std::string_view Target, bool NeedsAPI )
	{
		auto                           Executor = co_await asio::this_coro::executor;
		ssl::stream<beast::tcp_stream> Stream{ Executor, SSLC };

		if ( !SSL_set_tlsext_host_name( Stream.native_handle(), std::string{ Host }.c_str() ) ) throw std::runtime_error( "SNI failed" );

		tcp::resolver Resolver{ Executor };

		auto const [ EC, Results ] = co_await Resolver.async_resolve( Host, "443", asio::as_tuple( asio::use_awaitable ) );
		if ( EC.failed() ) co_return Response{};
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

	asio::awaitable<Riot::Response> Riot::GET( const std::string_view Target )
	{
		co_return co_await GET( REGIONAL_HOST, Target );
	}

	asio::awaitable<bool> Riot::AddAccount( const std::string_view ChannelName, const std::string_view PUUID )
	{
		const uint32_t Hash     = Globals::FNV1a( ChannelName );
		const auto     Iterator = this->Streamers.find( Hash );

		if ( Iterator == this->Streamers.end() ) co_return false;

		StreamerData*                Streamer = Iterator->second.get();
		std::shared_ptr<RiotAccount> Account  = co_await RiotAccount::Create( Streamer, std::string( PUUID ) );

		if ( !Account ) co_return false;

		Streamer->Accounts.push_back( std::move( Account ) );
		( void )Globals::DB->AddAccount( ChannelName, PUUID );
		co_return true;
	}

	asio::awaitable<bool> Riot::RemoveAccount( const std::string_view ChannelName, const std::string_view SummonerName, const std::string_view TagLine )
	{
		const uint32_t Hash     = Globals::FNV1a( ChannelName );
		const auto     Iterator = this->Streamers.find( Hash );

		if ( Iterator == this->Streamers.end() ) co_return false;

		const auto TryMatch = [&] ( const std::shared_ptr<RiotAccount>& Account )
		{
			return Account->Info.SummonerName == SummonerName && Account->Info.TagLine == TagLine;
		};

		const auto Result = std::ranges::remove_if( Iterator->second->Accounts, TryMatch );

		if ( Result.begin() == Result.end() ) co_return false; // No match

		Iterator->second->Accounts.erase( Result.begin(), Result.end() );

		{
			const auto PUUID = co_await Globals::LeagueAPI->GetPUUID( SummonerName, TagLine );

			if ( PUUID.has_value() )
			{
				( void )Globals::DB->RemoveAccount( ChannelName, PUUID.value() );
			}
		}

		co_return true;
	}

	bool Riot::AddStreamer( const std::string_view ChannelName )
	{
		const uint32_t Hash     = Globals::FNV1a( ChannelName );
		const auto     Iterator = this->Streamers.find( Hash );

		if ( Iterator != this->Streamers.end() ) return false;

		auto Name               = std::string( ChannelName );
		this->Streamers[ Hash ] = std::make_unique<StreamerData>( Name );

		return true;
	}

	void Riot::Connect( const std::span<Database::Streamer> Targets )
	{
		std::atomic<size_t> Pending = 0;
		std::promise<void>  Ready;

		for ( const auto& Target : Targets )
		{
			auto           Name    = std::string( Target.StreamerID );
			const uint32_t Hash    = Globals::FNV1a( Name );
			auto           Data    = std::make_unique<StreamerData>( Name );
			auto*          Pointer = Data.get();

			this->Streamers[ Hash ] = std::move( Data );

			Pointer->Accounts.reserve( Target.PUUIDs.size() );

			for ( const auto& PUUID : Target.PUUIDs )
			{
				++Pending;

				asio::co_spawn( Globals::IOC, [&Target, Pointer, &Pending, &Ready, PUUID = std::string( PUUID )]() -> asio::awaitable<void>
				{
					const auto Account = co_await RiotAccount::Create( Pointer, PUUID );
					if ( Account ) Pointer->Accounts.push_back( Account );

					if ( --Pending == 0 ) Ready.set_value();
				}, asio::detached );
			}
		}

		if ( Pending > 0 ) Ready.get_future().wait();

		asio::co_spawn( Globals::IOC, [this]() -> asio::awaitable<void>
		{
			auto Timer = asio::steady_timer{ Globals::IOC };

			while ( true )
			{
				for ( auto& Data : this->Streamers | std::views::values )
				{
					if ( Data ) co_await Data->Refresh();
				}

				Timer.expires_after( std::chrono::seconds( 15 ) );
				co_await Timer.async_wait( asio::use_awaitable );
			}
		}, asio::detached );
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
				PrintError( "Riot's DataDragon couldn't be reached; Status {}", Status );
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

			if ( Status != 200 )
			{
				PrintError( "Riot's DataDragon couldn't be reached; Status {}", Status );
				throw std::runtime_error( "Couldn't reach DataDragon" );
			}

			auto  J         = json::parse( Body );
			auto& Champions = J[ "data" ];

			ChampionMap.reserve( Champions.size() );

			for ( auto& [ _, Champion ] : Champions.items() )
			{
				const auto ID          = std::stoi( Champion[ "key" ].get<std::string>() );
				auto       DisplayName = Champion[ "name" ].get<std::string>();

				ChampionMap[ static_cast<uint32_t>( ID ) ] = std::move( DisplayName );
			}
		}

		PrintDebug( "DataDragon: Loaded {} champions", ChampionMap.size() );
	}

	Riot::Riot( std::string Key ) : API( std::move( Key ) ), SSLC( ssl::context::tlsv12_client )
	{
		SSLC.set_default_verify_paths();
	}
}
