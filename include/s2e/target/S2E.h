/**
 * This file declares target specific functions of S2E.
 */
#ifndef _S2E_TARGET_S2E_H
#define _S2E_TARGET_S2E_H

/********************************************
 ********* Includes *************************
 ********************************************/

#include "s2e/S2E.h"

#include "exec/exec-all.h"



#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

/********************************************
 ******** C function declarations ***********
 ********************************************/

/**
 * Call registered handlers for the OnTranslateBlockStart event.
 * @param self Instance pointer
 * @param state Current execution state
 * @param tb Current translation block
 * @param pc Current Program counter
 */
void S2E_CallOnTranslateBlockStartHandlers(S2E *self, S2EExecutionState *state, TranslationBlock *tb, uint64_t pc);

/**
 * Call registered handlers for the OnTranslateInstructionStart event.
 * @param self Instance pointer
 * @param state Current execution state
 * @param tb Current translation block
 * @param pc Current Program counter
 */
void S2E_CallOnTranslateInstructionStartHandlers(S2E *self, S2EExecutionState *state, TranslationBlock *tb, uint64_t pc);


/*
 * Call registered handlers for the OnTranslateInstructionEnd event.
 * @param self Instance pointer
 * @param state Current execution state
 * @param tb Current translation block
 * @param pc Current Program counter
 */
void S2e_CallOnTranslateInstructionEndHandlers(S2E *self, S2EExecutionState *state, TranslationBlock *tb, uint64_t pc, uint64_t nextPc);

/*
 * Call registered handlers for the OnTranslateBlockEnd event.
 * @param self Instance pointer
 * @param state Current execution state
 * @param tb Current translation block
 * @param pc Current Program counter
 */
void S2E_CallOnTranslateBlockEndHandlers(S2E *self, S2EExecutionState *state, TranslationBlock *tb, uint64_t pc, bool hasNextPc, uint64_t nextPc);

void S2E_ReadRegisterConcrete(S2E *self, S2EExecutionState *state, CPUArchState *env, size_t offset, uint8_t *result, size_t size);

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* _S2E_TARGET_S2E_H */
