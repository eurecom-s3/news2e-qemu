#ifndef _S2E_S2EEXECUTOR_H
#define _S2E_S2EEXECUTOR_H

/********************************************
 ********* Includes *************************
 ********************************************/

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

//#include "qemu-common.h"
//#include "qom/cpu.h"

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

//#if !defined(__cplusplus)
//#include <stdbool.h>
//#endif /* !defined(__cplusplus) */

#include "s2e/S2EExecutionState.h"

/********************************************
 ***** struct/class forward declaration *****
 ********************************************/

#if defined(__cplusplus)
namespace s2e {
	class S2EExecutor;
}
using s2e::S2EExecutor;
#else /* defined(__cplusplus) */
typedef struct S2EExecutor S2EExecutor;
#endif /* defined(__cplusplus) */

//We need the forward declaration break an include loop
struct CPUState;

/********************************************
 ******** variable declarations *************
 ********************************************/

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */


/********************************************
 ******** C function declarations ***********
 ********************************************/

/**
 * Create an initial execution state.
 * @param self S2EExecutor instance pointer
 * @return An initial execution state
 */
S2EExecutionState* S2EExecutor_CreateInitialState(S2EExecutor* self);

/**
 * Get the initial state.
 * @param self S2EExecutor instance pointer
 * @return Initial state instance pointer
 */
S2EExecutionState* S2EExecutor_GetCurrentState(S2EExecutor* self);


/** 
 * Initialize the CPU.
 * @param self S2EExecutor instance pointer
 * @param initial_state Initial S2EExecutionState pointer
 * @param cpu CPUState object pointer
 */
void S2EExecutor_InitCpu(S2EExecutor* self, S2EExecutionState* initial_state, struct CPUState* cpu);

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* _S2E_S2EEXECUTOR_H */
