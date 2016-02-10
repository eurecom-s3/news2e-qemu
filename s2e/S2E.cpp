#include <cassert>
#include <cstdlib>

#include <llvm/Support/raw_ostream.h>

#include "s2e/target/S2E.h"
#include "s2e/cxx/S2E.h"
#include "s2e/cxx/S2EExecutor.h"

S2E *g_s2e = NULL;


S2E* S2E_New(int argc, char * argv[], TCGLLVMContext *tcg_llvm_ctx, S2ECommandLineOptions *cmdline_opts)
{
    if (g_s2e != NULL) {
        llvm::errs() << "ERROR - " << __FILE__ << ":" << __LINE__ << ": g_s2e is already initialized" << '\n';
        exit(1);
    }
    
	S2E *self = new S2E(argc, argv, tcg_llvm_ctx,
			cmdline_opts->config_file ? cmdline_opts->config_file : "",
			cmdline_opts->output_dir ? cmdline_opts->output_dir : "",
			cmdline_opts->verbose, cmdline_opts->max_processes);
    
    g_s2e = self;

	return self; 
}

S2EExecutionState *S2E_CreateInitialState(S2E* self)
{
	if (g_s2e_state != NULL) {
        llvm::errs() << "ERROR - " << __FILE__ << ":" << __LINE__ << ": g_s2e_state is already initialized" << '\n';
        exit(1);
    }
    
    S2EExecutionState *state = self->getExecutor()->createInitialState();
    g_s2e_state = state;
    return state;
}


void S2E_InitTimers(S2E *self)
{
	llvm::errs() << "TODO: implement " << __func__ << '\n';
}

void S2E_InitExecution(S2E *self, S2EExecutionState *state)
{
	llvm::errs() << "TODO: implement " << __func__ << '\n';
}

void S2E_RegisterDirtyMask(S2E *self, S2EExecutionState *state)
{
	llvm::errs() << "TODO: implement " << __func__ << '\n';
}

void S2E_CallOnInitializationCompleteHandlers(S2E *self)
{
	llvm::errs() << "TODO: implement " << __func__ << '\n';
}

void S2E_CallOnDeviceRegistrationHandlers(S2E *self)
{
	llvm::errs() << "TODO: implement " << __func__ << '\n';
}

void S2E_CallOnPrivilegeChangeHandlers(S2E *self, unsigned from, unsigned to)
{
	llvm::errs() << "TODO: implement " << __func__ << '\n';
}

void S2E_CallOnTranslateBlockStartHandlers(S2E *self, S2EExecutionState *state, TranslationBlock *tb, uint64_t pc)
{
	llvm::errs() << "TODO: implement " << __func__ << '\n';
}

void S2E_CallOnTranslateInstructionStartHandlers(S2E *self, S2EExecutionState *state, TranslationBlock *tb, uint64_t pc)
{
	llvm::errs() << "TODO: implement " << __func__ << '\n';
}

void S2e_CallOnTranslateInstructionEndHandlers(S2E *self, S2EExecutionState *state, TranslationBlock *tb, uint64_t pc, uint64_t nextPc)
{
	llvm::errs() << "TODO: implement " << __func__ << '\n';
}

void S2E_CallOnTranslateBlockEndHandlers(S2E *self, S2EExecutionState *state, TranslationBlock *tb, uint64_t pc, bool hasNextPc, uint64_t nextPc)
{
	llvm::errs() << "TODO: implement " << __func__ << '\n';
}

bool S2E_IsForking(S2E *self)
{
	llvm::errs() << "TODO: implement " << __func__ << '\n';
	return false;
}
