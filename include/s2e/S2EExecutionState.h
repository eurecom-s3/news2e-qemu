#ifndef _S2E_S2EEXECUTIONSTATE_H
#define _S2E_S2EEXECUTIONSTATE_H

/********************************************
 ********* Includes *************************
 ********************************************/

#include <stdint.h>

/********************************************
 ***** struct/class forward declaration *****
 ********************************************/

#if defined(__cplusplus)
namespace s2e {
	class S2EExecutionState;
}
using s2e::S2EExecutionState;
#else /* defined(__cplusplus) */
typedef struct S2EExecutionState S2EExecutionState;
#endif /* defined(__cplusplus) */

/********************************************
 ******** variable declarations *************
 ********************************************/

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

extern S2EExecutionState *g_s2e_state;

/********************************************
 ******** C function declarations ***********
 ********************************************/

/**
 * Create a new instance.
 * @return new instance
 */
S2EExecutionState* S2EExecutionState_New(void);

/**
 * Initialize peripheral devices' state.
 * @param self Instance pointer
 */
void S2EExecutionState_InitDeviceState(S2EExecutionState *self);

/**
 * Switch state to symbolic execution.
 * This will do a longjump to exit the CPU loop of Qemu,
 * forcing a retranslation of the translation block starting from the
 * current instruction. This TB will then be executed using Klee.
 *
 * This function is supposed to be called only from C code.
 * Calling it from C++ code might result in bad stack cleanup.
 */
void S2EExecutionState_SwitchToSymbolic(S2EExecutionState *self);

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* _S2E_S2EEXECUTIONSTATE_H */
