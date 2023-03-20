#pragma once

#include "IFileSerializer.h"

#include <memory>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <libloaderapi.h>
#endif


namespace hako
{
    class SerializerList final
    {
        static constexpr char s_FactoryFunctionName[] = "CreateHakoSerializer";

    public:
        static SerializerList& GetInstance();

        SerializerList(SerializerList&) = delete;
        SerializerList(SerializerList const&) = delete;
        SerializerList& operator=(const SerializerList&) = delete;
        SerializerList(SerializerList&&) = delete;
        SerializerList& operator=(SerializerList&&) = delete;

        /**
         * Add a serializer that can be used for file serialization
         * @param a_Serializer The serializer to add. Takes ownership of the serializer.
         */
        void AddSerializer(IFileSerializer* a_Serializer);

        /**
        * Get the serializer to use for a file
        * @param a_FileName The file that needs to be serialized
        * @param a_TargetPlatform The platform the file should be serialized for
        * @return A non-owning pointer to the serializer if one was found, or a nullptr if no serializer could be found.
        */
        IFileSerializer* GetSerializerForFile(const std::string& a_FileName, Platform a_TargetPlatform) const;

    private:
        SerializerList();
        ~SerializerList();

        void FreeDynamicSerializers();

    private:
        /** All file serializers provided by the user */
        std::vector<std::unique_ptr<IFileSerializer>> m_FileSerializers;

#if defined(_WIN32)
        std::vector<HMODULE> m_LoadedSharedLibraries;
#endif
    };
}