#pragma once

#include "vm/vm_core.h"


class VM {
    public:
        VM();
        bool load(char *bytecode_path);
        int call(int cmd, ...);
    private:
        vm_t vm;
        bool active;
};
