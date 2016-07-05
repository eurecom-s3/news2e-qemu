#ifndef _S2E_TARGET_TCGLLVMEXECUTIONCONTEXT_H
#define _S2E_TARGET_TCGLLVMEXECUTIONCONTEXT_H

/********************************************
 ********* Includes *************************
 ********************************************/

#include "s2e/TCGLLVMContext.h"
#include "cpu.h"
#include "tcg/tcg.h"

/********************************************
 ***** struct/class forward declaration *****
 ********************************************/

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

/********************************************
 ******** variable declarations *************
 ********************************************/

/********************************************
 ******** C function declarations ***********
 ********************************************/

/**
 * Generate LLVM intermediate code for TB.
 */
int cpu_gen_llvm(CPUArchState *env, TranslationBlock *tb);

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* _S2E_TARGET_TCGLLVMEXECUTIONCONTEXT_H */
