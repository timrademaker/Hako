#pragma once

#include "HakoPlatforms.h"
#include "IFile.h"
#include "IFileSerializer.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace hako
{
    using FileFactorySignature = std::function<std::unique_ptr<IFile>(char const* a_FilePath, FileOpenMode a_FileOpenMode)>;

    /**
     * Set a factory function to be used for opening files. The function is expected to return a nullptr if the file couldn't be opened
     * @param a_FileFactory A file factory matching the function signature of FileFactorySignature
     */
    void SetFileIO(FileFactorySignature a_FileFactory);

    /**
     * Create an archive
     * @param a_FileNames The names of the files to put into the archive
     * @param a_TargetPlatform The platform for which to create the archive
     * @param a_ArchiveName The name of the archive to output
     * @param a_OverwriteExistingFile If a file with the provided name already exists, a value of true will result in this file being overwritten
     * @return True if the archive was created successfully
     */
    bool CreateArchive(std::vector<std::string>& a_FileNames, Platform a_TargetPlatform, char const* a_ArchiveName, bool a_OverwriteExistingFile = false);
    
    class Hako final
    {
    public:
        struct FileInfo
        {
            static constexpr size_t MaxFileNameLength = 64;

            char m_Name[MaxFileNameLength] = {};
            size_t m_Size = 0;
            size_t m_Offset = 0;
        };

    public:
        Hako() = default;
        ~Hako() = default;

        /**
         * Add a serializer to the content packer
         * The serializer should inherit from IFileSerializer
         */
        template<typename Serializer>
        static typename std::enable_if<std::is_base_of<IFileSerializer, Serializer>::value, void>::type
            AddSerializer();

        /**
         * Open an archive for reading
         * @param a_ArchiveName The name of the archive to open
         * @return True if the archive was opened successfully
         */
        bool OpenArchive(char const* a_ArchiveName);

        /**
         * Load all files from an archive into memory
         */
        void LoadAllFiles();

        /**
         * Read the content of an archived file from the archive that is currently open
         * @param a_FileName The file to read from the archive
         * @return A pointer to the file's content, or a nullptr if the file couldn't be read. The pointer is only guaranteed to be valid until another function is called on the archive reader, but should remain valid until the file or archive is closed if all files have already been loaded.
         */
        const std::vector<char>* ReadFile(char const* a_FileName);

        /**
         * Close a file that has been read from the archive to free memory
         * @param a_FileName The file to close
         */
        void CloseFile(char const* a_FileName);

        /**
         * Set the platform that Hako is currently being used on. This is only used when reading outside of the archive.
         */
        void SetCurrentPlatform(Platform a_Platform);

    private:
        /**
         * Get the FileInfo for a specific file
         * @param a_FileName The file to find file info for
         * @return The file info, or a nullptr if not found
         */
        const FileInfo* GetFileInfo(char const* a_FileName) const;

        /**
         * Read a file outside of the archive as if it was placed inside the archive.
         * @param a_FileName The file to read
         * @return A pointer to the file's content, or a nullptr if the file couldn't be read. The pointer is only guaranteed to be valid until the next call to ReadFile(), OpenArchive() or CloseArchive().
         */
        const std::vector<char>* ReadFileOutsideArchive(char const* a_FileName);

        /**
         * Read the content of an archived file from the archive that is currently open
         * @param a_FileInfo The file info for the file that should be loaded
         * @return A pointer to the file's content, or a nullptr if the file couldn't be read. The pointer is only guaranteed to be valid until another function is called on the archive reader.
         */
        const std::vector<char>* LoadFileContent(const FileInfo& a_FileInfo);

        /**
         * Internal implementation for adding a new serializer
         */
        static void AddSerializer_Internal(IFileSerializer* a_FileSerializer);

    private:
        /** Info on all files present in the archive opened with OpenArchive() */
        std::vector<FileInfo> m_FilesInArchive;
        /** All files that have already been opened with ReadFile() */
        std::map<std::string, std::vector<char>> m_OpenedFiles;
        /** The instance of FileIO that is currently being used to read from the archive */
        std::unique_ptr<IFile> m_ArchiveReader = nullptr;

        /** All files that have already been opened with ReadFile(), but that are placed outside of the archive */
        std::map<std::string, std::vector<char>> m_OpenedFilesOutsideArchive;

        /** The platform that Hako is currently being used on. Only used when reading outside of the archive, which is only supported on Windows by default */
        Platform m_CurrentPlatform = Platform::Windows;
    };

    template<typename Serializer>
    inline typename std::enable_if<std::is_base_of<IFileSerializer, Serializer>::value, void>::type Hako::AddSerializer()
    {
        IFileSerializer* fs = new Serializer;
        AddSerializer_Internal(fs);
    }
}

#if defined(_MSC_VER)
#define HAKO_DLL_EXPORT __declspec(dllexport)
#elif defined(__GNUC__)
#define HAKO_DLL_EXPORT extern __attribute__ ((visibility ("default"))
#else
#define HAKO_DLL_EXPORT
#endif

// Macro to register serializer from dll
#define HAKO_ADD_DYNAMIC_SERIALIZER(SerializerClass) \
extern "C" { \
    HAKO_DLL_EXPORT hako::IFileSerializer* __stdcall CreateHakoSerializer() { \
        return new SerializerClass; \
    } \
}
