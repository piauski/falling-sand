#define NOB_IMPLEMENTATION
#include "./external/include/nob.h"

int main(int argc, char **argv)
{
    NOB_GO_REBUILD_URSELF(argc, argv);

    Nob_Cmd cmd = {0};
    nob_cmd_append(&cmd, "cc", "-O3");
    nob_cmd_append(&cmd, "-Wall", "-Wextra");
    nob_cmd_append(&cmd, "-Iexternal/include/raylib");
    nob_cmd_append(&cmd, "-Iexternal/include");
    nob_cmd_append(&cmd, "-o", "build/main");
    nob_cmd_append(&cmd, "src/main.c");
    nob_cmd_append(&cmd, "-Wl,-rpath=../external/lib/raylib");
    nob_cmd_append(&cmd, "-L./external/lib/raylib");
    nob_cmd_append(&cmd, "-l:libraylib.a", "-lm");
    if (!nob_cmd_run_sync(cmd))
    {
        return 1;
    }

    return 0;
}
