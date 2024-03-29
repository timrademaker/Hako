#include "HakoFile.h"

#include <cassert>
#include <filesystem>

using namespace hako;

HakoFile::~HakoFile()
{
	CloseFile();
}

bool HakoFile::Open(std::string const& a_FilePath, FileOpenMode a_FileOpenMode)
{
	int openFlags = 0;
	if (a_FileOpenMode == FileOpenMode::Read)
	{
		openFlags = std::ios::in;
	}
	else if (a_FileOpenMode == FileOpenMode::WriteTruncate)
	{
		openFlags = std::ios::out | std::ios::trunc;
	}
	else if (a_FileOpenMode == FileOpenMode::WriteAppend)
	{
		openFlags = std::ios::out | std::ios::app;
	}

	openFlags |= std::ios::binary;

	m_FileHandle = std::make_unique<std::fstream>(a_FilePath, openFlags);

	return m_FileHandle->good();
}

bool HakoFile::Read(size_t a_NumBytes, size_t a_Offset, std::vector<char>& a_Buffer)
{
	assert(m_FileHandle != nullptr);

	m_FileHandle->seekg(a_Offset, std::ios_base::beg);
	m_FileHandle->read(a_Buffer.data(), a_NumBytes);

	return !m_FileHandle->fail();
}

size_t HakoFile::GetFileSize()
{
	assert(m_FileHandle != nullptr);

	m_FileHandle->seekg(0, std::ios_base::end);

	return m_FileHandle->tellg();
}

bool HakoFile::Write(size_t a_Offset, std::vector<char> const& a_Data)
{
	assert(m_FileHandle != nullptr);

	m_FileHandle->seekp(a_Offset, std::ios_base::beg);
	m_FileHandle->write(a_Data.data(), a_Data.size());
	return !m_FileHandle->fail();
}

void HakoFile::CloseFile()
{
	if (m_FileHandle != nullptr)
	{
		if (m_FileHandle->is_open())
		{
			m_FileHandle->close();
		}

		m_FileHandle = nullptr;
	}
}

std::unique_ptr<IFile> hako::HakoFileFactory(std::string const& a_FilePath, FileOpenMode a_FileOpenMode)
{
	std::unique_ptr<HakoFile> file = std::make_unique<HakoFile>();
	if (file->Open(a_FilePath, a_FileOpenMode))
	{
		return file;
	}
	return nullptr;
}
