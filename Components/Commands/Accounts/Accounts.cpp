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
				return std::isspace( c ) || c > 0x7E || c < 0x20;
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

		std::string Add( const Command* Data )
		{
			std::string Response;

			if ( Data->Argument2.empty() )
			{
				return "Missing username. Proper usage: !accounts add <username#tagline>. e.g.: !accounts add Azzapp#31415";
			}

			const auto [ SummonerName, TagLine ] = ParseAccount( Data->Argument2 );

			if ( SummonerName.empty() || TagLine.empty() || SummonerName.size() > 16 || TagLine.size() > 5 )
			{
				PrintDebug( "'{}' / '{}'", SummonerName, TagLine );
				return "Malformed Summoner Name, proper usage is: !accounts add <username#tagline>. e.g.: !accounts add Azzapp#31415";
			}

			const auto PUUID = Globals::LeagueAPI->GetPUUID( SummonerName, TagLine );

			if ( !PUUID.has_value() )
			{
				return "Could not find that summoner.";
			}

			const bool Status = Globals::LeagueAPI->AddAccount( Data->ChannelID, PUUID.value() );

			if ( Status )
			{
				return "Added this account";
			}

			return "That account was already added!";
		}

		std::string List( const Command* Data )
		{
			const auto  RiotData = Globals::LeagueAPI->GetData( Data->ChannelID );
			std::string Response = "Accounts: ";

			if ( RiotData.Accounts.empty() )
			{
				return std::format( "{} has no accounts registered", Data->ChannelID );
			}

			for ( const auto&& [ Index, Account ] : RiotData.Accounts | std::views::enumerate )
			{
				if ( Index != 0 ) Response += " / ";
				Response += std::format( "{}#{} ({})", Account.SummonerName, Account.TagLine, Account.Region );
			}

			return Response;
		}

		std::string Remove( const Command* Data )
		{
			std::string Response;

			if ( Data->Argument2.empty() )
			{
				return "Missing username. Proper usage: !accounts remove <username#tagline>. e.g.: !accounts remove Azzapp#31415";
			}

			const auto [ SummonerName, TagLine ] = ParseAccount( Data->Argument2 );

			if ( SummonerName.empty() || TagLine.empty() || SummonerName.size() > 16 || TagLine.size() > 5 )
			{
				PrintDebug( "'{}' / '{}'", SummonerName, TagLine );
				return "Malformed Summoner Name, proper usage is: !accounts remove <username#tagline>. e.g.: !accounts remove Azzapp#31415";
			}

			const bool Status = Globals::LeagueAPI->RemoveAccount( Data->ChannelID, SummonerName, TagLine );

			if ( Status )
			{
				return "Removed this account";
			}

			return "Could not find this account, is it present if you type '!accounts list'?";
		}
	}

	void Accounts( const Command* Data )
	{
		const auto SubCommand = Data->Argument1;

		std::string Response;

		if ( SubCommand == "add" )
		{
			Response = Add( Data );
		}
		else if ( SubCommand == "remove" )
		{
			Response = Remove( Data );
		}
		else if ( SubCommand == "list" )
		{
			Response = List( Data );
		}

		if ( !Response.empty() )
		{
			Globals::TwitchAPI->ReplyTo( *Data->Context, Response );
		}
	}
}
