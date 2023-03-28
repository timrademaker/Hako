#pragma once

#include "HakoPlatforms.h"

#include <vector>

namespace hako
{
    class IFileSerializer
    {
    public:
        virtual ~IFileSerializer() = default;

        /**
         * Check if this serializer should serialize a certain file
         * @param a_FileName The file that has to be serialized
         * @param a_TargetPlatform The platform the file should be serialized for
         * @return True if this serializer should serialize a given file
         */
        virtual bool ShouldHandleFile(char const* a_FileName, Platform a_TargetPlatform) const = 0;
        /**
         * Serialize a file
         * @param a_FileName The name of the file to serialize
         * @param a_TargetPlatform The platform for which to serialize the file
         * @param a_Buffer The buffer to put the serialized file content into
         * @return The number of bytes written to the buffer
         */
        virtual size_t SerializeFile(char const* a_FileName, Platform a_TargetPlatform, std::vector<char>& a_Buffer) = 0;
    };
}