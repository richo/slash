#include "value.h"
#include "vm.h"
#include "class.h"
#include "string.h"
#include "object.h"
#include "lib/bignum.h"
#include <gc.h>
#include <stdlib.h>
#include <time.h>

static SLVAL
object_exit(sl_vm_t* vm, SLVAL self, size_t argc, SLVAL* argv)
{
    SLVAL exit_code = sl_make_int(vm, 0);
    if(argc) {
        exit_code = argv[0];
    }
    sl_exit(vm, exit_code);
    return self; /* never reached */
}

static SLVAL
object_strftime(sl_vm_t* vm, SLVAL self, SLVAL format, SLVAL time)
{
    time_t t;
    struct tm* tm_ptr;
    size_t capacity = 256;
    char* fmt = sl_to_cstr(vm, format);
    char* buff = GC_MALLOC(capacity);
    if(sl_is_a(vm, time, vm->lib.Int)) {
        t = sl_get_int(time);
    } else if(sl_is_a(vm, time, vm->lib.Bignum)) {
        t = sl_bignum_get_long(vm, time);
    } else {
        sl_expect(vm, time, vm->lib.Int);
    }
    tm_ptr = gmtime(&t);
    while(strftime(buff, capacity, fmt, tm_ptr) == 0) {
        capacity *= 2;
        buff = GC_REALLOC(buff, capacity);
    }
    return sl_make_cstring(vm, buff);
    (void)self; /* never reached */
}

void
sl_init_system(sl_vm_t* vm)
{
    sl_define_method(vm, vm->lib.Object, "exit", -1, object_exit);
    sl_define_method(vm, vm->lib.Object, "strftime", 2, object_strftime);
}