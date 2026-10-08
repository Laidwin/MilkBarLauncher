#pragma once
#include <map>
#include <string>
#include "Compat.h"

namespace DTO
{

	class NamesDTO
	{
	public:
		std::map<byte, std::string> Names;
	};

}