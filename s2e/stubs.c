#include "s2e/target/S2E.h"
#include "s2e/target/S2EExecutionState.h"

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



