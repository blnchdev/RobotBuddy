#include "Database.h"

#include "Components/Riot/Riot.h"
#include "Components/TUI/TUI.h"
#include "Globals/Globals.h"

#pragma comment(lib, "Secur32.lib")
#pragma comment(lib, "Wldap32.lib")

namespace Components
{
	Database::Database( const std::string& ConnectionString ) : Connection{ ConnectionString }
	{
		std::lock_guard Lock( Mutex );
		pqxx::work      Work{ Connection };

		Work.exec( R"(
            CREATE TABLE IF NOT EXISTS Streamers (
                StreamerID   SERIAL PRIMARY KEY,
                ChannelName  TEXT NOT NULL UNIQUE
            ))" );

		Work.exec( R"(
            CREATE TABLE IF NOT EXISTS Accounts (
                StreamerID  INT NOT NULL REFERENCES Streamers(StreamerID) ON DELETE CASCADE,
                AccountID   TEXT NOT NULL,
                PRIMARY KEY (StreamerID, AccountID)
            ))" );

		Work.exec( R"(
            CREATE TABLE IF NOT EXISTS Settings (
                SettingID     SERIAL PRIMARY KEY,
                KeyName       TEXT NOT NULL UNIQUE,
                DefaultValue  TEXT NOT NULL
            ))" );

		Work.exec( R"(
            CREATE TABLE IF NOT EXISTS StreamerSettings (
                StreamerID  INT NOT NULL REFERENCES Streamers(StreamerID) ON DELETE CASCADE,
                SettingID   INT NOT NULL REFERENCES Settings(SettingID) ON DELETE CASCADE,
                Value       TEXT NOT NULL,
                PRIMARY KEY (StreamerID, SettingID)
            ))" );

		Work.exec( R"(
			CREATE TABLE IF NOT EXISTS RankData (
				StreamerID	INT NOT NULL,
				AccountID	TEXT NOT NULL,
				RankID		INT NOT NULL,
                AverageGain REAL NOT NULL,
				AverageLoss REAL NOT NULL,
				WinCount	INT NOT NULL,
				LossCount	INT NOT NULL,
				PeakLP		INT NOT NULL,
				PRIMARY KEY (StreamerID, AccountID, RankID),
				FOREIGN KEY (StreamerID, AccountID) REFERENCES Accounts(StreamerID, AccountID) ON DELETE CASCADE
			))" );

		Work.exec( "CREATE INDEX IF NOT EXISTS IDXAccountsStreamer ON Accounts(StreamerID)" );
		Work.exec( "CREATE INDEX IF NOT EXISTS IDXStreamerSettingsStreamer ON StreamerSettings(StreamerID)" );

		Work.commit();
	}

	std::optional<int32_t> Database::AddStreamer( const std::string_view ChannelName ) const
	{
		std::lock_guard Lock( Mutex );
		pqxx::work      Work{ Connection };

		const auto Result = Work.exec( "INSERT INTO Streamers(ChannelName) VALUES ($1) ON CONFLICT DO NOTHING RETURNING StreamerID", pqxx::params{ ChannelName } );

		Work.commit();

		if ( Result.empty() ) return std::nullopt;
		return Result.front()[ "StreamerID" ].as<int32_t>();
	}

	bool Database::RemoveStreamer( const int32_t StreamerID ) const
	{
		std::lock_guard Lock( Mutex );
		pqxx::work      Work{ Connection };

		const auto Result = Work.exec( "DELETE FROM Streamers WHERE StreamerID = $1", pqxx::params{ StreamerID } );

		Work.commit();
		return Result.affected_rows() > 0;
	}

	int32_t Database::GetStreamerID( std::string_view ChannelName ) const
	{
		std::lock_guard Lock( Mutex );
		pqxx::work      Work{ Connection };

		const auto Result = Work.exec( "SELECT StreamerID FROM Streamers WHERE ChannelName = $1", pqxx::params( ChannelName ) );

		if ( Result.empty() ) return -1;

		return Result[ 0 ][ "StreamerID" ].as<int32_t>();
	}

	std::vector<Database::Streamer> Database::GetStreamers() const
	{
		std::lock_guard Lock( Mutex );
		pqxx::work      Work{ Connection };

		const auto Result = Work.exec( "SELECT StreamerID, ChannelName FROM Streamers" );

		std::vector<Streamer> Out;
		Out.reserve( Result.size() );

		for ( const auto& Row : Result )
		{
			Streamer S
			{
				.StreamerID = Row[ "StreamerID" ].as<int32_t>(),
				.ChannelName = Row[ "ChannelName" ].as<std::string>(),
			};

			PrintDebug( "Streamer: {} / {}", S.StreamerID, S.ChannelName );
			Out.emplace_back( PopulateStreamer( Work, std::move( S ) ) );
		}

		return Out;
	}

	bool Database::AddAccount( const int32_t StreamerID, const std::string& AccountID ) const
	{
		std::lock_guard Lock( Mutex );
		pqxx::work      Work{ Connection };

		const auto Result = Work.exec( "INSERT INTO Accounts(StreamerID, AccountID) VALUES ($1, $2) ON CONFLICT DO NOTHING", pqxx::params{ StreamerID, AccountID } );

		Work.commit();
		return Result.affected_rows() > 0;
	}

	bool Database::RemoveAccount( const int32_t StreamerID, const std::string& AccountID ) const
	{
		std::lock_guard Lock( Mutex );
		pqxx::work      Work{ Connection };

		const auto Result = Work.exec( "DELETE FROM Accounts WHERE StreamerID = $1 AND AccountID = $2", pqxx::params{ StreamerID, AccountID } );

		Work.commit();
		return Result.affected_rows() > 0;
	}

	std::optional<PersistentRankData> Database::GetRankData( int32_t StreamerID, std::string_view AccountID, const GameType Type ) const
	{
		std::lock_guard Lock( Mutex );
		pqxx::work      Work{ Connection };

		if ( Type != GameType::SOLOQ && Type != GameType::FLEX ) return std::nullopt;

		const int32_t RankID = Type == GameType::SOLOQ ? 1 : 2;

		const auto Result = Work.exec(
		                              "SELECT AverageGain, AverageLoss, WinCount, LossCount, PeakLP FROM RankData WHERE StreamerID = $1 AND AccountID = $2 AND RankID = $3",
		                              pqxx::params{ StreamerID, AccountID, RankID }
		                             );

		if ( Result.empty() ) return std::nullopt;

		const auto& Row = Result[ 0 ];

		return PersistentRankData
		{
			.AverageGain = Row[ "AverageGain" ].as<float>(),
			.AverageLoss = Row[ "AverageLoss" ].as<float>(),
			.WinCount = Row[ "WinCount" ].as<int32_t>(),
			.LossCount = Row[ "LossCount" ].as<int32_t>(),
			.PeakLP = Row[ "PeakLP" ].as<int16_t>(),
		};
	}

	void Database::PushRankData( int32_t StreamerID, std::string_view AccountID, const GameType Type, PersistentRankData& Data ) const
	{
		if ( Type != GameType::SOLOQ && Type != GameType::FLEX ) return;

		const int32_t RankID = Type == GameType::SOLOQ ? 1 : 2;

		std::lock_guard Lock( Mutex );
		pqxx::work      Work{ Connection };

		const auto Result = Work.exec(
		                              "INSERT INTO RankData (StreamerID, AccountID, RankID, AverageGain, AverageLoss, WinCount, LossCount, PeakLP)"
		                              " VALUES		($1, $2, $3, $4, $5, $6, $7, $8)"
		                              " ON CONFLICT (StreamerID, AccountID, RankID) DO UPDATE SET"
		                              " AverageGain = EXCLUDED.AverageGain,"
		                              " AverageLoss = EXCLUDED.AverageLoss,"
		                              " WinCount    = EXCLUDED.WinCount,"
		                              " LossCount   = EXCLUDED.LossCount,"
		                              " PeakLP      = EXCLUDED.PeakLP",
		                              pqxx::params{ StreamerID, AccountID, RankID, Data.AverageGain, Data.AverageLoss, Data.WinCount, Data.LossCount, Data.PeakLP }
		                             );

		Work.commit();
	}

	std::optional<int32_t> Database::AddSetting( const std::string_view KeyName, const std::string_view DefaultValue ) const
	{
		std::lock_guard Lock( Mutex );
		pqxx::work      Work{ Connection };

		const auto Result = Work.exec( "INSERT INTO Settings(KeyName, DefaultValue) VALUES ($1, $2) ON CONFLICT DO NOTHING RETURNING SettingID", pqxx::params{ KeyName, DefaultValue } );

		Work.commit();

		if ( Result.empty() ) return std::nullopt;
		return Result.front()[ "SettingID" ].as<int32_t>();
	}

	bool Database::RemoveSetting( const int32_t SettingID ) const
	{
		std::lock_guard Lock( Mutex );
		pqxx::work      Work{ Connection };

		const auto Result = Work.exec( "DELETE FROM Settings WHERE SettingID = $1", pqxx::params{ SettingID } );

		Work.commit();
		return Result.affected_rows() > 0;
	}

	template <typename T>
	T Database::GetSetting( int32_t StreamerID, SettingIDs SettingID, T DefaultValue )
	{
		std::lock_guard Lock( Mutex );
		pqxx::work      Work{ Connection };

		const auto Result = Work.exec( R"(
						SELECT Value
						FROM StreamerSettings
						WHERE StreamerID = $1 AND SettingID = $2)", pqxx::params{ StreamerID, static_cast<uint16_t>( SettingID ) } );

		if ( Result.empty() ) return DefaultValue;

		return Result[ 0 ][ 0 ].as<T>();
	}

	template <>
	bool Database::GetSetting( int32_t StreamerID, SettingIDs SettingID, const bool DefaultValue )
	{
		std::lock_guard Lock( Mutex );
		pqxx::work      Work{ Connection };

		const auto Result = Work.exec( R"(
						SELECT Value
						FROM StreamerSettings
						WHERE StreamerID = $1 AND SettingID = $2)", pqxx::params{ StreamerID, static_cast<uint16_t>( SettingID ) } );

		if ( Result.empty() ) return DefaultValue;

		return Result[ 0 ][ 0 ].as<std::string>() == "true";
	}

	template <>
	std::string Database::GetSetting( int32_t StreamerID, SettingIDs SettingID, const std::string DefaultValue )
	{
		std::lock_guard Lock( Mutex );
		pqxx::work      Work{ Connection };

		const auto Result = Work.exec( R"(
						SELECT Value
						FROM StreamerSettings
						WHERE StreamerID = $1 AND SettingID = $2)", pqxx::params{ StreamerID, static_cast<uint16_t>( SettingID ) } );

		if ( Result.empty() ) return DefaultValue;

		return Result[ 0 ][ 0 ].as<std::string>();
	}

	bool Database::SetStreamerSetting( const int32_t StreamerID, const int32_t SettingID, const std::string_view Value ) const
	{
		std::lock_guard Lock( Mutex );
		pqxx::work      Work{ Connection };

		const auto Result = Work.exec( R"(
                INSERT INTO StreamerSettings(StreamerID, SettingID, Value)
                VALUES ($1, $2, $3)
                ON CONFLICT (StreamerID, SettingID) DO UPDATE SET Value = EXCLUDED.Value
            )", pqxx::params{ StreamerID, SettingID, Value } );

		Work.commit();
		return Result.affected_rows() > 0;
	}

	bool Database::RemoveStreamerSetting( const int32_t StreamerID, const int32_t SettingID ) const
	{
		std::lock_guard Lock( Mutex );
		pqxx::work      Work{ Connection };

		const auto Result = Work.exec(
		                              "DELETE FROM StreamerSettings WHERE StreamerID = $1 AND SettingID = $2",
		                              pqxx::params{ StreamerID, SettingID }
		                             );

		Work.commit();
		return Result.affected_rows() > 0;
	}

	Database::Streamer Database::PopulateStreamer( pqxx::work& Work, Streamer S ) const
	{
		{
			const auto Result = Work.exec( "SELECT AccountID FROM Accounts WHERE StreamerID = $1", pqxx::params{ S.StreamerID } );

			S.Accounts.reserve( Result.size() );

			for ( const auto& Row : Result ) S.Accounts.emplace_back( Row[ "AccountID" ].as<std::string>() );
		}

		{
			const auto Result = Work.exec( "SELECT SettingID, Value FROM StreamerSettings WHERE StreamerID = $1", pqxx::params{ S.StreamerID } );

			for ( const auto& Row : Result )
			{
				int32_t SettingID = Row[ "SettingID" ].as<int32_t>();

				switch ( static_cast<SettingIDs>( SettingID ) )
				{
				case SettingIDs::IsJoinEnabled:
					S.Settings.IsJoinEnabled = Row[ "Value" ].as<std::string>() == "true";
					break;
				case SettingIDs::WinEmoji:
					S.Settings.WinEmoji = Row[ "Value" ].as<std::string>();
					break;
				case SettingIDs::LoseEmoji:
					S.Settings.LoseEmoji = Row[ "Value" ].as<std::string>();
					break;
				default:
					break;
				}
			}
		}

		return S;
	}

	template std::string Database::GetSetting( int32_t, SettingIDs, std::string );
	template int32_t     Database::GetSetting( int32_t, SettingIDs, int32_t );
}
