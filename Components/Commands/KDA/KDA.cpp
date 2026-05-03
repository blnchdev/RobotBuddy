#include "KDA.h"

#include "Globals/Globals.h"

namespace Components::Operation
{
	namespace
	{
		std::string GetResponse( const Command* Data )
		{
			const auto ActiveAccount = Globals::LeagueAPI->GetActiveAccount( Data->ChannelName );

			if ( !ActiveAccount ) return "No recorded activity today";

			auto Accumulate = [&] ( const Components::KDA& Accumulated, const GameSummary& Summary ) -> Components::KDA
			{
				return Components::KDA( Accumulated.Kills + Summary.KDA.Kills, Accumulated.Deaths + Summary.KDA.Deaths, Accumulated.Assists + Summary.KDA.Assists );
			};

			const auto GameData = ActiveAccount->GetData( ActiveAccount->LastGameModePlayed );

			const auto [ K, D, A, _ ] = std::accumulate( GameData->Games.begin(), GameData->Games.end(), Components::KDA( 0, 0, 0 ), Accumulate );

			std::string Ratio = D > 0 ? std::format( "{:.2f}", static_cast<float>( K + A ) / static_cast<float>( D ) ) : "Perfect";

			return std::format( "{} currently has a KDA of {}/{}/{} ({}) over {} games", Data->ChannelName, K, D, A, Ratio, GameData->Games.size() );
		}
	}

	asio::awaitable<void> KDA( const Command* Data )
	{
		const std::string Response = GetResponse( Data );

		Globals::TwitchAPI->ReplyTo( Data->Context, Response );
		co_return;
	}
}
