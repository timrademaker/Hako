#include "Hako/inc/Hako.h"

using hako::Hako;

#ifdef HAKO_STANDALONE
int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        printf("Not enough arguments provided!\n");
        printf("Usage: HakoContentPacker <output_archive_name> <file_to_archive> [<files_to_archive>...]\n");
        printf("Example: HakoContentPacker Content.bin a.txt b.txt\n");
        return 1;
    }

    std::string archiveName = std::string(argv[1]);

    std::vector<std::string> files;

    for (int i = 2; i < argc; ++i)
    {
        files.push_back(argv[i]);
    }

    bool success = Hako::CreateArchive(files, archiveName);
    if (success)
    {
        printf("Successfully created archive %s\n", archiveName.c_str());
    }
    else
    {
        printf("Failed to create archive %s\n", archiveName.c_str());
    }

    return success ? 0 : 1;
}
#endif
