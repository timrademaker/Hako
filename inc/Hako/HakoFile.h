#pragma once

#include "IFile.h"

#include <fstream>

namespace hako
{
    class HakoFile final : public IFile
    {
    public:
        HakoFile() = default;
        virtual ~HakoFile() override;

        bool Open(std::string const& a_FilePath, FileOpenMode a_FileOpenMode);

        virtual bool Read(size_t a_NumBytes, size_t a_Offset, std::vector<char>& a_Buffer) override;
        virtual bool Write(size_t a_Offset, std::vector<char> const& a_Data) override;
        virtual size_t GetFileSize() override;

    private:
        void CloseFile();

    private:
        std::unique_ptr<std::fstream> m_FileHandle = nullptr;
    };

    std::unique_ptr<IFile> HakoFileFactory(std::string const& a_FilePath, FileOpenMode a_FileOpenMode);
}
