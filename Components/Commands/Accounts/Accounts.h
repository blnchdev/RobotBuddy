#pragma once
#include "Components/Commands/Commands.h"

namespace Components::Operation
{
	asio::awaitable<void> Accounts( const Command* Data );
}
