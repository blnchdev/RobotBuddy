#pragma once
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/http.hpp>
#include <nlohmann/json.hpp>

#include <chrono>
#include <fstream>
#include <string>

namespace Components
{
	// Classic boost moment
	namespace asio = boost::asio;
	namespace beast = boost::beast;
	namespace http = beast::http;
	namespace ssl = asio::ssl;
	using tcp  = asio::ip::tcp;
	using json = nlohmann::json;

	using namespace std::chrono_literals;

	class TokenManager
	{
	public:
		TokenManager( std::string ClientID, std::string ClientSecret, std::string AccessToken, std::string RefreshToken );

		const std::string& GetAccessToken();
		const std::string& GetClientID();

	private:
		bool NeedsRefresh() const;
		void Refresh();
		void PersistTokens() const;

		std::string                           ClientID;
		std::string                           ClientSecret;
		std::string                           AccessToken;
		std::string                           RefreshToken;
		ssl::context                          SSLC;
		std::chrono::steady_clock::time_point ExpiresAt;
	};
}
