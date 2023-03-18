#include "Hako/inc/Hako.h"

#include <cstring>
#include <string>
#include <vector>

using hako::Hako;

#ifdef HAKO_STANDALONE
int main(int argc, char* argv[])
{
    if (argc < 7)
    {
        printf("Not enough arguments provided!\n");
        printf("Usage: Hako --platform <platform_name> --out_path <output_archive_name> --files <file_to_archive> [<files_to_archive>...]\n");
        printf("Example: Hako --platform Windows --out_path Content.bin --files a.txt b.txt\n");
        return 1;
    }

    std::vector<std::string> files;
    std::string platformName{};
    char const* archiveName = nullptr;

    for (int i = 1; i < argc; ++i)
    {
        if (platformName.empty() && strcmp(argv[i], "--platform") == 0)
        {
            if (i + 1 < argc)
            {
                platformName = std::string(argv[i + 1]);
                ++i;
            }
        }
        else if (archiveName == nullptr && strcmp(argv[i], "--out_path") == 0)
        {
            if (i + 1 < argc)
            {
                archiveName = argv[i + 1];
            }
        }
        else if (strcmp(argv[i], "--files") == 0)
        {
            // Everything until the next flag is a file name
            while (i + 1 < argc && argv[i + 1][0] != '-')
            {
                files.emplace_back(std::string(argv[i + 1]));
                ++i;
            }
        }
    }

    const hako::Platform archivePlatform = hako::GetPlatformByName(platformName.c_str());
    if (archivePlatform == hako::Platform::Invalid)
    {
        printf("Invalid platform '%s' specified!\nAvailable platforms:\n", platformName.c_str());

        for (const auto* platform : hako::PlatformNames)
        {
            printf("%s\n", platform);
        }

        return 1;
    }

    const bool success = hako::CreateArchive(files, archivePlatform, archiveName);
    if (success)
    {
        printf("Successfully created archive %s\n", archiveName);
    }
    else
    {
        printf("Failed to create archive %s\n", archiveName);
    }

    return success ? 0 : 1;
}
#endif
