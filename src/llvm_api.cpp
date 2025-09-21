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

#include "llvm_api.h"

#undef array
_SHUT_UP_BEGIN
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DebugLoc.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
_SHUT_UP_END

#include <mutex>

using namespace llvm;

struct llvm_context {
	LLVMContext       ctx;
	LLVMTargetDataRef TD;
	std::mutex        lock;
};

llvm_context_ref_t llvm_context_create(LLVMTargetDataRef TD) {
	llvm_context_ref_t ctx = new llvm_context();
	ctx->TD                = TD;
	return ctx;
}

void llvm_context_dispose(llvm_context_ref_t ctx) {
	delete ctx;
}

void llvm_lock_context(llvm_context_ref_t ctx) {
	ctx->lock.lock();
}

void llvm_unlock_context(llvm_context_ref_t ctx) {
	ctx->lock.unlock();
}

LLVMTypeRef llvm_float_type_in_context(llvm_context_ref_t ctx) {
	return (LLVMTypeRef)Type::getFloatTy(ctx->ctx);
}

LLVMTypeRef llvm_double_type_in_context(llvm_context_ref_t ctx) {
	return (LLVMTypeRef)Type::getDoubleTy(ctx->ctx);
}

LLVMTypeRef llvm_void_type_in_context(llvm_context_ref_t ctx) {
	return (LLVMTypeRef)Type::getVoidTy(ctx->ctx);
}

LLVMTypeRef llvm_int_type_in_context(llvm_context_ref_t ctx, s32 bitcount) {
	return (LLVMTypeRef)Type::getIntNTy(ctx->ctx, (unsigned)bitcount);
}

LLVMTypeRef llvm_struct_create_named(llvm_context_ref_t ctx, const str_t Name) {
	StringRef sName(Name.ptr, (size_t)Name.len);
	return wrap(StructType::create(ctx->ctx, sName));
}

LLVMMetadataRef llvm_di_builder_create_debug_location(llvm_context_ref_t ctx, s32 line, s32 col, LLVMMetadataRef scope, LLVMMetadataRef inlined_at) {
	return wrap(DILocation::get(ctx->ctx, (unsigned)line, (unsigned)col, unwrap(scope), unwrap(inlined_at)));
}

LLVMValueRef llvm_add_global(LLVMModuleRef M, LLVMTypeRef Ty, const str_t Name) {
	StringRef sName(Name.ptr, (size_t)Name.len);
	return wrap(new GlobalVariable(
	    *unwrap(M), unwrap(Ty), false, GlobalValue::ExternalLinkage, nullptr, sName));
}

LLVMValueRef llvm_add_function(LLVMModuleRef M, const str_t Name, LLVMTypeRef FunctionTy) {
	StringRef sName(Name.ptr, (size_t)Name.len);
	return wrap(Function::Create(
	    unwrap<FunctionType>(FunctionTy), GlobalValue::ExternalLinkage, sName, unwrap(M)));
}

LLVMValueRef llvm_build_alloca(LLVMBuilderRef B, LLVMTypeRef Ty, const str_t Name) {
	StringRef sName(Name.ptr, (size_t)Name.len);
	return wrap(unwrap(B)->CreateAlloca(unwrap(Ty), nullptr, sName));
}

LLVMBasicBlockRef llvm_append_basic_block_in_context(llvm_context_ref_t ctx, LLVMValueRef Fn, const str_t Name) {
	StringRef sName(Name.ptr, (size_t)Name.len);
	return wrap(BasicBlock::Create(ctx->ctx, sName, unwrap<Function>(Fn)));
}

u32 llvm_get_md_kind_id_in_context(llvm_context_ref_t ctx, const str_t name) {
	return ctx->ctx.getMDKindID(StringRef(name.ptr, (size_t)name.len));
}

LLVMAttributeRef llvm_create_enum_attribute(llvm_context_ref_t ctx, u32 kind, u64 val) {
	auto AttrKind = (Attribute::AttrKind)kind;
	return wrap(Attribute::get(ctx->ctx, AttrKind, val));
}

LLVMAttributeRef llvm_create_type_attribute(llvm_context_ref_t ctx, u32 kind, LLVMTypeRef type_ref) {
	auto AttrKind = (Attribute::AttrKind)kind;
	return wrap(Attribute::get(ctx->ctx, AttrKind, unwrap(type_ref)));
}

LLVMValueRef llvm_const_string_in_context(llvm_context_ref_t ctx, const str_t str, bool dont_null_terminate) {
	return wrap(ConstantDataArray::getString(ctx->ctx, StringRef(str.ptr, (size_t)str.len), dont_null_terminate == 0));
}

LLVMValueRef llvm_const_byte_blob_in_context(llvm_context_ref_t ctx, LLVMTypeRef elem_type_ref, const u8 *ptr, s64 len) {
	const u64       elem_size_bytes = LLVMSizeOfTypeInBits(ctx->TD, elem_type_ref) / 8;
	const StringRef data((char *)ptr, (size_t)(len * elem_size_bytes));
	return wrap(ConstantDataArray::getString(ctx->ctx, data, false));
}

LLVMTypeRef llvm_struct_type_in_context(llvm_context_ref_t ctx, LLVMTypeRef *elems, u32 elem_num, LLVMBool packed) {
	ArrayRef<Type *> Tys(unwrap(elems), elem_num);
	return wrap(StructType::get(ctx->ctx, Tys, packed != 0));
}

LLVMModuleRef llvm_module_create_with_name_in_context(llvm_context_ref_t ctx, const char *name) {
	return wrap(new Module(name, ctx->ctx));
}

static Intrinsic::ID llvm_map_to_intrinsic_id(unsigned ID) {
	assert(ID < llvm::Intrinsic::num_intrinsics && "Intrinsic ID out of range");
	return llvm::Intrinsic::ID(ID);
}

LLVMTypeRef llvm_intrinsic_get_type(llvm_context_ref_t ctx, u32 id, LLVMTypeRef *types, size_t types_num) {
	const auto             IID = llvm_map_to_intrinsic_id(id);
	const ArrayRef<Type *> Tys(unwrap(types), types_num);
	return wrap(Intrinsic::getType(ctx->ctx, IID, Tys));
}

LLVMBuilderRef llvm_create_builder_in_context(llvm_context_ref_t ctx) {
	return wrap(new IRBuilder<>(ctx->ctx));
}