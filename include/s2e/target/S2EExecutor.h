#ifndef _S2E_TARGET_S2EEXECUTOR_H
#define _S2E_TARGET_S2EEXECUTOR_H

/********************************************
 ********* Includes *************************
 ********************************************/

#include "s2e/S2EExecutor.h"

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

#include "qemu-common.h"
//#include "qom/cpu.h"
#include "tcg/tcg.h"

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

/********************************************
 ***** struct/class forward declaration *****
 ********************************************/

struct TranslationBlock;

/********************************************
 ******** C function declarations ***********
 ********************************************/

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

/**
 * Execute a single translation block in concrete or symbolic mode.
 * @param self S2EExecutor instance pointer
 * @param cpu CPUState pointer
 * @param tb TranslationBlock pointer
 * @return 
 */
tcg_target_ulong S2EExecutor_ExecuteTranslationBlock(S2EExecutor* self, struct CPUState* cpu, struct TranslationBlock* tb);


#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* _S2E_TARGET_S2EEXECUTOR_H */
