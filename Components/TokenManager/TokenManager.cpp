#include "TokenManager.h"

#include "Components/TUI/TUI.h"

namespace Components
{
	TokenManager::TokenManager( std::string ClientID, std::string ClientSecret, std::string AccessToken, std::string RefreshToken ) : ClientID( std::move( ClientID ) ), ClientSecret( std::move( ClientSecret ) ),
	                                                                                                                                  AccessToken( std::move( AccessToken ) ), RefreshToken( std::move( RefreshToken ) ),
	                                                                                                                                  SSLC( ssl::context::tlsv12_client )
	{
		SSLC.set_default_verify_paths();
		ExpiresAt = std::chrono::steady_clock::now();
	}

	const std::string& TokenManager::GetAccessToken()
	{
		if ( NeedsRefresh() ) Refresh();
		return AccessToken;
	}

	const std::string& TokenManager::GetClientID()
	{
		return ClientID;
	}

	bool TokenManager::NeedsRefresh() const
	{
		return std::chrono::steady_clock::now() >= ExpiresAt - 60s;
	}

	void TokenManager::Refresh()
	{
		PrintTrace( "TokenManager: Refreshing..." );
		asio::io_context         IOC;
		ssl::stream<tcp::socket> Stream( IOC, SSLC );

		tcp::resolver Resolver( IOC );
		const auto    Results = Resolver.resolve( "id.twitch.tv", "443" );
		asio::connect( Stream.lowest_layer(), Results );

		Stream.handshake( ssl::stream_base::client );

		std::string Body = "grant_type=refresh_token"
		                   "&refresh_token=" + RefreshToken +
		                   "&client_id=" + ClientID +
		                   "&client_secret=" + ClientSecret;

		http::request<http::string_body> Request{ http::verb::post, "/oauth2/token", 11 };
		Request.set( http::field::host, "id.twitch.tv" );
		Request.set( http::field::content_type, "application/x-www-form-urlencoded" );
		Request.content_length( Body.size() );
		Request.body() = Body;
		Request.prepare_payload();

		http::write( Stream, Request );

		beast::flat_buffer                Buffer;
		http::response<http::string_body> Response;
		http::read( Stream, Buffer, Response );

		auto J = json::parse( Response.body() );

		if ( !J.contains( "access_token" ) )
		{
			PrintError( "No Access Token: {}", Response.body() );
			throw std::runtime_error( "Token Refresh Failed: " + Response.body() );
		}

		J[ "access_token" ].get_to( AccessToken );
		J[ "refresh_token" ].get_to( RefreshToken );

		auto ExpiresIn = J.value( "expires_in", 3600 );
		ExpiresAt      = std::chrono::steady_clock::now() + std::chrono::seconds( ExpiresIn );

		PrintNote( "TokenManager: Refreshed successfully, expires in {} seconds.", ExpiresIn );
	}

	void TokenManager::PersistTokens() const
	{
		std::ifstream In( ".env" ); // TODO: Custom path
		std::string   Content( ( std::istreambuf_iterator( In ) ), std::istreambuf_iterator<char>() );
		In.close();

		auto ReplaceLine = [&] ( const std::string& Key, const std::string& Value )
		{
			const auto Position = Content.find( Key + '=' );
			if ( Position == std::string::npos ) return;
			const auto End = Content.find( '\n', Position );
			Content.replace( Position, End - Position, Key + "=" + Value );
		};

		ReplaceLine( "TWITCH_ACCESS_TOKEN", AccessToken );
		ReplaceLine( "TWITCH_REFRESH_TOKEN", RefreshToken );

		std::ofstream Out( ".env" ); // TODO: Custom path
		Out << Content;
	}
}
