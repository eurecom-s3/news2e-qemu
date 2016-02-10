#ifndef _S2E_TARGET_TCG_LLVM_H
#define _S2E_TARGET_TCG_LLVM_H

/********************************************
 ********* Includes *************************
 ********************************************/

#include "s2e/tcg-llvm.h"
#include "exec/exec-all.h"
#include "cpu.h"

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

/********************************************
 ***** struct/class forward declaration *****
 ********************************************/

/********************************************
 ******** variable declarations *************
 ********************************************/

/********************************************
 ******** C function declarations ***********
 ********************************************/

void TCGLLVM_TbAlloc(TranslationBlock *tb);
void TCGLLVM_TbFree(TranslationBlock *tb);
void TCGLLVM_GenCode(TCGContext *s, TranslationBlock *tb);
void TCGLLVM_GetFunctionName(TranslationBlock *tb);
void TCGLLVM_TbExec(CPUArchState *env, TranslationBlock *tb);

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* _S2E_TARGET_TCG_LLVM_H */
