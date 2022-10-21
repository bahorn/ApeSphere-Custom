#include <stdarg.h>
#include <mkb.h>

#include "heap.h"

#include "vm/vm.h"
#include "vm/vm_util.h"

VM::VM()
{
    active = false;
}


/* Load file into the VM */
bool VM::load(char *bytecode_path)
{
    mkb::DVDFileInfo bytecode_file_info;
    bool open_success = mkb::DVDOpen(bytecode_path, &bytecode_file_info);
    if (!open_success) {
        return false;
    }
    mkb::OSReport("Successfully Read Bytecode!\n");
    char *bytecode_file_buf = static_cast<char*>(
        heap::alloc(bytecode_file_info.length)
    );
    u32 read_length = mkb::read_entire_file_using_dvdread_prio_async(
        &bytecode_file_info, bytecode_file_buf, bytecode_file_info.length, 0
    );

    VM_Create(
        &vm,
        bytecode_path,
        (uint8_t *)bytecode_file_buf,
        read_length,
        systemCalls
    );

    active = true;

    return true;
}



/* Call the VM */
int VM::call(int command, ...)
{
    int result;
    va_list args;
    va_start(args, 0);

    result = VM_Call(&vm, command, args);
    return result;
}
