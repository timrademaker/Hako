#include "Hako/inc/Hako.h"

#include <cstring>
#include <string>
#include <vector>

#ifdef HAKO_STANDALONE
char const* GetFlagValue(int& flagIndex, int argc, char* argv[])
{
    if (flagIndex + 1 < argc)
    {
        ++flagIndex;
        return argv[flagIndex];
    }

    return nullptr;
}

void PrintAvailablePlatforms(char const* separator)
{
    bool first_platform = true;

    for (const auto* platform : hako::PlatformNames)
    {
        if(first_platform)
        {
            first_platform = false;
        }
        else
        {
            printf("%s", separator);
        }

        printf("%s", platform);
    }
}

void PrintHelp()
{
    printf(R"""(Available flags:
    --help
        Print this help message and quit

    --intermediate_dir <path_to_intermediate_directory>
        Path to the intermediate asset directory. Required for both serialization and archive creation.

    --serialize <path_to_serialize> [<path_to_serialize>...]
        Specify paths of files or directories to serialize.

    --ext <file_extension>
        When specified, only serialize files with this extension
        Does not apply to files explicitly specified with --serialize

    --platform <platform_name>
        Specify the platform to serialize the assets for. Required when --serialize is used.
        Available platforms:
)""");

    printf("            ");
    PrintAvailablePlatforms(", ");
    printf("\n");

printf(R"""(
    --archive <archive_out_path>
        Path to the archive to output to

    --overwrite_archive
        When used, overwrite the archive specified with --archive if it exists

)""");

    printf(R"""(Example usage:
    Hako --platform Windows --serialize Assets/Models Assets/Textures --intermediate_dir intermediate
    Hako --platform Windows --serialize Assets --ext gltf --intermediate_dir intermediate
    Hako --intermediate_dir intermediate --archive arc.bin --overwrite_archive
    Hako --platform Windows --serialize Assets --intermediate_dir intermediate --archive arc.bin --overwrite_archive)""");
}

int main(int argc, char* argv[])
{
    // The name of the platform we're serializing files for
    std::string platformName{};
    // Paths to whatever we're expected to serialize. Can be a directories or files.
    std::vector<char const*> pathsToSerialize{};
    // When set, only serialize files with this extension
    char const* fileExtensionToSerialize = nullptr;
    // Path to the intermediate directory, both for serialization and archiving.
    char const* intermediateDirectory = nullptr;
    // Path to the archive we'll be outputting to
    char const* archivePath = nullptr;
    // If true, the archive at archivePath is overwritten if it exists
    bool overwriteExistingArchive = false;

    for (int i = 1; i < argc; ++i)
    {
        if (platformName.empty() && strcmp(argv[i], "--platform") == 0)
        {
            platformName = GetFlagValue(i, argc, argv);
        }
        else if (archivePath == nullptr && strcmp(argv[i], "--archive") == 0)
        {
            archivePath = GetFlagValue(i, argc, argv);
        }
        else if(intermediateDirectory == nullptr && strcmp(argv[i], "--intermediate_dir") == 0)
        {
            intermediateDirectory = GetFlagValue(i, argc, argv);
        }
        else if (pathsToSerialize.empty() && strcmp(argv[i], "--serialize") == 0)
        {
            // Everything until the next flag is a file or folder path
            while (i + 1 < argc && argv[i + 1][0] != '-')
            {
                pathsToSerialize.emplace_back(argv[i + 1]);
                ++i;
            }
        }
        else if(fileExtensionToSerialize == nullptr && strcmp(argv[i], "--ext") == 0)
        {
            fileExtensionToSerialize = GetFlagValue(i, argc, argv);
        }
        else if(strcmp(argv[i], "--overwrite_archive") == 0)
        {
            overwriteExistingArchive = true;
        }
        else if(strcmp(argv[i], "--help") == 0)
        {
            PrintHelp();
            return EXIT_SUCCESS;
        }
    }

    if(intermediateDirectory == nullptr)
    {
        printf("No intermediate directory specified. Use --help for more info.\n");
        return EXIT_FAILURE;
    }

    if (platformName.empty())
    {
        printf("No platform name specified. Use --help for more info.\n");
        return EXIT_FAILURE;
    }

    if (!archivePath && pathsToSerialize.empty())
    {
        printf("No archive path or paths to serialize specified. Use --help for more info.\n");
        return EXIT_FAILURE;
    }

    const hako::Platform targetPlatform = hako::GetPlatformByName(platformName.c_str());
    if (targetPlatform == hako::Platform::Invalid)
    {
        printf("Invalid platform '%s' specified!\nAvailable platforms:\n", platformName.c_str());
        PrintAvailablePlatforms("\n");

        return EXIT_FAILURE;
    }

    bool success = true;

    for (auto const& path : pathsToSerialize)
    {
        if (hako::Serialize(targetPlatform, intermediateDirectory, path, fileExtensionToSerialize))
        {
            printf("Successfully serialized %s\n", path);
        }
        else
        {
            printf("Failed to serialize %s\n", path);
            success = false;
        }
    }
    
    // Only create an archive if we have an archive path and nothing before this failed
    if (archivePath && success)
    {
        success = hako::CreateArchive(intermediateDirectory, targetPlatform, archivePath, overwriteExistingArchive);
        if (success)
        {
            printf("Successfully created archive %s\n", archivePath);
        }
        else
        {
            printf("Failed to create archive %s\n", archivePath);
        }
    }

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
#endif
