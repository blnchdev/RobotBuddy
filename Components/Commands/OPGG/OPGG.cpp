#include "OPGG.h"

#include "Globals/Globals.h"

namespace Components::Operation
{
	namespace
	{
		std::string_view GetServerDisplayName( const std::string_view Region )
		{
			using namespace std::string_view_literals;

			if ( Region == "br1" ) return "br"sv;
			if ( Region == "eun1" ) return "eune"sv;
			if ( Region == "euw1" ) return "euw"sv;
			if ( Region == "la1" ) return "lan"sv;
			if ( Region == "la2" ) return "las"sv;
			if ( Region == "na1" ) return "na"sv;
			if ( Region == "sg2" ) return "sea"sv;
			if ( Region == "oc1" ) return "oce"sv;
			if ( Region == "ru" ) return "ru"sv;
			if ( Region == "tr1" ) return "tr"sv;
			if ( Region == "me1" ) return "me"sv;
			if ( Region == "jp1" ) return "jp"sv;
			if ( Region == "kr" ) return "kr"sv;
			if ( Region == "tw2" ) return "tw"sv;
			if ( Region == "vn2" ) return "vn"sv;

			std::unreachable();
		}

		std::string GetResponse( std::string_view ChannelID )
		{
			const auto ActiveAccount = Globals::LeagueAPI->GetActiveAccount( ChannelID );

			if ( !ActiveAccount )
			{
				return std::format( "No account is linked to {}", ChannelID );
			}

			return std::format( "https://op.gg/lol/summoners/{}/{}-{}", GetServerDisplayName( ActiveAccount->Info.Region ), ActiveAccount->Info.SummonerName, ActiveAccount->Info.TagLine );
		}
	}

	asio::awaitable<void> OPGG( const Command* Data )
	{
		const std::string Response = GetResponse( Data->ChannelName );

		Globals::TwitchAPI->ReplyTo( Data->Context, Response );
		co_return;
	}
}
