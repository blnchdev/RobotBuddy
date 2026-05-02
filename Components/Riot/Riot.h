#pragma once
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/url.hpp>

#include <nlohmann/json.hpp>

#include "Components/Database/Database.h"

namespace Components
{
	namespace asio = boost::asio;
	namespace beast = boost::beast;
	namespace http = beast::http;
	namespace ssl = asio::ssl;
	using tcp  = asio::ip::tcp;
	using json = nlohmann::json;

	struct RankEntry
	{
		std::string Rank;     // e.g. 'Diamond'
		std::string Division; // e.g. 'IV'
		int32_t     LP;
		bool        IsApex = false;
	};

	struct KDA
	{
		int32_t Kills, Deaths, Assists;
		float   Ratio;
	};

	enum GameType
	{
		UNRANKED = 0, // Both Swift Play & Draft
		SOLOQ    = 1,
		FLEX     = 2,
		ARAM     = 3, // Both regular and mayhem
		ARENA    = 4,
		CLASH    = 5,
		ROTATING = 6
	};

	struct _GameSummary
	{
		uint64_t GameID;
		int32_t  ChampionID;
		GameType Type;

		bool Win;
		KDA  KDA;

		uint64_t Duration;
		uint64_t GameEnd;

		int32_t CreepScore;
		double  VisionScore;

		int32_t DeltaLP = 0;
	};

	struct GameSummary
	{
		uint64_t    GameID;
		std::string Champion;
		std::string Role;

		bool     Win;
		KDA      KDA;
		uint64_t Duration; // Seconds
		uint64_t GameEnd;  // Unix
		int32_t  CreepScore;
		double   VisionScore;

		std::optional<int32_t> DeltaLP = std::nullopt;
	};

	struct ActiveGame
	{
		std::string PUUID;
		std::string Champion;
		std::string AverageElo;
	};

	struct RiotAccount
	{
		std::string              SummonerName;
		std::string              TagLine;
		std::string              PUUID;
		std::string              Region;
		bool                     Valid         = false;
		std::vector<GameSummary> Summaries     = {};
		std::optional<RankEntry> LastKnownRank = std::nullopt;

		// TODO: Rework this, async the creation (std::future std::optional RiotAccount "Create" method, ctor should just be = default)
		explicit                                           RiotAccount() = default;
		static asio::awaitable<std::optional<RiotAccount>> Create( std::string PUUID );
	};

	struct RiotData
	{
		std::string TwitchChannel;

		std::string              ActivePUUID = {};
		std::vector<RiotAccount> Accounts    = {};

		RiotData() = default;
		explicit RiotData( std::string Channel );
	};

	class Riot
	{
	public:
		explicit Riot( std::string Key );

		struct Response
		{
			uint32_t    Status;
			std::string Body;
		};

		void                                        InitializeDataDragon();
		void                                        Connect( const std::vector<Database::Streamer>& Streamers );
		asio::awaitable<Response>                   GET( std::string_view Host, std::string_view Target, bool NeedsAPI = true );
		Response                                    GETSync( std::string_view Host, std::string_view Target, bool NeedsAPI = true );
		asio::awaitable<Response>                   GET( std::string_view Target );
		RiotAccount*                                GetActiveAccount( std::string_view StreamerID );
		asio::awaitable<std::optional<ActiveGame>>  GetCurrentGame( std::string_view StreamerID );
		asio::awaitable<std::optional<RankEntry>>   GetLeagueRank( const RiotAccount& Account );
		asio::awaitable<std::optional<std::string>> GetLeagueRankFormatted( std::string_view StreamerID );
		asio::awaitable<std::optional<std::string>> GetPUUID( std::string_view SummonerName, std::string_view TagLine );
		RiotData&                                   GetData( std::string_view StreamerID );
		asio::awaitable<bool>                       AddAccount( std::string_view StreamerID, std::string_view PUUID );
		asio::awaitable<bool>                       RemoveAccount( std::string_view StreamerID, std::string_view SummonerName, std::string_view TagLine );

	private:
		std::unordered_map<std::string, RiotData> Data = {};
		std::jthread                              WorkerThread;
		std::stop_source                          StopSource;
		std::string                               API;
		asio::io_context                          IOC;
		ssl::context                              SSLC;
	};

	int32_t   RankToLP( const RankEntry& Entry );
	RankEntry LPToRank( int32_t LP );
}
