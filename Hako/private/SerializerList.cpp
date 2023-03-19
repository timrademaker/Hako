#include "SerializerList.h"

#if defined(_WIN32)
#include <fileapi.h>
#include <shellapi.h>

#include <filesystem>
#include <iostream>
#endif

namespace hako
{
#if defined(_WIN32)
    void FindDllFilesInDirectory(std::filesystem::path a_Directory, std::vector<std::wstring>& a_OutFileList)
    {
        WIN32_FIND_DATAW fileData{};
        a_Directory /= L"*.dll";

        auto fileHandle = FindFirstFileW(a_Directory.c_str(), &fileData);
        if (fileHandle == INVALID_HANDLE_VALUE)
        {
            std::cout << "No serialization DLLs found in " << std::filesystem::absolute(a_Directory.parent_path()).generic_string() << std::endl;
            return;
        }

        do
        {
            a_OutFileList.emplace_back(fileData.cFileName);
        } while (FindNextFileW(fileHandle, &fileData) != 0);

        if (GetLastError() != ERROR_NO_MORE_FILES)
        {
            std::cout << "An error occurred while trying to get all DLLs in the working directory" << std::endl;
        }

        FindClose(fileHandle);
    }

    void GatherSerializationDLLs(std::vector<std::wstring>& a_OutFileList)
    {
        // Find DLLs in current working directory
        FindDllFilesInDirectory(".", a_OutFileList);

        // Find DLLs relative to the Hako executable
        int argc = 0;
        auto* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv != nullptr)
        {
            wchar_t const* exePath = argv[0];
            auto const relativeExePath = std::filesystem::relative(exePath, std::filesystem::current_path());
            if (relativeExePath.has_parent_path())
            {
                FindDllFilesInDirectory(relativeExePath.parent_path(), a_OutFileList);
            }
        }
        LocalFree(argv);
    }
#endif
}

using namespace hako;

SerializerList& hako::SerializerList::GetInstance()
{
    static SerializerList instance;
    if (!instance.m_HasGatheredDynamicSerializers)
    {
        instance.GatherDynamicSerializers();
    }

    return instance;
}

void hako::SerializerList::AddSerializer(IFileSerializer* a_Serializer)
{
    m_FileSerializers.push_back(std::unique_ptr<IFileSerializer>(a_Serializer));
}

IFileSerializer* hako::SerializerList::GetSerializerForFile(const std::string& a_FileName, Platform a_TargetPlatform) const
{
    IFileSerializer* serializer = nullptr;

    // Find serializer for this file
    for (auto const& m_FileSerializer : m_FileSerializers)
    {
        if (m_FileSerializer->ShouldHandleFile(a_FileName, a_TargetPlatform))
        {
            serializer = m_FileSerializer.get();
            break;
        }
    }

    return serializer;
}

hako::SerializerList::~SerializerList()
{
    m_FileSerializers.clear();
    FreeDynamicSerializers();
}

void hako::SerializerList::GatherDynamicSerializers()
{
#if defined(_WIN32)
    if (m_HasGatheredDynamicSerializers)
    {
        return;
    }
    m_HasGatheredDynamicSerializers = true;

    std::vector<std::wstring> dllNames;
    GatherSerializationDLLs(dllNames);
    
    for (auto& name : dllNames)
    {
        typedef hako::IFileSerializer* (*SerializerFactory_t)();
        auto serializerDLL = LoadLibraryW(name.c_str());
        if (serializerDLL)
        {
            auto serializerFactoryFunc = reinterpret_cast<SerializerFactory_t>(GetProcAddress(serializerDLL, s_FactoryFunctionName));
            if (serializerFactoryFunc)
            {
                hako::IFileSerializer* s = serializerFactoryFunc();
                m_FileSerializers.push_back(std::unique_ptr<hako::IFileSerializer>(s));

                m_LoadedSharedLibraries.push_back(serializerDLL);

                std::wcout << L"Loaded " << name << std::endl;
            }
            else
            {
                FreeLibrary(serializerDLL);
            }
        }
    }
#endif
}

void hako::SerializerList::FreeDynamicSerializers()
{
#if defined(_WIN32)
    for (HMODULE const& m : m_LoadedSharedLibraries)
    {
        FreeLibrary(m);
    }
    m_LoadedSharedLibraries.clear();
#endif
}

