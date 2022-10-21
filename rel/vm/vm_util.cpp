#include <mkb.h>
#include "heap.h"
#include "patch.h"
#include "vm/vm_core.h"

void* Com_malloc(size_t size, vm_t* vm, vmMallocType_t type)
{
    (void)vm;
    (void)type;
    return heap::alloc(size);
}

void Com_free(void* p, vm_t* vm, vmMallocType_t type)
{
    (void)vm;
    (void)type;
    heap::free(p);
}


void Com_Error(vmErrorCode_t level, const char* error)
{
    mkb::OSReport("[q3vm] Err (%i): %s\n", level, error);
}


int systemCalls(vm_t* vm, int* args)
{
    int result = 0;
    const int id = -1 - args[0];

    switch (id)
    {
    case -1: /* PRINTF */
        mkb::OSReport("%s", (const char*)VMA(1, vm));
        return 1;

    case -2: /* ERROR */
        mkb::OSReport("[q3vm] %s\n", (const char*)VMA(1, vm));
        return 1;

    case -10: /* Peek */
        result = patch::read_word((void *)args[1]);
        //mkb::OSReport("peek @ %x : %x\n", args[1], result);
        return result;

    case -11: /* Poke */
        //mkb::OSReport("poke @ %x : %x\n", args[1], args[2]);
        return patch::write_word((void *)args[1], args[2]);

    default:
        mkb::OSReport("[q3vm] Bad system call: %i\n", id);
    }

    return 0;
}



