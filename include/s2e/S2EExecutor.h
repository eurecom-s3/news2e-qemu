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
void S2EExecutor_RegisterCpu(S2EExecutor* self, S2EExecutionState* initial_state, struct CPUState* cpu);

/**
 * Register a memory region with S2E.
 * @param self S2EExecutor instance pointer
 * @param initial_state Initial S2ExecutionState pointer
 * @param address Physical address in the guest's address space
 * @param size Region size
 * @param hostAddress Host address of this memory region
 * @param isSharedConcrete
 * @param saveOnContextSwitch
 * @param name Region's name
 */
void S2EExecutor_RegisterRam(
        S2EExecutor* self, 
        S2EExecutionState* initial_state, 
        uint64_t address, 
        uint64_t size, 
        uint64_t hostAddress, 
        bool isSharedConcrete, 
        bool saveOnContextSwitch, 
        const char* name);

/**
 * Initialize execution.
 * @param self SEExecutor instance pointer
 * @param initial_state Initial S2EExecutionState pointer
 * @param always_execute_klee <b>true</b> to always execute in Klee, or <b>false</b> to execute in mixed Qemu/Klee mode
 */
void S2EExecutor_InitializeExecution(S2EExecutor* self, S2EExecutionState* initial_state, bool always_execute_klee);

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* _S2E_S2EEXECUTOR_H */
