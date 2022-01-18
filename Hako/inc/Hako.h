#pragma once

#include "IFileSerializer.h"
#include "IFile.h"

#include <vector>
#include <string>
#include <functional>
#include <map>
#include <memory>

namespace hako
{
    class Hako final
    {
    public:
        using FileName_t = std::string;
    private:
        using FileCount_t = uint32_t;
        using FileFactorySignature = std::function<std::unique_ptr<IFile>(const FileName_t& a_FilePath, IFile::FileOpenMode a_FileOpenMode)>;

        static constexpr size_t WriteChunkSize = 10 * 1024; // 10 MiB
        static constexpr int8_t CurrentVersion = 1;

        struct FileInfo
        {
            static constexpr size_t MaxFileNameLength = 64;

            char m_Name[MaxFileNameLength] = {};
            size_t m_Size = 0;
            size_t m_Offset = 0;
        };

        struct HakoHeader
        {
            char m_Magic[4] = { 'H', 'A', 'K', 'O' };
            int8_t m_Version = CurrentVersion;
            FileCount_t m_FileCount = 0;
        };

    public:
        Hako() = default;
        ~Hako() = default;

        /**
         * Set a factory function to be used for opening files. The function is expected to return a nullptr if the file couldn't be opened
         * @param a_FileFactory A file factory with a signature like std::unique_ptr<IFile> OpenFile(const Hako::FileName_t& a_FilePath, FileOpenMode a_FileOpenMode)
         */
        static void SetFileIO(FileFactorySignature a_FileFactory);

        /**
         * Add a serializer to the content packer
         * The serializer should inherit from IFileSerializer
         */
        template<typename Serializer>
        static typename std::enable_if<std::is_base_of<IFileSerializer, Serializer>::value, bool>::type
            AddSerializer();

        /**
         * Create an archive
         * @param a_FileNames The names of the files to put into the archive
         * @param a_ArchiveName The name of the archive to output
         * @param a_OverwriteExistingFile If a file with the provided name already exists, a value of true will result in this file being overwritten
         * @return True if the archive was created successfully
         */
        static bool CreateArchive(const std::vector<FileName_t>& a_FileNames, const FileName_t& a_ArchiveName, bool a_OverwriteExistingFile = false);

        /**
         * Open an archive for reading
         * @param a_ArchiveName The name of the archive to open
         * @return True if the archive was opened successfully
         */
        bool OpenArchive(const FileName_t& a_ArchiveName);

        /**
         * Load all files from an archive into memory
         */
        void LoadAllFiles();

        /**
         * Read the content of an archived file from the archive that is currently open
         * @param a_FileName The file to read from the archive
         * @return A pointer to the file's content, or a nullptr if the file couldn't be read. The pointer is only guaranteed to be valid until another function is called on the archive reader, but should remain valid until the file or archive is closed if all files have already been loaded.
         */
        const std::vector<char>* ReadFile(const FileName_t& a_FileName);

        /**
         * Close a file that has been read from the archive to free memory
         * @param a_FileName The file to close
         */
        const void CloseFile(const FileName_t& a_FileName);

    private:
        /**
         * Write data to the archive
         * @param a_Archive The opened archive to write to
         * @param a_Data The data to write to the archive
         * @param a_NumBytes The number of bytes to write to the archive
         * @param a_WriteOffset The offset at which to write to the archive
         */
        static void WriteToArchive(IFile* a_Archive, void* a_Data, size_t a_NumBytes, size_t a_WriteOffset);

        /**
         * Get the FileInfo for a specific file
         * @param a_FileName The file to find file info for
         * @return The file info, or a nullptr if not found
         */
        const FileInfo* GetFileInfo(const FileName_t& a_FileName) const;

        /**
         * Serialize a file into the archive
         * @param a_Archive The archive to write to
         * @param a_FileInfo File info for the file that should be serialized into the archive
         * @return The number of bytes written
         */
        static size_t SerializeFile(IFile* a_Archive, const FileInfo& a_FileInfo);

        /**
         * Get the serializer to use for a file
         * @param a_FileName The file that needs to be serialized
         * @return A non-owning pointer to the serializer if one was found, or a nullptr if no serializer could be found.
         */
        static IFileSerializer* GetSerializerForFile(const FileName_t& a_FileName);

        /**
         * If no serializer can be found for a certain file, simply copy its content to the archive
         * @param a_Archive The archive to serialize to
         * @param a_ArchiveWriteOffset The offset in the archive at which to start writing
         * @param a_FileName The file to serialize
         * @return The number of bytes written
         */
        static size_t DefaultSerializeFile(IFile* a_Archive, size_t a_ArchiveWriteOffset, const FileName_t& a_FileName);

        /**
         * Read a file outside of the archive as if it was placed inside the archive.
         * @param a_FileName The file to read
         * @return A pointer to the file's content, or a nullptr if the file couldn't be read. The pointer is only guaranteed to be valid until the next call to ReadFile(), OpenArchive() or CloseArchive().
         */
        const std::vector<char>* ReadFileOutsideArchive(const FileName_t& a_FileName);

        /**
         * Read the content of an archived file from the archive that is currently open
         * @param a_FileInfo The file info for the file that should be loaded
         * @return A pointer to the file's content, or a nullptr if the file couldn't be read. The pointer is only guaranteed to be valid until another function is called on the archive reader.
         */
        const std::vector<char>* LoadFileContent(const FileInfo& a_FileInfo);


    private:
        /** Info on all files present in the archive opened with OpenArchive() */
        std::vector<FileInfo> m_FilesInArchive;
        /** All files that have already been opened with ReadFile() */
        std::map<FileName_t, std::vector<char>> m_OpenedFiles;
        /** The instance of FileIO that is currently being used to read from the archive */
        std::unique_ptr<IFile> m_ArchiveReader = nullptr;

        /** All files that have already been opened with ReadFile(), but that are placed outside of the archive */
        std::map<FileName_t, std::vector<char>> m_OpenedFilesOutsideArchive;

        /** All file serializers provided by the user */
        static std::vector<std::unique_ptr<IFileSerializer>> s_FileSerializers;
        /** The factory function to use for file IO */
        static FileFactorySignature s_FileFactory;
    };

    template<typename Serializer>
    inline typename std::enable_if<std::is_base_of<IFileSerializer, Serializer>::value, bool>::type Hako::AddSerializer()
    {
        s_FileSerializers.push_back(std::make_unique<Serializer>());

        return true;
    }
}

// Macro to register serializer classes in namespace scope
#define HAKO_ADD_SERIALIZER(SerializerClass) namespace hako { \
    const bool addedSerializer_##SerializerClass = Hako::AddSerializer<SerializerClass>(); }
