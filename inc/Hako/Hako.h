#pragma once

#include "HakoPlatforms.h"
#include "IFile.h"
#include "Serializer.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace hako
{
    inline constexpr char DefaultIntermediateDirectory[] = "HakoIntermediate";

    using FileFactorySignature = std::function<std::unique_ptr<IFile>(char const* a_FilePath, FileOpenMode a_FileOpenMode)>;

    struct ResourcePathHash
    {
        static ResourcePathHash FromString(char const* a_Hash);
        [[nodiscard]] std::string ToString() const;
        /**
         * @param a_OutBuffer Buffer is expected to be (at least) MaxResourcePathHashLength in length.
         */
        void ToString(char* a_OutBuffer) const;

        bool operator==(ResourcePathHash const& a_Rhs) const
        {
            return hash64[0] == a_Rhs.hash64[0] && hash64[1] == a_Rhs.hash64[1];
        }

        bool operator!=(ResourcePathHash const& a_Rhs) const
        {
            return !(*this == a_Rhs);
        }

        uint64_t hash64[2]{};
    };

    /**
     * Set a factory function to be used for opening files. The function is expected to return a nullptr if the file could not be opened
     * @param a_FileFactory A file factory matching the function signature of FileFactorySignature
     */
    void SetFileIO(FileFactorySignature a_FileFactory);

    /**
     * Add a serializer to use for serialization
     */
    inline void AddSerializer(Serializer const& a_FileSerializer)
    {
        extern void AddSerializer_Internal(Serializer);
        AddSerializer_Internal(a_FileSerializer);
    }

    /**
     * Set the intermediate directory to which assets will be exported.
     * If intermediate file reading is enabled, intermediate assets will also be read from here.
     * @param a_IntermediateDirectory The directory to which intermediate files should be exported
     */
    void SetIntermediateDirectory(char const* a_IntermediateDirectory);

    /**
     * Create an archive
     * @param a_TargetPlatform The platform for which to create the archive
     * @param a_ArchiveName The name of the archive to output
     * @param a_OverwriteExistingFile If a file with the provided name already exists, a value of true will result in this file being overwritten
     * @return True if the archive was created successfully
     */
    bool CreateArchive(Platform a_TargetPlatform, char const* a_ArchiveName, bool a_OverwriteExistingFile = false);

    /**
     * Serialize a file or the content of a directory into the intermediate directory
     * @param a_TargetPlatform The platform for which to serialize the file
     * @param a_Path The file or directory to serialize
     * @param a_ForceSerialization If true, serialize files regardless of whether they were changed since they were last serialized
     * @param a_FileExt When set, only serialize assets with the given file extension if a_Path is a directory
     * @return True if the file was serialized successfully
     */
    bool Serialize(Platform a_TargetPlatform, char const* a_Path, bool a_ForceSerialization = false, char const* a_FileExt = nullptr);

    /**
     * Export an in-memory resource.
     * This could be useful if a resource is embedded in another resource, but you do not want to save it as such.
     * An example of this would be a texture embedded into a gltf model.
     * @param a_TargetPlatform The platform for which to export the file
     * @param a_ResourceName The name of the resource to export
     * @param a_Data The data to export
     */
    bool ExportResource(Platform a_TargetPlatform, char const* a_ResourceName, std::vector<char> const& a_Data);

    /**
     * Get a hash for a resource path or name
     * @param a_Path The path or name of the resource
     * @param a_OutHash The hash for a_Path (out)
     */
    void GetResourcePathHash(char const* a_Path, ResourcePathHash& a_OutHash);

    class Archive final
    {
    public:
        struct FileInfo
        {
            ResourcePathHash m_ResourcePathHash{};
            char m_Padding[7]{};
            size_t m_Size = 0;
            size_t m_Offset = 0;
        };
        static_assert(sizeof(FileInfo) == 40 && "FileInfo size changed");

    public:
        Archive() = default;
        Archive(char const* a_ArchivePath, char const* a_IntermediateDirectory = nullptr, Platform a_Platform = Platform::Windows);
        ~Archive() = default;

        Archive(Archive&) = delete;
        Archive(Archive const&) = delete;
        Archive& operator=(Archive const&) = delete;
        Archive(Archive&&) = delete;
        Archive& operator=(Archive&&) = delete;

        /**
         * Open an archive for reading
         * @param a_ArchivePath The path to the archive to open
         * @param a_IntermediateDirectory The directory in which intermediate files are located. Overrides whatever directory was passed to SetIntermediateDirectory.
         * @param a_Platform The current platform. Used when intermediate file reading is enabled.
         */
        void Open(char const* a_ArchivePath, char const* a_IntermediateDirectory = nullptr, Platform a_Platform = Platform::Windows);
        /**
         * Close the archive
         */
        void Close();

        /**
         * Read the content of an archived file from the archive
         * @param a_FileName The file to read from the archive
         * @param a_OutData The vector to read data into
         * @return True if the file was successfully read
         */
        bool ReadFile(char const* a_FileName, std::vector<char>& a_OutData) const;

        /**
         * Read the content of an archived file from the archive
         * @param a_ResourcePathHash The hash of the file to read from the archive
         * @param a_OutData The vector to read data into
         * @return True if the file was successfully read
         */
        bool ReadFile(ResourcePathHash const& a_ResourcePathHash, std::vector<char>& a_OutData) const;

    private:
        /**
         * Get the FileInfo for a specific file
         * @param a_ResourcePathHash The file to find file info for
         * @return The file info, or a nullptr if not found
         */
        FileInfo const* GetFileInfo(ResourcePathHash const& a_ResourcePathHash) const;

        /**
         * Read a file outside of the archive as if it was placed inside the archive.
         * @param a_Hash The hash of the file to read
         * @param a_OutData The vector to read data into
         * @return True if the file was successfully read
         */
        bool ReadFileOutsideArchive(ResourcePathHash const& a_Hash, std::vector<char>& a_OutData) const;

        /**
         * Read the content of an archived file from the archive that is currently open
         * @param a_FileInfo The file info for the file that should be loaded
         * @param a_Data The vector to read data into
         * @return True if the file was successfully read
         */
        bool LoadFileContent(FileInfo const& a_FileInfo, std::vector<char>& a_Data) const;

    private:
        /** Info on all files present in the archive opened with OpenArchive() */
        std::vector<FileInfo> m_FilesInArchive;
        /** The instance of FileIO that is currently being used to read from the archive */
        std::unique_ptr<IFile> m_ArchiveReader = nullptr;

        /** Timestamp of the last time the archive was modified when we opened it */
        time_t m_LastWriteTimestamp = 0;

        /** The platform that Hako is currently being used on. Only used when reading outside of the archive. */
        Platform m_CurrentPlatform = Platform::Windows;
    };
}

#if defined(_MSC_VER)
#define HAKO_DLL_EXPORT __declspec(dllexport)
#elif defined(__GNUC__)
#define HAKO_DLL_EXPORT extern __attribute__ ((visibility ("default"))
#else
#define HAKO_DLL_EXPORT
#endif

// Macro to register serializer from dll
#define HAKO_ADD_DYNAMIC_SERIALIZER(SerializerPredicate, SerializationFunction) \
extern "C" { \
    HAKO_DLL_EXPORT hako::Serializer __stdcall CreateHakoSerializer() { \
        return hako::Serializer{SerializerPredicate, SerializationFunction}; \
    } \
}
