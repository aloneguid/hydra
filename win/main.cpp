#include "global.h"

using namespace std;

// global.h
hydra::machine hydra::mac;

int wmain(int argc, wchar_t* argv[], wchar_t* envp[]) {
   
    hydra::mac.win32_run();

    return 0;
}