#pragma once

#include "IFile.h"

#include <filesystem>
#include <fstream>

namespace hako
{
    class HakoFile final : public IFile
    {
    public:
        HakoFile() = default;
        virtual ~HakoFile() override;

        bool Open(const std::string& a_FilePath, FileOpenMode a_FileOpenMode);

        virtual bool Read(size_t a_NumBytes, size_t a_Offset, std::vector<char>& a_Buffer) override;
        virtual bool Write(size_t a_Offset, const std::vector<char>& a_Data) override;
        virtual size_t GetFileSize() override;
        virtual time_t GetLastWriteTime() override;

    private:
        void CloseFile();

    private:
        std::unique_ptr<std::fstream> m_FileHandle = nullptr;
        std::filesystem::path m_FilePath{};
    };

    std::unique_ptr<IFile> HakoFileFactory(const std::string& a_FilePath, FileOpenMode a_FileOpenMode);
}
