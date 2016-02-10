#ifndef _S2E_TCG_LLVM_H
#define _S2E_TCG_LLVM_H

/********************************************
 ********* Includes *************************
 ********************************************/

#include "s2e/TCGLLVMContext.h"

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

TCGLLVMContext* TCGLLVM_Initialize(void);

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* _S2E_TCG_LLVM_H */
