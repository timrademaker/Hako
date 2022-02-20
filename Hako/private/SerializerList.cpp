#include "SerializerList.h"

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

IFileSerializer* hako::SerializerList::GetSerializerForFile(const std::string& a_FileName)
{
    IFileSerializer* serializer = nullptr;

    // Find serializer for this file
    for (auto iter = m_FileSerializers.begin(); iter != m_FileSerializers.end(); ++iter)
    {
        if ((*iter)->ShouldHandleFile(a_FileName))
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
    if (m_HasGatheredDynamicSerializers)
    {
        return;
    }
    m_HasGatheredDynamicSerializers = true;

    // TODO: Find all dlls in working directory and loop over them
    std::vector<std::string> dllNames;

    for (auto& name : dllNames)
    {
        typedef hako::IFileSerializer* (*SerializerFactory_t)();
        auto serializerDLL = LoadLibrary(name.c_str());
        if (serializerDLL)
        {
            auto serializerFactoryFunc = reinterpret_cast<SerializerFactory_t>(GetProcAddress(serializerDLL, "CreateHakoSerializer"));
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
}

void hako::SerializerList::FreeDynamicSerializers()
{
    for (HMODULE& m : m_LoadedSharedLibraries)
    {
        FreeLibrary(m);
    }
    m_LoadedSharedLibraries.clear();
}

