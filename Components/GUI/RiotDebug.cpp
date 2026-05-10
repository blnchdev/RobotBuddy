#include "RiotDebug.h"

#include "Components/Riot/Riot.h"
#include "Globals/Globals.h"

namespace Debug
{
	namespace
	{
		const char* GameTypeStr( const Components::GameType T )
		{
			switch ( T )
			{
			case Components::GameType::UNRANKED:
				return "Unranked / Swift / Draft";
			case Components::GameType::SOLOQ:
				return "Solo/Duo";
			case Components::GameType::FLEX:
				return "Flex";
			case Components::GameType::ARAM:
				return "ARAM";
			case Components::GameType::ARENA:
				return "Arena";
			case Components::GameType::CLASH:
				return "Clash";
			case Components::GameType::OTHER:
				return "Other";
			default:
				break;
			}

			return "?";
		}

		const char* RoleStr( const Components::RoleEnum R )
		{
			switch ( R )
			{
			case Components::TOP:
				return "Top";
			case Components::JUNGLE:
				return "Jungle";
			case Components::MIDDLE:
				return "Mid";
			case Components::ADC:
				return "Bot / ADC";
			case Components::SUPPORT:
				return "Support";
			default:
				break;
			}

			return "None";
		}

		std::string FormatEpoch( const uint64_t Epoch )
		{
			if ( Epoch == 0 ) return "N/A";

			const std::time_t T = static_cast<std::time_t>( Epoch / 1000 );
			std::tm           TM{};
			( void )localtime_s( &TM, &T );
			char Buf[ 32 ];
			( void )std::strftime( Buf, sizeof( Buf ), "%Y-%m-%d %H:%M:%S", &TM );

			return Buf;
		}

		std::string FormatDuration( const uint64_t Seconds )
		{
			const uint64_t M = Seconds / 60;
			const uint64_t S = Seconds % 60;

			return std::format( "{}m {:02}s", M, S );
		}

		struct DebugState
		{
			int32_t                   SelectedStreamer = -1;
			int32_t                   SelectedAccount  = -1;
			int32_t                   SelectedGame     = -1;
			Components::StreamerData* pStreamer        = nullptr;
		};

		void DrawGameModeData( const char* Label, const Components::GameModeData& Data, int32_t& SelectedGame, const Components::GameType FilterType )
		{
			const bool HasRank  = Data.Rank.LastKnown != nullptr;
			const bool HasGames = !Data.Games.empty();

			if ( !HasRank && !HasGames ) return;

			if ( ImGui::CollapsingHeader( Label ) )
			{
				ImGui::Indent( 8.f );

				if ( HasRank )
				{
					ImGui::SeparatorText( "Rank" );

					if ( Data.Rank.IsUnranked )
					{
						ImGui::TextDisabled( "Unranked" );
					}
					else
					{
						if ( Data.Rank.SessionStart )
						{
							const Components::RankEntry& S = *Data.Rank.SessionStart;
							ImGui::Text( "Session start : %s", std::string( S.IsApex() ? std::format( "{} {} LP", Components::RankToDisplay( S.Rank ), S.LP ) : std::format( "{} {} {} LP", Components::RankToDisplay( S.Rank ), Components::ToRoman( S.Division ), S.LP ) ).c_str() );
						}

						const Components::RankEntry& L = *Data.Rank.LastKnown;
						ImGui::Text( "Last known    : %s", std::string( L.IsApex() ? std::format( "{} {} LP", Components::RankToDisplay( L.Rank ), L.LP ) : std::format( "{} {} {} LP", Components::RankToDisplay( L.Rank ), Components::ToRoman( L.Division ), L.LP ) ).c_str() );

						const int16_t Delta = Data.Rank.SessionDeltaLP;

						if ( Delta > 0 )
						{
							ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 80, 220, 100, 255 ) );
							ImGui::Text( "Session delta : +%d LP", Delta );
						}
						else if ( Delta < 0 )
						{
							ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 220, 80, 80, 255 ) );
							ImGui::Text( "Session delta : %d LP", Delta );
						}
						else
						{
							ImGui::PushStyleColor( ImGuiCol_Text, ImGui::GetStyleColorVec4( ImGuiCol_TextDisabled ) );
							ImGui::Text( "Session delta : 0 LP" );
						}

						ImGui::PopStyleColor();
					}

					if ( Data.Rank.PersistentData )
					{
						const auto& P = *Data.Rank.PersistentData;
						ImGui::Spacing();
						ImGui::Text( "Wins / Losses   : %d / %d", P.WinCount, P.LossCount );
						const int32_t Total = P.WinCount + P.LossCount;

						if ( Total > 0 )
						{
							const float WR = 100.f * static_cast<float>( P.WinCount ) / static_cast<float>( Total );
							ImGui::Text( "Win-rate        : %.1f%%", WR );
							ImGui::ProgressBar( WR / 100.f, ImVec2( -1.f, 0.f ) );
						}

						ImGui::Text( "Avg gain / loss : +%.1f LP / %.1f LP", P.AverageGain, P.AverageLoss );
						ImGui::Text( "Peak LP         : %d", P.PeakLP );
					}
				}

				if ( HasGames )
				{
					ImGui::SeparatorText( std::format( "Games ({})", Data.Games.size() ).c_str() );

					if ( ImGui::BeginTable( std::format( "##games_{}", Label ).c_str(), 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit, ImVec2( 0.f, 200.f ) ) )
					{
						ImGui::TableSetupScrollFreeze( 0, 1 );
						ImGui::TableSetupColumn( "#", ImGuiTableColumnFlags_WidthFixed, 30.f );
						ImGui::TableSetupColumn( "Champion", ImGuiTableColumnFlags_WidthFixed, 100.f );
						ImGui::TableSetupColumn( "Role", ImGuiTableColumnFlags_WidthFixed, 70.f );
						ImGui::TableSetupColumn( "W/L", ImGuiTableColumnFlags_WidthFixed, 30.f );
						ImGui::TableSetupColumn( "KDA", ImGuiTableColumnFlags_WidthFixed, 100.f );
						ImGui::TableSetupColumn( "Duration", ImGuiTableColumnFlags_WidthFixed, 80.f );
						ImGui::TableSetupColumn( "Delta LP", ImGuiTableColumnFlags_WidthStretch );
						ImGui::TableHeadersRow();

						for ( auto i{ 0ull }; i < Data.Games.size(); ++i )
						{
							const auto& G = Data.Games[ i ];
							ImGui::TableNextRow();

							ImGui::TableSetColumnIndex( 0 );
							const bool        Selected = std::cmp_equal( SelectedGame, i ) && G.Type == FilterType;
							const std::string RowID    = std::format( "{}##{}", i, Label );

							if ( ImGui::Selectable( RowID.c_str(), Selected, ImGuiSelectableFlags_SpanAllColumns ) )
							{
								SelectedGame = Selected ? -1 : static_cast<int32_t>( i );
							}

							ImGui::TableSetColumnIndex( 1 );
							ImGui::TextUnformatted( G.Champion.c_str() );

							ImGui::TableSetColumnIndex( 2 );
							ImGui::TextDisabled( "%s", RoleStr( G.Role ) );

							ImGui::TableSetColumnIndex( 3 );

							if ( G.Win )
							{
								ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 80, 210, 100, 255 ) );
								ImGui::TextUnformatted( "W" );
							}
							else
							{
								ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 220, 80, 80, 255 ) );
								ImGui::TextUnformatted( "L" );
							}

							ImGui::PopStyleColor();

							ImGui::TableSetColumnIndex( 4 );
							if ( G.KDA.IsPerfect() ) ImGui::Text( "%d/%d/%d (Perfect)", G.KDA.Kills, G.KDA.Deaths, G.KDA.Assists );
							else ImGui::Text( "%d/%d/%d (%.2f)", G.KDA.Kills, G.KDA.Deaths, G.KDA.Assists, G.KDA.Ratio );

							ImGui::TableSetColumnIndex( 5 );
							ImGui::TextUnformatted( FormatDuration( G.Duration ).c_str() );

							ImGui::TableSetColumnIndex( 6 );
							if ( G.WasRanked() )
							{
								if ( G.DeltaLP > 0 )
								{
									ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 80, 210, 100, 255 ) );
									ImGui::Text( "+%d", G.DeltaLP );
								}
								else
								{
									ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 220, 80, 80, 255 ) );
									ImGui::Text( "%d", G.DeltaLP );
								}

								ImGui::PopStyleColor();
							}
							else
							{
								ImGui::TextDisabled( "-" );
							}
						}

						ImGui::EndTable();
					}
				}

				ImGui::Unindent( 8.f );
			}
		}

		void DrawGameSummary( const Components::GameSummary& G )
		{
			ImGui::SeparatorText( std::format( "Game #{}", G.GameID ).c_str() );

			if ( ImGui::BeginTable( "##gamesummary", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit ) )
			{
				auto Row = [] ( const char* K, const std::string& V )
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex( 0 );
					ImGui::TextDisabled( "%s", K );
					ImGui::TableSetColumnIndex( 1 );
					ImGui::TextUnformatted( V.c_str() );
				};

				Row( "Game ID", std::format( "{}", G.GameID ) );
				Row( "Champion", G.Champion );
				Row( "Role", RoleStr( G.Role ) );
				Row( "Queue", GameTypeStr( G.Type ) );
				Row( "Result", G.Win ? "Victory" : "Defeat" );
				Row( "Duration", FormatDuration( G.Duration ) );
				Row( "Ended at", FormatEpoch( G.GameEnd ) );
				Row( "KDA", G.KDA.IsPerfect() ? std::format( "{}/{}/{} - Perfect", G.KDA.Kills, G.KDA.Deaths, G.KDA.Assists ) : std::format( "{}/{}/{} - {:.2f}", G.KDA.Kills, G.KDA.Deaths, G.KDA.Assists, G.KDA.Ratio ) );
				Row( "CS", std::format( "{}", G.CreepScore ) );
				Row( "Vision", std::format( "{:.1f}", G.VisionScore ) );

				if ( G.WasRanked() ) Row( "LP delta", G.DeltaLP >= 0 ? std::format( "+{} LP", G.DeltaLP ) : std::format( "{} LP", G.DeltaLP ) );

				ImGui::EndTable();
			}
		}

		void DrawActiveGame( const Components::ActiveGame& AG )
		{
			ImGui::SeparatorText( "Active Game" );

			ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 255, 200, 50, 255 ) );
			ImGui::Bullet();
			ImGui::SameLine();
			ImGui::Text( "Currently in-game!" );
			ImGui::PopStyleColor();

			ImGui::Text( "Champion  : %s", AG.Champion.c_str() );
			ImGui::Text( "Queue     : %s", GameTypeStr( AG.Type ) );

			const auto& AE = AG.AverageElo;
			ImGui::Text( "Average elo   : %s", std::string( AE.IsApex() ? std::format( "{} {} LP", Components::RankToDisplay( AE.Rank ), AE.LP ) : std::format( "{} {} {} LP", Components::RankToDisplay( AE.Rank ), Components::ToRoman( AE.Division ), AE.LP ) ).c_str() );
		}

		DebugState State = {};
	}

	void Draw()
	{
		const ImGuiID              DockspaceID = ImGui::GetID( "RiotDebugDockspace" );
		constexpr ImGuiWindowFlags HostFlags   =
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoDocking;

		const ImGuiViewport* VP = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos( VP->WorkPos );
		ImGui::SetNextWindowSize( VP->WorkSize );
		ImGui::SetNextWindowViewport( VP->ID );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 0.f );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.f );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 0.f, 0.f ) );
		ImGui::Begin( "##DebugHost", nullptr, HostFlags );
		ImGui::PopStyleVar( 3 );

		ImGui::DockSpace( DockspaceID, ImVec2( 0.f, 0.f ), ImGuiDockNodeFlags_None );

		static bool LayoutBuilt = false;

		if ( !LayoutBuilt )
		{
			LayoutBuilt = true;
			ImGui::DockBuilderRemoveNode( DockspaceID );
			ImGui::DockBuilderAddNode( DockspaceID, ImGuiDockNodeFlags_DockSpace );
			ImGui::DockBuilderSetNodeSize( DockspaceID, VP->WorkSize );

			ImGuiID Left, Center, Right;

			ImGui::DockBuilderSplitNode( DockspaceID, ImGuiDir_Left, 0.20f, &Left, &Center );

			ImGui::DockBuilderSplitNode( Center, ImGuiDir_Left, 0.43f, &Center, &Right );

			ImGuiID CenterTop, CenterBottom;
			ImGui::DockBuilderSplitNode( Center, ImGuiDir_Up, 0.45f, &CenterTop, &CenterBottom );

			ImGui::DockBuilderDockWindow( "Streamers##dbg", Left );
			ImGui::DockBuilderDockWindow( "Riot Accounts##dbg", CenterTop );
			ImGui::DockBuilderDockWindow( "Game Summary##dbg", CenterBottom );
			ImGui::DockBuilderDockWindow( "Account Detail##dbg", Right );

			ImGui::DockBuilderFinish( DockspaceID );
		}

		ImGui::End();

		if ( ImGui::Begin( "Streamers##dbg" ) )
		{
			ImGui::SeparatorText( "Tracked Streamers" );

			const auto& Map   = Globals::LeagueAPI->GetStreamers();
			int32_t     Index = 0;

			for ( const auto& [ Name, Data ] : Map )
			{
				const bool Sel = State.SelectedStreamer == Index;
				if ( ImGui::Selectable( Name.c_str(), Sel ) )
				{
					State.SelectedStreamer = Sel ? -1 : Index;
					State.SelectedAccount  = -1;
					State.SelectedGame     = -1;
					State.pStreamer        = Data.get();
				}
				++Index;
			}
		}

		ImGui::End();

		const Components::StreamerData* SelectedStreamer = State.pStreamer;

		if ( ImGui::Begin( "Riot Accounts##dbg" ) )
		{
			if ( SelectedStreamer == nullptr )
			{
				ImGui::TextDisabled( "No streamer selected." );
			}
			else
			{
				ImGui::Text( "Channel : %s  (id %d)", SelectedStreamer->Channel.c_str(), SelectedStreamer->ID );
				ImGui::Separator();

				for ( auto i{ 0ull }; i < SelectedStreamer->Accounts.size(); ++i )
				{
					const auto& Acc = SelectedStreamer->Accounts[ i ];
					if ( !Acc ) continue;

					const std::string Label = std::format( "{}#{} [{}]##acc{}", Acc->Info.SummonerName, Acc->Info.TagLine, Acc->Info.Region, i );

					const bool Sel = std::cmp_equal( State.SelectedAccount, i );
					if ( ImGui::Selectable( Label.c_str(), Sel ) )
					{
						State.SelectedAccount = Sel ? -1 : static_cast<int32_t>( i );
						State.SelectedGame    = -1;
					}

					if ( Acc->CurrentGame )
					{
						ImGui::SameLine();
						ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 255, 200, 50, 255 ) );
						ImGui::TextUnformatted( "[LIVE]" );
						ImGui::PopStyleColor();
					}

					if ( !Acc->Valid )
					{
						ImGui::SameLine();
						ImGui::PushStyleColor( ImGuiCol_Text, IM_COL32( 200, 80, 80, 255 ) );
						ImGui::TextUnformatted( "[INVALID]" );
						ImGui::PopStyleColor();
					}
				}
			}
		}

		ImGui::End();

		Components::RiotAccount* SelectedAccount = nullptr;

		if ( SelectedStreamer && State.SelectedAccount >= 0 && std::cmp_less_equal( State.SelectedAccount, SelectedStreamer->Accounts.size() ) )
		{
			SelectedAccount = SelectedStreamer->Accounts[ State.SelectedAccount ].get();
		}

		if ( ImGui::Begin( "Account Detail##dbg" ) )
		{
			if ( SelectedAccount == nullptr )
			{
				ImGui::TextDisabled( "No account selected." );
			}
			else
			{
				ImGui::Text( "%s#%s", SelectedAccount->Info.SummonerName.c_str(), SelectedAccount->Info.TagLine.c_str() );
				ImGui::SameLine();
				ImGui::TextDisabled( "PUUID: %s", SelectedAccount->Info.PUUID.c_str() );

				ImGui::Text( "Region : %s", SelectedAccount->Info.Region.c_str() );
				ImGui::Text( "Valid  : %s  |  Populated : %s", SelectedAccount->Valid ? "yes" : "no", SelectedAccount->WasPopulated ? "yes" : "no" );
				ImGui::Text( "Last mode played : %s", GameTypeStr( SelectedAccount->LastGameModePlayed ) );

				if ( SelectedAccount->CurrentGame ) DrawActiveGame( *SelectedAccount->CurrentGame );

				ImGui::Spacing();

				DrawGameModeData( "Solo/Duo", SelectedAccount->SoloQ, State.SelectedGame, Components::GameType::SOLOQ );
				DrawGameModeData( "Flex", SelectedAccount->FlexQ, State.SelectedGame, Components::GameType::FLEX );
				DrawGameModeData( "Normal", SelectedAccount->Normal, State.SelectedGame, Components::GameType::UNRANKED );
				DrawGameModeData( "ARAM", SelectedAccount->ARAM, State.SelectedGame, Components::GameType::ARAM );
				DrawGameModeData( "Arena", SelectedAccount->Arena, State.SelectedGame, Components::GameType::ARENA );
				DrawGameModeData( "Other", SelectedAccount->Other, State.SelectedGame, Components::GameType::OTHER );
			}
		}

		ImGui::End();

		if ( ImGui::Begin( "Game Summary##dbg" ) )
		{
			if ( SelectedAccount == nullptr || State.SelectedGame < 0 )
			{
				ImGui::TextDisabled( "No game selected." );
			}
			else
			{
				const Components::GameModeData* ActiveMode = SelectedAccount->GetData( SelectedAccount->LastGameModePlayed );

				if ( ActiveMode && std::cmp_less( State.SelectedGame, ActiveMode->Games.size() ) )
				{
					DrawGameSummary( ActiveMode->Games[ State.SelectedGame ] );
				}
				else
				{
					ImGui::TextDisabled( "Game index out of range for last played mode." );
					ImGui::TextDisabled( "Consider storing (GameType, index) in DebugState." );
				}
			}
		}

		ImGui::End();
	}
}
