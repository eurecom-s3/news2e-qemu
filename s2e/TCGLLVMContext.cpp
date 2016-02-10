#include <cassert>
#include <cstdlib>

#include <llvm/Support/Threading.h>
#include <llvm/Support/raw_ostream.h>

#include "s2e/TCGLLVMContext.h"
#include "s2e/cxx/TCGLLVMContext.h"

using llvm::llvm_is_multithreaded;


TCGLLVMContext* TCGLLVMContext_GetInstance(void)
{
	static TCGLLVMContext *self = NULL;

	if (self == NULL) {
		if (!llvm_is_multithreaded()) {
			llvm::errs() << "ERROR - " << __FILE__ << ":" << __LINE__ << ": Could not initialize LLVM threading" << '\n';
			exit(-1);
		}

		self = new TCGLLVMContext;
	}

	return self;
}
