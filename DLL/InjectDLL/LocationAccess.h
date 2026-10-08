#pragma once
#include "Memory.h"
#include "Platform.h"
#include "CharacterLocation.h"

namespace DataTypes
{
	class LocationAccess
	{
	public:
		CharacterLocation LastKnown;

		LocationAccess()
		{
			std::vector<int> signature = { 0x03, -1, 0xB5, 0xCC, 0x00, 0x00, 0x00, 0x00, 0x10, 0x54, 0xF6, 0x44, 0x10, 0x2B, 0x7E, 0x78, 0x00, 0x00, 0x00 };

			Memory::ScanOffset = 0xe000000;

			uint64_t addr = Memory::PatternScan(signature, Memory::getBaseAddress(), 8, Memory::ScanOffset);
			
			int retries = 0;

			while (addr < 30000)
			{
				Sleep(2000);

				if (retries == 15)
				{
					Logging::LoggerService::LogError("Could not find location address. Closing...");
					throw L"Failed to find mod address. Make sure that the mod is installed correctly in BCML and that BCML, Extended Memory and Multiplayer Utilities graphics packs are enabled.";
				}

				retries++;

				Logging::LoggerService::LogDebug("Failed to find address", __FUNCTION__);
				addr = Memory::PatternScan(signature, Memory::getBaseAddress(), 8, Memory::ScanOffset);
			}

			Memory::RegionStart = Memory::findRegionBaseAddress(Memory::getBaseAddress(), 8);
			
			Memory::ScanOffset = addr - Memory::RegionStart;

			setAddress(addr, __FUNCTION__, true);
		}

		void setAddress(uint64_t Address, const char* caller, bool Validate)
		{
			if (Validate)
				AddressSet = ValidateAddress(Address, caller);
			else
				AddressSet = true;

			if (AddressSet)
			{
				this->Address = Address;
				LastKnown = CharacterLocation();
				LastKnown.Map = "Empty";
				LastKnown.Section = "Empty";
			}
		}

		CharacterLocation get(const char* caller)
		{
			CharacterLocation result;

			if (!AddressSet)
			{
				Logging::LoggerService::LogWarning("Tried to get value of not set address. Returning default.", caller);
				return result;
			}

			result.Map = Memory::extractLocName(Address + 0x14);
			result.Section = Memory::extractLocName(Address + 0x40);

			LastKnown = result;

			return result;
		}

	private:
		uint64_t Address;
		bool AddressSet = false;

		bool ValidateAddress(uint64_t Address, const char* caller)
		{
			if (Address == 0)
			{
				Logging::LoggerService::LogWarning("Address set to 0.", caller);
				return false;
			}

			Platform::MemoryRegion region;

			if (Platform::QueryRegion(Address, region))
			{
				if (!region.readable) {
					Logging::LoggerService::LogError("Failed to validate address.", caller);

					exit(1);
				}
			}

			return true;
		}
	};

}