#ifndef _S2E_S2E_H
#define _S2E_S2E_H

/********************************************
 ********* Includes *************************
 ********************************************/

#if !defined(__cplusplus)
#include <stdbool.h>
#endif /* !defined(__cplusplus) */

#include "qapi/qmp/qdict.h"
#include "qapi/error.h"

#include "s2e/S2EExecutionState.h"
#include "s2e/S2ECommandLineOptions.h"
#include "s2e/TCGLLVMContext.h"
#include "s2e/S2EExecutor.h"


/********************************************
 ***** struct/class forward declaration *****
 ********************************************/

#if defined(__cplusplus)
namespace s2e {
	class S2E;
}
using s2e::S2E;
#else /* defined(__cplusplus) */
typedef struct S2E S2E;
#endif /* defined(__cplusplus) */

/********************************************
 ******** variable declarations *************
 ********************************************/

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

extern S2E *g_s2e;

/********************************************
 ******** C function declarations ***********
 ********************************************/

/**
 * Do a first initialization of S2E.
 * @param argc Command line argument count.
 * @param argv Command line argument strings.
 * @param cmdline_opts Command line options for S2E already parsed by Qemu.
 */
void S2E_Initialize(int argc, char * argv[], S2ECommandLineOptions *cmdline_opts);

/**
 * Initialize timers.
 * @param self S2E instance pointer
 */
void S2E_InitTimers(S2E *self);

/**
 * Initialize system for execution.
 * @param self S2E instance pointer
 * @param state 
 */
void S2E_InitExecution(S2E *self, S2EExecutionState *state);

/**
 * Call registered handlers for the OnInitializationComplete event.
 * @param self S2E instance pointer
 */
void S2E_CallOnInitializationCompleteHandlers(S2E *self);


/**
 * Call registered handlers for the OnDeviceInitialization event.
 * @param self S2E instance pointer
 */
void S2E_CallOnDeviceRegistrationHandlers(S2E *self);

/**
 * Call registered handlers for the OnPrivilegeChange event.
 * @param self S2E instance pointer
 */
void S2E_CallOnPrivilegeChangeHandlers(S2E *self, unsigned from, unsigned to);


/**
 * Test if forking is enabled.
 * @param self Instance pointer
 * @return <b>true</b> if forking is enabled
 */
bool S2E_IsForking(S2E *self);

/**
 * Execute a QMP subcommand of "s2e".
 * @param qdict Parsed input subcommand in form of a QDict.
 * @param ret To be assigned with pointer to response QDict.
 * @param err To be assigned with pointer to Error struct in case of error.
 */
void S2E_ExecuteQMPCommand(QDict *qdict, QObject **ret, Error **err);

/**
 * Delete the global S2E object.
 */
void S2E_Destroy(void);

/**
 * Get the singleton instance of the S2E object.
 * @return S2E instance
 */
S2E* S2E_GetInstance(void);

/**
 * Get the S2EExecutor from the S2E object.
 * @param self S2E instance pointer
 * @return S2EExecutor instance pointer
 */
S2EExecutor* S2E_GetExecutor(S2E* self);

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* _S2E_S2E_H */
