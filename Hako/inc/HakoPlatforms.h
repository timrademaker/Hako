#pragma once
#include <cstdint>
#include <cstring>

namespace hako
{
#define HAKO_PLATFORMS \
    ENTRY(Windows) \
    ENTRY(Linux) \
    ENTRY(MacOS) \
    ENTRY(Android) \
    ENTRY(IOS) \
    ENTRY(PS4) \
    ENTRY(PS5) \
    ENTRY(XboxOne) \
    ENTRY(XboxSeriesX) \
    ENTRY(NintendoSwitch)

	enum class Platform : uint8_t
	{
#define ENTRY(PlatformName) PlatformName,
		HAKO_PLATFORMS
#undef ENTRY
        Invalid
	};

    static constexpr const char* PlatformNames[] = {
#define ENTRY(PlatformName) #PlatformName, 
            HAKO_PLATFORMS
#undef ENTRY
    };

    inline Platform GetPlatformByName(const char* a_Name)
    {
        for (size_t i = 0; i < sizeof(PlatformNames) / sizeof(PlatformNames[0]); ++i)
        {
            if (strcmp(a_Name, PlatformNames[i]) == 0)
            {
                return static_cast<Platform>(i);
            }
        }

        return Platform::Invalid;
    }

#undef HAKO_PLATFORMS
}
