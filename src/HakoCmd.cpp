#include "HakoCmd.h"

#include "Hako.h"

#include <cstring>
#include <string>
#include <vector>

namespace hako
{
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
            if (first_platform)
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

--intermediate <path_to_intermediate_directory>
    Path to the intermediate asset directory. Required for both serialization and archive creation.
    Defaults to "./%s/"

--serialize <path_to_serialize> [<path_to_serialize>...]
    Specify paths of files or directories to serialize. Should be relative to your working directory.

--ext <file_extension>
    When specified, only serialize files with this extension
    Does not apply to files explicitly specified with --serialize

--force_serialization
    When used, serialize files regardless of whether they were changed since they were last serialized

--platform <platform_name>
    Specify the platform to serialize the assets for
    Available platforms: )""", hako::DefaultIntermediateDirectory);

        PrintAvailablePlatforms(", ");
        printf("\n");

        printf(R"""(
--archive <archive_out_path>
    Path to the archive to output to

--overwrite_archive
    When used, overwrite the archive specified with --archive if it exists
)""");

        printf(R"""(
Example usage:
    Hako --platform Windows --serialize Assets/Models Assets/Textures --intermediate intermediate
    Hako --platform Windows --serialize Assets --ext gltf --intermediate intermediate
    Hako --intermediate intermediate --archive arc.bin --overwrite_archive
    Hako --platform Windows --serialize Assets --intermediate intermediate --archive arc.bin --overwrite_archive
)""");
    }

    struct CommandLineParams
    {
        // The name of the platform we're serializing or archiving files for
        std::string platformName{};
        hako::Platform platformEnum = hako::Platform::Invalid;
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
        // If true, serialize files regardless of when they were last serialized
        bool forceSerialization = false;
        // If true, a help message should be printed
        bool m_ShouldPrintHelp = false;
    };

    CommandLineParams ParseCommandLineParams(int argc, char* argv[])
    {
        CommandLineParams params;

        for (int i = 1; i < argc; ++i)
        {
            if (params.platformName.empty() && strcmp(argv[i], "--platform") == 0)
            {
                params.platformName = GetFlagValue(i, argc, argv);
            }
            else if (params.archivePath == nullptr && strcmp(argv[i], "--archive") == 0)
            {
                params.archivePath = GetFlagValue(i, argc, argv);
            }
            else if (params.intermediateDirectory == nullptr && strcmp(argv[i], "--intermediate") == 0)
            {
                params.intermediateDirectory = GetFlagValue(i, argc, argv);
            }
            else if (params.pathsToSerialize.empty() && strcmp(argv[i], "--serialize") == 0)
            {
                // Everything until the next flag is a file or folder path
                while (i + 1 < argc && argv[i + 1][0] != '-')
                {
                    params.pathsToSerialize.emplace_back(argv[i + 1]);
                    ++i;
                }
            }
            else if (params.fileExtensionToSerialize == nullptr && strcmp(argv[i], "--ext") == 0)
            {
                params.fileExtensionToSerialize = GetFlagValue(i, argc, argv);
            }
            else if (strcmp(argv[i], "--overwrite_archive") == 0)
            {
                params.overwriteExistingArchive = true;
            }
            else if (strcmp(argv[i], "--force_serialization") == 0)
            {
                params.forceSerialization = true;
            }
            else if (strcmp(argv[i], "--help") == 0)
            {
                params.m_ShouldPrintHelp = true;
                break;
            }
        }

        return params;
    }

    bool VerifyCommandLineParameters(CommandLineParams& a_Params)
    {
        bool success = true;

        if (a_Params.intermediateDirectory == nullptr)
        {
            printf("No intermediate directory specified. Defaulting to %s\n", hako::DefaultIntermediateDirectory);
            a_Params.intermediateDirectory = hako::DefaultIntermediateDirectory;
        }

        if (a_Params.platformName.empty())
        {
            printf("No platform name specified.\n");
            success = false;
        }
        else
        {
            a_Params.platformEnum = hako::GetPlatformByName(a_Params.platformName.c_str());
            if (a_Params.platformEnum == hako::Platform::Invalid)
            {
                printf("Invalid platform '%s' specified!\nAvailable platforms:\n", a_Params.platformName.c_str());
                PrintAvailablePlatforms("\n");

                success = false;
            }
        }

        if (!a_Params.archivePath && a_Params.pathsToSerialize.empty())
        {
            printf("No archive path or paths to serialize specified.\n");
            success = false;
        }

        if (!success)
        {
            printf("Use --help for more info.\n");
        }

        return success;
    }

    int CmdEntryPoint(int argc, char* argv[])
    {
        if (argc < 2)
        {
            PrintHelp();
            return EXIT_SUCCESS;
        }

        auto params = ParseCommandLineParams(argc, argv);

        if (params.m_ShouldPrintHelp)
        {
            PrintHelp();
            return EXIT_SUCCESS;
        }

        if (!VerifyCommandLineParameters(params))
        {
            return EXIT_FAILURE;
        }

        bool success = true;

        if (params.intermediateDirectory)
        {
            SetIntermediateDirectory(params.intermediateDirectory);
        }

        for (auto const& path : params.pathsToSerialize)
        {
            if (hako::Serialize(params.platformEnum, path, params.forceSerialization, params.fileExtensionToSerialize))
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
        if (params.archivePath && success)
        {
            success = hako::CreateArchive(params.platformEnum, params.archivePath, params.overwriteExistingArchive);
            if (success)
            {
                printf("Successfully created archive %s\n", params.archivePath);
            }
            else
            {
                printf("Failed to create archive %s\n", params.archivePath);
            }
        }

        return success ? EXIT_SUCCESS : EXIT_FAILURE;
    }
}
