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
				return { .Kills = Accumulated.Kills + Summary.KDA.Kills, .Deaths = Accumulated.Deaths + Summary.KDA.Deaths, .Assists = Accumulated.Assists + Summary.KDA.Assists, .Ratio = 0.f };
			};

			const auto [ K, D, A, _ ] = std::accumulate( ActiveAccount->Summaries.begin(), ActiveAccount->Summaries.end(), Components::KDA{}, Accumulate );

			std::string Ratio = D > 0 ? std::format( "{:.2f}", static_cast<float>( K + A ) / static_cast<float>( D ) ) : "Perfect";

			return std::format( "{} currently has a KDA of {}/{}/{} ({}) over {} games", Data->ChannelName, K, D, A, Ratio, ActiveAccount->Summaries.size() );
		}
	}

	asio::awaitable<void> KDA( const Command* Data )
	{
		const std::string Response = GetResponse( Data );

		Globals::TwitchAPI->ReplyTo( Data->Context, Response );
		co_return;
	}
}
