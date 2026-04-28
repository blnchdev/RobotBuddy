#include "TwitchBot.h"

#include "Components/TUI/TUI.h"

namespace Components
{
	namespace
	{
		void StripCRLF( std::string_view& Line )
		{
			while ( !Line.empty() && ( Line.back() == '\n' || Line.back() == '\r' ) ) Line.remove_suffix( 1 );
		}

		std::optional<TwitchMessage> ParseMessage( std::string_view Line )
		{
			if ( Line.empty() ) return std::nullopt;

			StripCRLF( Line );

			std::string_view Tags;

			if ( Line.starts_with( '@' ) )
			{
				const size_t TagsEnd = Line.find( ' ' );
				if ( TagsEnd == std::string_view::npos ) return std::nullopt;

				Tags = Line.substr( 1, TagsEnd - 1 );
				Line = Line.substr( TagsEnd + 1 );
			}

			const size_t PrivMessage = Line.find( " PRIVMSG " );
			if ( PrivMessage == std::string_view::npos ) return std::nullopt;

			const std::string_view Left  = Line.substr( 0, PrivMessage );
			std::string_view       Right = Line.substr( PrivMessage + 9 );

			const size_t Space = Right.find( ' ' );
			if ( Space == std::string_view::npos ) return std::nullopt;

			const auto Channel = std::string( Right.substr( 0, Space ) );

			const size_t MessageStart = Right.find( " :" );
			if ( MessageStart == std::string_view::npos ) return std::nullopt;

			const auto Message = std::string( Right.substr( MessageStart + 2 ) );

			std::string_view Prefix = Left;
			if ( !Prefix.starts_with( ':' ) ) return std::nullopt;
			Prefix = Prefix.substr( 1 );

			const size_t Bang = Prefix.find( '!' );
			if ( Bang == std::string_view::npos ) return std::nullopt;

			const auto Sender = std::string( Prefix.substr( 0, Bang ) );

			const auto GetTag = [&Tags] ( const std::string_view Key )
			{
				size_t Pos = 0;
				while ( Pos < Tags.size() )
				{
					const size_t           Semi = Tags.find( ';', Pos );
					const std::string_view Pair = Tags.substr( Pos, Semi == std::string_view::npos ? Semi : Semi - Pos );

					const size_t Equal = Pair.find( '=' );
					if ( Equal != std::string_view::npos && Pair.substr( 0, Equal ) == Key ) return std::string( Pair.substr( Equal + 1 ) );

					if ( Semi == std::string_view::npos ) break;
					Pos = Semi + 1;
				}

				return std::string();
			};

			const auto DisplayName = GetTag( "display-name" );
			const auto Badges      = GetTag( "badges" );
			const bool IsStreamer  = Badges.find( "broadcaster/" ) != std::string_view::npos;

			return TwitchMessage
			{
				.ID = GetTag( "id" ),
				.Username = DisplayName.empty() ? Sender : DisplayName,
				.Channel = Channel,
				.Text = Message,
				.IsModerator = GetTag( "mod" ) == "1",
				.IsSubscriber = GetTag( "subscriber" ) == "1",
				.IsOwner = IsStreamer,
			};
		}
	}

	TwitchBot::TwitchBot( TokenManager& Tokens, const std::string_view Nick ) : Tokens( Tokens ), Nick( Nick ), SSLC( ssl::context::tlsv12_client ), WebSocket( IOC, SSLC )
	{
		SSLC.set_default_verify_paths();
	}

	void TwitchBot::Connect()
	{
		tcp::resolver Resolver{ IOC };
		const auto    Results = Resolver.resolve( "irc-ws.chat.twitch.tv", "443" );
		asio::connect( beast::get_lowest_layer( WebSocket ), Results );

		if ( !SSL_set_tlsext_host_name( WebSocket.next_layer().native_handle(), "irc-ws.chat.twitch.tv" ) ) throw beast::system_error( { static_cast<int>( ::ERR_get_error() ), asio::error::get_ssl_category() } );

		WebSocket.next_layer().handshake( ssl::stream_base::client );
		WebSocket.handshake( "irc-ws.chat.twitch.tv", "/" );
		PrintOk( "IRC: Connected" );
	}

	void TwitchBot::Login( const std::span<const std::string_view> ChannelsToJoin )
	{
		SendRaw( "PASS oauth:" + Tokens.GetAccessToken() );
		SendRaw( "NICK " + Nick );
		SendRaw( "CAP REQ :twitch.tv/tags twitch.tv/commands" );

		for ( const auto Channel : ChannelsToJoin )
		{
			std::string Lower{ Channel };
			std::ranges::transform( Lower, Lower.begin(), tolower );
			Channels.insert( Lower );
			PrintDebug( "IRC: Joined {}", Channel );
			SendRaw( "JOIN #" + Lower );
		}

		PrintOk( "IRC: Logged in as {}, joined {} channel(s)", Nick, Channels.size() );
	}

	void TwitchBot::Join( const std::string_view Channel )
	{
		std::string Lower{ Channel };
		std::ranges::transform( Lower, Lower.begin(), ::tolower );
		if ( Channels.insert( Lower ).second ) SendRaw( "JOIN #" + Lower );
	}

	void TwitchBot::Part( const std::string_view Channel )
	{
		std::string Lower{ Channel };
		std::ranges::transform( Lower, Lower.begin(), ::tolower );
		if ( Channels.erase( Lower ) ) SendRaw( "PART #" + Lower );
	}

	void TwitchBot::Run( const MessageHandler& OnMessage )
	{
		beast::flat_buffer Buffer;

		while ( true )
		{
			WebSocket.read( Buffer );
			auto Raw = beast::buffers_to_string( Buffer.data() );
			Buffer.clear();

			std::istringstream Stream( Raw );
			std::string        Line;

			while ( std::getline( Stream, Line ) )
			{
				if ( Line.starts_with( "PING" ) )
				{
					SendRaw( "PONG :tmi.twitch.tv" );
					continue;
				}

				if ( auto Message = ParseMessage( Line ); Message.has_value() )
				{
					OnMessage( Message.value() );
				}
			}
		}
	}

	void TwitchBot::SendChat( const std::string_view Channel, const std::string_view Message )
	{
		SendRaw( "PRIVMSG #" + std::string( Channel ) + " : " + std::string( Message ) );
	}

	void TwitchBot::ReplyTo( const TwitchMessage& Parent, const std::string_view Message )
	{
		SendRaw( "@reply-parent-msg-id=" + Parent.ID + " PRIVMSG " + Parent.Channel + " :" + std::string( Message ) );
	}

	void TwitchBot::SendRaw( const std::string& Line )
	{
		WebSocket.write( asio::buffer( Line + "\r\n" ) );
	}
}
