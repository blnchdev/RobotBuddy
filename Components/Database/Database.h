#pragma once
#include <SQLiteCpp/SQLiteCpp.h>

#include <string>

namespace Components
{
	class Database
	{
	public:
		struct Streamer
		{
			std::string              StreamerID;
			std::vector<std::string> PUUIDs        = {};
			bool                     IsJoinEnabled = true;
			std::string              WinEmoji      = "🟦";
			std::string              LoseEmoji     = "🟥";
		};

		explicit Database( const std::string& Path );

		bool                    AddStreamer( std::string_view StreamerID ) const;
		bool                    RemoveStreamer( std::string_view StreamerID ) const;
		std::optional<Streamer> GetStreamer( std::string_view StreamerID ) const;
		std::vector<Streamer>   GetStreamers() const;
		bool                    UpdateStreamer( std::string_view StreamerID, bool Join, std::string_view WinEmoji, std::string_view LoseEmoji ) const;

		bool AddAccount( std::string_view StreamerID, std::string_view PUUID ) const;
		bool RemoveAccount( std::string_view StreamerID, std::string_view PUUID ) const;

	private:
		Streamer PopulateAccounts( Streamer S ) const;

		SQLite::Database DB;
	};
}
