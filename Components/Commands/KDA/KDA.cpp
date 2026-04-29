#include "KDA.h"

#include "Globals/Globals.h"

namespace Components::Operation
{
	namespace
	{
		std::string GetResponse( const Command* Data )
		{
			const auto ActiveAccount = Globals::LeagueAPI->GetActiveAccount( Data->ChannelID );

			if ( !ActiveAccount ) return "No recorded activity today";

			auto Accumulate = [&] ( const Components::KDA& Accumulated, const GameSummary& Summary ) -> Components::KDA
			{
				return { Accumulated.Kills + Summary.KDA.Kills, Accumulated.Deaths + Summary.KDA.Deaths, Accumulated.Assists + Summary.KDA.Assists, 0.f };
			};

			const auto [ K, D, A, _ ] = std::accumulate( ActiveAccount->Summaries.begin(), ActiveAccount->Summaries.end(), Components::KDA{}, Accumulate );

			std::string Ratio = D > 0 ? std::format( "{:.2f}", static_cast<float>( K + A ) / static_cast<float>( D ) ) : "Perfect";

			return std::format( "{} currently has a KDA of {}/{}/{} ({}) over {} games", Data->ChannelID, K, D, A, Ratio, ActiveAccount->Summaries.size() );
		}
	}

	void KDA( const Command* Data )
	{
		const std::string Response = GetResponse( Data );

		Globals::TwitchAPI->ReplyTo( *Data->Context, Response );
	}
}
