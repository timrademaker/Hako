#include "Hako.h"
#include "HakoFile.h"
#include "SerializerList.h"

#include <algorithm>
#include <cassert>
#include <iostream>

namespace hako
{
    constexpr size_t WriteChunkSize = 10 * 1024; // 10 MiB
    constexpr uint8_t HeaderVersion = 2;
    constexpr char ArchiveMagic[] = { 'H', 'A', 'K', 'O' };
    constexpr uint8_t MagicLength = sizeof(ArchiveMagic);

    struct HakoHeader
    {
        HakoHeader()
        {
            memcpy(m_Magic, ArchiveMagic, MagicLength);
        }

        char m_Magic[MagicLength];
        uint8_t m_HeaderVersion = HeaderVersion;
        uint8_t m_HeaderSize = sizeof(HakoHeader);
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

    void EnsureIntermediateDirectoryExists(std::filesystem::path& a_Directory)
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

        char fileHashHex[Hako::FileInfo::MaxFilePathHashLength]{};
        HashFilePath(a_FilePath, fileHashHex, sizeof(fileHashHex));

        intermediateFilePath.append(fileHashHex);

        return intermediateFilePath.generic_string();
    }

    /**
     * Copy a file into the archive
     * @param a_Archive The archive to write to
     * @param a_FilePath The path of the file to archive
     * @param a_FileInfo File info for the file that should be copied into the archive
     * @return The number of bytes written
     */
    size_t ArchiveFile(IFile* a_Archive, char const* a_FilePath, Hako::FileInfo const& a_FileInfo)
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

    bool CreateArchive(char const* a_IntermediateDirectory, Platform a_TargetPlatform, char const* const a_ArchiveName, bool a_OverwriteExistingFile)
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

        std::unique_ptr<IFile> archive = s_FileFactory(a_ArchiveName, FileOpenMode::WriteTruncate);
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
                strncpy_s(m_FilePathHash, sizeof(m_FilePathHash), a_FilePath.filename().generic_string().c_str(), Hako::FileInfo::MaxFilePathHashLength);
            }

            std::string m_FilePath{};
            char m_FilePathHash[Hako::FileInfo::MaxFilePathHashLength]{};
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
            HakoHeader header;
            header.m_FileCount = filePaths.size();
            WriteToArchive(archive.get(), &header, sizeof(HakoHeader), archiveInfoBytesWritten);
            archiveInfoBytesWritten += sizeof(HakoHeader);
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

            Hako::FileInfo fi{};
            strncpy_s(fi.m_FilePathHash, sizeof(fi.m_FilePathHash), filePaths[fileIndex].m_FilePathHash, Hako::FileInfo::MaxFilePathHashLength);
            fi.m_Offset = sizeof(HakoHeader) + sizeof(Hako::FileInfo) * filePaths.size() + totalFileSize;

            fi.m_Size = ArchiveFile(archive.get(), filePaths[fileIndex].m_FilePath.c_str(), fi);
            totalFileSize += fi.m_Size;

            // Write file info to the archive
            WriteToArchive(archive.get(), &fi, sizeof(Hako::FileInfo), archiveInfoBytesWritten);
            archiveInfoBytesWritten += sizeof(Hako::FileInfo);
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

    bool Serialize(Platform a_TargetPlatform, char const* a_IntermediateDirectory, char const* a_Path, char const* a_FileExt)
    {
        assert(a_Path);
        assert(a_IntermediateDirectory);

        if (std::filesystem::is_directory(a_Path))
        {
            return SerializeDirectory(a_TargetPlatform, a_IntermediateDirectory, a_Path, a_FileExt);
        }
        else if (std::filesystem::is_regular_file(a_Path))
        {
            return SerializeFile(a_TargetPlatform, a_IntermediateDirectory, a_Path);
        }

        return false;
    }

    bool SerializeFile(Platform a_TargetPlatform, char const* a_IntermediateDirectory, char const* a_FilePath)
    {
        assert(a_IntermediateDirectory);
        assert(a_FilePath);

        IFileSerializer* serializer = SerializerList::GetInstance().GetSerializerForFile(a_FilePath, a_TargetPlatform);

        if (serializer != nullptr)
        {
            std::vector<char> data{};
            const size_t serializedByteCount = serializer->SerializeFile(a_FilePath, a_TargetPlatform, data);
            data.resize(serializedByteCount);

            auto const intermediateFile = s_FileFactory(GetIntermediateFilePath(a_TargetPlatform, a_IntermediateDirectory, a_FilePath).generic_string().c_str(), FileOpenMode::WriteTruncate);
            return intermediateFile->Write(0, data);
        }
        
        return DefaultSerializeFile(a_TargetPlatform, a_IntermediateDirectory, a_FilePath);
    }

    bool SerializeDirectory(Platform a_TargetPlatform, char const* a_IntermediateDirectory, char const* a_Directory, char const* a_FileExt)
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
                if(!SerializeFile(a_TargetPlatform, a_IntermediateDirectory, dirEntry.path().generic_string().c_str()))
                {
                    success = false;
                }
            }
        }

        return success;
    }
}

using namespace hako;

Hako::Hako(char const* a_ArchiveName, char const* a_IntermediateDirectory)
{
    m_OpenedFiles.clear();
    m_FilesInArchive.clear();
#ifdef HAKO_READ_OUTSIDE_OF_ARCHIVE
    m_OpenedFilesOutsideArchive.clear();
    m_IntermediateDirectory = std::string(a_IntermediateDirectory);
#endif
    
    // Open archive
    m_ArchiveReader = s_FileFactory(a_ArchiveName, FileOpenMode::Read);
    if (m_ArchiveReader == nullptr)
    {
        std::cout << "Unable to open archive " << a_ArchiveName << " for reading!" << std::endl;
        assert(m_ArchiveReader == nullptr);
        return;
    }

    m_LastWriteTimestamp = m_ArchiveReader->GetLastWriteTime();

    std::vector<char> buffer{};
    buffer.resize(sizeof(HakoHeader));
    m_ArchiveReader->Read(sizeof(HakoHeader), 0, buffer);

    const HakoHeader header = *reinterpret_cast<HakoHeader*>(buffer.data());
    if (memcmp(header.m_Magic, ArchiveMagic, MagicLength) != 0)
    {
        std::cout << "The archive does not seem to a Hako archive, or the file might be corrupted." << std::endl;
        assert(false);
        return;
    }

    if(header.m_HeaderVersion != HeaderVersion)
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

void hako::Hako::LoadAllFiles()
{
    for (const FileInfo& fi : m_FilesInArchive)
    {
        LoadFileContent(fi);
    }
}

const std::vector<char>* Hako::ReadFile(char const* a_FileName)
{
#ifdef HAKO_READ_OUTSIDE_OF_ARCHIVE
    const std::vector<char>* outsideData = ReadFileOutsideArchive(a_FileName);
    if (outsideData != nullptr)
    {
        return outsideData;
    }
#endif

    const FileInfo* fi = GetFileInfo(a_FileName);
    if (fi == nullptr)
    {
        std::cout << "Unable to find file " << a_FileName << " in archive." << std::endl;
        assert(fi == nullptr);
        return nullptr;
    }

    return LoadFileContent(*fi);
}

void Hako::CloseFile(char const* a_FileName)
{
#ifdef HAKO_READ_OUTSIDE_OF_ARCHIVE
    m_OpenedFilesOutsideArchive.erase(a_FileName);
#endif
    m_OpenedFiles.erase(a_FileName);
}

void Hako::SetCurrentPlatform(Platform a_Platform)
{
    m_CurrentPlatform = a_Platform;
}

const Hako::FileInfo* Hako::GetFileInfo(char const* a_FileName) const
{
    assert(!m_FilesInArchive.empty());

    char filePathHash[Hako::FileInfo::MaxFilePathHashLength]{};
    HashFilePath(a_FileName, filePathHash, Hako::FileInfo::MaxFilePathHashLength);
    
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

const std::vector<char>* Hako::ReadFileOutsideArchive(char const* a_FileName)
{
    char fileNameHash[Hako::FileInfo::MaxFilePathHashLength]{};
    HashFilePath(a_FileName, fileNameHash, sizeof(fileNameHash));

    auto const intermediatePath = GetIntermediateFilePath(m_CurrentPlatform, m_IntermediateDirectory.c_str(), a_FileName);

    // Try to open the file
    std::unique_ptr<IFile> file = s_FileFactory(intermediatePath.generic_string().c_str(), FileOpenMode::Read);

    if (file == nullptr)
    {
        // File doesn't exist outside of the archive, or it can't be opened
        return nullptr;
    }

    auto const lastIntermediateFileWriteTime = file->GetLastWriteTime();
    if(lastIntermediateFileWriteTime <= m_LastWriteTimestamp)
    {
        // File hasn't been updated since the archive was created, so we can just read it from the archive
        return nullptr;
    }

    // Check if the file has already been opened and didn't change since then
    auto const openedFile = m_OpenedFilesOutsideArchive.find(fileNameHash);
    if (openedFile != m_OpenedFilesOutsideArchive.end() && openedFile->second.m_LastWriteTime >= lastIntermediateFileWriteTime)
    {
        return &openedFile->second.m_Data;
    }

    std::vector<char>& data = m_OpenedFilesOutsideArchive[fileNameHash].m_Data;

    // Read intermediate file content
    const size_t fileSize = file->GetFileSize();
    data.resize(fileSize);

    if (file->Read(fileSize, 0, data))
    {
        return &data;
    }

    return nullptr;
}

const std::vector<char>* Hako::LoadFileContent(const FileInfo& a_FileInfo)
{
    // Check if the file has already been read
    {
        const auto openedFile = m_OpenedFiles.find(a_FileInfo.m_FilePathHash);
        if (openedFile != m_OpenedFiles.end())
        {
            return &openedFile->second;
        }
    }

    // Add a new entry for this file
    std::vector<char>& data = m_OpenedFiles[a_FileInfo.m_FilePathHash];
    if (!data.empty())
    {
        return &data;
    }

    // Read the file's content
    data.resize(a_FileInfo.m_Size);
    if (m_ArchiveReader->Read(a_FileInfo.m_Size, a_FileInfo.m_Offset, data))
    {
        return &data;
    }

    return nullptr;
}

void hako::Hako::AddSerializer_Internal(IFileSerializer* a_FileSerializer)
{
    SerializerList::GetInstance().AddSerializer(a_FileSerializer);
}
