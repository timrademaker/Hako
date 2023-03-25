# Hako
Content packing framework for game assets.

# Output Platforms
Hako comes with a generic list of possible asset export platforms in `inc/HakoPlatforms.h`. Change and expand this list as you see fit.
You can also use your own enum (or anything else), but be sure to note that the standalone version of Hako compares the command-line value for the target platform to the values in `PlatformNames[]`.

# Optional Defines
## HAKO_READ_OUTSIDE_OF_ARCHIVE
When defined, Hako will try to find files outside of the archive before checking if the file exists inside the archive.
This prevents the need to rebuild the entire archive every time you want to test a change to a file.

Hako will attempt to read the file from the intermediate directory, so be sure to pass in the intermediate path and current platform when opening an archive.

# Creating And Registering New Serializers
To create a new serializer, inherit from `hako::IFileSerializer` and implement its functions.  
Serializers that are compiled to a dll should use the macro `HAKO_ADD_DYNAMIC_SERIALIZER(SerializerClass)` in their source file to make sure Hako can use them.  
Serializers that are not exported to dynamic libraries can be registered using `hako::AddSerializer<SerializerClass>()`.
