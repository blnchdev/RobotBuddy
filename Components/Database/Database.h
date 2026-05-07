#pragma once
#include <pqxx/pqxx>

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <boost/asio/awaitable.hpp>

namespace Components
{
	// Riot Types

	struct RiotAccount;
	struct PersistentRankData;
	enum class GameType : uint8_t;
	struct GameSummary;

	enum class SettingIDs : uint8_t
	{
		IsJoinEnabled = 1,
		WinEmoji      = 2,
		LoseEmoji     = 3
	};

	struct StreamerSettings
	{
		bool        IsJoinEnabled = true;
		std::string WinEmoji      = "🟦";
		std::string LoseEmoji     = "🟥";
	} inline Default;

	class Database
	{
	public:
		struct Streamer
		{
			int32_t                  StreamerID;
			std::string              ChannelName;
			std::vector<std::string> Accounts;

			StreamerSettings Settings;
		};

		std::optional<int32_t> AddStreamer( std::string_view ChannelName ) const;
		bool                   RemoveStreamer( int32_t StreamerID ) const;

		std::vector<Streamer> GetStreamers() const;

		bool AddAccount( int32_t StreamerID, std::string_view AccountID ) const;
		bool RemoveAccount( int32_t StreamerID, std::string_view AccountID ) const;

		std::optional<PersistentRankData> GetRankData( int32_t StreamerID, std::string_view AccountID, GameType Type ) const;
		boost::asio::awaitable<void>      PushRankData( int32_t StreamerID, std::string_view AccountID, GameType Type, PersistentRankData& Data ) const;

		std::optional<int32_t> AddSetting( std::string_view KeyName, std::string_view DefaultValue ) const;
		bool                   RemoveSetting( int32_t SettingID ) const;

		// GetSetting declarations
		template <typename T>
		T GetSetting( int32_t StreamerID, SettingIDs SettingID, T DefaultValue );
		template <>
		bool GetSetting( int32_t StreamerID, SettingIDs SettingID, bool DefaultValue );

		bool SetStreamerSetting( int32_t StreamerID, int32_t SettingID, std::string_view Value ) const;
		bool RemoveStreamerSetting( int32_t StreamerID, int32_t SettingID ) const;

		explicit Database( const std::string& ConnectionString );

	private:
		Streamer PopulateStreamer( pqxx::work& Txn, Streamer S ) const;

		mutable pqxx::connection Connection;
	};
}
