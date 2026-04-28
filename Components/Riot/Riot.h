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

	struct GameSummary
	{
		uint64_t    GameID;
		std::string Champion;
		std::string Role;

		bool     Win;
		int32_t  Kills, Deaths, Assists;
		float    KDA;
		uint64_t Duration; // Seconds
		uint64_t GameEnd;  // Unix
		int32_t  CreepScore;
		double   VisionScore;
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
		bool                     Valid     = false;
		std::vector<GameSummary> Summaries = {};

		explicit RiotAccount( std::string PUUID );
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

		void                       InitializeDataDragon();
		void                       Connect( const std::vector<Database::Streamer>& Streamers );
		Response                   GET( std::string_view Host, std::string_view Target, bool NeedsAPI = true );
		Response                   GET( std::string_view Target );
		std::optional<RiotAccount> GetActiveAccount( std::string_view StreamerID );
		std::optional<ActiveGame>  GetCurrentGame( std::string_view StreamerID );
		std::optional<std::string> GetLeagueRank( std::string_view StreamerID );
		std::optional<std::string> GetPUUID( std::string_view SummonerName, std::string_view TagLine );
		RiotData&                  GetData( std::string_view StreamerID );
		bool                       AddAccount( std::string_view StreamerID, std::string_view PUUID );
		bool                       RemoveAccount( std::string_view StreamerID, std::string_view SummonerName, std::string_view TagLine );

	private:
		std::unordered_map<std::string, RiotData> Data = {};
		std::jthread                              WorkerThread;
		std::stop_source                          StopSource;
		std::string                               API;
		asio::io_context                          IOC;
		ssl::context                              SSLC;
	};
}
