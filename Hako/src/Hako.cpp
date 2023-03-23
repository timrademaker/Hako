#include "Hako.h"
#include "HakoFile.h"
#include "SerializerList.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <iostream>

namespace hako
{
    constexpr size_t WriteChunkSize = 10 * 1024; // 10 MiB
    constexpr uint8_t HeaderVersion = 2;
    constexpr char ArchiveMagic[] = { 'H', 'A', 'K', 'O' };
    constexpr uint8_t MagicLength = sizeof(ArchiveMagic);

    struct ArchiveHeader
    {
        ArchiveHeader()
        {
            memcpy(m_Magic, ArchiveMagic, MagicLength);
        }

        char m_Magic[MagicLength];
        uint8_t m_HeaderVersion = HeaderVersion;
        uint8_t m_HeaderSize = sizeof(ArchiveHeader);
        char m_Padding[2] = {};
        uint32_t m_FileCount = 0;
        char m_Padding2[4] = {};
    };

    /** The factory function to use for file IO */
    FileFactorySignature s_FileFactory = hako::HakoFileFactory;

    void SetFileIO(FileFactorySignature a_FileFactory)
    {
        s_FileFactory = std::move(a_FileFactory);
    }

    void HashFilePath(char const* a_FilePath, char* out_buffer, size_t out_buffer_len)
    {
        snprintf(out_buffer, out_buffer_len, "%zX", std::filesystem::hash_value(a_FilePath));
    }

    void EnsureIntermediateDirectoryExists(std::filesystem::path const& a_Directory)
    {
        static bool createdIntermediateDirectory = false;
        if (!createdIntermediateDirectory)
        {
            std::filesystem::create_directories(a_Directory);
            createdIntermediateDirectory = true;
        }
    }

    std::filesystem::path GetIntermediateDirectoryPath(Platform a_TargetPlatform, char const* a_IntermediateDirectory)
    {
        std::filesystem::path intermediateFilePath(a_IntermediateDirectory);
        intermediateFilePath.append(GetPlatformName(a_TargetPlatform));

        EnsureIntermediateDirectoryExists(intermediateFilePath);

        return intermediateFilePath;
    }

    std::filesystem::path GetIntermediateFilePath(Platform a_TargetPlatform, char const* a_IntermediateDirectory, char const* a_FilePath)
    {
        assert(a_IntermediateDirectory);
        assert(a_FilePath);

        std::filesystem::path intermediateFilePath = GetIntermediateDirectoryPath(a_TargetPlatform, a_IntermediateDirectory);
        EnsureIntermediateDirectoryExists(intermediateFilePath);

        char fileHashHex[Archive::FileInfo::MaxFilePathHashLength]{};
        HashFilePath(a_FilePath, fileHashHex, sizeof(fileHashHex));

        intermediateFilePath.append(fileHashHex);

        return intermediateFilePath.generic_string();
    }

    /** Add a static serializer to the list of known serializers */
    void AddSerializer_Internal(IFileSerializer* a_FileSerializer)
    {
        SerializerList::GetInstance().AddSerializer(a_FileSerializer);
    }

    /**
     * Copy a file into the archive
     * @param a_Archive The archive to write to
     * @param a_FilePath The path of the file to archive
     * @param a_FileInfo File info for the file that should be copied into the archive
     * @return The number of bytes written
     */
    size_t ArchiveFile(IFile* a_Archive, char const* a_FilePath, Archive::FileInfo const& a_FileInfo)
    {
        auto const intermediateFile = s_FileFactory(a_FilePath, FileOpenMode::Read);
        if(!intermediateFile)
        {
            return 0;
        }

        auto const fileSize = intermediateFile->GetFileSize();
        size_t bytesRead = 0;
        std::vector<char> data{};

        while (bytesRead < fileSize)
        {
            size_t const bytesToRead = std::min(fileSize - bytesRead, WriteChunkSize);
            data.clear();
            data.resize(bytesToRead);

            if (!intermediateFile->Read(bytesToRead, bytesRead, data))
            {
                std::cout << "Unable to archive file " << a_FilePath << std::endl;
                return 0;
            }

            a_Archive->Write(a_FileInfo.m_Offset + bytesRead, data);
            bytesRead += bytesToRead;
        }

        return bytesRead;
    }

    /**
     * Write data to the archive
     * @param a_Archive The opened archive to write to
     * @param a_Data The data to write to the archive
     * @param a_NumBytes The number of bytes to write to the archive
     * @param a_WriteOffset The offset at which to write to the archive
     */
    void WriteToArchive(IFile* a_Archive, void* a_Data, size_t a_NumBytes, size_t a_WriteOffset)
    {
        std::vector<char> buffer(a_NumBytes, 0);

        char* data = static_cast<char*>(a_Data);

        for (size_t i = 0; i < a_NumBytes; ++i)
        {
            buffer[i] = *data;
            ++data;
        }

        if (!a_Archive->Write(a_WriteOffset, buffer))
        {
            std::cout << "Error while writing to the archive!" << std::endl;
            assert(false);
        }
    }

    bool CreateArchive(Platform a_TargetPlatform, char const* a_IntermediateDirectory, char const* const a_ArchiveName, bool a_OverwriteExistingFile)
    {
        if(a_IntermediateDirectory == nullptr)
        {
            std::cout << "No intermediate directory specified for archive creation" << std::endl;
            assert(a_IntermediateDirectory != nullptr);
            return false;
        }

        if (a_ArchiveName == nullptr)
        {
            std::cout << "No archive path specified for archive creation" << std::endl;
            assert(a_ArchiveName != nullptr);
            return false;
        }

        if (!a_OverwriteExistingFile && std::filesystem::exists(a_ArchiveName))
        {
            // The archive already exists, and we don't want to overwrite it
            std::cout << "Failed to create archive " << a_ArchiveName << " - file already exists" << std::endl;
            return false;
        }

        std::unique_ptr<IFile> const archive = s_FileFactory(a_ArchiveName, FileOpenMode::WriteTruncate);
        if (archive == nullptr)
        {
            std::cout << "Unable to open archive " << a_ArchiveName << " for writing!" << std::endl;
            assert(archive == nullptr);
            return false;
        }

        struct HashFileNamePair
        {
            HashFileNamePair(std::filesystem::path const& a_FilePath)
                : m_FilePath(a_FilePath.generic_string())
            {
                strncpy_s(m_FilePathHash, sizeof(m_FilePathHash), a_FilePath.filename().generic_string().c_str(), Archive::FileInfo::MaxFilePathHashLength);
            }

            std::string m_FilePath{};
            char m_FilePathHash[Archive::FileInfo::MaxFilePathHashLength]{};
        };

        std::vector<HashFileNamePair> filePaths;
        filePaths.reserve(256);

        for (const std::filesystem::directory_entry& dirEntry :
            std::filesystem::recursive_directory_iterator(GetIntermediateDirectoryPath(a_TargetPlatform, a_IntermediateDirectory), std::filesystem::directory_options::skip_permission_denied))
        {
            if (dirEntry.is_regular_file())
            {
                filePaths.emplace_back(dirEntry.path());
            }
        }

        size_t archiveInfoBytesWritten = 0;

        {
            ArchiveHeader header;
            header.m_FileCount = filePaths.size();
            WriteToArchive(archive.get(), &header, sizeof(ArchiveHeader), archiveInfoBytesWritten);
            archiveInfoBytesWritten += sizeof(ArchiveHeader);
        }

        // Sort file hashes alphabetically
        std::sort(filePaths.begin(), filePaths.end(), [](HashFileNamePair const& a_Lhs, HashFileNamePair const& a_Rhs)
            {
                return strcmp(a_Lhs.m_FilePathHash, a_Rhs.m_FilePathHash) < 0;
            }
        );

        size_t totalFileSize = 0;

        // Create FileInfo objects and serialize file content to archive
        for (size_t fileIndex = 0; fileIndex < filePaths.size(); ++fileIndex)
        {
            std::unique_ptr<IFile> currentFile = s_FileFactory(filePaths[fileIndex].m_FilePath.c_str(), FileOpenMode::Read);

            Archive::FileInfo fi{};
            strncpy_s(fi.m_FilePathHash, sizeof(fi.m_FilePathHash), filePaths[fileIndex].m_FilePathHash, Archive::FileInfo::MaxFilePathHashLength);
            fi.m_Offset = sizeof(ArchiveHeader) + sizeof(Archive::FileInfo) * filePaths.size() + totalFileSize;

            fi.m_Size = ArchiveFile(archive.get(), filePaths[fileIndex].m_FilePath.c_str(), fi);
            totalFileSize += fi.m_Size;

            // Write file info to the archive
            WriteToArchive(archive.get(), &fi, sizeof(Archive::FileInfo), archiveInfoBytesWritten);
            archiveInfoBytesWritten += sizeof(Archive::FileInfo);
        }

        return true;
    }

    /**
     * If no serializer can be found for a certain file, simply copy its content to the intermediate directory
     * @param a_IntermediateDirectory The intermediate directory to serialize the assets to
     * @param a_FilePath The file to serialize
     * @param a_TargetPlatform The asset's target platform
     * @return True if the file was successfully copied
     */
    bool DefaultSerializeFile(Platform a_TargetPlatform, char const* a_IntermediateDirectory, char const* a_FilePath)
    {
        assert(a_IntermediateDirectory);
        assert(a_FilePath);

        std::error_code ec;
        std::filesystem::copy_file(a_FilePath, GetIntermediateFilePath(a_TargetPlatform, a_IntermediateDirectory, a_FilePath), std::filesystem::copy_options::update_existing, ec);
        return ec.value() == 0;
    }

    /**
     * Serialize a file into the intermediate directory
     * @param a_TargetPlatform The platform for which to serialize the file
     * @param a_IntermediateDirectory The intermediate directory to serialize the assets to
     * @param a_FilePath The file to serialize
     * @param a_ForceSerialization If true, serialize files regardless of whether they were changed since they were last serialized
     * @return True if the file was serialized successfully
     */
    bool SerializeFile(Platform a_TargetPlatform, char const* a_IntermediateDirectory, char const* a_FilePath, bool a_ForceSerialization)
    {
        assert(a_IntermediateDirectory);
        assert(a_FilePath);

        auto const intermediatePath = GetIntermediateFilePath(a_TargetPlatform, a_IntermediateDirectory, a_FilePath);

        if (!a_ForceSerialization && std::filesystem::exists(intermediatePath))
        {
            auto const sourceAssetWriteTime = std::filesystem::last_write_time(a_FilePath);
            auto const intermediateAssetWriteTime = std::filesystem::last_write_time(intermediatePath);

            if(intermediateAssetWriteTime >= sourceAssetWriteTime)
            {
                // Skipping serialization for this file, as it hasn't changed since the last time it was serialized
                return true;
            }
        }

        IFileSerializer* serializer = SerializerList::GetInstance().GetSerializerForFile(a_FilePath, a_TargetPlatform);

        if (serializer != nullptr)
        {
            std::vector<char> data{};
            const size_t serializedByteCount = serializer->SerializeFile(a_FilePath, a_TargetPlatform, data);
            data.resize(serializedByteCount);

            auto const intermediateFile = s_FileFactory(intermediatePath.generic_string().c_str(), FileOpenMode::WriteTruncate);
            return intermediateFile->Write(0, data);
        }
        
        return DefaultSerializeFile(a_TargetPlatform, a_IntermediateDirectory, a_FilePath);
    }

    /**
     * Serialize all files in a directory into the intermediate directory
     * @param a_TargetPlatform The platform for which to serialize the file
     * @param a_IntermediateDirectory The intermediate directory to serialize the assets to
     * @param a_Directory The directory to serialize
     * @param a_ForceSerialization If true, serialize files regardless of whether they were changed since they were last serialized
     * @param a_FileExt When set, only serialize assets with the given file extension
     * @return True if all files were serialized successfully
     */
    bool SerializeDirectory(Platform a_TargetPlatform, char const* a_IntermediateDirectory, char const* a_Directory, bool a_ForceSerialization, char const* a_FileExt)
    {
        assert(a_IntermediateDirectory);
        assert(a_Directory);

        std::string fileExtension{};
        if (a_FileExt)
        {
            fileExtension = a_FileExt;

            // Ensure that the file extension we receive starts with a "."
            if (a_FileExt[0] != '.')
            {
                fileExtension = "." + fileExtension;
            }
        }

        bool success = true;

        for (const std::filesystem::directory_entry& dirEntry :
            std::filesystem::recursive_directory_iterator(a_Directory, std::filesystem::directory_options::skip_permission_denied))
        {
            if(dirEntry.is_regular_file() && (a_FileExt == nullptr || dirEntry.path().extension() == fileExtension))
            {
                if(!SerializeFile(a_TargetPlatform, a_IntermediateDirectory, dirEntry.path().generic_string().c_str(), a_ForceSerialization))
                {
                    success = false;
                }
            }
        }

        return success;
    }

    bool Serialize(Platform a_TargetPlatform, char const* a_IntermediateDirectory, char const* a_Path, bool a_ForceSerialization, char const* a_FileExt)
    {
        assert(a_Path);
        assert(a_IntermediateDirectory);

        if (std::filesystem::is_directory(a_Path))
        {
            return SerializeDirectory(a_TargetPlatform, a_IntermediateDirectory, a_Path, a_ForceSerialization, a_FileExt);
        }
        else if (std::filesystem::is_regular_file(a_Path))
        {
            return SerializeFile(a_TargetPlatform, a_IntermediateDirectory, a_Path, a_ForceSerialization);
        }

        return false;
    }
}

using namespace hako;

Archive::Archive(char const* a_ArchivePath, char const* a_IntermediateDirectory, Platform a_Platform)
{
    m_FilesInArchive.clear();
#ifdef HAKO_READ_OUTSIDE_OF_ARCHIVE
    m_IntermediateDirectory = std::string(a_IntermediateDirectory);
    m_LastWriteTimestamp = std::filesystem::last_write_time(a_ArchivePath).time_since_epoch().count();
    m_CurrentPlatform = a_Platform;
#endif

    // Open archive
    m_ArchiveReader = s_FileFactory(a_ArchivePath, FileOpenMode::Read);
    if (m_ArchiveReader == nullptr)
    {
        std::cout << "Unable to open archive " << a_ArchivePath << " for reading!" << std::endl;
        assert(m_ArchiveReader == nullptr);
        return;
    }

    std::vector<char> buffer{};
    buffer.resize(sizeof(ArchiveHeader));
    m_ArchiveReader->Read(sizeof(ArchiveHeader), 0, buffer);

    const ArchiveHeader header = *reinterpret_cast<ArchiveHeader*>(buffer.data());
    if (memcmp(header.m_Magic, ArchiveMagic, MagicLength) != 0)
    {
        std::cout << "The archive does not seem to a Hako archive, or the file might be corrupted." << std::endl;
        assert(false);
        return;
    }

    if (header.m_HeaderVersion != HeaderVersion)
    {
        std::cout << "Archive version mismatch. The archive should be rebuilt." << std::endl;
        assert(false);
        return;
    }

    buffer.reserve(sizeof(FileInfo));
    size_t bytesRead = header.m_HeaderSize;

    for (size_t fileNum = 0; fileNum < header.m_FileCount; ++fileNum)
    {
        buffer.clear();
        buffer.resize(sizeof(FileInfo));

        m_ArchiveReader->Read(sizeof(FileInfo), bytesRead, buffer);
        bytesRead += sizeof(FileInfo);

        m_FilesInArchive.push_back(*reinterpret_cast<FileInfo*>(buffer.data()));
    }
}

bool Archive::ReadFile(char const* a_FileName, std::vector<char>& a_Data)
{
#ifdef HAKO_READ_OUTSIDE_OF_ARCHIVE
    if(ReadFileOutsideArchive(a_FileName, a_Data))
    {
        return true;
    }
#endif

    const FileInfo* fi = GetFileInfo(a_FileName);
    if (fi == nullptr)
    {
        std::cout << "Unable to find file " << a_FileName << " in archive." << std::endl;
        assert(fi == nullptr);
        return nullptr;
    }

    return LoadFileContent(*fi, a_Data);
}

const Archive::FileInfo* Archive::GetFileInfo(char const* a_FileName) const
{
    assert(!m_FilesInArchive.empty());

    char filePathHash[Archive::FileInfo::MaxFilePathHashLength]{};
    HashFilePath(a_FileName, filePathHash, Archive::FileInfo::MaxFilePathHashLength);
    
    // Find file (assumes that file names were sorted before this)
    auto const foundFile = std::lower_bound(m_FilesInArchive.begin(), m_FilesInArchive.end(), filePathHash, [](FileInfo const& a_Lhs, char const* a_Rhs)
        {
            return strcmp(a_Lhs.m_FilePathHash, a_Rhs) < 0;
        }
    );

    if (foundFile == m_FilesInArchive.end())
    {
        return nullptr;
    }
    else
    {
        return &(*foundFile);
    }
}

bool Archive::ReadFileOutsideArchive(char const* a_FileName, std::vector<char>& a_Data) const
{
#ifdef HAKO_READ_OUTSIDE_OF_ARCHIVE
    char fileNameHash[Archive::FileInfo::MaxFilePathHashLength]{};
    HashFilePath(a_FileName, fileNameHash, sizeof(fileNameHash));

    auto const intermediatePath = GetIntermediateFilePath(m_CurrentPlatform, m_IntermediateDirectory.c_str(), a_FileName);

    auto const lastIntermediateFileWriteTime = std::filesystem::last_write_time(intermediatePath).time_since_epoch().count();
    if (lastIntermediateFileWriteTime <= m_LastWriteTimestamp)
    {
        // File hasn't been updated since the archive was created, so we can just read it from the archive
        return nullptr;
    }

    // Try to open the file
    std::unique_ptr<IFile> const file = s_FileFactory(intermediatePath.generic_string().c_str(), FileOpenMode::Read);

    if (file == nullptr)
    {
        // File doesn't exist outside of the archive, or it can't be opened
        return false;
    }

    // Read intermediate file content
    const size_t fileSize = file->GetFileSize();
    a_Data.clear();
    a_Data.resize(fileSize);

    if (file->Read(fileSize, 0, a_Data))
    {
        return true;
    }
#endif

    return false;
}

bool Archive::LoadFileContent(const FileInfo& a_FileInfo, std::vector<char>& a_Data) const
{
    // Read the file's content
    a_Data.clear();
    a_Data.resize(a_FileInfo.m_Size);
    return m_ArchiveReader->Read(a_FileInfo.m_Size, a_FileInfo.m_Offset, a_Data);
}
