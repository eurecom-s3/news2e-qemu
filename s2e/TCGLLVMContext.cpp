#include <cassert>
#include <cstdlib>

#include <llvm/Support/Threading.h>
#include <llvm/Support/raw_ostream.h>

#include "s2e/TCGLLVMContext.h"
#include "s2e/cxx/TCGLLVMContext.h"

using llvm::llvm_is_multithreaded;


TCGLLVMContext* TCGLLVMContext_GetInstance(void)
{
	return TCGLLVMContext::getInstance();
}
