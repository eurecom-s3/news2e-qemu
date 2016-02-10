#ifndef _S2E_TCGLLVMEXECUTIONCONTEXT_H
#define _S2E_TCGLLVMEXECUTIONCONTEXT_H

/********************************************
 ********* Includes *************************
 ********************************************/

/********************************************
 ***** struct/class forward declaration *****
 ********************************************/

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

#if defined(__cplusplus)
class TCGLLVMContext;
#else /* defined(__cplusplus) */
typedef struct TCGLLVMContext TCGLLVMContext;
#endif /* defined(__cplusplus) */

/********************************************
 ******** variable declarations *************
 ********************************************/

extern TCGLLVMContext *tcg_llvm_ctx;

/********************************************
 ******** C function declarations ***********
 ********************************************/

/**
 * Get the singleton instance of TCGLLVMContext.
 * @return singleton instance.
 */
TCGLLVMContext* TCGLLVMContext_GetInstance(void);

void TCGLLVMContext_Close(TCGLLVMContext *self);

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* _S2E_TCGLLVMEXECUTIONCONTEXT_H */
