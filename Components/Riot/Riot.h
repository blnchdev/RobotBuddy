#pragma once
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/url.hpp>

#include <nlohmann/json.hpp>

#include "Components/Database/Database.h"
#include "Components/TUI/TUI.h"

namespace Components
{
	namespace asio = boost::asio;
	namespace beast = boost::beast;
	namespace http = beast::http;
	namespace ssl = asio::ssl;
	using namespace boost::urls;
	using tcp  = asio::ip::tcp;
	using json = nlohmann::json;

	enum class GameType : uint8_t
	{
		UNRANKED = 0, // Both Swift Play & Draft
		SOLOQ    = 1,
		FLEX     = 2,
		ARAM     = 3, // Both regular and mayhem
		ARENA    = 4,
		CLASH    = 5,
		OTHER    = 6,

		_SIZE = 7,
		NONE  = 100
	};

	enum RoleEnum : uint8_t
	{
		TOP     = 0,
		JUNGLE  = 1,
		MIDDLE  = 2,
		ADC     = 3,
		SUPPORT = 4,
		NONE    = 5
	};

	enum RankEnum : uint8_t
	{
		Iron        = 0,
		Bronze      = 1,
		Silver      = 2,
		Gold        = 3,
		Platinum    = 4,
		Emerald     = 5,
		Diamond     = 6,
		Master      = 7,
		Grandmaster = 8,
		Challenger  = 9,

		_SIZE = 10
	};

	inline std::string_view ToRoman( const uint8_t Value )
	{
		static std::array<std::string_view, 5> Romans =
		{
			"0",
			"I",
			"II",
			"III",
			"IV"
		};

		return Romans[ Value ];
	}

	// One of my eureka moments I can't lie
	inline uint8_t RomanToByte( const std::string_view String )
	{
		if ( String == "IV" ) return 4;
		return static_cast<uint8_t>( String.size() );
	}

	inline std::string_view RankToDisplay( const RankEnum Enum )
	{
		static std::array<std::string_view, static_cast<size_t>( _SIZE )> Ranks =
		{
			"Iron",
			"Bronze",
			"Silver",
			"Gold",
			"Platinum",
			"Emerald",
			"Diamond",
			"Master",
			"Grandmaster",
			"Challenger"
		};

		return Ranks[ std::to_underlying( Enum ) ];
	}

	inline RankEnum DisplayToRank( const std::string_view Display )
	{
		if ( Display.length() < 2 ) return Iron;

		const char C = Display[ 0 ];

		switch ( C )
		{
		case 'I':
			return Iron;
		case 'B':
			return Bronze;
		case 'S':
			return Silver;
		case 'G':
			return Display[ 1 ] == 'O' ? Gold : Grandmaster;
		case 'P':
			return Platinum;
		case 'E':
			return Emerald;
		case 'D':
			return Diamond;
		case 'M':
			return Master;
		case 'C':
			return Challenger;
		default:
			break;
		}

		std::unreachable();
	}

	inline RoleEnum DisplayToRole( const std::string_view Display )
	{
		if ( Display.empty() ) return NONE;

		const char C = Display[ 0 ];

		switch ( C )
		{
		case 'T':
			return TOP;
		case 'J':
			return JUNGLE;
		case 'M':
			return MIDDLE;
		case 'B': // BOTTOM
			return ADC;
		case 'U': // UTILITY
			return SUPPORT;
		default:
			return NONE;
		}
	}

	inline GameType QueueIDToType( const uint32_t QueueID )
	{
		switch ( QueueID )
		{
		case 420:
			return GameType::SOLOQ;
		case 440:
			return GameType::FLEX;
		case 450:
		case 2400:
			return GameType::ARAM;
		case 700:
		case 720:
			return GameType::CLASH;
		case 1710:
			return GameType::ARENA;
		default:
			break;
		}

		return GameType::UNRANKED;
	}

	struct KDA
	{
		uint16_t Kills, Deaths, Assists;
		float    Ratio;

		[[nodiscard]] bool IsPerfect() const { return Ratio > 10'000.f; }

		explicit KDA( const uint16_t K, const uint16_t D, const uint16_t A ) : Kills( K ), Deaths( D ), Assists( A )
		{
			if ( D == 0 )
			{
				Ratio = FLT_MAX;
			}
			else
			{
				Ratio = static_cast<float>( K + A ) / static_cast<float>( D );
			}
		}
	};

	struct RankEntry
	{
		int16_t  LP       = 0;
		uint8_t  Division = 4;
		RankEnum Rank     = Iron;

		[[nodiscard]] bool             IsApex() const { return Rank >= Master; }
		[[nodiscard]] std::string_view Formatted() { return this->RankDisplay; }

		void RefreshData()
		{
			if ( this->IsApex() )
			{
				this->RankDisplay = std::format( "{} {} LP", RankToDisplay( Rank ), LP );
			}
			else
			{
				this->RankDisplay = std::format( "{} {} {} LP", RankToDisplay( Rank ), ToRoman( Division ), LP );
			}
		}

		RankEntry() = default;

		explicit RankEntry( const RankEnum Rank, const uint8_t Division, const int16_t LP ) : LP( LP ), Division( Division ), Rank( Rank )
		{
			RefreshData();
		}

	private:
		std::string RankDisplay;
	};

	inline int16_t RankToLP( const RankEntry& Rank )
	{
		static std::array<int16_t, static_cast<size_t>( _SIZE )> Ranks =
		{
			0,
			400,
			800,
			1200,
			1600,
			2000,
			2400,
			2800,
			2800,
			2800
		};

		static std::array<int16_t, 5> Divisions =
		{
			0,
			300, // I
			200, // II
			100, // III
			0    // IV
		};

		if ( Rank.Rank >= Master ) { return Ranks[ std::to_underlying( Rank.Rank ) ] + Rank.LP; }

		return static_cast<int16_t>( Ranks[ std::to_underlying( Rank.Rank ) ] + Divisions[ Rank.Division ] + Rank.LP );
	}

	inline RankEntry LPToRank( const int16_t LP )
	{
		static std::array<int16_t, static_cast<size_t>( _SIZE )> Ranks =
		{
			0,
			400,
			800,
			1200,
			1600,
			2000,
			2400,
			2800,
			2800,
			2800
		};

		static std::array<int16_t, 5> Divisions =
		{
			0,
			300,
			200,
			100,
			0
		};

		if ( LP > 2800 ) return RankEntry( Master, 1, LP - 2800 );

		RankEnum Rank = Iron;
		for ( auto i = static_cast<int64_t>( Diamond ); i >= 0LL; --i )
		{
			if ( LP >= Ranks[ i ] )
			{
				Rank = static_cast<RankEnum>( i );
				break;
			}
		}

		const int32_t Remainder = LP - Ranks[ std::to_underlying( Rank ) ];
		uint8_t       Division  = 4;

		for ( auto i{ 1ull }; i < Divisions.size(); ++i )
		{
			if ( Remainder >= Divisions[ i ] )
			{
				Division = static_cast<uint8_t>( i );
				break;
			}
		}

		const int16_t RemainingLP = Remainder - Divisions[ Division ];

		return RankEntry( Rank, Division, RemainingLP );
	}

	struct PersistentRankData
	{
		float   AverageGain, AverageLoss;
		int32_t WinCount,    LossCount;
		int16_t PeakLP;
	};

	struct GameSummary
	{
		uint64_t    GameID;
		std::string Champion;
		RoleEnum    Role = NONE;
		GameType    Type;

		bool Win;
		KDA  KDA;

		uint64_t Duration;
		uint64_t GameEnd;

		int32_t CreepScore;
		double  VisionScore;

		int32_t DeltaLP = 0;

		[[nodiscard]] bool WasRanked() const { return Type == GameType::SOLOQ || Type == GameType::FLEX; }
	};

	struct RiotAccount; // Forward...
	struct StreamerData;

	struct GameModeData
	{
		struct
		{
			std::unique_ptr<RankEntry> SessionStart   = nullptr;
			std::unique_ptr<RankEntry> LastKnown      = nullptr;
			int16_t                    SessionDeltaLP = 0;
			bool                       IsUnranked     = false;

			std::unique_ptr<PersistentRankData> PersistentData = {};
		} Rank;

		RiotAccount*             Owner = nullptr;
		GameType                 Type;
		std::vector<GameSummary> Games = {};

		// Returns DeltaLP
		int16_t Push( const RankEntry& NewRank );

		void Populate( const RankEntry& StartRank )
		{
			this->Rank.SessionStart = std::make_unique<RankEntry>( StartRank.Rank, StartRank.Division, StartRank.LP );
			this->Rank.LastKnown    = std::make_unique<RankEntry>( StartRank.Rank, StartRank.Division, StartRank.LP );
		}

		GameModeData() = default;

		explicit GameModeData( RiotAccount* Owner, const GameType Type ) : Owner( Owner ), Type( Type )
		{
		}
	};

	struct ActiveGame
	{
		RiotAccount* Player;
		std::string  Champion;
		RankEntry    AverageElo;
		uint32_t     QueueID;
		GameType     Type;
	};

	struct RiotAccount
	{
		struct
		{
			std::string SummonerName;
			std::string TagLine;
			std::string PUUID;
			std::string Region;

			StreamerData* Owner;
		} Info;

		// Ranked Modes
		GameModeData SoloQ = GameModeData( this, GameType::SOLOQ );
		GameModeData FlexQ = GameModeData( this, GameType::FLEX );

		// Unranked Modes
		GameModeData Normal = GameModeData( this, GameType::UNRANKED );
		GameModeData ARAM   = GameModeData( this, GameType::ARAM );
		GameModeData Arena  = GameModeData( this, GameType::ARENA );
		GameModeData Other  = GameModeData( this, GameType::OTHER );

		GameType                    LastGameModePlayed = GameType::SOLOQ;
		std::unique_ptr<ActiveGame> CurrentGame        = nullptr;

		bool Valid        = false;
		bool WasPopulated = false;

		GameModeData* GetData( const GameType Type )
		{
			switch ( Type )
			{
			case GameType::UNRANKED:
				return &this->Normal;
			case GameType::SOLOQ:
				return &this->SoloQ;
			case GameType::FLEX:
				return &this->FlexQ;
			case GameType::ARAM:
				return &this->ARAM;
			case GameType::ARENA:
				return &this->Arena;
			case GameType::CLASH:
				return &this->Other;
			case GameType::OTHER:
				return &this->Other;
			default:
				return nullptr;
			}
		}

		asio::awaitable<void> Populate();
		asio::awaitable<void> Refresh();

		explicit                                             RiotAccount() = default;
		static asio::awaitable<std::shared_ptr<RiotAccount>> Create( StreamerData* Owner, std::string PUUID );
	};

	struct StreamerData
	{
		std::string Channel;
		int32_t     ID;

		std::shared_ptr<RiotAccount>              Active   = nullptr;
		std::vector<std::shared_ptr<RiotAccount>> Accounts = {};

		asio::awaitable<void> Refresh();

		StreamerData() = default;
		explicit StreamerData( std::string Channel );
	};

	class RateLimiter
	{
	public:
		RateLimiter( asio::io_context& IOC, const size_t Capacity, const std::chrono::steady_clock::duration RefillInterval ) : Timer( IOC ), Capacity( Capacity ), Tokens( Capacity ), RefillInterval( RefillInterval )
		{
		}

		asio::awaitable<void> Acquire()
		{
			while ( Tokens == 0 )
			{
				Timer.expires_after( RefillInterval );
				co_await Timer.async_wait( asio::use_awaitable );
				Tokens = Capacity;
			}

			--Tokens;
		}

	private:
		asio::steady_timer                  Timer;
		size_t                              Capacity;
		size_t                              Tokens;
		std::chrono::steady_clock::duration RefillInterval;
	};

	class Riot
	{
	public:
		struct Response
		{
			uint32_t    Status;
			std::string Body;
		};

		// Internal Use Data
		asio::awaitable<std::optional<std::string>> GetPUUID( std::string_view SummonerName, std::string_view TagLine );
		StreamerData*                               GetData( std::string_view ChannelName );

		// Visible Player Data
		asio::awaitable<std::optional<RankEntry>>  GetLeagueRank( const RiotAccount& Account, GameType Type = GameType::SOLOQ );
		asio::awaitable<std::optional<ActiveGame>> GetCurrentGame( std::string_view ChannelName );
		asio::awaitable<std::optional<ActiveGame>> GetCurrentGame( RiotAccount* Account );
		RiotAccount*                               GetActiveAccount( std::string_view ChannelName );

		// Raw requests
		Response                  GETSync( std::string_view Host, std::string_view Target, bool NeedsAPI = true );
		asio::awaitable<Response> GET( std::string_view Host, std::string_view Target, bool NeedsAPI = true );
		asio::awaitable<Response> GET( std::string_view Target );

		// Account Management
		asio::awaitable<bool> AddAccount( std::string_view ChannelName, std::string_view PUUID );
		asio::awaitable<bool> RemoveAccount( std::string_view ChannelName, std::string_view SummonerName, std::string_view TagLine );

		// Streamer Management
		bool AddStreamer( std::string_view ChannelName );
		bool RemoveStreamer( std::string_view ChannelName );

		// Initialization
		void Connect( std::span<Database::Streamer> Targets );
		void InitializeDataDragon();

		struct view_hash
		{
			using is_transparent = void;

			size_t operator()( const std::string_view sv ) const noexcept
			{
				return std::hash<std::string_view>{}( sv );
			}

			size_t operator()( const std::string& s ) const noexcept
			{
				return std::hash<std::string>{}( s );
			}
		};

		std::unordered_map<std::string, std::unique_ptr<StreamerData>, view_hash, std::equal_to<>>& GetStreamers() { return Streamers; }

		explicit Riot( std::string Key );

	private:
		std::unordered_map<std::string, std::unique_ptr<StreamerData>, view_hash, std::equal_to<>> Streamers = {};
		std::string                                                                                API;
		ssl::context                                                                               SSLC;
	};
}
