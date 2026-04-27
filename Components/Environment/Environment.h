#pragma once
#include <filesystem>
#include <unordered_map>

namespace Components
{
	class Environment
	{
	public:
		static void        Load( const std::filesystem::path& Path = ".env" );
		static std::string Get( const std::string& Key );
		static std::string Get( const std::string& Key, const std::string& Fallback );
	};
}
