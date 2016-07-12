extern "C" {
#include "qemu-common.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "tcg/tcg.h"
#include "qemu/bswap.h"
}
#include "s2e/cxx/S2E.h"
#include "s2e/cxx/Utils.h"
#include "s2e/Plugins/CorePlugin.h"
#include "s2e/cxx/S2EExecutor.h"

using s2e::hexval;
using s2e::CpuExitException;
using s2e::ExecutionSignal;

/** TODO: This is a hack, put in a better location */
static bool g_s2e_enable_signals = true;


static uint64_t sign_extend(uint64_t val, unsigned size)
{
	switch (size) {
		case MO_8:  return (val & 0x0000000000000080) ? (0xffffffffffffff00 | val) : val;
		case MO_16: return (val & 0x0000000000008000) ? (0xffffffffffff0000 | val) : val;
		case MO_32: return (val & 0x0000000080000000) ? (0xffffffff00000000 | val) : val;
		case MO_64: return val;
	}
	abort();
}
/*
static uint64_t byteswap(uint64_t val, unsigned size)
{
	switch (size) {
		case MO_8:  return val;
		case MO_16: return bswap16((uint16_t) val);
		case MO_32: return bswap32((uint32_t) val);
		case MO_64: return bswap64(val);
	}
	abort();
}
*/

static uint64_t Env_getPc(CPUArchState* env) 
{
#if defined(TARGET_ARM)
	return env->regs[15];
#elif defined(TARGET_I386)
	return env->eip;
#else /* defined(TARGET_ARM) */
#error Do not know how to get the PC for architecture 
#endif /* defined(TARGET_ARM) */
}

void helper_s2e_base_instruction(CPUArchState* env, uint32_t op_idx)
{
	CPUState* cpu = CPU(ENV_GET_CPU(env));

	if (S2E::getInstance()->getCorePlugin()->onCustomInstruction.empty()) {
		S2E::getInstance()->getWarningsStream() << "Encountered base instruction at PC 0x"
			<< hexval(Env_getPc(env))
			<< ", but no plugin is registered to receive signals about base instructions." << '\n';
	}
	else {
		try {
			S2E::getInstance()->getCorePlugin()->onCustomInstruction.emit(g_s2e_state, op_idx);
		} 
		catch(CpuExitException&) {
			s2e_longjmp(cpu->jmp_env, 1);
		}
	}
}

void helper_s2e_instrument_code(CPUArchState* env, void* _signal, uint64_t pc)
{
    ExecutionSignal* signal = static_cast<ExecutionSignal*>(_signal);
    CPUState* cpu = CPU(ENV_GET_CPU(env));
    
    try {
        if (g_s2e_enable_signals) {
            signal->emit(S2EExecutor_GetCurrentState(S2E::getInstance()->getExecutor()), pc);
        }
    } catch(s2e::CpuExitException&) {
        s2e_longjmp(cpu->jmp_env, 1);
    }
}
