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

        void AddSerializer(IFileSerializer* a_Serializer);

        /**
        * Get the serializer to use for a file
        * @param a_FileName The file that needs to be serialized
        * @param a_TargetPlatform The platform the file should be serialized for
        * @return A non-owning pointer to the serializer if one was found, or a nullptr if no serializer could be found.
        */
        IFileSerializer* GetSerializerForFile(const std::string& a_FileName, Platform a_TargetPlatform);

    private:
        SerializerList() = default;
        ~SerializerList();

        void GatherDynamicSerializers();
        void FreeDynamicSerializers();

        void FindDllFilesInWorkingDirectory(std::vector<std::wstring>& a_OutFileList) const;

    private:
        /** All file serializers provided by the user */
        std::vector<std::unique_ptr<IFileSerializer>> m_FileSerializers;

        /** Whether the class has already gathered (or started gathering) dynamic serializers or not */
        bool m_HasGatheredDynamicSerializers = false;

#if defined(_WIN32)
        std::vector<HMODULE> m_LoadedSharedLibraries;
#endif
    };
}