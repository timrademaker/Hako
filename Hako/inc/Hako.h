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
    private:
        using FileFactorySignature = std::function<std::unique_ptr<IFile>(const std::string & a_FilePath, IFile::FileOpenMode a_FileOpenMode)>;
        using FileCountType = uint32_t;

        static constexpr size_t WriteChunkSize = 10 * 1024; // 10 MiB

        struct FileInfo
        {
            static constexpr size_t MaxFileNameLength = 64;

            char m_Name[MaxFileNameLength] = {};
            size_t m_Size = 0;
            size_t m_Offset = 0;
        };

    public:
        /**
         * Set a class to be used for reading/writing
         * @param a_FileFactory A file factory with a signature like std::unique_ptr<IFile> OpenFile(const std::string& a_FilePath, FileOpenMode a_FileOpenMode)
         */
        static void SetFileIO(FileFactorySignature a_FileFactory);

#ifndef HAKO_READ_ONLY
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
         * @return True if the archive was created successfully
         */
        static bool CreateArchive(const std::vector<std::string>& a_FileNames, const std::string& a_ArchiveName);
#endif

        /**
         * Open an archive for reading
         * @param a_ArchiveName The name of the archive to open
         * @return True if the archive was opened successfully
         */
        static bool OpenArchive(const std::string& a_ArchiveName);
        /**
         * Read the content of an archived file from the archive that is currently open
         * @param a_FileName The file to read from the archive
         * @return A pointer to the file's content, or a nullptr if the file couldn't be read. The pointer is only guaranteed to be valid until the next call to ReadFile() or OpenArchive().
         */
        static const std::vector<char>* ReadFile(const std::string& a_FileName);

    private:
        Hako();
        Hako(const Hako&) = delete;
        Hako(Hako&) = delete;
        ~Hako() = default;

        static Hako& GetInstance();

#ifndef HAKO_READ_ONLY
        /**
         * Write data to the archive
         * @param a_Archive The opened archive to write to
         * @param a_Data The data to write to the archive
         * @param a_NumBytes The number of bytes to write to the archive
         * @param a_WriteOffset The offset at which to write to the archive
         */
        void WriteToArchive(IFile* a_Archive, void* a_Data, size_t a_NumBytes, size_t a_WriteOffset) const;
#endif

        /**
         * Get the FileInfo for a specific file
         * @param a_FileName The file to find file info for
         * @return The file info, or a nullptr if not found
         */
        const FileInfo* GetFileInfo(const std::string& a_FileName) const;

        /**
         * Serialize a file into the archive
         * @param a_Archive The archive to write to
         * @param a_FileInfo File info for the file that should be serialized into the archive
         * @return The number of bytes written
         */
        size_t SerializeFile(IFile* a_Archive, const FileInfo& a_FileInfo) const;

        /**
         * If no serializer can be found for a certain file, simply copy its content to the archive
         * @param a_Archive The archive to serialize to
         * @param a_ArchiveWriteOffset The offset in the archive at which to start writing
         * @param a_FileName The file to serialize
         * @return The number of bytes written
         */
        size_t DefaultSerializeFile(IFile* a_Archive, size_t a_ArchiveWriteOffset, const std::string& a_FileName) const;


    private:
        std::vector<std::unique_ptr<IFileSerializer>> m_FileSerializers;
        std::vector<FileInfo> m_FilesInArchive;
        std::map<std::string, std::vector<char>> m_OpenedFiles;
        FileFactorySignature m_FileFactory = nullptr;

        std::unique_ptr<IFile> m_ArchiveReader = nullptr;
    };

#ifndef HAKO_READ_ONLY
    template<typename Serializer>
    inline typename std::enable_if<std::is_base_of<IFileSerializer, Serializer>::value, bool>::type Hako::AddSerializer()
    {
        GetInstance().m_FileSerializers.push_back(std::make_unique<Serializer>());

        return true;
    }
#endif
}

// Macro to register serializer classes in namespace scope
#define HAKO_ADD_SERIALIZER(SerializerClass) namespace hako { \
    const bool addedSerializer_##SerializerClass = Hako::AddSerializer<SerializerClass>(); }
