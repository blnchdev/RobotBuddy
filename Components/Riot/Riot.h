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
		case 'C':
			return Challenger;
		default:
			break;
		}

		return Iron;
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
			300,
			200,
			100,
			0
		};

		return static_cast<int16_t>( Ranks[ std::to_underlying( Rank.Rank ) ] + Divisions[ Rank.Division ] + Rank.LP );
	}

	inline RankEntry LPToRank( const int16_t LP )
	{
		static constexpr std::array<int16_t, 8> RankBases =
		{
			0,
			400,
			800,
			1200,
			1600,
			2000,
			2400,
			2800
		};

		static constexpr std::array<int16_t, 4> DivisionBases =
		{
			0,
			100,
			200,
			300
		};

		if ( LP >= 2800 ) return RankEntry( Master, 0, static_cast<int16_t>( LP - 2800 ) );

		int16_t  RankBase = 0;
		RankEnum Rank     = Iron;

		for ( const auto [ Index, Base ] : RankBases | std::views::reverse | std::views::enumerate )
		{
			if ( LP >= Base )
			{
				Rank     = static_cast<RankEnum>( RankBases.size() - 1 - Index );
				RankBase = Base;
				break;
			}
		}

		int16_t Remainder = LP - RankBase;
		int8_t  Division  = 4;

		for ( const auto [ Index, Base ] : DivisionBases | std::views::reverse | std::views::enumerate )
		{
			if ( Remainder >= Base )
			{
				Division = static_cast<int8_t>( Index + 1 );
				Remainder -= Base;
				break;
			}
		}

		return RankEntry( Rank, Division, Remainder );
	}

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

	struct GameModeData
	{
		struct
		{
			std::unique_ptr<RankEntry> SessionStart = nullptr;
			std::unique_ptr<RankEntry> LastKnown    = nullptr;
			int16_t                    TotalDeltaLP = 0;
		} Rank;

		GameType                 Type;
		std::vector<GameSummary> Games = {};

		// Returns DeltaLP
		int16_t Push( const RankEntry& NewRank )
		{
			if ( !Rank.SessionStart || !Rank.LastKnown )
			{
				this->Populate( NewRank );
				return 0;
			}

			const int16_t OldLP = RankToLP( *Rank.LastKnown );
			const int16_t NewLP = RankToLP( NewRank );
			const int16_t Delta = static_cast<int16_t>( NewLP ) - OldLP;

			Rank.TotalDeltaLP += static_cast<int16_t>( Delta );

			Rank.LastKnown->Rank     = NewRank.Rank;
			Rank.LastKnown->Division = NewRank.Division;
			Rank.LastKnown->LP       = NewRank.LP;
			Rank.LastKnown->RefreshData();

			return Delta;
		}

		void Populate( const RankEntry& StartRank )
		{
			this->Rank.SessionStart = std::make_unique<RankEntry>( StartRank.Rank, StartRank.Division, StartRank.LP );
			this->Rank.LastKnown    = std::make_unique<RankEntry>( StartRank.Rank, StartRank.Division, StartRank.LP );
		}

		GameModeData() = default;

		explicit GameModeData( const GameType Type ) : Type( Type )
		{
		}
	};

	struct RiotAccount; // Forward...
	struct StreamerData;

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
		GameModeData SoloQ = GameModeData( GameType::SOLOQ );
		GameModeData FlexQ = GameModeData( GameType::FLEX );

		// Unranked Modes
		GameModeData Normal = GameModeData( GameType::UNRANKED );
		GameModeData ARAM   = GameModeData( GameType::ARAM );
		GameModeData Arena  = GameModeData( GameType::ARENA );
		GameModeData Other  = GameModeData( GameType::OTHER );

		GameType                    LastGameModePlayed = GameType::SOLOQ;
		std::unique_ptr<ActiveGame> CurrentGame        = nullptr;

		bool Valid        = false;
		bool WasPopulated = false;

		// Please just... don't mind this okay, let's pretend this is not here.
		// Yes I could just &SoloQ, &Normal instead of offsetof(...), but where is the fun in that
		GameModeData* GetData( const GameType Type )
		{
			static std::array Offsets =
			{
				offsetof( RiotAccount, Normal ),
				offsetof( RiotAccount, SoloQ ),
				offsetof( RiotAccount, FlexQ ),
				offsetof( RiotAccount, ARAM ),
				offsetof( RiotAccount, Arena ),
				offsetof( RiotAccount, Other ),
				offsetof( RiotAccount, Other )
			};

			return reinterpret_cast<GameModeData*>( reinterpret_cast<uintptr_t>( this ) + Offsets[ std::to_underlying( Type ) ] );
		}

		asio::awaitable<void> Populate();
		asio::awaitable<void> Refresh();

		explicit                                             RiotAccount() = default;
		static asio::awaitable<std::shared_ptr<RiotAccount>> Create( StreamerData* Owner, std::string PUUID );
	};

	struct StreamerData
	{
		std::string Channel;

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

		explicit Riot( std::string Key );

	private:
		std::unordered_map<uint32_t, std::unique_ptr<StreamerData>> Streamers = {};
		std::string                                                 API;
		ssl::context                                                SSLC;
	};
}
