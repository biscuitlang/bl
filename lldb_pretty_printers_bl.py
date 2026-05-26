# lldb_pretty_printers_bl.py
import lldb

def resolve_elem_size_from_ptr(valobj):
    real     = valobj.GetNonSyntheticValue()
    ptr_val  = real.GetChildMemberWithName('ptr')
    ptr_type = ptr_val.GetType()

    # try GetPointeeType directly first
    pointee = ptr_type.GetPointeeType()
    if pointee.IsValid() and pointee.GetByteSize() > 0:
        return pointee, pointee.GetByteSize()

    # if ptr is a typedef or opaque, dereference manually
    canonical = ptr_type.GetCanonicalType()
    pointee   = canonical.GetPointeeType()
    if pointee.IsValid() and pointee.GetByteSize() > 0:
        return pointee, pointee.GetByteSize()

    return None, 0


# ---------------------------
# String type
# ---------------------------
class StringSyntheticProvider:
    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.update()

    def update(self):
        self.len_val = self.valobj.GetChildMemberWithName('len')
        self.ptr_val = self.valobj.GetChildMemberWithName('ptr')
        self.length  = self.len_val.GetValueAsSigned() if self.len_val.IsValid() else 0
        return True

    def num_children(self):
        return 2

    def get_child_at_index(self, index):
        if index == 0: return self.len_val
        if index == 1: return self.ptr_val
        return None

    def get_child_index(self, name):
        if name == '[len]': return 0
        if name == '[ptr]': return 1
        return -1

    def has_children(self):
        return True

def string_summary(valobj, internal_dict):
    try:
        real = valobj.GetNonSyntheticValue()
        length = real.GetChildMemberWithName('len').GetValueAsSigned()
        ptr    = real.GetChildMemberWithName('ptr')
        if length <= 0 or length > 1024 * 1024:
            return '""'
        addr  = ptr.GetValueAsUnsigned()
        error = lldb.SBError()
        data  = valobj.GetProcess().ReadMemory(addr, length, error)
        if error.Success():
            return '"' + data.decode('utf-8', errors='replace') + '"'
        return '<invalid string>'
    except Exception:
        return '<invalid or corrupted string>'


class DaSyntheticProvider:
    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.update()

    def update(self):
        real           = self.valobj.GetNonSyntheticValue()
        self.len_val   = real.GetChildMemberWithName('len')
        self.ptr_val   = real.GetChildMemberWithName('ptr')
        self.alloc_val = real.GetChildMemberWithName('allocated_elems')
        self.allocator = real.GetChildMemberWithName('allocator')
        self.length    = max(0, self.len_val.GetValueAsSigned())
        self.ptr_addr  = self.ptr_val.GetValueAsUnsigned()
        self.elem_type, self.elem_size = resolve_elem_size_from_ptr(real)
        if self.elem_size == 0:
            print(f'[da] could not resolve elem size, ptr type: {self.ptr_val.GetType().GetName()}')
        return True

    def num_children(self):
        if self.elem_size == 0: return 4
        return 4 + self.length

    def get_child_at_index(self, index):
        if index == 0: return self.len_val
        if index == 1: return self.ptr_val
        if index == 2: return self.alloc_val
        if index == 3: return self.allocator
        if self.elem_size == 0 or not self.elem_type: return None
        i     = index - 4
        addr  = self.ptr_addr + i * self.elem_size
        child = self.valobj.CreateValueFromAddress(f'[{i}]', addr, self.elem_type)
        if not child.IsValid():
            print(f'[da] invalid child {i} at 0x{addr:x} type={self.elem_type.GetName()}')
        return child

    def get_child_index(self, name):
        if name == '[len]':             return 0
        if name == '[ptr]':             return 1
        if name == '[allocated_elems]': return 2
        if name == '[allocator]':       return 3
        try:   return int(name[1:-1]) + 4
        except: return -1

    def has_children(self):
        return True

def da_summary(valobj, internal_dict):
    try:
        real = valobj.GetNonSyntheticValue()
        length    = real.GetChildMemberWithName('len').GetValueAsSigned()
        allocated = real.GetChildMemberWithName('allocated_elems').GetValueAsSigned()
        return f'Array[{length}/{allocated}]'
    except Exception:
        return '<da print error>'


# ---------------------------
# Slice 'sl.*'
# ---------------------------
class SlSyntheticProvider:
    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.update()

    def update(self):
        real           = self.valobj.GetNonSyntheticValue()
        self.len_val   = real.GetChildMemberWithName('len')
        self.ptr_val   = real.GetChildMemberWithName('ptr')
        self.length    = max(0, self.len_val.GetValueAsSigned())
        self.ptr_addr  = self.ptr_val.GetValueAsUnsigned()
        self.elem_type, self.elem_size = resolve_elem_size_from_ptr(real)
        if self.elem_size == 0:
            print(f'[sl] could not resolve elem size, ptr type: {self.ptr_val.GetName()}')
        return True

    def num_children(self):
        if self.elem_size == 0: return 1
        return 1 + self.length

    def get_child_at_index(self, index):
        if index == 0: return self.len_val
        if self.elem_size == 0 or not self.elem_type: return None
        i     = index - 1
        addr  = self.ptr_addr + i * self.elem_size
        child = self.valobj.CreateValueFromAddress(f'[{i}]', addr, self.elem_type)
        if not child.IsValid():
            print(f'[sl] invalid child {i} at 0x{addr:x} type={self.elem_type.GetName()}')
        return child

    def get_child_index(self, name):
        if name == '[len]': return 0
        try:   return int(name[1:-1]) + 1
        except: return -1

    def has_children(self):
        return True

def sl_summary(valobj, internal_dict):
    try:
        real   = valobj.GetNonSyntheticValue()
        length = real.GetChildMemberWithName('len').GetValueAsSigned()
        return f'Slice[{length}]'
    except Exception as e:
        return f'<sl print error: {e}>'


# ---------------------------
# Hash table 's.{...}'
# ---------------------------
class HashTableSyntheticProvider:
    def __init__(self, valobj, internal_dict):
        self.valobj = valobj
        self.update()

    def update(self):
        real              = self.valobj.GetNonSyntheticValue()
        self.len_val      = real.GetChildMemberWithName('len')
        self.length       = max(0, self.len_val.GetValueAsSigned())
        keys_struct       = real.GetChildMemberWithName('keys')
        values_struct     = real.GetChildMemberWithName('values')
        self.keys_ptr     = keys_struct.GetChildMemberWithName('ptr')
        self.values_ptr   = values_struct.GetChildMemberWithName('ptr')
        self.key_type,   self.key_size   = resolve_elem_size_from_ptr(keys_struct)
        self.value_type, self.value_size = resolve_elem_size_from_ptr(values_struct)
        self.keys_addr    = self.keys_ptr.GetValueAsUnsigned()
        self.values_addr  = self.values_ptr.GetValueAsUnsigned()
        if self.key_size == 0:
            print(f'[ht] could not resolve key size, ptr type: {self.keys_ptr.GetType().GetName()}')
        if self.value_size == 0:
            print(f'[ht] could not resolve value size, ptr type: {self.values_ptr.GetType().GetName()}')
        return True

    def num_children(self):
        if self.key_size == 0 or self.value_size == 0: return 0
        return self.length * 2

    def get_child_at_index(self, index):
        if self.key_size == 0 or self.value_size == 0: return None
        i        = index // 2
        is_value = index % 2
        if is_value:
            addr = self.values_addr + i * self.value_size
            return self.valobj.CreateValueFromAddress(f'[{i}].value', addr, self.value_type)
        else:
            addr = self.keys_addr + i * self.key_size
            return self.valobj.CreateValueFromAddress(f'[{i}].key', addr, self.key_type)

    def get_child_index(self, name):
        try:
            parts = name.split('.')
            i     = int(parts[0][1:-1])
            return i * 2 + (1 if parts[1] == 'value' else 0)
        except: return -1

    def has_children(self):
        return self.length > 0

def hash_table_summary(valobj, internal_dict):
    try:
        real      = valobj.GetNonSyntheticValue()
        length    = real.GetChildMemberWithName('len').GetValueAsSigned()
        slots_len = real.GetChildMemberWithName('slots').GetChildMemberWithName('len').GetValueAsSigned()
        return f'Table[{length}/{slots_len}]'
    except Exception as e:
        return f'<hash table print error: {e}>'

# ---------------------------
# Register all formatters
# ---------------------------
def __lldb_init_module(debugger, internal_dict):
    mod = 'lldb_pretty_printers_bl'

    # String
    debugger.HandleCommand(f'type summary add -F {mod}.string_summary "string"')
    debugger.HandleCommand(f'type synthetic add -l {mod}.StringSyntheticProvider "string"')

    # String View
    debugger.HandleCommand(f'type summary add -F {mod}.string_summary "sl.{{s64,p.u8}}"')
    debugger.HandleCommand(f'type synthetic add -l {mod}.StringSyntheticProvider "sl.{{s64,p.u8}}"')

    # Dynamic Array
    debugger.HandleCommand(f'type summary add --regex --cascade true -F {mod}.da_summary "da\\.{{.*}}"')
    debugger.HandleCommand(f'type synthetic add --regex --cascade true -l {mod}.DaSyntheticProvider "da\\.{{.*}}"')

    # Slice
    debugger.HandleCommand(f'type summary add --regex -F {mod}.sl_summary "sl\\.{{.*}}"')
    debugger.HandleCommand(f'type synthetic add --regex -l {mod}.SlSyntheticProvider "sl\\.{{.*}}"')

    # hash table
    debugger.HandleCommand(f'type summary add --regex -F {mod}.hash_table_summary "s.{{sl\\.{{s64,p\\.s\\.103\\.Slot}}"')
    debugger.HandleCommand(f'type synthetic add --regex -l {mod}.HashTableSyntheticProvider "s.{{sl\\.{{s64,p\\.s\\.103\\.Slot}}"')

    print('[lldb_pretty_printers_bl] loaded')
