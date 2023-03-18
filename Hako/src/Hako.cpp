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

    /**
     * If no serializer can be found for a certain file, simply copy its content to the archive
     * @param a_Archive The archive to serialize to
     * @param a_ArchiveWriteOffset The offset in the archive at which to start writing
     * @param a_FileName The file to serialize
     * @return The number of bytes written
     */
    size_t DefaultSerializeFile(IFile* a_Archive, size_t a_ArchiveWriteOffset, char const* a_FileName)
    {
        std::vector<char> data{};

        std::unique_ptr<IFile> const file = s_FileFactory(a_FileName, FileOpenMode::Read);
        if (!file)
        {
            std::cout << "Unable to open file " << a_FileName << " for reading" << std::endl;
            return 0;
        }

        const size_t fileSize = file->GetFileSize();
        size_t bytesRead = 0;

        while (bytesRead < fileSize)
        {
            size_t bytesToRead = std::min(fileSize - bytesRead, WriteChunkSize);
            data.clear();
            data.resize(bytesToRead);

            if (!file->Read(bytesToRead, bytesRead, data))
            {
                std::cout << "Unable to serialize file " << a_FileName << std::endl;
                return 0;
            }
            bytesRead += bytesToRead;

            a_Archive->Write(a_ArchiveWriteOffset, data);
            a_ArchiveWriteOffset += bytesToRead;
        }

        return fileSize;
    }

    /**
     * Serialize a file into the archive
     * @param a_Archive The archive to write to
     * @param a_FileInfo File info for the file that should be serialized into the archive
     * @param a_TargetPlatform The platform for which to serialize the file
     * @return The number of bytes written
     */
    size_t SerializeFile(IFile* a_Archive, Hako::FileInfo const& a_FileInfo, Platform a_TargetPlatform)
    {
        IFileSerializer* serializer = SerializerList::GetInstance().GetSerializerForFile(a_FileInfo.m_Name, a_TargetPlatform);

        if (serializer != nullptr)
        {
            std::vector<char> data{};
            const size_t serializedByteCount = serializer->SerializeFile(a_FileInfo.m_Name, a_TargetPlatform, data);
            data.resize(serializedByteCount);
            a_Archive->Write(a_FileInfo.m_Offset, data);
            return serializedByteCount;
        }
        else
        {
            return DefaultSerializeFile(a_Archive, a_FileInfo.m_Offset, a_FileInfo.m_Name);
        }
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
            data++;
        }

        if (!a_Archive->Write(a_WriteOffset, buffer))
        {
            std::cout << "Error while writing to the archive!" << std::endl;
            assert(false);
        }
    }

    bool CreateArchive(std::vector<std::string>& a_FileNames, Platform a_TargetPlatform, char const* const a_ArchiveName, bool a_OverwriteExistingFile)
    {
        if (!a_OverwriteExistingFile)
        {
            // Check if the file already exists
            if (s_FileFactory(a_ArchiveName, FileOpenMode::Read) != nullptr)
            {
                // The file already exists
#ifndef HAKO_STANDALONE
                return false;
#else
                char response = 0;
                std::cout << "The file " << a_ArchiveName << " already exists. Overwrite?" << std::endl << "(Y/N): ";
                std::cin >> response;
                if (response != 'Y' && response != 'y')
                {
                    std::cout << "Not creating archive " << a_ArchiveName << " as the file already exists" << std::endl;
                    return false;
                }
                std::cout << "Overwriting existing archive " << a_ArchiveName << std::endl;
#endif
            }
        }

        std::unique_ptr<IFile> archive = s_FileFactory(a_ArchiveName, FileOpenMode::WriteTruncate);
        if (archive == nullptr)
        {
            std::cout << "Unable to open archive " << a_ArchiveName << " for writing!" << std::endl;
            assert(archive == nullptr);
            return false;
        }

        size_t archiveInfoBytesWritten = 0;

        {
            HakoHeader header;
            header.m_FileCount = a_FileNames.size();
            WriteToArchive(archive.get(), &header, sizeof(HakoHeader), archiveInfoBytesWritten);
            archiveInfoBytesWritten += sizeof(HakoHeader);
        }

        // Sort file names alphabetically
        std::sort(a_FileNames.begin(), a_FileNames.end());

        size_t totalFileSize = 0;

        // Create FileInfo objects and serialize file content to archive
        for (size_t fileIndex = 0; fileIndex < a_FileNames.size(); ++fileIndex)
        {
            std::unique_ptr<IFile> currentFile = s_FileFactory(a_FileNames[fileIndex].c_str(), FileOpenMode::Read);

            Hako::FileInfo fi{};
            strcpy_s(fi.m_Name, Hako::FileInfo::MaxFileNameLength, a_FileNames[fileIndex].c_str());
            fi.m_Offset = sizeof(HakoHeader) + sizeof(Hako::FileInfo) * a_FileNames.size() + totalFileSize;

            // Serialize file into the archive
            fi.m_Size = SerializeFile(archive.get(), fi, a_TargetPlatform);
            totalFileSize += fi.m_Size;

            // Write file info to the archive
            WriteToArchive(archive.get(), &fi, sizeof(Hako::FileInfo), archiveInfoBytesWritten);
            archiveInfoBytesWritten += sizeof(Hako::FileInfo);
        }

        return true;
    }
}

using namespace hako;

bool Hako::OpenArchive(char const* a_ArchiveName)
{
    m_OpenedFiles.clear();
    m_FilesInArchive.clear();
#ifdef HAKO_READ_OUTSIDE_OF_ARCHIVE
    m_OpenedFilesOutsideArchive.clear();
#endif
    
    // Open archive
    m_ArchiveReader = s_FileFactory(a_ArchiveName, FileOpenMode::Read);
    if (m_ArchiveReader == nullptr)
    {
        std::cout << "Unable to open archive " << a_ArchiveName << " for reading!" << std::endl;
        assert(m_ArchiveReader == nullptr);
        return false;
    }

    std::vector<char> buffer{};
    buffer.resize(sizeof(HakoHeader));
    m_ArchiveReader->Read(sizeof(HakoHeader), 0, buffer);

    const HakoHeader header = *reinterpret_cast<HakoHeader*>(buffer.data());
    if (memcmp(header.m_Magic, ArchiveMagic, MagicLength) != 0)
    {
        std::cout << "The archive does not seem to a Hako archive, or the file might be corrupted." << std::endl;
        assert(false);
        return false;
    }

    if(header.m_HeaderVersion != HeaderVersion)
    {
        std::cout << "Archive version mismatch. The archive should be rebuilt." << std::endl;
        assert(false);
        return false;
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

    return true;
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
    
    // Find file (assumes that file names were sorted before this)
    auto const foundFile = std::lower_bound(m_FilesInArchive.begin(), m_FilesInArchive.end(), a_FileName, [](FileInfo const& a_Lhs, char const* a_Rhs)
        {
            return strcmp(a_Lhs.m_Name, a_Rhs) < 0;
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
    // Check if the file has already been opened
    auto const openedFile = m_OpenedFilesOutsideArchive.find(a_FileName);
    if (openedFile != m_OpenedFilesOutsideArchive.end())
    {
        return &openedFile->second;
    }

    // Try to open the file
    std::unique_ptr<IFile> file = s_FileFactory(a_FileName, FileOpenMode::Read);

    if (file == nullptr)
    {
        // File doesn't exist outside of the archive, or it can't be opened
        return nullptr;
    }

    std::vector<char>& data = m_OpenedFilesOutsideArchive[a_FileName];
    
    // Serialize file
    IFileSerializer* serializer = SerializerList::GetInstance().GetSerializerForFile(a_FileName, m_CurrentPlatform);
    if (serializer != nullptr)
    {
        // Close the file to prevent possible issues
        file = nullptr;

        const size_t serializedByteCount = serializer->SerializeFile(a_FileName, m_CurrentPlatform, data);
        data.resize(serializedByteCount);
        return &data;
    }
    else
    {
        const size_t fileSize = file->GetFileSize();
        data.resize(fileSize);

        // Read file content
        if (file->Read(fileSize, 0, data))
        {
            return &data;
        }
    }

    return nullptr;
}

const std::vector<char>* Hako::LoadFileContent(const FileInfo& a_FileInfo)
{
    // Check if the file has already been read
    {
        const auto openedFile = m_OpenedFiles.find(a_FileInfo.m_Name);
        if (openedFile != m_OpenedFiles.end())
        {
            return &openedFile->second;
        }
    }

    // Add a new entry for this file
    std::vector<char>& data = m_OpenedFiles[a_FileInfo.m_Name];
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
