#pragma once

namespace hako
{
    /**
     * Entry point for command-line based Hako
     * Can be used when you want to use Hako as a command-line tool, but you want to do some things before serializing/archiving (e.g. registering static serializers)
     */
    int CmdEntryPoint(int argc, char* argv[]);
}
