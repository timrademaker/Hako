#include "HakoCmd.h"

#ifdef HAKO_STANDALONE
int main(int argc, char* argv[])
{
    return hako::CmdEntryPoint(argc, argv);
}
#endif
