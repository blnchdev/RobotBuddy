#include "Accounts.h"

#include "Components/TUI/TUI.h"
#include "Globals/Globals.h"

namespace Components::Operation
{
	namespace
	{
		std::string_view Trim( const std::string_view SV )
		{
			auto IsJunk = [] ( const unsigned char c )
			{
				return std::isspace( c ) || c < 0x20;
			};

			size_t Start = 0;
			while ( Start < SV.size() && IsJunk( SV[ Start ] ) ) ++Start;

			size_t End = SV.size();
			while ( End > Start && IsJunk( SV[ End - 1 ] ) ) --End;

			return SV.substr( Start, End - Start );
		}

		std::pair<std::string_view, std::string_view> ParseAccount( std::string_view Input )
		{
			const size_t Index = Input.find( '#' );
			if ( Index == std::string_view::npos ) return {};

			const std::string_view SummonerName = Trim( Input.substr( 0, Index ) );
			const std::string_view TagLine      = Trim( Input.substr( Index + 1 ) );

			return { SummonerName, TagLine };
		}

		size_t LengthUTF8( std::string_view String )
		{
			size_t Length = 0;

			auto Count = [&] ( const unsigned char c )
			{
				if ( ( c & 0xC0 ) != 0x80 )
				{
					Length++;
				}
			};

			std::ranges::for_each( String, Count );
			return Length;
		}

		asio::awaitable<std::string> Add( const Command* Data )
		{
			std::string Response;

			if ( Data->Argument2.empty() )
			{
				co_return "Missing username. Proper usage: !accounts add <username#tagline>. e.g.: !accounts add Azzapp#31415";
			}

			const auto [ SummonerName, TagLine ] = ParseAccount( Data->Argument2 );

			if ( SummonerName.empty() || TagLine.empty() || LengthUTF8( SummonerName ) > 16 || LengthUTF8( TagLine ) > 5 )
			{
				PrintDebug( "'{}' size({}) / '{}' size({})", SummonerName, LengthUTF8( SummonerName ), TagLine, LengthUTF8( TagLine ) );
				co_return "Malformed Summoner Name, proper usage is: !accounts add <username#tagline>. e.g.: !accounts add Azzapp#31415";
			}

			const auto PUUID = co_await Globals::LeagueAPI->GetPUUID( SummonerName, TagLine );

			if ( !PUUID.has_value() )
			{
				co_return "Could not find that summoner.";
			}

			const bool Status = co_await Globals::LeagueAPI->AddAccount( Data->ChannelName, PUUID.value() );

			if ( Status )
			{
				co_return "Added this account";
			}

			co_return "That account was already added!";
		}

		std::string List( const Command* Data )
		{
			const auto RiotData = Globals::LeagueAPI->GetData( Data->ChannelName );
			if ( !RiotData ) return "";
			std::string Response = "Accounts: ";

			if ( RiotData->Accounts.empty() )
			{
				return std::format( "{} has no accounts registered", Data->ChannelName );
			}

			for ( const auto&& [ Index, Account ] : RiotData->Accounts | std::views::enumerate )
			{
				if ( Index != 0 ) Response += " / ";
				Response += std::format( "{}#{} ({})", Account->Info.SummonerName, Account->Info.TagLine, Account->Info.Region );
			}

			return Response;
		}

		asio::awaitable<std::string> Remove( const Command* Data )
		{
			std::string Response;

			if ( Data->Argument2.empty() )
			{
				co_return "Missing username. Proper usage: !accounts remove <username#tagline>. e.g.: !accounts remove Azzapp#31415";
			}

			const auto [ SummonerName, TagLine ] = ParseAccount( Data->Argument2 );

			if ( SummonerName.empty() || TagLine.empty() || LengthUTF8( SummonerName ) > 16 || LengthUTF8( TagLine ) > 5 )
			{
				co_return "Malformed Summoner Name, proper usage is: !accounts remove <username#tagline>. e.g.: !accounts remove Azzapp#31415";
			}

			const bool Status = co_await Globals::LeagueAPI->RemoveAccount( Data->ChannelName, SummonerName, TagLine );

			if ( Status )
			{
				co_return "Removed this account";
			}

			co_return "Could not find this account, is it present if you type '!accounts list'?";
		}
	}

	asio::awaitable<void> Accounts( const Command* Data )
	{
		if ( !Data->Context.IsOwner && !Data->Context.IsModerator )
		{
			Globals::TwitchAPI->ReplyTo( Data->Context, "You do not have the permissions required to run this command" );
			co_return;
		}

		const auto SubCommand = Data->Argument1;

		std::string Response;

		if ( SubCommand == "add" )
		{
			Response = co_await Add( Data );
		}
		else if ( SubCommand == "remove" )
		{
			Response = co_await Remove( Data );
		}
		else if ( SubCommand == "list" )
		{
			Response = List( Data );
		}

		if ( !Response.empty() )
		{
			Globals::TwitchAPI->ReplyTo( Data->Context, Response );
		}

		co_return;
	}
}
