#include "s2e/target/S2E.h"
#include "s2e/target/S2EExecutionState.h"
#include "exec/helper-proto.h"

struct S2E {};
struct S2EExecutionState {};

S2E* g_s2e;
S2EExecutionState* g_s2e_state;

void S2E_Initialize(int argc, char * argv[], S2ECommandLineOptions *cmdline_opts) { }

void S2E_CallOnDeviceRegistrationHandlers(S2E *self) { }

void S2E_ExecuteQMPCommand(QDict *qdict, QObject **ret, Error **err) {
	error_setg(err, "Cannot execute S2E commands, S2E is not compiled into this binary");
}

void S2E_InitTimers(S2E *self) { }

void S2E_InitExecution(S2E *self, S2EExecutionState *state) { }

void S2E_RegisterDirtyMask(S2E *self, S2EExecutionState *state) { }

void S2E_CallOnInitializationCompleteHandlers(S2E *self) { }

void S2E_Destroy(void) { }

void S2EExecutionState_InitDeviceState(S2EExecutionState *self) { }

void S2EExecutor_RegisterCpu(S2EExecutor* self, S2EExecutionState* initial_state, struct CPUState* cpu) { }

void S2EExecutor_RegisterRam(
        S2EExecutor* self, 
        S2EExecutionState* initial_state, 
        uint64_t address, 
        uint64_t size, 
        uint64_t hostAddress, 
        bool isSharedConcrete, 
        bool saveOnContextSwitch, 
        const char* name)
{
}

S2E* S2E_GetInstance(void) {return NULL;}

S2EExecutor* S2E_GetExecutor(S2E* self) {return NULL;}

S2EExecutionState* S2EExecutor_GetCurrentState(S2EExecutor* self) {return NULL;}

void S2EExecutor_InitializeExecution(S2EExecutor* self, S2EExecutionState* initial_state, bool always_execute_klee) { }

uint64_t helper_s2e_ld(CPUArchState* env, uint64_t addr, uint32_t memop, uint32_t idx) {abort();}

void helper_s2e_st(CPUArchState* env, uint64_t addr, uint32_t memop, uint32_t idx, uint64_t val) {abort();}

void helper_s2e_base_instruction(CPUArchState* env, uint32_t op_idx) {abort();}

void helper_s2e_instrument_code(CPUArchState* env, void* signal, uint64_t pc) {abort();}
