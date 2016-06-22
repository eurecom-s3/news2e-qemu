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

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* _S2E_S2EEXECUTIONSTATE_H */
