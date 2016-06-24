#include <cassert>

#include <llvm/Support/raw_ostream.h>

#include "cpu.h"
#include "s2e/cxx/S2EExecutionState.h"

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

void S2EExecutionState_UpdateTlbEntry(S2EExecutionState* self, CPUArchState *env, int mmu_idx, uint64_t vaddr, uintptr_t addend)
{
	self->updateTlbEntry(env, mmu_idx, vaddr, addend);
}

void S2EExecutionState_FlushTlb(S2EExecutionState *self)
{
	self->flushTlbCache();
}

void S2EExecutionState_ReadRamConcrete(S2EExecutionState *self, uint64_t hostAddress, uint8_t* buf, uint64_t size)
{
	self->readRamConcrete(hostAddress, buf, size);
}

void S2EExecutionState_WriteRamConcrete(S2EExecutionState *self, uint64_t hostAddress, const uint8_t* buf, uint64_t size)
{
	self->writeRamConcrete(hostAddress, buf, size);
}

void S2EExecutionState_SwitchToSymbolic(S2EExecutionState *self)
{
	assert(false && "Switching to symbolic mode not implemented");
}



