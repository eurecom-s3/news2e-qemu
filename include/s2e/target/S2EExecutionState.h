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
void S2EExecutionState_UpdateTlbEntry(S2EExecutionState* state, CPUArchState *env, int mmu_idx, uint64_t vaddr, uintptr_t addend);
void S2EExecutionState_FlushTlb(S2EExecutionState *self);
void S2EExecutionState_ReadRamConcrete(S2EExecutionState *self, uint64_t hostAddress, uint8_t* buf, uint64_t size);
/**
 * Write concrete RAM.
 */
void S2EExecutionState_WriteRamConcrete(S2EExecutionState *self, uint64_t hostAddress, const uint8_t* buf, uint64_t size);
#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* _S2E_TARGET_S2EEXECUTIONSTATE_H */
