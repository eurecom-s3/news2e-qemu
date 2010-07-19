// XXX: qemu stuff should be included before anything from KLEE or LLVM !
extern "C" {
#include <tcg.h>
#include <tcg-op.h>
#include <qemu-timer.h>

extern struct CPUX86State *env;
}

#include "CorePlugin.h"
#include <s2e/S2E.h>
#include <s2e/Utils.h>

#include <s2e/S2EExecutionState.h>
#include <s2e/S2EExecutor.h>

#include <s2e/s2e_qemu.h>

using namespace std;

namespace s2e {
    S2E_DEFINE_PLUGIN(CorePlugin, "S2E core functionality", "Core",);
} // namespace s2e

using namespace s2e;



static void s2e_timer_cb(void *opaque)
{
    CorePlugin *c = (CorePlugin*)opaque;
    g_s2e->getDebugStream() << "Firing timer event" << std::endl;
    c->onTimer.emit();
    qemu_mod_timer(c->getTimer(), qemu_get_clock(host_clock) +
        get_ticks_per_sec());
}

void CorePlugin::initializeTimers()
{
    s2e()->getDebugStream() << "Initializing periodic timer" << std::endl;
    /* Initialize the timer handler */
    m_Timer = qemu_new_timer(host_clock, s2e_timer_cb, this);
    qemu_mod_timer(m_Timer, qemu_get_clock(host_clock) +
        get_ticks_per_sec());

}

void CorePlugin::initialize()
{

}

/******************************/
/* Functions called from QEMU */

void s2e_tb_alloc(TranslationBlock *tb)
{
    tb->s2e_tb = new S2ETranslationBlock;

    /* Push one copy of a signal to use it as a cache */
    tb->s2e_tb->executionSignals.push_back(new ExecutionSignal);

    tb->s2e_tb_next[0] = 0;
    tb->s2e_tb_next[1] = 0;
}

void s2e_tb_free(TranslationBlock *tb)
{
//    assert(0 && "NOT IMPLEMENTED");
    if(tb->s2e_tb) {
        foreach(ExecutionSignal* s, tb->s2e_tb->executionSignals) {
            delete s;
        }
        delete tb->s2e_tb;
    }
}

void s2e_tcg_execution_handler(void* signal, uint64_t pc)
{
    try {
        ExecutionSignal *s = (ExecutionSignal*)signal;
        s->emit(g_s2e_state, pc);
    } catch(s2e::CpuExitException&) {
        longjmp(env->jmp_env, 1);
    } catch(s2e::StateTerminatedException&) {
        longjmp(env->jmp_env_s2e, 1);
    }
}

void s2e_tcg_custom_instruction_handler(uint64_t arg)
{
    assert(!g_s2e->getCorePlugin()->onCustomInstruction.empty());

    try {
        g_s2e->getCorePlugin()->onCustomInstruction(g_s2e_state, arg);
    } catch(s2e::CpuExitException&) {
        longjmp(env->jmp_env, 1);
    } catch(s2e::StateTerminatedException&) {
        longjmp(env->jmp_env_s2e, 1);
    }
}

void s2e_tcg_emit_custom_instruction(S2E*, uint64_t arg)
{
    TCGv_ptr t0 = tcg_temp_new_i64();
    tcg_gen_movi_i64(t0, arg);

    TCGArg args[1];
    args[0] = GET_TCGV_I64(t0);
    tcg_gen_helperN((void*) s2e_tcg_custom_instruction_handler,
                0, 2, TCG_CALL_DUMMY_ARG, 1, args);

    tcg_temp_free_i64(t0);
}

/* Instrument generated code to emit signal on execution */
void s2e_tcg_instrument_code(S2E*, ExecutionSignal* signal, uint64_t pc)
{
    TCGv_ptr t0 = tcg_temp_new_ptr();
    TCGv_i64 t1 = tcg_temp_new_i64();

    // XXX: here we relay on CPUState being the first tcg global temp
    TCGArg args[2];
    args[0] = GET_TCGV_PTR(t0);
    args[1] = GET_TCGV_I64(t1);

#if TCG_TARGET_REG_BITS == 64
    const int sizemask = 4 | 2;
    tcg_gen_movi_i64(t0, (tcg_target_ulong) signal);
#else
    const int sizemask = 4;
    tcg_gen_movi_i32(t0, (tcg_target_ulong) signal);
#endif

    tcg_gen_movi_i64(t1, pc);

    tcg_gen_helperN((void*) s2e_tcg_execution_handler,
                0, sizemask, TCG_CALL_DUMMY_ARG, 2, args);

    tcg_temp_free_i64(t1);
    tcg_temp_free_ptr(t0);
}

void s2e_on_translate_block_start(
        S2E* s2e, S2EExecutionState* state,
        TranslationBlock *tb, uint64_t pc)
{
    assert(state->isActive());

    ExecutionSignal *signal = tb->s2e_tb->executionSignals.back();
    assert(signal->empty());

    try {
        s2e->getCorePlugin()->onTranslateBlockStart.emit(signal, state, tb, pc);
        if(!signal->empty()) {
            s2e_tcg_instrument_code(s2e, signal, pc);
            tb->s2e_tb->executionSignals.push_back(new ExecutionSignal);
        }
    } catch(s2e::CpuExitException&) {
        longjmp(env->jmp_env, 1);
    } catch(s2e::StateTerminatedException&) {
        longjmp(env->jmp_env_s2e, 1);
    }
}

void s2e_on_translate_block_end(
        S2E* s2e, S2EExecutionState *state,
        TranslationBlock *tb,
        uint64_t insPc, int staticTarget, uint64_t targetPc)
{
    assert(state->isActive());

    ExecutionSignal *signal = tb->s2e_tb->executionSignals.back();
    assert(signal->empty());

    try {
        s2e->getCorePlugin()->onTranslateBlockEnd.emit(
                signal, state, tb, insPc,
                staticTarget, targetPc);
    } catch(s2e::CpuExitException&) {
        longjmp(env->jmp_env, 1);
    } catch(s2e::StateTerminatedException&) {
        longjmp(env->jmp_env_s2e, 1);
    }

    if(!signal->empty()) {
        s2e_tcg_instrument_code(s2e, signal, insPc);
        tb->s2e_tb->executionSignals.push_back(new ExecutionSignal);
    }
}

void s2e_on_translate_instruction_start(
        S2E* s2e, S2EExecutionState* state,
        TranslationBlock *tb, uint64_t pc)
{
    assert(state->isActive());

    ExecutionSignal *signal = tb->s2e_tb->executionSignals.back();
    assert(signal->empty());

    try {
        s2e->getCorePlugin()->onTranslateInstructionStart.emit(signal, state, tb, pc);
        if(!signal->empty()) {
            s2e_tcg_instrument_code(s2e, signal, pc);
            tb->s2e_tb->executionSignals.push_back(new ExecutionSignal);
        }
    } catch(s2e::CpuExitException&) {
        longjmp(env->jmp_env, 1);
    } catch(s2e::StateTerminatedException&) {
        longjmp(env->jmp_env_s2e, 1);
    }
}

void s2e_on_translate_jump_start(
        S2E* s2e, S2EExecutionState* state,
        TranslationBlock *tb, uint64_t pc, int jump_type)
{
    assert(state->isActive());

    ExecutionSignal *signal = tb->s2e_tb->executionSignals.back();
    assert(signal->empty());

    try {
        s2e->getCorePlugin()->onTranslateJumpStart.emit(signal, state, tb,
                                                        pc, jump_type);
        if(!signal->empty()) {
            s2e_tcg_instrument_code(s2e, signal, pc);
            tb->s2e_tb->executionSignals.push_back(new ExecutionSignal);
        }
    } catch(s2e::CpuExitException&) {
        longjmp(env->jmp_env, 1);
    }
}

void s2e_on_translate_instruction_end(
        S2E* s2e, S2EExecutionState* state,
        TranslationBlock *tb, uint64_t pc)
{
    assert(state->isActive());

    ExecutionSignal *signal = tb->s2e_tb->executionSignals.back();
    assert(signal->empty());

    try {
        s2e->getCorePlugin()->onTranslateInstructionEnd.emit(signal, state, tb, pc);
        if(!signal->empty()) {
            s2e_tcg_instrument_code(s2e, signal, pc);
            tb->s2e_tb->executionSignals.push_back(new ExecutionSignal);
        }
    } catch(s2e::CpuExitException&) {
        longjmp(env->jmp_env, 1);
    } catch(s2e::StateTerminatedException&) {
        longjmp(env->jmp_env_s2e, 1);
    }
}

void s2e_on_exception(S2E *s2e, S2EExecutionState* state,
                      unsigned intNb)
{
    assert(state->isActive());

    try {
        s2e->getCorePlugin()->onException.emit(state, intNb, state->getPc());
    } catch(s2e::CpuExitException&) {
        longjmp(env->jmp_env, 1);
    } catch(s2e::StateTerminatedException&) {
        longjmp(env->jmp_env_s2e, 1);
    }
}

void s2e_init_timers(S2E* s2e)
{
    s2e->getCorePlugin()->initializeTimers();
}

void s2e_trace_memory_access(
        struct S2E *s2e, struct S2EExecutionState* state,
        uint64_t vaddr, uint64_t haddr, uint8_t* buf, unsigned size,
        int isWrite, int isIO)
{
    if(!s2e->getCorePlugin()->onDataMemoryAccess.empty()) {
        uint64_t value = 0;
        memcpy((void*) &value, buf, size);

        try {
            s2e->getCorePlugin()->onDataMemoryAccess.emit(state,
                klee::ConstantExpr::create(vaddr, 64),
                klee::ConstantExpr::create(haddr, 64),
                klee::ConstantExpr::create(value, size*8),
                isWrite, isIO);
        } catch(s2e::CpuExitException&) {
            longjmp(env->jmp_env, 1);
        } catch(s2e::StateTerminatedException&) {
            longjmp(env->jmp_env_s2e, 1);
        }
    }
}

void s2e_on_page_fault(S2E *s2e, S2EExecutionState* state, uint64_t addr, int is_write)
{
    try {
        s2e->getCorePlugin()->onPageFault.emit(state, addr, (bool)is_write);
    } catch(s2e::CpuExitException&) {
        longjmp(env->jmp_env, 1);
    } catch(s2e::StateTerminatedException&) {
        longjmp(env->jmp_env_s2e, 1);
    }
}

void s2e_on_tlb_miss(S2E *s2e, S2EExecutionState* state, uint64_t addr, int is_write)
{
    try {
        s2e->getCorePlugin()->onTlbMiss.emit(state, addr, (bool)is_write);
    } catch(s2e::CpuExitException&) {
        longjmp(env->jmp_env, 1);
    } catch(s2e::StateTerminatedException&) {
        longjmp(env->jmp_env_s2e, 1);
    }
}

void s2e_on_device_registration(S2E *s2e)
{
    s2e->getCorePlugin()->onDeviceRegistration();
}

void s2e_on_device_activation(S2E *s2e, struct PCIBus *bus)
{
    s2e->getCorePlugin()->onDeviceActivation(bus);
}


void s2e_trace_port_access(
        S2E *s2e, S2EExecutionState* state,
        uint64_t port, uint64_t value, unsigned size,
        int isWrite)
{
    if(!s2e->getCorePlugin()->onPortAccess.empty()) {
        try {
            s2e->getCorePlugin()->onPortAccess.emit(state,
                klee::ConstantExpr::create(port, 64),
                klee::ConstantExpr::create(value, size),
                isWrite);
        } catch(s2e::CpuExitException&) {
            longjmp(env->jmp_env, 1);
        } catch(s2e::StateTerminatedException&) {
            longjmp(env->jmp_env_s2e, 1);
        }
    }
}

int s2e_is_port_symbolic(struct S2E *s2e, struct S2EExecutionState* state, uint64_t port)
{
    return s2e->getCorePlugin()->isPortSymbolic(port);
}
