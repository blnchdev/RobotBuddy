#include "Environment.h"

#include <fstream>

namespace Components
{
	namespace
	{
		std::unordered_map<std::string, std::string> Stored = {};
	}

	void Environment::Load( const std::filesystem::path& Path )
	{
		std::ifstream File( Path );

		if ( !File.is_open() ) throw std::runtime_error( "Could not open .env file at " + Path.string() );

		std::string Line;

		while ( std::getline( File, Line ) )
		{
			std::string_view View{ Line };

			if ( View.empty() || View.starts_with( '#' ) ) continue;

			const auto Delimiter = View.find( '=' );
			if ( Delimiter == std::string_view::npos ) continue;

			auto Key   = View.substr( 0, Delimiter );
			auto Value = View.substr( Delimiter + 1 );

			auto Trim = [] ( std::string_view& String )
			{
				const auto First = String.find_first_not_of( " \t" );
				const auto Last  = String.find_last_not_of( " \t" );

				if ( First == std::string_view::npos )
				{
					String = {};
				}
				else
				{
					String = String.substr( First, Last - First + 1 );
				}
			};

			Trim( Key );
			Trim( Value );

			if ( const auto Comment = Value.find( " #" ); Comment != std::string_view::npos )
			{
				Value = Value.substr( 0, Comment );
				Trim( Value );
			}

			if ( Value.size() >= 2 && ( ( Value.front() == '"' && Value.back() == '"' ) || ( Value.front() == '\'' && Value.back() == '\'' ) ) )
			{
				Value.remove_prefix( 1 );
				Value.remove_suffix( 1 );
			}

			Stored[ std::string( Key ) ] = std::string( Value );
		}
	}

	std::string Environment::Get( const std::string& Key )
	{
		if ( const auto Iterator = Stored.find( Key ); Iterator != Stored.end() )
		{
			return Iterator->second;
		}

		throw std::runtime_error( "Environment variable not found: " + std::string( Key ) );
	}

	std::string Environment::Get( const std::string& Key, const std::string& Fallback )
	{
		if ( const auto Iterator = Stored.find( Key ); Iterator != Stored.end() )
		{
			return Iterator->second;
		}

		return Fallback;
	}
}
