#include <stdlib.h>
#include <string.h>
#include <slash.h>

typedef struct {
    sl_object_t base;
    int inspecting;
    size_t count;
    size_t capacity;
    SLVAL* items;
}
sl_array_t;

typedef struct {
    sl_object_t base;
    SLVAL* items;
    size_t count;
    size_t at;
}
sl_array_enumerator_t;

static sl_object_t*
allocate_array(sl_vm_t* vm)
{
    size_t i;
    sl_array_t* ary = sl_alloc(vm->arena, sizeof(sl_array_t));
    ary->capacity = 2;
    ary->count = 0;
    ary->items = sl_alloc(vm->arena, sizeof(SLVAL) * ary->capacity);
    ary->base.primitive_type = SL_T_ARRAY;
    for(i = 0; i < ary->capacity; i++) {
        ary->items[i] = vm->lib.nil;
    }
    return (sl_object_t*)ary;
}

static sl_object_t*
allocate_array_enumerator(sl_vm_t* vm)
{
    return sl_alloc(vm->arena, sizeof(sl_array_enumerator_t));
}

static sl_array_t*
get_array(sl_vm_t* vm, SLVAL array)
{
    sl_expect(vm, array, vm->lib.Array);
    return (sl_array_t*)sl_get_ptr(array);
}

static void
array_resize(sl_vm_t* vm, sl_array_t* aryp, size_t new_size)
{
    size_t i, old_capacity;
    aryp->count = new_size;
    if(new_size == 0) {
        return;
    }
    if(new_size > aryp->capacity) {
        old_capacity = aryp->capacity;
        while(new_size > aryp->capacity) {
            aryp->capacity *= 2;
        }
        aryp->items = sl_realloc(vm->arena, aryp->items, sizeof(SLVAL) * aryp->capacity);
        for(i = old_capacity; i < aryp->capacity; i++) {
            aryp->items[i] = vm->lib.nil;
        }
    } else if(new_size < aryp->capacity / 2) {
        while(new_size < aryp->capacity / 2) {
            aryp->capacity /= 2;
        }
        aryp->items = sl_realloc(vm->arena, aryp->items, sizeof(SLVAL) * aryp->capacity);
    }
}

static SLVAL
sl_array_init(sl_vm_t* vm, SLVAL array, size_t count, SLVAL* items)
{
    sl_array_t* aryp = get_array(vm, array);
    aryp->count = count;
    aryp->capacity = count > 2 ? count : 2;
    aryp->items = sl_alloc(vm->arena, sizeof(SLVAL) * aryp->capacity);
    aryp->items[0] = vm->lib.nil;
    aryp->items[1] = vm->lib.nil;
    memcpy(aryp->items, items, sizeof(SLVAL) * count);
    return vm->lib.nil;
}

static SLVAL
sl_array_to_s(sl_vm_t* vm, SLVAL array)
{
    sl_vm_frame_t frame;
    sl_array_t* aryp = get_array(vm, array);
    size_t i;
    volatile SLVAL str;
    if(aryp->inspecting) {
        return sl_make_cstring(vm, "[ <recursive> ]");
    }
    SL_ENSURE(frame, {
        aryp->inspecting = 1;
        str = sl_make_cstring(vm, "[");
        for(i = 0; i < aryp->count; i++) {
            if(i != 0) {
                str = sl_string_concat(vm, str, sl_make_cstring(vm, ", "));
            }
            str = sl_string_concat(vm, str, sl_inspect(vm, aryp->items[i]));
        }
        str = sl_string_concat(vm, str, sl_make_cstring(vm, "]"));
    }, {
        aryp->inspecting = 0;
    });
    return str;
}

static SLVAL
sl_array_enumerate(sl_vm_t* vm, SLVAL array)
{
    return sl_new(vm, vm->lib.Array_Enumerator, 1, &array);
}

static SLVAL
sl_array_enumerator_init(sl_vm_t* vm, SLVAL self, SLVAL array)
{
    sl_array_enumerator_t* e = (sl_array_enumerator_t*)sl_get_ptr(self);
    sl_array_t* a = get_array(vm, array);
    e->items = sl_alloc(vm->arena, sizeof(SLVAL) * a->count);
    memcpy(e->items, a->items, sizeof(SLVAL) * a->count);
    e->at = 0;
    e->count = a->count;
    return vm->lib.nil;
}

static SLVAL
sl_array_enumerator_next(sl_vm_t* vm, SLVAL self)
{
    sl_array_enumerator_t* e = (sl_array_enumerator_t*)sl_get_ptr(self);
    if(!e->items) {
        sl_throw_message2(vm, vm->lib.Error, "Invalid operation on Array::Enumerator");
    }
    if(e->at > e->count) {
        return vm->lib._false;
    } else if(e->at == e->count) {
        e->at++;
        return vm->lib._false;
    } else {
        e->at++;
        return vm->lib._true;
    }
}

static SLVAL
sl_array_enumerator_current(sl_vm_t* vm, SLVAL self)
{
    sl_array_enumerator_t* e = (sl_array_enumerator_t*)sl_get_ptr(self);
    if(!e->items) {
        sl_throw_message2(vm, vm->lib.Error, "Invalid operation on Array::Enumerator");
    }
    if(e->at == 0 || e->at > e->count) {
        sl_throw_message2(vm, vm->lib.Error, "Invalid operation on Array::Enumerator");
    }
    return e->items[e->at - 1];
}

static SLVAL
sl_array_hash(sl_vm_t* vm, SLVAL self)
{
    sl_array_t* ary = get_array(vm, self);
    int hash = 0;
    size_t i;
    for(i = 0; i < ary->count; i++) {
        hash ^= sl_hash(vm, ary->items[i]);
    }
    return sl_make_int(vm, hash % SL_MAX_INT);
}

static SLVAL
sl_array_to_a(sl_vm_t* vm, SLVAL self)
{
    return self;
    (void)vm; /* never reached */
}

static SLVAL
sl_array_eq(sl_vm_t* vm, SLVAL self, SLVAL other)
{
    if(!sl_is_a(vm, other, vm->lib.Array)) {
        return vm->lib._false;
    }
    sl_array_t* ary = get_array(vm, self);
    sl_array_t* oth = get_array(vm, other);
    if(ary->count != oth->count) {
        return vm->lib._false;
    }
    for(size_t i = 0; i < ary->count; i++) {
        if(!sl_eq(vm, ary->items[i], oth->items[i])) {
            return vm->lib._false;
        }
    }
    return vm->lib._true;
}

SLVAL
sl_array_join(sl_vm_t* vm, SLVAL self, size_t argc, SLVAL* argv)
{
    sl_string_t* joiner;
    if(argc) {
        joiner = sl_get_string(vm, argv[0]);
    } else {
        joiner = sl_cstring(vm, "");
    }
    sl_array_t* ary = get_array(vm, self);
    size_t length = 0;
    SLVAL* elements = sl_alloc(vm->arena, sizeof(SLVAL) * ary->count);
    for(size_t i = 0; i < ary->count; i++) {
        if(i > 0) {
            length += joiner->buff_len;
        }
        elements[i] = sl_to_s(vm, ary->items[i]);
        length += ((sl_string_t*)sl_get_ptr(elements[i]))->buff_len;
    }
    uint8_t* buff = sl_alloc_buffer(vm->arena, length + 1);
    size_t index = 0;
    for(size_t i = 0; i < ary->count; i++) {
        if(i > 0) {
            memcpy(buff + index, joiner->buff, joiner->buff_len);
            index += joiner->buff_len;
        }
        sl_string_t* elstr = (sl_string_t*)sl_get_ptr(elements[i]);
        memcpy(buff + index, elstr->buff, elstr->buff_len);
        index += elstr->buff_len;
    }
    return sl_make_string_no_copy(vm, buff, length);
}

void
sl_init_array(sl_vm_t* vm)
{
    vm->lib.Array = sl_define_class(vm, "Array", vm->lib.Enumerable);
    sl_class_set_allocator(vm, vm->lib.Array, allocate_array);
    sl_define_method(vm, vm->lib.Array, "init", -1, sl_array_init);
    sl_define_method(vm, vm->lib.Array, "enumerate", 0, sl_array_enumerate);
    sl_define_method(vm, vm->lib.Array, "[]", 1, sl_array_get2);
    sl_define_method(vm, vm->lib.Array, "[]=", 2, sl_array_set2);
    sl_define_method(vm, vm->lib.Array, "length", 0, sl_array_length);
    sl_define_method(vm, vm->lib.Array, "push", -1, sl_array_push);
    sl_define_method(vm, vm->lib.Array, "pop", 0, sl_array_pop);
    sl_define_method(vm, vm->lib.Array, "unshift", -1, sl_array_unshift);
    sl_define_method(vm, vm->lib.Array, "shift", 0, sl_array_shift);
    sl_define_method(vm, vm->lib.Array, "to_a", 0, sl_array_to_a);
    sl_define_method(vm, vm->lib.Array, "to_s", 0, sl_array_to_s);
    sl_define_method(vm, vm->lib.Array, "inspect", 0, sl_array_to_s);
    sl_define_method(vm, vm->lib.Array, "hash", 0, sl_array_hash);
    sl_define_method(vm, vm->lib.Array, "sort", -1, sl_array_sort);
    sl_define_method(vm, vm->lib.Array, "concat", 1, sl_array_concat);
    sl_define_method(vm, vm->lib.Array, "+", 1, sl_array_concat);
    sl_define_method(vm, vm->lib.Array, "-", 1, sl_array_diff);
    sl_define_method(vm, vm->lib.Array, "==", 1, sl_array_eq);
    sl_define_method(vm, vm->lib.Array, "join", -1, sl_array_join);
    
    vm->lib.Array_Enumerator = sl_define_class3(
        vm, sl_intern(vm, "Enumerator"), vm->lib.Object, vm->lib.Array);
        
    sl_class_set_allocator(vm, vm->lib.Array_Enumerator, allocate_array_enumerator);
    sl_define_method(vm, vm->lib.Array_Enumerator, "init", 1, sl_array_enumerator_init);
    sl_define_method(vm, vm->lib.Array_Enumerator, "next", 0, sl_array_enumerator_next);
    sl_define_method(vm, vm->lib.Array_Enumerator, "current", 0, sl_array_enumerator_current);
}

SLVAL
sl_make_array(sl_vm_t* vm, size_t count, SLVAL* items)
{
    return sl_new(vm, vm->lib.Array, count, items);
}

SLVAL
sl_array_get(sl_vm_t* vm, SLVAL array, int i)
{
    return sl_array_get2(vm, array, sl_make_int(vm, i));
}

SLVAL
sl_array_get2(sl_vm_t* vm, SLVAL array, SLVAL vi)
{
    int i = sl_get_int(sl_expect(vm, vi, vm->lib.Int));
    sl_array_t* aryp = get_array(vm, array);
    if(i < 0) {
        i += aryp->count;
    }
    if(i < 0 || (size_t)i >= aryp->count) {
        return vm->lib.nil;
    }
    return aryp->items[i];
}

SLVAL
sl_array_set(sl_vm_t* vm, SLVAL array, int i, SLVAL val)
{
    return sl_array_set2(vm, array, sl_make_int(vm, i), val);
}

SLVAL
sl_array_set2(sl_vm_t* vm, SLVAL array, SLVAL vi, SLVAL val)
{
    int i = sl_get_int(sl_expect(vm, vi, vm->lib.Int));
    sl_array_t* aryp = get_array(vm, array);
    if(i < 0) {
        i += aryp->count;
    }
    if(i < 0) {
        sl_throw_message2(vm, vm->lib.ArgumentError, "index too small");
    }
    if((size_t)i >= aryp->capacity) {
        array_resize(vm, aryp, (size_t)i + 1);
    }
    if((size_t)i >= aryp->count) {
        aryp->count = (size_t)i + 1;
    }
    return aryp->items[i] = val;
}

SLVAL
sl_array_length(sl_vm_t* vm, SLVAL array)
{
    return sl_make_int(vm, get_array(vm, array)->count);
}

SLVAL
sl_array_push(sl_vm_t* vm, SLVAL array, size_t count, SLVAL* items)
{
    sl_array_t* aryp = get_array(vm, array);
    size_t i, sz = aryp->count;
    array_resize(vm, aryp, aryp->count + count);
    for(i = 0; i < count; i++) {
        aryp->items[sz + i] = items[i];
    }
    return sl_make_int(vm, aryp->count);
}

SLVAL
sl_array_pop(sl_vm_t* vm, SLVAL array)
{
    sl_array_t* aryp = get_array(vm, array);
    if(aryp->count > 0) {
        SLVAL val = aryp->items[aryp->count - 1];
        array_resize(vm, aryp, aryp->count - 1);
        return val;
    } else {
        return vm->lib.nil;
    }
}

SLVAL
sl_array_unshift(sl_vm_t* vm, SLVAL array, size_t count, SLVAL* items)
{
    sl_array_t* aryp = get_array(vm, array);
    size_t i, sz = aryp->count;
    array_resize(vm, aryp, aryp->count + count);
    memmove(aryp->items + count, aryp->items, sz * sizeof(SLVAL));
    for(i = 0; i < count; i++) {
        aryp->items[i] = items[i];
    }
    return sl_make_int(vm, aryp->count);
}

SLVAL
sl_array_shift(sl_vm_t* vm, SLVAL array)
{
    sl_array_t* aryp = get_array(vm, array);
    if(aryp->count > 0) {
        SLVAL val = aryp->items[0];
        memmove(aryp->items, aryp->items + 1, (aryp->count - 1) * sizeof(SLVAL));
        array_resize(vm, aryp, aryp->count - 1);
        return val;
    } else {
        return vm->lib.nil;
    }
}

static void
quicksort(sl_vm_t* vm, SLVAL* items, size_t count, SLVAL* comparator)
{
    size_t pivot_idx, m, i;
    SLVAL pivot, tmp;
    if(count < 2) return;
    pivot_idx = sl_rand(vm) % count;
    pivot = items[pivot_idx];
    items[pivot_idx] = items[count - 1];
    items[count - 1] = pivot;
    m = 0;
    for(i = 0; i < count - 1; i++) {
        int cmp_result;
        if(comparator) {
            SLVAL cmpv = sl_expect(vm, sl_send_id(vm, *comparator, vm->id.call, 2, items[i], pivot), vm->lib.Int);
            cmp_result = sl_get_int(cmpv);
        } else {
            cmp_result = sl_cmp(vm, items[i], pivot);
        }
        if(cmp_result < 0) {
            tmp = items[i];
            items[i] = items[m];
            items[m] = tmp;
            m++;
        }
    }
    tmp = items[m];
    items[m] = items[count - 1];
    items[count - 1] = tmp;
    quicksort(vm, items, m, comparator);
    quicksort(vm, items + m + 1, count - m - 1, comparator);
}

SLVAL
sl_array_sort(sl_vm_t* vm, SLVAL array, size_t argc, SLVAL* argv)
{
    sl_array_t* aryp = get_array(vm, array);
    sl_array_t* copy = get_array(vm, sl_make_array(vm, aryp->count, aryp->items));
    quicksort(vm, copy->items, copy->count, argc ? &argv[0] : NULL);
    return sl_make_ptr((sl_object_t*)copy);
}

size_t
sl_array_items_no_copy(sl_vm_t* vm, SLVAL array, SLVAL** items)
{
    sl_array_t* aryp = get_array(vm, array);
    *items = aryp->items;
    return aryp->count;
}

size_t
sl_array_items(sl_vm_t* vm, SLVAL array, SLVAL** items)
{
    sl_array_t* aryp = get_array(vm, array);
    *items = sl_alloc(vm->arena, sizeof(SLVAL) * aryp->count);
    memcpy(*items, aryp->items, sizeof(SLVAL) * aryp->count);
    return aryp->count;
}

SLVAL
sl_array_concat(sl_vm_t* vm, SLVAL array, SLVAL other)
{
    sl_array_t* arrayp = get_array(vm, array);
    sl_array_t* otherp = get_array(vm, other);
    sl_array_t* new_ary = get_array(vm, sl_allocate(vm, vm->lib.Array));
    new_ary->capacity = arrayp->count + otherp->count;
    new_ary->count = new_ary->capacity;
    new_ary->items = sl_alloc(vm->arena, sizeof(SLVAL) * new_ary->capacity);
    memcpy(new_ary->items, arrayp->items, sizeof(SLVAL) * arrayp->count);
    memcpy(new_ary->items + arrayp->count, otherp->items, sizeof(SLVAL) * otherp->count);
    return sl_make_ptr((sl_object_t*)new_ary);
}


SLVAL
sl_array_diff(sl_vm_t* vm, SLVAL array, SLVAL other)
{
    size_t i;
    size_t j = 0;
    sl_array_t* arrayp = get_array(vm, array);
    sl_array_t* otherp = get_array(vm, other);
    sl_array_t* new_ary = get_array(vm, sl_allocate(vm, vm->lib.Array));
    sl_st_table_t* other_table = sl_st_init_table(vm, &dict_hash_type);

    new_ary->capacity = arrayp->capacity;
    new_ary->items = sl_alloc(vm->arena, sizeof(SLVAL) * new_ary->capacity);

    for(i = 0; i < otherp->count; i++) {
        sl_st_insert(other_table, (sl_st_data_t)sl_get_ptr(otherp->items[i]), (sl_st_data_t)1);
    }

    for(i = 0; i < arrayp->count; i++) {
        if(!sl_st_lookup(other_table, (sl_st_data_t)sl_get_ptr(arrayp->items[i]), NULL)) {
            new_ary->items[j] = arrayp->items[i];
            j++;
        }
    }

    new_ary->count = j;

    return sl_make_ptr((sl_object_t*)new_ary);
}
