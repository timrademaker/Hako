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
     * @param a_IntermediateDirectory The directory in which intermediate assets are located
     * @param a_TargetPlatform The platform for which to create the archive
     * @param a_ArchiveName The name of the archive to output
     * @param a_OverwriteExistingFile If a file with the provided name already exists, a value of true will result in this file being overwritten
     * @return True if the archive was created successfully
     */
    bool CreateArchive(char const* a_IntermediateDirectory, Platform a_TargetPlatform, char const* a_ArchiveName, bool a_OverwriteExistingFile = false);

    /**
     * Serialize a file or the content of a directory into the intermediate directory
     * @param a_TargetPlatform The platform for which to serialize the file
     * @param a_IntermediateDirectory The intermediate directory to serialize the assets to
     * @param a_Path The file or directory to serialize
     * @param a_FileExt When set, only serialize assets with the given file extension if a_Path is a directory
     * @return True if the file was serialized successfully
     */
    bool Serialize(Platform a_TargetPlatform, char const* a_IntermediateDirectory, char const* a_Path, char const* a_FileExt = nullptr);

    /**
     * Serialize a file into the intermediate directory
     * @param a_TargetPlatform The platform for which to serialize the file
     * @param a_IntermediateDirectory The intermediate directory to serialize the assets to
     * @param a_FilePath The file to serialize
     * @return True if the file was serialized successfully
     */
    bool SerializeFile(Platform a_TargetPlatform, char const* a_IntermediateDirectory, char const* a_FilePath);

    /**
     * Serialize all files in a directory into the intermediate directory
     * @param a_TargetPlatform The platform for which to serialize the file
     * @param a_IntermediateDirectory The intermediate directory to serialize the assets to
     * @param a_Directory The directory to serialize
     * @param a_FileExt When set, only serialize assets with the given file extension
     * @return True if all files were serialized successfully
     */
    bool SerializeDirectory(Platform a_TargetPlatform, char const* a_IntermediateDirectory, char const* a_Directory, char const* a_FileExt = nullptr);

    class Hako final
    {
    public:
        struct FileInfo
        {
            static constexpr size_t MaxFilePathHashLength = 17;

            char m_FilePathHash[MaxFilePathHashLength] = {};
            size_t m_Size = 0;
            size_t m_Offset = 0;
        };

    public:
        /**
         * Open an archive for reading
         * @param a_ArchiveName The name of the archive to open
         * @param a_IntermediateDirectory The directory in which intermediate files are located. Used when intermediate file reading is enabled.
         */
        Hako(char const* a_ArchiveName, char const* a_IntermediateDirectory = nullptr);
        ~Hako() = default;

        Hako(Hako&) = delete;
        Hako(Hako const&) = delete;
        Hako& operator=(const Hako&) = delete;
        Hako(Hako&&) = delete;
        Hako& operator=(Hako&&) = delete;

        /**
         * Add a serializer to the content packer
         * The serializer should inherit from IFileSerializer
         */
        template<typename Serializer>
        static typename std::enable_if<std::is_base_of<IFileSerializer, Serializer>::value, void>::type
            AddSerializer();

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
        std::string m_IntermediateDirectory{};

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
