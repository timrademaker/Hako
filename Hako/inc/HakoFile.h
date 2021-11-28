#pragma once

#include "IFile.h"
#include "Hako.h"

#include <fstream>
#include <iosfwd>

namespace hako
{
    class HakoFile : public IFile
    {
    public:
        HakoFile(const std::string& a_FilePath, IFile::FileOpenMode a_FileOpenMode);
        virtual ~HakoFile();

        virtual bool Read(size_t a_NumBytes, size_t a_Offset, std::vector<char>& a_Buffer) override;
        virtual size_t GetFileSize() override;
#ifndef HAKO_READ_ONLY
        virtual bool Write(size_t a_Offset, const std::vector<char>& a_Data) override;
#endif

    private:
        void CloseFile();

    private:
        std::fstream* m_FileHandle;
    };

    std::unique_ptr<IFile> HakoFileFactory(const std::string& a_FilePath, IFile::FileOpenMode a_FileOpenMode);
}
