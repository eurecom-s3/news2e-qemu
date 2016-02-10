#include <cassert>

#include <llvm/Support/raw_ostream.h>

#include "cpu.h"
#include "s2e/S2EExecutionState.h"

S2EExecutionState *g_s2e_state = NULL;


void S2EExecutionState_InitDeviceState(S2EExecutionState *self)
{
	llvm::errs() << "TODO: implement " << __func__ << '\n';
}


void S2EExecutionState_ReadRegisterConcrete(S2EExecutionState *state, CPUArchState *env, size_t offset, uint8_t *result, size_t size)
{
	llvm::errs() << "TODO: implement " << __func__ << '\n';
}

void S2EExecutionState_WriteRegisterConcrete(S2EExecutionState *state, CPUArchState *env, size_t offset, uint8_t const *result, size_t size)
{
	llvm::errs() << "TODO: implement " << __func__ << '\n';
}


