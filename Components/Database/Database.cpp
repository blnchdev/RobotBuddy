#include "Database.h"

namespace Components
{
	Database::Database( const std::string& Path ) : DB{ Path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE }
	{
		DB.exec( "PRAGMA foreign_keys = ON" );

		DB.exec( R"(
	        CREATE TABLE IF NOT EXISTS Streamers (
	            StreamerID      TEXT PRIMARY KEY,
	            IsJoinEnabled   INTEGER NOT NULL DEFAULT 1,
	            WinEmoji        TEXT NOT NULL DEFAULT '🟦',
	            LoseEmoji       TEXT NOT NULL DEFAULT '🟥'
	        )
	    )" );

		DB.exec( R"(
	        CREATE TABLE IF NOT EXISTS Accounts (
	            ID          INTEGER PRIMARY KEY AUTOINCREMENT,
	            StreamerID  TEXT NOT NULL REFERENCES Streamers(StreamerID) ON DELETE CASCADE,
	            PUUID       TEXT NOT NULL,
	            UNIQUE(StreamerID, PUUID)
	        )
	    )" );

		DB.exec( "CREATE INDEX IF NOT EXISTS IDXAccountsStreamer ON Accounts(StreamerID)" );
	}

	bool Database::AddStreamer( const std::string_view StreamerID ) const
	{
		SQLite::Statement Query{ DB, "INSERT OR IGNORE INTO Streamers(StreamerID) VALUES (?)" };
		Query.bind( 1, StreamerID.data() );
		return Query.exec() > 0;
	}

	bool Database::RemoveStreamer( const std::string_view StreamerID ) const
	{
		SQLite::Statement Query{ DB, "DELETE FROM Streamers WHERE StreamerID = ?" };
		Query.bind( 1, StreamerID.data() );
		return Query.exec() > 0;
	}

	std::optional<Database::Streamer> Database::GetStreamer( const std::string_view StreamerID ) const
	{
		SQLite::Statement Query{ DB, "SELECT StreamerID, IsJoinEnabled, WinEmoji, LoseEmoji FROM Streamers WHERE StreamerID = ?" };
		Query.bind( 1, StreamerID.data() );

		if ( !Query.executeStep() ) return std::nullopt;

		Streamer S
		{
			.StreamerID = Query.getColumn( 0 ).getText(),
			.IsJoinEnabled = Query.getColumn( 1 ).getInt() != 0,
			.WinEmoji = Query.getColumn( 2 ).getText(),
			.LoseEmoji = Query.getColumn( 3 ).getText(),
		};

		return PopulateAccounts( std::move( S ) );
	}

	std::vector<Database::Streamer> Database::GetStreamers() const
	{
		SQLite::Statement Query{ DB, "SELECT StreamerID, IsJoinEnabled, WinEmoji, LoseEmoji FROM Streamers" };

		std::vector<Streamer> Out;

		while ( Query.executeStep() )
		{
			Streamer S
			{
				.StreamerID = Query.getColumn( 0 ).getText(),
				.IsJoinEnabled = Query.getColumn( 1 ).getInt() != 0,
				.WinEmoji = Query.getColumn( 2 ).getText(),
				.LoseEmoji = Query.getColumn( 3 ).getText(),
			};

			Out.emplace_back( PopulateAccounts( std::move( S ) ) );
		}

		return Out;
	}

	bool Database::UpdateStreamer( const std::string_view StreamerID, const bool Join, const std::string_view WinEmoji, const std::string_view LoseEmoji ) const
	{
		SQLite::Statement Query
		{
			DB,
			"UPDATE Streamers SET IsJoinEnabled = ?, WinEmoji = ?, LoseEmoji = ? WHERE StreamerID = ?"
		};

		Query.bind( 1, Join );
		Query.bind( 2, WinEmoji.data() );
		Query.bind( 3, LoseEmoji.data() );
		Query.bind( 4, StreamerID.data() );

		return Query.exec() > 0;
	}

	bool Database::AddAccount( const std::string_view StreamerID, const std::string_view PUUID ) const
	{
		SQLite::Statement Query
		{
			DB,
			"INSERT OR IGNORE INTO Accounts(StreamerID, PUUID) VALUES (?, ?)"
		};

		Query.bind( 1, StreamerID.data() );
		Query.bind( 2, PUUID.data() );

		return Query.exec() > 0;
	}

	bool Database::RemoveAccount( const std::string_view StreamerID, const std::string_view PUUID ) const
	{
		SQLite::Statement Query
		{
			DB,
			"DELETE FROM Accounts WHERE StreamerID = ? AND PUUID = ?"
		};

		Query.bind( 1, StreamerID.data() );
		Query.bind( 2, PUUID.data() );

		return Query.exec() > 0;
	}

	Database::Streamer Database::PopulateAccounts( Streamer S ) const
	{
		SQLite::Statement Query{ DB, "SELECT PUUID FROM Accounts WHERE StreamerID = ?" };
		Query.bind( 1, S.StreamerID );

		while ( Query.executeStep() ) S.PUUIDs.emplace_back( Query.getColumn( 0 ).getText() );

		return S;
	}
}
