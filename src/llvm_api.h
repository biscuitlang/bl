// =================================================================================================
// bl
//
// File:   llvm_api.h
// Author: Martin Dorazil
// Date:   9/21/19
//
// Copyright 2019 Martin Dorazil
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
// =================================================================================================

#ifndef BL_LLVM_API_H
#define BL_LLVM_API_H

#include "common.h"

_SHUT_UP_BEGIN
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <llvm-c/Core.h>
#include <llvm-c/DebugInfo.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Linker.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Transforms/PassBuilder.h>
#include <llvm-c/Types.h>
_SHUT_UP_END

#define BL_CODE_VIEW_VERSION 1
#define BL_DWARF_VERSION     3

#define LLVM_SRET_INDEX        0
#define LLVM_ATTR_NOINLINE     LLVMGetEnumAttributeKindForName("noinline", 8)
#define LLVM_ATTR_ALWAYSINLINE LLVMGetEnumAttributeKindForName("alwaysinline", 12)
#define LLVM_ATTR_BYVAL        LLVMGetEnumAttributeKindForName("byval", 5)
#define LLVM_ATTR_NOALIAS      LLVMGetEnumAttributeKindForName("noalias", 7)
#define LLVM_ATTR_STRUCTRET    LLVMGetEnumAttributeKindForName("sret", 4)
#define LLVM_ATTR_ALIGNMENT    LLVMGetEnumAttributeKindForName("align", 5)

#define LLVM_MEMSET_INTRINSIC_ID LLVMLookupIntrinsicID("llvm.memset", 11)
#define LLVM_MEMCPY_INTRINSIC_ID LLVMLookupIntrinsicID("llvm.memcpy", 11)

typedef enum {
	DW_ATE_adderess      = 1,
	DW_ATE_boolean       = 2,
	DW_ATE_complex_float = 3,
	DW_ATE_float         = 4,
	DW_ATE_signed        = 5,
	DW_ATE_signed_char   = 6,
	DW_ATE_unsigned      = 7,
	DW_ATE_unsigned_char = 8,
} DW_ATE_Encoding;

typedef enum {
#if LLVM_VERSION_MAJOR >= 10
#define HANDLE_DW_TAG(ID, NAME, VERSION, VENDOR, KIND) DW_TAG_##NAME = ID,
#else
#define HANDLE_DW_TAG(ID, NAME, VERSION, VENDOR) DW_TAG_##NAME = ID,
#endif
#include "llvm/BinaryFormat/Dwarf.def"
	DW_TAG_lo_user   = 0x4080,
	DW_TAG_hi_user   = 0xffff,
	DW_TAG_user_base = 0x1000 ///< Recommended base for user tags.
} DW_TAG;

#ifdef __cplusplus
extern "C" {
#endif

struct llvm_context;

typedef struct llvm_context *llvm_context_ref_t;

// We need these because LLVM C API does not provide length parameter for string names.

llvm_context_ref_t llvm_context_create(void);
void               llvm_context_dispose(llvm_context_ref_t ctx);
void               llvm_lock_context(llvm_context_ref_t ctx);
void               llvm_unlock_context(llvm_context_ref_t ctx);
LLVMTypeRef        llvm_int_type_in_context(llvm_context_ref_t ctx, s32 bitcount);
LLVMTypeRef        llvm_float_type_in_context(llvm_context_ref_t ctx);
LLVMTypeRef        llvm_double_type_in_context(llvm_context_ref_t ctx);
LLVMTypeRef        llvm_void_type_in_context(llvm_context_ref_t ctx);
LLVMTypeRef        llvm_struct_create_named(llvm_context_ref_t ctx, const str_t Name);
LLVMValueRef       llvm_add_global(LLVMModuleRef M, LLVMTypeRef Ty, const str_t Name);
LLVMValueRef       llvm_add_function(LLVMModuleRef M, const str_t Name, LLVMTypeRef FunctionTy);
LLVMValueRef       llvm_build_alloca(LLVMBuilderRef B, LLVMTypeRef Ty, const str_t Name);
LLVMBasicBlockRef  llvm_append_basic_block_in_context(llvm_context_ref_t ctx, LLVMValueRef Fn, const str_t Name);
LLVMMetadataRef    llvm_di_builder_create_debug_location(llvm_context_ref_t ctx, s32 line, s32 col, LLVMMetadataRef scope, LLVMMetadataRef inlined_at);
u32                llvm_get_md_kind_id_in_context(llvm_context_ref_t ctx, const str_t name);
LLVMAttributeRef   llvm_create_enum_attribute(llvm_context_ref_t ctx, u32 kind, u64 val);
LLVMAttributeRef   llvm_create_type_attribute(llvm_context_ref_t ctx, u32 kind, LLVMTypeRef type_ref);
LLVMValueRef       llvm_const_string_in_context(llvm_context_ref_t ctx, const str_t str, bool dont_null_terminate);
LLVMTypeRef        llvm_struct_type_in_context(llvm_context_ref_t ctx, LLVMTypeRef *elems, u32 elem_num, LLVMBool packed);
LLVMModuleRef      llvm_module_create_with_name_in_context(llvm_context_ref_t ctx, const char *name);
LLVMTypeRef        llvm_intrinsic_get_type(llvm_context_ref_t ctx, u32 id, LLVMTypeRef *types, size_t types_num);
LLVMBuilderRef     llvm_create_builder_in_context(llvm_context_ref_t ctx);

#ifdef __cplusplus
}
#endif

#endif
