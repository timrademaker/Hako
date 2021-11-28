#include "HakoFile.h"

using namespace hako;

HakoFile::HakoFile(const std::string& a_FilePath, IFile::FileOpenMode a_FileOpenMode)
{
	int openFlags = 0;
	if (a_FileOpenMode == IFile::FileOpenMode::Read)
	{
		openFlags = std::ios::in;
	}
	else if (a_FileOpenMode == IFile::FileOpenMode::WriteTruncate)
	{
		openFlags = std::ios::out | std::ios::trunc;
	}
	else if (a_FileOpenMode == IFile::FileOpenMode::WriteAppend)
	{
		openFlags = std::ios::out | std::ios::app;
	}

	openFlags |= std::ios::binary;

	m_FileHandle = std::make_unique<std::fstream>(a_FilePath, openFlags);
}

HakoFile::~HakoFile()
{
	CloseFile();
}

bool HakoFile::Read(size_t a_NumBytes, size_t a_Offset, std::vector<char>& a_Buffer)
{
	m_FileHandle->seekg(a_Offset, std::ios_base::beg);
	m_FileHandle->read(a_Buffer.data(), a_NumBytes);

	return !m_FileHandle->fail();
}

size_t HakoFile::GetFileSize()
{
	m_FileHandle->seekg(0, std::ios_base::end);

	return m_FileHandle->tellg();
}

#ifndef HAKO_READ_ONLY
bool HakoFile::Write(size_t a_Offset, const std::vector<char>& a_Data)
{
	m_FileHandle->seekp(a_Offset, std::ios_base::beg);
	m_FileHandle->write(a_Data.data(), a_Data.size());
	return !m_FileHandle->fail();
}
#endif

void HakoFile::CloseFile()
{
	if (m_FileHandle != nullptr)
	{
		if (m_FileHandle->is_open())
		{
			m_FileHandle->close();
		}
	}
}

std::unique_ptr<IFile> hako::HakoFileFactory(const std::string& a_FilePath, IFile::FileOpenMode a_FileOpenMode)
{
	return std::make_unique<HakoFile>(a_FilePath, a_FileOpenMode);
}
