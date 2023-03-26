#include "SerializerList.h"

#include "HakoLog.h"

#if defined(_WIN32) && !defined(HAKO_NO_DYNAMIC_SERIALIZERS)
#include <fileapi.h>
#include <shellapi.h>

#include <filesystem>
#endif

namespace hako
{
#if !defined(HAKO_NO_DYNAMIC_SERIALIZERS)
#if defined(_WIN32)
    void FindDllFilesInDirectory(std::filesystem::path a_Directory, std::vector<std::wstring>& a_OutFileList)
    {
        WIN32_FIND_DATAW fileData{};
        a_Directory /= L"*.dll";

        auto const fileHandle = FindFirstFileW(a_Directory.c_str(), &fileData);
        if (fileHandle == INVALID_HANDLE_VALUE)
        {
            hako::Log("No serialization DLLs found in \"%ls\"\n", std::filesystem::absolute(a_Directory.parent_path()).c_str());
            return;
        }

        do
        {
            a_OutFileList.emplace_back(fileData.cFileName);
        } while (FindNextFileW(fileHandle, &fileData) != 0);

        if (GetLastError() != ERROR_NO_MORE_FILES)
        {
            hako::Log("An error occurred while trying to get all DLLs in \"%ls\"\n", std::filesystem::absolute(a_Directory.parent_path()).c_str());
        }

        FindClose(fileHandle);
    }

    void GatherSerializationDLLs(std::vector<std::wstring>& a_OutFileList)
    {
        // Find DLLs in current working directory
        FindDllFilesInDirectory(".", a_OutFileList);

        // Find DLLs next to the Hako executable
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

            LocalFree(argv);
        }
    }
#endif // defined(_WIN32)
#endif // !defined(HAKO_NO_DYNAMIC_SERIALIZERS)
}

using namespace hako;

SerializerList& hako::SerializerList::GetInstance()
{
    static SerializerList instance;
    return instance;
}

void hako::SerializerList::AddSerializer(IFileSerializer* a_Serializer)
{
    m_FileSerializers.push_back(std::unique_ptr<IFileSerializer>(a_Serializer));
}

IFileSerializer* hako::SerializerList::GetSerializerForFile(char const* a_FileName, Platform a_TargetPlatform) const
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

SerializerList::SerializerList()
{
#if !defined(HAKO_NO_DYNAMIC_SERIALIZERS)
#if defined(_WIN32)
    // Find and load DLLs that (might) contain serializers
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
                AddSerializer(serializerFactoryFunc());

                m_LoadedSharedLibraries.push_back(serializerDLL);

                hako::Log(L"Loaded %ls\n", name.c_str());
            }
            else
            {
                FreeLibrary(serializerDLL);
            }
        }
    }
#endif // defined(_WIN32)
#endif // !defined(HAKO_NO_DYNAMIC_SERIALIZERS)
}

hako::SerializerList::~SerializerList()
{
    m_FileSerializers.clear();
    FreeDynamicSerializers();
}

void hako::SerializerList::FreeDynamicSerializers()
{
#if !defined(HAKO_NO_DYNAMIC_SERIALIZERS)
#if defined(_WIN32)
    for (HMODULE const& m : m_LoadedSharedLibraries)
    {
        FreeLibrary(m);
    }
    m_LoadedSharedLibraries.clear();
#endif // defined(_WIN32)
#endif // !defined(HAKO_NO_DYNAMIC_SERIALIZERS)
}

