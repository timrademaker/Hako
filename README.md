# Hako
 Content packer or something

 # Output Platforms
 Hako comes with a generic list of possible asset export platforms in `inc/HakoPlatforms.h`. Change and expand this list as you see fit.
 You can also use your own enum (or anything else), but be sure to note that the standalone version of Hako compares the command-line value for the target platform to the values in `PlatformNames[]`.

# Optional Defines
## HAKO_READ_OUTSIDE_OF_ARCHIVE
When defined, Hako will try to find files outside of the archive before checking if the file exists inside the archive.
This prevents the need to rebuild the entire archive every time you want to test a change to a file.

The read file passes through the serialization process as if it was exported to the archive file, so this only works on Windows by default. If needed, support for different platforms can be added to `private/SerializerList.h` and `private/SerializerList.cpp`.
Note that the application should call `Hako::SetCurrentPlatform()` to make sure that files are serialized for the correct platform when they are loaded from outside the archive.
