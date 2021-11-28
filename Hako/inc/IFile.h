#pragma once

#include <vector>

namespace hako
{
    /**
     * Base class for Hako file IO
     */
    class IFile
    {
    public:
        enum class FileOpenMode : uint8_t
        {
            Read,
            WriteAppend,
            WriteTruncate
        };

    public:
        virtual ~IFile() = default;

        /**
         * Read from the opened file
         * @param a_NumBytes The number of bytes to read from the file
         * @param a_Offset The offset from the start of the file at which the file's content should be read
         * @param a_Buffer The buffer to output the read file content into
         * @return True if the file was successfully read from
         */
        virtual bool Read(size_t a_NumBytes, size_t a_Offset, std::vector<char>& a_Buffer) = 0;

        /**
         * Write data to the opened file
         * @param a_Offset The offset from the start of the file at which the file's content should be written
         * @param a_Data The data to write to the file
         * @return True if writing was successful
         */
        virtual bool Write(size_t a_Offset, const std::vector<char>& a_Data) = 0;

        /**
         * Get the size of the opened file
         * @return The size of the file (in bytes)
         */
        virtual size_t GetFileSize() = 0;
    };
}
