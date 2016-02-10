#ifndef _S2E_TARGET_S2EEXECUTIONSTATE_H
#define _S2E_TARGET_S2EEXECUTIONSTATE_H

/********************************************
 ********* Includes *************************
 ********************************************/

#include "s2e/S2EExecutionState.h"

/********************************************
 ***** struct/class forward declaration *****
 ********************************************/

/********************************************
 ******** C function declarations ***********
 ********************************************/

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

void S2EExecutionState_ReadRegisterConcrete(S2EExecutionState *state, CPUArchState *env, size_t offset, uint8_t *result, size_t size);
void S2EExecutionState_WriteRegisterConcrete(S2EExecutionState *state, CPUArchState *env, size_t offset, uint8_t const *result, size_t size);

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* _S2E_TARGET_S2EEXECUTIONSTATE_H */
