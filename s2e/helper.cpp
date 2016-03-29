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


uint64_t helper_s2e_ld(CPUArchState* env, uint64_t addr, uint32_t memop, uint32_t idx)
{
	printf("mem_read(addr = 0x%08" PRIx32 ", size = %d, idx = %d)\n", (uint32_t) addr, (int) (memop & MO_SIZE), (int) idx);
	uint64_t v = 0;
	switch (memop & MO_BSWAP) {
		case MO_BE:
			switch (memop & MO_SIZE) {
				case MO_8:  v = helper_ret_ldub_mmu(env, addr, idx, 0); break;
				case MO_16: v = helper_be_lduw_mmu(env, addr, idx, 0); break;
				case MO_32: v = helper_be_ldul_mmu(env, addr, idx, 0); break;
				case MO_64: v = helper_be_ldq_mmu(env, addr, idx, 0); break;
				default: abort();
			}
			break;
		case MO_LE:
			switch (memop & MO_SIZE) {
				case MO_8:  v = helper_ret_ldub_mmu(env, addr, idx, 0); break;   
				case MO_16: v = helper_le_lduw_mmu(env, addr, idx, 0); break;
				case MO_32: v = helper_le_ldul_mmu(env, addr, idx, 0); break;
				case MO_64: v = helper_le_ldq_mmu(env, addr, idx, 0); break;
				default: abort();
			}
			break;
		default:
			abort();
	}

	if (memop & MO_SIGN) {
		v = sign_extend(v, memop & MO_SIZE);
	}
	return v;
}

void helper_s2e_st(CPUArchState* env, uint64_t addr, uint32_t memop, uint32_t idx, uint64_t val)
{
	printf("mem_write(addr = 0x%08" PRIx32 ", size = %d, idx = %d, val = 0x%08" PRIx32 ")\n", (uint32_t) addr, (int) (memop & MO_SIZE), (int) idx, (uint32_t) val);
	switch (memop & MO_BSWAP) {
		case MO_BE:
			switch (memop & MO_SIZE) {
				case MO_8:  helper_ret_stb_mmu(env, addr, val, idx, 0); break;
				case MO_16: helper_be_stw_mmu(env, addr, val, idx, 0); break;
				case MO_32: helper_be_stl_mmu(env, addr, val, idx, 0); break;
				case MO_64: helper_be_stq_mmu(env, addr, val, idx, 0); break;
				default: abort();
			}
			break;
		case MO_LE:
			switch (memop & MO_SIZE) {
				case MO_8:  helper_ret_stb_mmu(env, addr, val, idx, 0); break;   
				case MO_16: helper_le_stw_mmu(env, addr, val, idx, 0); break;
				case MO_32: helper_le_stl_mmu(env, addr, val, idx, 0); break;
				case MO_64: helper_le_stq_mmu(env, addr, val, idx, 0); break;
				default: abort();
			}
			break;
		default:
			abort();
	}
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
