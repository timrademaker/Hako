#pragma once

#include "HakoPlatforms.h"

#include <vector>

#define HAKO_SHOULD_SERIALIZE_FILE_FUNC(name) bool name(char const* a_FilePath, hako::Platform a_TargetPlatform)
#define HAKO_SERIALIZE_FILE_FUNC(name) size_t name(char const* a_FilePath, hako::Platform a_TargetPlatform, std::vector<char>& a_OutBuffer)

namespace hako
{
    struct Serializer
    {
        /**
         * Signature for a function that checks if this serializer should serialize a certain file
         * @param a_FilePath The file that has to be serialized
         * @param a_TargetPlatform The platform the file should be serialized for
         * @return True if this serializer should serialize a given file
         */
        using ShouldSerializeFilePredicate = bool(char const* a_FilePath, Platform a_TargetPlatform);

        /**
         * Signature for a function that serializes a file
         * @param a_FilePath The name of the file to serialize
         * @param a_TargetPlatform The platform for which to serialize the file
         * @param a_OutBuffer The buffer to put the serialized file content into
         * @return The number of bytes written to the buffer
         */
        using SerializeFileSignature = size_t(char const* a_FilePath, Platform a_TargetPlatform, std::vector<char>& a_OutBuffer);

        ShouldSerializeFilePredicate* m_ShouldSerializeFile = nullptr;
        SerializeFileSignature* m_SerializeFile = nullptr;
    };
}
