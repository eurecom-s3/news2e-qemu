#include <cassert>
#include <cstdlib>

#include <llvm/Support/raw_ostream.h>

extern "C" {
#include "qemu/error-report.h"
}

#include "s2e/target/S2E.h"
#include "s2e/cxx/S2E.h"
#include "s2e/cxx/S2EExecutor.h"
#include "s2e/cxx/TCGLLVMContext.h"

S2E *g_s2e = NULL;

void S2E_Initialize(int argc, char * argv[], S2ECommandLineOptions *cmdline_opts)
{
    assert(g_s2e == NULL && "S2E is already initialized");
    assert(g_s2e_state == NULL && "Initial execution state already exists");

	if (!cmdline_opts->config_file) {
        error_report("No S2E configuration file specified (with -s2e-config-file)");
        return;
	}

	TCGLLVMContext *tcg_llvm_ctx = TCGLLVMContext::getInstance();
	llvm::outs() << "Initializing S2E" << '\n';
	g_s2e = new S2E(argc, argv, tcg_llvm_ctx,
			cmdline_opts->config_file ? cmdline_opts->config_file : "",
			cmdline_opts->output_dir ? cmdline_opts->output_dir : "",
			cmdline_opts->verbose, cmdline_opts->max_processes);
	g_s2e_state = g_s2e->getExecutor()->createInitialState();

	atexit(S2E_Destroy);
}

S2E* S2E_GetInstance(void) 
{
    assert(g_s2e && "S2E instance should be initialized by now");
    return g_s2e;
}


S2EExecutor* S2E_GetExecutor(S2E* self)
{
    return self->getExecutor();
}


void S2E_InitTimers(S2E *self)
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


void S2E_Destroy(void)
{
	if (g_s2e) {
		delete g_s2e;
		g_s2e = NULL;
	}
}
