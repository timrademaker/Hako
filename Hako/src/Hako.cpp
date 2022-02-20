#include "Hako.h"
#include "HakoFile.h"
#include "SerializerList.h"

#include <assert.h>
#include <algorithm>
#include <iostream>

using namespace hako;

Hako::FileFactorySignature Hako::s_FileFactory = hako::HakoFileFactory;

void Hako::SetFileIO(FileFactorySignature a_FileFactory)
{
    s_FileFactory = a_FileFactory;
}

bool Hako::CreateArchive(const std::vector<FileName_t>& a_FileNames, const FileName_t& a_ArchiveName, bool a_OverwriteExistingFile)
{
    // TODO: Sort a_FileNames alphabetically

    if (!a_OverwriteExistingFile)
    {
        // Check if the file already exists
        if (s_FileFactory(a_ArchiveName, IFile::FileOpenMode::Read) != nullptr)
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

    std::unique_ptr<IFile> archive = s_FileFactory(a_ArchiveName, IFile::FileOpenMode::WriteTruncate);
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

    size_t totalFileSize = 0;
    // Create FileInfo objects and serialize file content to archive
    for (size_t fileIndex = 0; fileIndex < a_FileNames.size(); ++fileIndex)
    {
        std::unique_ptr<IFile> currentFile = s_FileFactory(a_FileNames[fileIndex], IFile::FileOpenMode::Read);

        FileInfo fi{};
        strcpy_s(fi.m_Name, FileInfo::MaxFileNameLength, a_FileNames[fileIndex].c_str()); // TODO: Cut off file name at last /?
        fi.m_Offset = sizeof(HakoHeader) + sizeof(FileInfo) * a_FileNames.size() + totalFileSize;

        // Serialize file into the archive
        fi.m_Size = SerializeFile(archive.get(), fi);
        totalFileSize += fi.m_Size;
        
        // Write file info to the archive
        WriteToArchive(archive.get(), &fi, sizeof(FileInfo), archiveInfoBytesWritten);
        archiveInfoBytesWritten += sizeof(FileInfo);
    }

    return true;
}

bool Hako::OpenArchive(const FileName_t& a_ArchiveName)
{
    m_OpenedFiles.clear();
    m_FilesInArchive.clear();
#ifdef HAKO_READ_OUTSIDE_OF_ARCHIVE
    m_OpenedFilesOutsideArchive.clear();
#endif
    
    // Open archive
    m_ArchiveReader = s_FileFactory(a_ArchiveName, IFile::FileOpenMode::Read);
    if (m_ArchiveReader == nullptr)
    {
        std::cout << "Unable to open archive " << a_ArchiveName << " for reading!" << std::endl;
        assert(m_ArchiveReader == nullptr);
        return false;
    }

    std::vector<char> buffer{};
    buffer.resize(sizeof(HakoHeader));
    m_ArchiveReader->Read(sizeof(HakoHeader), 0, buffer);
    size_t bytesRead = sizeof(HakoHeader);

    const HakoHeader header = *reinterpret_cast<HakoHeader*>(buffer.data());
    if (strcmp(header.m_Magic, Magic) != 0)
    {
        std::cout << "The archive does not seem to a Hako archive, or the file might be corrupted." << std::endl;
        return false;
    }
    
    buffer.reserve(sizeof(FileInfo));

    for (FileCount_t fileNum = 0; fileNum < header.m_FileCount; ++fileNum)
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

const std::vector<char>* Hako::ReadFile(const FileName_t& a_FileName)
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

const void Hako::CloseFile(const FileName_t& a_FileName)
{
#ifdef HAKO_READ_OUTSIDE_OF_ARCHIVE
    m_OpenedFilesOutsideArchive.erase(a_FileName);
#endif
    m_OpenedFiles.erase(a_FileName);
}

void Hako::WriteToArchive(IFile* a_Archive, void* a_Data, size_t a_NumBytes, size_t a_WriteOffset)
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

const Hako::FileInfo* Hako::GetFileInfo(const FileName_t& a_FileName) const
{
    // TODO: Binary search

    for (auto iter = m_FilesInArchive.begin(); iter != m_FilesInArchive.end(); ++iter)
    {
        if (iter->m_Name == a_FileName)
        {
            return &(*iter);
        }
    }

    return nullptr;
}

size_t Hako::SerializeFile(IFile* a_Archive, const FileInfo& a_FileInfo)
{
    IFileSerializer* serializer = SerializerList::GetInstance().GetSerializerForFile(a_FileInfo.m_Name);

    if (serializer != nullptr)
    {
        std::vector<char> data{};
        const size_t serializedByteCount = serializer->SerializeFile(a_FileInfo.m_Name, data);
        data.resize(serializedByteCount);
        a_Archive->Write(a_FileInfo.m_Offset, data);
        return serializedByteCount;
    }
    else
    {
        return DefaultSerializeFile(a_Archive, a_FileInfo.m_Offset, a_FileInfo.m_Name);
    }
}

size_t Hako::DefaultSerializeFile(IFile* a_Archive, size_t a_ArchiveWriteOffset, const FileName_t& a_FileName)
{
    std::vector<char> data{};

    std::unique_ptr<IFile> file = s_FileFactory(a_FileName, IFile::FileOpenMode::Read);
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

const std::vector<char>* Hako::ReadFileOutsideArchive(const FileName_t& a_FileName)
{
    // Check if the file has already been opened
    const auto openedFile = m_OpenedFilesOutsideArchive.find(a_FileName);
    if (openedFile != m_OpenedFilesOutsideArchive.end())
    {
        return &openedFile->second;
    }

    // Try to open the file
    std::unique_ptr<IFile> file = s_FileFactory(a_FileName, IFile::FileOpenMode::Read);

    if (file == nullptr)
    {
        // File doesn't exist outside of the archive, or it can't be opened
        return nullptr;
    }

    std::vector<char>& data = m_OpenedFilesOutsideArchive[a_FileName];
    
    // Serialize file
    IFileSerializer* serializer = SerializerList::GetInstance().GetSerializerForFile(a_FileName);
    if (serializer != nullptr)
    {
        // Close the file to prevent possible issues
        file = nullptr;

        const size_t serializedByteCount = serializer->SerializeFile(a_FileName, data);
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

const std::vector<char>* hako::Hako::LoadFileContent(const FileInfo& a_FileInfo)
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
    if (data.size() != 0)
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
