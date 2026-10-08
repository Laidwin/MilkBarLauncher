#include "Memory.h"
#include "Platform.h"
#include <iostream>

uint64_t Memory::PatternScan(std::vector<int> signature, uint64_t baseAddr, int region, uint64_t regionOffset, bool Multiple, bool multipleRegions, uint64_t regionMaxOffset)
{

    // Regions are numbered from baseAddr in VirtualQuery order, uncommitted ones included.
    std::vector<Platform::MemoryRegion> regions = Platform::EnumerateRegions(baseAddr, region != 0 && !multipleRegions ? region : SIZE_MAX);

    int contador = 0;
    for (const Platform::MemoryRegion& Scanmbi : regions) {
        contador++;

        if (!Scanmbi.readable)
            continue; // if bad adress then dont read from it

        if (region != 0 && contador < region)
            continue;

        if (contador > region && region != 0 && !multipleRegions)
        {
            break;
        }

        //get last '?' position in pattern and use it to calculate the max shift value.
        //the last position in the pattern should never be a '?' -> we do not bother checking it
        uint64_t maxShift = signature.size() - 1;
        uint64_t maxIndex = signature.size() - 2;
        uint64_t wildCardIndex = 0;
        for (uint64_t i = 0; i < maxIndex + 1; i++) {
            if (signature.at(i) == -1) {
                maxShift = maxIndex - i;
                wildCardIndex = i;
            }
        }

        //initialize the shift table
        uint64_t shiftTable[256];
        for (uint64_t i = 0; i <= 255; i++) {
            shiftTable[i] = maxShift;
        }

        //fill shiftTable
        //forgot this in the video: Because max shift should always be '?' we only update the shift table for bytes to the right of the last '?'
        for (uint64_t i = wildCardIndex + 1; i < maxIndex; i++) {
            shiftTable[signature.at(i)] = maxIndex - i;
        }

        uint64_t startingAddress = 0;
        uint64_t endAddress = Scanmbi.size - signature.size();

        if (region != 0 && regionOffset != 0)
        {
            startingAddress = regionOffset;
        }

        if (region != 0 && regionMaxOffset != 0)
        {
            if (regionMaxOffset < endAddress)
                endAddress = regionMaxOffset;
        }

        for (uint64_t currentIndex = startingAddress; currentIndex < endAddress;) {

            for (uint64_t sigIndex = maxIndex; sigIndex >= 0; sigIndex--) {
                byte reading;

                try 
                {
                    reading = *(byte*)(Scanmbi.start + currentIndex + sigIndex);
                }
                catch (const std::exception& ex)
                {
                    std::stringstream stream;
                    stream << "Exception thrown: " << ex.what();
                    Logging::LoggerService::LogError(stream.str(), __FUNCTION__);
                    return NULL;
                }
                catch (...)
                {
                    Logging::LoggerService::LogError("Catched unknown exception.", __FUNCTION__);
                    return NULL;
                }

                if (reading != signature.at(sigIndex) && signature.at(sigIndex) != -1) {
                    currentIndex += shiftTable[reading];
                    break;
                }
                else if (sigIndex == 0) {

                    if (signature.at(signature.size() - 1) != *(byte*)(Scanmbi.start + currentIndex + signature.size() - 1))
                    {
                        currentIndex += 1;
                        break;
                    }

                    return Scanmbi.start + currentIndex;
                }
            }
        }

        if (region != 0 && contador == region && !multipleRegions)
        {
            break;
        }
    }
    return NULL;

}

std::vector<uint64_t> Memory::PatternScanMultiple(std::vector<int> signature, uint64_t baseAddr, int region, uint64_t regionOffset, bool multipleRegions, uint64_t regionMaxOffset, int expectedValues)
{
    std::vector<uint64_t> result;

    // Regions are numbered from baseAddr in VirtualQuery order, uncommitted ones included.
    std::vector<Platform::MemoryRegion> regions = Platform::EnumerateRegions(baseAddr, region != 0 && !multipleRegions ? region : SIZE_MAX);

    int contador = 0;
    for (const Platform::MemoryRegion& mbi : regions) {
        contador++;

        if (!mbi.readable)
            continue; // if bad adress then dont read from it

        if (region != 0 && contador < region)
            continue;

        if (contador > region && region != 0 && !multipleRegions)
        {
            break;
        }


        //get last '?' position in pattern and use it to calculate the max shift value.
        //the last position in the pattern should never be a '?' -> we do not bother checking it
        uint64_t maxShift = signature.size() - 1;
        uint64_t maxIndex = signature.size() - 2;
        uint64_t wildCardIndex = 0;
        for (uint64_t i = 0; i < maxIndex + 1; i++) {
            if (signature.at(i) == -1) {
                maxShift = maxIndex - i;
                wildCardIndex = i;
            }
        }

        //initialize the shift table
        uint64_t shiftTable[256];
        for (uint64_t i = 0; i <= 255; i++) {
            shiftTable[i] = maxShift;
        }


        //fill shiftTable
        //forgot this in the video: Because max shift should always be '?' we only update the shift table for bytes to the right of the last '?'
        for (uint64_t i = wildCardIndex + 1; i < maxIndex; i++) {
            shiftTable[signature.at(i)] = maxIndex - i;
        }


        uint64_t startingAddress = 0;
        uint64_t endAddress = mbi.size - signature.size();

        if (region != 0 && regionOffset != 0)
        {
            startingAddress = regionOffset;
        }

        if (region != 0 && regionMaxOffset != 0)
        {
            if (regionMaxOffset < endAddress)
                endAddress = regionMaxOffset;
        }

        for (uint64_t currentIndex = startingAddress; currentIndex < endAddress;) {

            for (uint64_t sigIndex = maxIndex; sigIndex >= 0; sigIndex--) {
                byte reading = *(byte*)(mbi.start + currentIndex + sigIndex);

                if (reading != signature.at(sigIndex) && signature.at(sigIndex) != -1) {
                    currentIndex += shiftTable[reading];
                    break;
                }
                else if (sigIndex == 0) {

                    if (signature.at(signature.size() - 1) != *(byte*)(mbi.start + currentIndex + signature.size() - 1))
                    {
                        currentIndex += 1;
                        break;
                    }

                    //return mbi.start + currentIndex;
                    result.push_back(mbi.start + currentIndex);

                    if (result.size() == expectedValues) return result;

                    currentIndex++;
                    break;

                }
            }
        }


        if (region != 0 && contador == region && !multipleRegions)
        {
            break;
        }
    }
    return result;

}

uint64_t Memory::TryPatternScan(std::vector<int> signature, uint64_t baseAddr, int region, uint64_t regionOffset, bool Multiple, bool multipleRegions, uint64_t regionMaxOffset, int retries, std::string flagName)
{
    uint64_t result = 0;
    int retryCounter = 0;
    bool infinite = false;

    if (retries == 0)
        infinite = true;

    while (result < 30000 && (infinite || retryCounter < retries))
    {
        result = PatternScan(signature, baseAddr, region, regionOffset, Multiple, multipleRegions, regionMaxOffset);
        retryCounter++;

        if (result < 30000)
        {
            Logging::LoggerService::LogDebug("Could not complete pattern scan." + flagName != "" ? "Flag: " + flagName : "", __FUNCTION__);

            Sleep(2000);
        }
    }

    return result;
}

uint64_t Memory::findRegionBaseAddress(uint64_t baseAddr, int region)
{
    std::vector<Platform::MemoryRegion> regions = Platform::EnumerateRegions(baseAddr, region);

    if (region < 1 || regions.size() < (size_t)region)
        return 0;

    return regions[region - 1].start;
}