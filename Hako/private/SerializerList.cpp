#include "SerializerList.h"

#if defined(_WIN32)
#include <fileapi.h>
#include <iostream>
#endif

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

IFileSerializer* hako::SerializerList::GetSerializerForFile(const std::string& a_FileName, Platform a_TargetPlatform)
{
    IFileSerializer* serializer = nullptr;

    // Find serializer for this file
    for (auto iter = m_FileSerializers.begin(); iter != m_FileSerializers.end(); ++iter)
    {
        if ((*iter)->ShouldHandleFile(a_FileName, a_TargetPlatform))
        {
            serializer = iter->get();
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
    FindDllFilesInWorkingDirectory(dllNames);
    
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
    for (HMODULE& m : m_LoadedSharedLibraries)
    {
        FreeLibrary(m);
    }
    m_LoadedSharedLibraries.clear();
#endif
}

void hako::SerializerList::FindDllFilesInWorkingDirectory(std::vector<std::wstring>& a_OutFileList) const
{
#if defined(_WIN32)
    WIN32_FIND_DATAW fileData{};
    auto fileHandle = FindFirstFileW(L"*.dll", &fileData);
    if (fileHandle == INVALID_HANDLE_VALUE)
    {
        std::cout << "No DLLs found in working directory" << std::endl;
        return;
    }

    do
    {
        a_OutFileList.push_back(fileData.cFileName);
    } while (FindNextFileW(fileHandle, &fileData) != 0);

    if (GetLastError() != ERROR_NO_MORE_FILES)
    {
        std::cout << "An error occurred while trying to get all DLLs in the working directory" << std::endl;
    }

    FindClose(fileHandle);
#endif
}

