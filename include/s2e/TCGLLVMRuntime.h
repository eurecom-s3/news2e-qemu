#ifndef _S2E_TCGLLVMRUNTIME_H
#define _S2E_TCGLLVMRUNTIME_H

/********************************************
 ********* Includes *************************
 ********************************************/

#include <stdint.h>

/********************************************
 ***** struct/class forward declaration *****
 ********************************************/

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

#if !defined(__cplusplus)
typedef struct TCGLLVMRuntime TCGLLVMRuntime;
#endif /* !defined(__cplusplus) */

struct TCGLLVMRuntime {
	// NOTE: The order of these are fixed !
	uint64_t helper_ret_addr;
	uint64_t helper_call_addr;
	uint64_t helper_regs[3];
	// END of fixed block
	
	/* run-time tb linking mechanism */
	uint8_t goto_tb;
};

/********************************************
 ******** variable declarations *************
 ********************************************/

extern TCGLLVMRuntime tcg_llvm_runtime;

/********************************************
 ******** C function declarations ***********
 ********************************************/

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* _S2E_TCGLLVMRUNTIME_H */
