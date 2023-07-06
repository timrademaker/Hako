#include "Hako.h"
#include "HakoFile.h"

#include "HakoLog.h"
#include "MurmurHash3.h"
#include "SerializerList.h"

#include <algorithm>
#include <cassert>
#include <filesystem>

#define HAKO_ASSERT(x, ...) do { bool const result = (x); if(!result) { hako::Log(__VA_ARGS__); assert(result); } } while(false)

namespace
{
    inline constexpr size_t MaxResourcePathHashLength = 33; // Length including null-terminator
    using ResourcePathHashStr = char[MaxResourcePathHashLength];

    std::string IntermediateDirectory{ hako::DefaultIntermediateDirectory };

    /**
     * @return < 0 if a_Lhs < a_Rhs
     * @return 0 if the hashes are the same
     * @return > 0 if a_Lhs > a_Rhs
     */
    int CompareHash(hako::ResourcePathHash const& a_Lhs, hako::ResourcePathHash const& a_Rhs)
    {
        for(size_t i = 0; i < std::size(a_Lhs.hash64); ++i)
        {
            if(a_Lhs.hash64[i] < a_Rhs.hash64[i])
            {
                return -1;
            }
            else if(a_Lhs.hash64[i] > a_Rhs.hash64[i])
            {
                return 1;
            }
        }

        return 0;
    }

    void GetResourcePathHashStr(char const* a_FilePath, ResourcePathHashStr& a_OutBuffer, size_t a_OutBufferSize = sizeof(ResourcePathHashStr))
    {
        assert(a_OutBufferSize >= sizeof(ResourcePathHashStr) && "Hash won't fit into buffer");

        hako::ResourcePathHash hash{};
        hako::GetResourcePathHash(a_FilePath, hash);
        hash.ToString(a_OutBuffer);
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

    std::filesystem::path GetIntermediateDirectoryPath(hako::Platform a_TargetPlatform)
    {
        std::filesystem::path intermediateFilePath(IntermediateDirectory);
        intermediateFilePath.append(GetPlatformName(a_TargetPlatform));

        EnsureIntermediateDirectoryExists(intermediateFilePath);

        return intermediateFilePath;
    }

    std::filesystem::path GetIntermediateFilePath(hako::Platform a_TargetPlatform, hako::ResourcePathHash const& a_Hash)
    {
        std::filesystem::path intermediateFilePath = GetIntermediateDirectoryPath(a_TargetPlatform);
        EnsureIntermediateDirectoryExists(intermediateFilePath);

        ResourcePathHashStr hashStr{};
        a_Hash.ToString(hashStr);
        intermediateFilePath.append(hashStr);

        return intermediateFilePath;
    }

    std::filesystem::path GetIntermediateFilePath(hako::Platform a_TargetPlatform, char const* a_FilePath)
    {
        HAKO_ASSERT(a_FilePath, "No file path provided\n");

        hako::ResourcePathHash resourcePathHash{};
        hako::GetResourcePathHash(a_FilePath, resourcePathHash);

        return GetIntermediateFilePath(a_TargetPlatform, resourcePathHash);
    }
}

namespace hako
{
    constexpr size_t WriteChunkSize = 10 * 1024; // 10 MiB
    constexpr uint8_t ArchiveVersion = 2;
    constexpr char ArchiveMagic[] = { 'H', 'A', 'K', 'O' };
    constexpr uint8_t MagicLength = sizeof(ArchiveMagic);
    constexpr uint32_t Murmur3Seed = 0x48'41'4B'4F;

    struct ArchiveHeader
    {
        ArchiveHeader()
        {
            memcpy(m_Magic, ArchiveMagic, MagicLength);
        }

        char m_Magic[MagicLength]{};
        uint8_t m_ArchiveVersion = ArchiveVersion;
        uint8_t m_HeaderSize = sizeof(ArchiveHeader);
        char m_Padding[2] = {};
        uint32_t m_FileCount = 0;
    };

    /** The factory function to use for file IO */
    FileFactorySignature s_FileFactory = hako::HakoFileFactory;

    std::string ResourcePathHash::ToString() const
    {
        ResourcePathHashStr buffer;
        ToString(buffer);
        return { buffer };
    }

    void ResourcePathHash::ToString(char* a_OutBuffer) const
    {
        snprintf(a_OutBuffer, MaxResourcePathHashLength, "%zX%zX", hash64[0], hash64[1]);
    }

    bool ResourcePathHash::operator<(ResourcePathHash const& a_Rhs) const
    {
        return CompareHash(*this, a_Rhs) < 0;
    }

    bool ResourcePathHash::operator<=(ResourcePathHash const& a_Rhs) const
    {
        return CompareHash(*this, a_Rhs) <= 0;
    }

    bool ResourcePathHash::operator>(ResourcePathHash const& a_Rhs) const
    {
        return CompareHash(*this, a_Rhs) > 0;
    }

    bool ResourcePathHash::operator>=(ResourcePathHash const& a_Rhs) const
    {
        return CompareHash(*this, a_Rhs) >= 0;
    }

    ResourcePathHash ResourcePathHash::FromString(char const* a_Hash)
    {
        static constexpr uint8_t HashPartCount = 2;
        static constexpr size_t PartialHashLength = (MaxResourcePathHashLength - 1) / HashPartCount;

        std::string partialHash;
        partialHash.resize(PartialHashLength + 1);

        auto const path = a_Hash;
        size_t offset = 0;

        ResourcePathHash hash;
        for (auto& outHash : hash.hash64)
        {
            strncpy_s(partialHash.data(), partialHash.length(), path + offset, PartialHashLength);
            outHash = std::stoull(partialHash, nullptr, 16);

            offset += PartialHashLength;
        }

        return hash;
    }

    void SetFileIO(FileFactorySignature a_FileFactory)
    {
        s_FileFactory = std::move(a_FileFactory);
    }

    /** Add a static serializer to the list of known serializers */
    void AddSerializer_Internal(Serializer a_FileSerializer)
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
        HAKO_ASSERT(a_FilePath && a_FilePath[0] != 0, "No file path provided!");

        auto const intermediateFile = s_FileFactory(a_FilePath, FileOpenMode::Read);
        if (!intermediateFile)
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
                hako::Log("Unable to archive file %s\n", a_FilePath);
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
            hako::Log("Error while writing to the archive!\n");
            assert(false);
        }
    }

    void SetIntermediateDirectory(char const* a_IntermediateDirectory)
    {
        HAKO_ASSERT(a_IntermediateDirectory, "No intermediate directory specified\n");
        IntermediateDirectory = std::string(a_IntermediateDirectory);
    }

    bool CreateArchive(Platform a_TargetPlatform, char const* const a_ArchiveName, bool a_OverwriteExistingFile)
    {
        HAKO_ASSERT(a_ArchiveName, "No archive path specified for archive creation\n");

        if (!a_OverwriteExistingFile && std::filesystem::exists(a_ArchiveName))
        {
            // The archive already exists, and we don't want to overwrite it
            hako::Log("Failed to create archive \"%s\" - file already exists.\n", a_ArchiveName);
            return false;
        }

        std::unique_ptr<IFile> const archive = s_FileFactory(a_ArchiveName, FileOpenMode::WriteTruncate);
        HAKO_ASSERT(archive != nullptr, "Unable to open archive \"%s\" for writing!\n", a_ArchiveName);

        struct HashPathPair
        {
            HashPathPair(std::filesystem::path const& a_FilePath)
                : m_FilePath(a_FilePath.generic_string())
            , m_ResourcePathHash(ResourcePathHash::FromString(a_FilePath.filename().generic_string().c_str()))
            { }

            // Full file path (with hashed file name)
            std::string m_FilePath{};
            // Hashed file name
            ResourcePathHash m_ResourcePathHash;
        };

        std::vector<HashPathPair> filePaths;
        filePaths.reserve(256);

        for (std::filesystem::directory_entry const& dirEntry :
            std::filesystem::recursive_directory_iterator(GetIntermediateDirectoryPath(a_TargetPlatform), std::filesystem::directory_options::skip_permission_denied))
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
        std::sort(filePaths.begin(), filePaths.end(), [](HashPathPair const& a_Lhs, HashPathPair const& a_Rhs)
            {
                return CompareHash(a_Lhs.m_ResourcePathHash, a_Rhs.m_ResourcePathHash) < 0;
            }
        );

        size_t totalFileSize = 0;

        // Create FileInfo objects and serialize file content to archive
        for (size_t fileIndex = 0; fileIndex < filePaths.size(); ++fileIndex)
        {
            std::unique_ptr<IFile> currentFile = s_FileFactory(filePaths[fileIndex].m_FilePath.c_str(), FileOpenMode::Read);

            Archive::FileInfo fi{};
            fi.m_ResourcePathHash = filePaths[fileIndex].m_ResourcePathHash;
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
     * @param a_FilePath The file to serialize
     * @param a_TargetPlatform The asset's target platform
     * @return True if the file was successfully copied
     */
    bool DefaultSerializeFile(Platform a_TargetPlatform, char const* a_FilePath)
    {
        HAKO_ASSERT(a_FilePath && a_FilePath[0] != 0, "No file path provided\n");

        std::error_code ec;
        std::filesystem::copy_file(a_FilePath, GetIntermediateFilePath(a_TargetPlatform, a_FilePath), std::filesystem::copy_options::update_existing, ec);
        return ec.value() == 0;
    }

    /**
     * Serialize a file into the intermediate directory
     * @param a_TargetPlatform The platform for which to serialize the file
     * @param a_FilePath The file to serialize
     * @param a_ForceSerialization If true, serialize files regardless of whether they were changed since they were last serialized
     * @return True if the file was serialized successfully
     */
    bool SerializeFile(Platform a_TargetPlatform, char const* a_FilePath, bool a_ForceSerialization)
    {
        HAKO_ASSERT(a_FilePath && a_FilePath[0] != 0, "No file path provided\n");

        auto const intermediatePath = GetIntermediateFilePath(a_TargetPlatform, a_FilePath);

        if (!a_ForceSerialization && std::filesystem::exists(intermediatePath))
        {
            auto const sourceAssetWriteTime = std::filesystem::last_write_time(a_FilePath);
            auto const intermediateAssetWriteTime = std::filesystem::last_write_time(intermediatePath);

            if (intermediateAssetWriteTime >= sourceAssetWriteTime)
            {
                // Skipping serialization for this file, as it hasn't changed since the last time it was serialized
                return true;
            }
        }

        Serializer const* serializer = SerializerList::GetInstance().GetSerializerForFile(a_FilePath, a_TargetPlatform);

        if (serializer != nullptr)
        {
            std::vector<char> data{};
            size_t const serializedByteCount = serializer->m_SerializeFile(a_FilePath, a_TargetPlatform, data);
            data.resize(serializedByteCount);

            auto const intermediateFile = s_FileFactory(intermediatePath.generic_string().c_str(), FileOpenMode::WriteTruncate);
            return intermediateFile->Write(0, data);
        }

        hako::Log("Using default serializer for %s\n", a_FilePath);
        return DefaultSerializeFile(a_TargetPlatform, a_FilePath);
    }

    /**
     * Serialize all files in a directory into the intermediate directory
     * @param a_TargetPlatform The platform for which to serialize the file
     * @param a_Directory The directory to serialize
     * @param a_ForceSerialization If true, serialize files regardless of whether they were changed since they were last serialized
     * @param a_FileExt When set, only serialize assets with the given file extension
     * @return True if all files were serialized successfully
     */
    bool SerializeDirectory(Platform a_TargetPlatform, char const* a_Directory, bool a_ForceSerialization, char const* a_FileExt)
    {
        HAKO_ASSERT(a_Directory && a_Directory[0] != 0, "No directory provided\n");

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

        for (std::filesystem::directory_entry const& dirEntry :
            std::filesystem::recursive_directory_iterator(a_Directory, std::filesystem::directory_options::skip_permission_denied))
        {
            if (dirEntry.is_regular_file() && (a_FileExt == nullptr || dirEntry.path().extension() == fileExtension))
            {
                if (!SerializeFile(a_TargetPlatform, dirEntry.path().generic_string().c_str(), a_ForceSerialization))
                {
                    success = false;
                }
            }
        }

        return success;
    }

    bool Serialize(Platform a_TargetPlatform, char const* a_Path, bool a_ForceSerialization, char const* a_FileExt)
    {
        HAKO_ASSERT(a_Path && a_Path[0] != 0, "No path provided\n");

        if (std::filesystem::is_directory(a_Path))
        {
            return SerializeDirectory(a_TargetPlatform, a_Path, a_ForceSerialization, a_FileExt);
        }
        else if (std::filesystem::is_regular_file(a_Path))
        {
            return SerializeFile(a_TargetPlatform, a_Path, a_ForceSerialization);
        }

        return false;
    }

    bool ExportResource(Platform a_TargetPlatform, char const* a_ResourceName, const std::vector<char>& a_Data)
    {
        ResourcePathHashStr resourceHashStr{};
        GetResourcePathHashStr(a_ResourceName, resourceHashStr);

        auto const intermediatePath = GetIntermediateFilePath(a_TargetPlatform, a_ResourceName);

        auto const intermediateFile = s_FileFactory(intermediatePath.generic_string().c_str(), FileOpenMode::WriteTruncate);
        return intermediateFile->Write(0, a_Data);
    }

    void GetResourcePathHash(char const* a_Path, ResourcePathHash& a_OutHash)
    {
        HAKO_ASSERT(a_Path && a_Path[0] != 0, "No path provided\n");

        MurmurHash3_x64_128(a_Path, strlen(a_Path), Murmur3Seed, a_OutHash.hash64);
    }
}

using namespace hako;

Archive::Archive(char const* a_ArchivePath, char const* a_IntermediateDirectory, Platform a_Platform)
{
    Open(a_ArchivePath, a_IntermediateDirectory, a_Platform);
}

void Archive::Open(char const* a_ArchivePath, char const* a_IntermediateDirectory, Platform a_Platform)
{
    HAKO_ASSERT(m_ArchiveReader == nullptr, "An archive has already been opened. Close it before opening another one.");

    HAKO_ASSERT(a_ArchivePath && a_ArchivePath[0] != 0, "No archive path provided\n");

    m_FilesInArchive.clear();

    // Open archive
    m_ArchiveReader = s_FileFactory(a_ArchivePath, FileOpenMode::Read);
    HAKO_ASSERT(m_ArchiveReader != nullptr, "Unable to open archive \"%s\" for reading!\n", a_ArchivePath);

#ifdef HAKO_READ_OUTSIDE_OF_ARCHIVE
    if (a_IntermediateDirectory)
    {
        SetIntermediateDirectory(a_IntermediateDirectory);
    }

    std::filesystem::path const intermediatePath(IntermediateDirectory);
    if (std::filesystem::exists(IntermediateDirectory))
    {
        HAKO_ASSERT(std::filesystem::is_directory(intermediatePath), "Intermediate path is not a directory!");
    }
    else
    {
        hako::Log("Intermediate directory \"%s\" does not exist.\n", IntermediateDirectory.c_str());
    }

    m_LastWriteTimestamp = std::filesystem::last_write_time(a_ArchivePath).time_since_epoch().count();
    m_CurrentPlatform = a_Platform;
#endif

    // Read from archive
    std::vector<char> buffer{};
    buffer.resize(sizeof(ArchiveHeader));
    m_ArchiveReader->Read(sizeof(ArchiveHeader), 0, buffer);

    ArchiveHeader const header = *reinterpret_cast<ArchiveHeader*>(buffer.data());
    HAKO_ASSERT(memcmp(header.m_Magic, ArchiveMagic, MagicLength) == 0, "The archive does not seem to a Hako archive, or the file might be corrupted.\n");
    HAKO_ASSERT(header.m_ArchiveVersion == ArchiveVersion, "Archive version mismatch. The archive should be rebuilt.\n");

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

void Archive::Close()
{
    m_FilesInArchive.clear();
    m_LastWriteTimestamp = 0;
    m_ArchiveReader = nullptr;

}

bool Archive::ReadFile(char const* a_FileName, std::vector<char>& a_OutData) const
{
    ResourcePathHash hash;
    GetResourcePathHash(a_FileName, hash);

    return ReadFile(hash, a_OutData);
}

bool Archive::ReadFile(ResourcePathHash const& a_ResourcePathHash, std::vector<char>& a_OutData) const
{
#ifdef HAKO_READ_OUTSIDE_OF_ARCHIVE

    if (ReadFileOutsideArchive(a_ResourcePathHash, a_OutData))
    {
        return true;
    }
#endif

    FileInfo const* fi = GetFileInfo(a_ResourcePathHash);
    HAKO_ASSERT(fi != nullptr, "Unable to find file with hash \"%s\" in archive.\n", a_ResourcePathHash.ToString().c_str());

    return LoadFileContent(*fi, a_OutData);
}

Archive::FileInfo const* Archive::GetFileInfo(ResourcePathHash const& a_ResourcePathHash) const
{
    HAKO_ASSERT(!m_FilesInArchive.empty(), "Archive is empty");

    // Find file (assumes that file names were sorted before this)
    auto const foundFile = std::lower_bound(m_FilesInArchive.begin(), m_FilesInArchive.end(), a_ResourcePathHash, [](FileInfo const& a_Lhs, ResourcePathHash const& a_Rhs)
        {
            return CompareHash(a_Lhs.m_ResourcePathHash, a_Rhs) < 0;
        }
    );

    if (foundFile == m_FilesInArchive.end() || foundFile->m_ResourcePathHash != a_ResourcePathHash)
    {
        return nullptr;
    }

    return &(*foundFile);
}

bool Archive::ReadFileOutsideArchive(ResourcePathHash const& a_Hash, std::vector<char>& a_OutData) const
{
#ifdef HAKO_READ_OUTSIDE_OF_ARCHIVE
    auto const intermediatePath = GetIntermediateFilePath(m_CurrentPlatform, a_Hash);
    if (!std::filesystem::exists(intermediatePath))
    {
        return false;
    }

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
    size_t const fileSize = file->GetFileSize();
    a_OutData.clear();
    a_OutData.resize(fileSize);

    if (file->Read(fileSize, 0, a_OutData))
    {
        return true;
    }
#endif

    return false;
}

bool Archive::LoadFileContent(FileInfo const& a_FileInfo, std::vector<char>& a_Data) const
{
    a_Data.clear();
    a_Data.resize(a_FileInfo.m_Size);
    return m_ArchiveReader->Read(a_FileInfo.m_Size, a_FileInfo.m_Offset, a_Data);
}
