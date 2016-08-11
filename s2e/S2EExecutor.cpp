/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Currently maintained by:
 *    Volodymyr Kuznetsov (vova.kuznetsov@epfl.ch)
 *    Vitaly Chipounov (vitaly.chipounov@epfl.ch)
 *
 * Revision List:
 *    v1.0 - Initial Release - Vitaly Chipounov, Vova Kuznetsov
 *
 * All contributors listed in S2E-AUTHORS.
 *
 */
#include <llvm/Support/Process.h> /* XXX: Needs to be here because of weird compiler errors */


extern "C" {
#include <setjmp.h>

#include <qemu-common.h>
#include "exec/cpu-all.h"
#include <tcg.h>
#include "exec/exec-all.h"
#include "exec/ioport.h"
#include "sysemu/sysemu.h"
#include "sysemu/cpus.h"
#include "qom/cpu.h"
#include "trace.h"
#include "exec/ram_addr.h"

void QEMU_NORETURN raise_exception(CPUArchState *env, int exception_index);
void QEMU_NORETURN raise_exception_err(CPUArchState *env, int exception_index, int error_code);
extern const uint8_t parity_table[256];
extern const uint8_t rclw_table[32];
extern const uint8_t rclb_table[32];




#ifdef TARGET_ARM
void do_interrupt(CPUArchState *env);
void s2e_do_interrupt(void);
#elif defined(TARGET_I386)
void do_interrupt_all(int intno, int is_int, int error_code,
                  target_ulong next_eip, int is_hw);
void s2e_do_interrupt_all(int intno, int is_int, int error_code,
                             target_ulong next_eip, int is_hw);
#endif

uint64_t helper_set_cc_op_eflags(void);

}

#ifndef __APPLE__
#include <malloc.h>
#endif

#include "exec/s2e.h"
#include "s2e/helpers.h"
#include "s2e/cxx/S2EExecutor.h"
#include "s2e/cxx/TCGLLVMContext.h"
#include "s2e/TCGLLVMRuntime.h"
#include "s2e/s2e_config.h"
#include "s2e/cxx/S2E.h"
#include "s2e/cxx/S2EExecutionState.h"
#include "s2e/cxx/Utils.h"
#include "s2e/Plugins/CorePlugin.h"
#include "s2e/cxx/ConfigFile.h"
#include "s2e/cxx/S2EMergingSearcher.h"

#include "s2e/cxx/S2EDeviceState.h"
#include "s2e/cxx/SelectRemovalPass.h"
#include "s2e/cxx/S2EStatsTracker.h"

//XXX: Remove this from executor
#include "s2e/Plugins/ModuleExecutionDetector.h"

#include "s2e/s2e_qemu.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/CommandLine.h>

//KLEE includes
#include "lib/Core/PTree.h"
#include "lib/Core/Memory.h"
#include "lib/Core/Searcher.h"
#include "lib/Core/ExternalDispatcher.h"
#include "lib/Core/UserSearcher.h"
#include "lib/Core/CoreStats.h"
#include "lib/Core/TimingSolver.h"
#include <klee/TimerStatIncrementer.h>
#include <klee/Solver.h>
#include <klee/Internal/Module/InstructionInfoTable.h>

#include <llvm/Transforms/IPO.h>
#include <llvm/Support/TimeValue.h>

#include <vector>

#include <sstream>

#ifdef WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#include <tr1/functional>

//#define S2E_DEBUG_MEMORY
//#define S2E_DEBUG_INSTRUCTIONS
//#define S2E_TRACE_EFLAGS
//#define FORCE_CONCRETIZATION

using namespace std;
using namespace llvm;
using namespace klee;

using llvm::legacy::PassManager;
using llvm::createInternalizePass;
using llvm::createGlobalOptimizerPass;

using klee::ExternalDispatcher;

extern "C" {
    // XXX
    //void* g_s2e_exec_ret_addr = 0;
}

namespace {
    uint64_t hash64(uint64_t val, uint64_t initial = 14695981039346656037ULL) {
        const char* __first = (const char*) &val;
        for (unsigned int i = 0; i < sizeof(uint64_t); ++i) {
            initial ^= static_cast<uint64_t>(*__first++);
            initial *= static_cast<uint64_t>(1099511628211ULL);
        }
        return initial;
    }
}

namespace {
    llvm::cl::opt<bool>
    UseSelectCleaner("use-select-cleaner",
            llvm::cl::desc("Remove Select statements from LLVM code"),
            llvm::cl::init(false));

    llvm::cl::opt<bool>
    StateSharedMemory("state-shared-memory",
            llvm::cl::desc("Allow unimportant memory regions (like video RAM) to be shared between states"),
            llvm::cl::init(false));


    llvm::cl::opt<bool>
    FlushTBsOnStateSwitch("flush-tbs-on-state-switch",
            llvm::cl::desc("Flush translation blocks when switching states -"
                     " disabling leads to faster but possibly incorrect execution"),
            llvm::cl::init(true));

    llvm::cl::opt<bool>
    KeepLLVMFunctions("keep-llvm-functions",
            llvm::cl::desc("Never delete generated LLVM functions"),
            llvm::cl::init(false));

    //The default is true for two reasons:
    //1. Symbolic addresses are very expensive to handle
    //2. There is lazy forking which will eventually enumerate
    //all possible addresses.
    //Overall, we have more path explosion, but at least execution
    //does not get stuck in various places.
    llvm::cl::opt<bool>
    ForkOnSymbolicAddress("fork-on-symbolic-address",
            llvm::cl::desc("Fork on each memory access with symbolic address"),
            llvm::cl::init(true));

    llvm::cl::opt<bool>
    ConcretizeIoAddress("concretize-io-address",
            llvm::cl::desc("Concretize symbolic I/O addresses"),
            llvm::cl::init(true));

    //XXX: Works for MMIO only, add support for port I/O
    llvm::cl::opt<bool>
    ConcretizeIoWrites("concretize-io-writes",
            llvm::cl::desc("Concretize symbolic I/O writes"),
            llvm::cl::init(true));

    llvm::cl::opt<bool>
    S2EDebugInstructions("print-llvm-instructions",
                   llvm::cl::desc("Traces all LLVM instructions sent to KLEE"),  llvm::cl::init(false));

    llvm::cl::opt<bool>
    VerboseFork("verbose-fork-info",
                   llvm::cl::desc("Print detailed information on forks"),  llvm::cl::init(false));

    llvm::cl::opt<bool>
    VerboseStateSwitching("verbose-state-switching",
                   llvm::cl::desc("Print detailed information on state switches"),  llvm::cl::init(false));

    llvm::cl::opt<bool>
    VerboseTbFinalize("verbose-tb-finalize",
                   llvm::cl::desc("Print detailed information when finalizing a partially-completed TB"),  llvm::cl::init(false));

    llvm::cl::opt<bool>
    UseFastHelpers("use-fast-helpers",
                   llvm::cl::desc("Replaces LLVM bitcode with fast symbolic-aware equivalent native helpers"),  llvm::cl::init(false));

    llvm::cl::opt<unsigned>
    ClockSlowDown("clock-slow-down",
                   llvm::cl::desc("Slow down factor when interpreting LLVM code"),  llvm::cl::init(101));

    llvm::cl::opt<unsigned>
    ClockSlowDownFastHelpers("clock-slow-down-fast-helpers",
                   llvm::cl::desc("Slow down factor when interpreting LLVM code and using fast helpers"),  llvm::cl::init(11));
}

//The logs may be flooded with messages when switching execution mode.
//This option allows disabling printing mode switches.
llvm::cl::opt<bool>
PrintModeSwitch("print-mode-switch",
                llvm::cl::desc("Print message when switching from symbolic to concrete and vice versa"),
                llvm::cl::init(false));

llvm::cl::opt<bool>
PrintForkingStatus("print-forking-status",
                llvm::cl::desc("Print message when enabling/disabling forking."),
                llvm::cl::init(false));

llvm::cl::opt<bool>
VerboseStateDeletion("verbose-state-deletion",
               llvm::cl::desc("Print detailed information on state deletion"),  llvm::cl::init(false));

//Concolic mode is the default because it works better than symbex.
llvm::cl::opt<bool>
ConcolicMode("use-concolic-execution",
               llvm::cl::desc("Concolic execution mode"),  llvm::cl::init(true));

llvm::cl::opt<bool>
DebugConstraints("debug-constraints",
               llvm::cl::desc("Check that added constraints are satisfiable"),  llvm::cl::init(false));




extern llvm::cl::opt<bool> UseExprSimplifier;

extern "C" {
    bool g_s2e_fork_on_symbolic_address = 0;
    int g_s2e_concretize_io_addresses = 1;
    int g_s2e_concretize_io_writes = 1;

    void s2e_print_instructions(int v);
    void s2e_print_instructions(int v) {
        S2EDebugInstructions = v;
    }
}


namespace s2e {

/* Global array to hold tb function arguments,
 * Needs to be global because it is registered as external memory
 * object to KLEE.
 */
volatile void* tb_function_args[3];

/* External dispatcher to convert QEMU s2e_longjmp's into C++ exceptions */
class S2EExternalDispatcher: public klee::ExternalDispatcher
{
protected:
    virtual bool runProtectedCall(llvm::Function *f, uint64_t *args);

public:
    S2EExternalDispatcher(ExecutionEngine* engine):
            ExternalDispatcher(engine) {}

    void removeFunction(llvm::Function *f);
};

extern "C" {

// FIXME: This is not reentrant.
static s2e_jmp_buf s2e_escapeCallJmpBuf;
static s2e_jmp_buf s2e_cpuExitJmpBuf;

#ifdef _WIN32
static void s2e_ext_sigsegv_handler(int signal)
{
}
#else
static void s2e_ext_sigsegv_handler(int signal, siginfo_t *info, void *context) {
  s2e_longjmp(s2e_escapeCallJmpBuf, 1);
}
#endif

}

bool S2EExternalDispatcher::runProtectedCall(llvm::Function *f, uint64_t *args) {
//  CPUState *cpu = ENV_GET_CPU(
  #ifndef _WIN32
  struct sigaction segvAction, segvActionOld;
  #endif
  bool res;
  CPUState* cpu = g_s2e_state->getCPUState();

  if (!f)
    return false;

  gTheArgsP = args;

  #ifdef _WIN32
  signal(SIGSEGV, s2e_ext_sigsegv_handler);
  #else
  segvAction.sa_handler = 0;
  memset(&segvAction.sa_mask, 0, sizeof(segvAction.sa_mask));
  segvAction.sa_flags = SA_SIGINFO;
  segvAction.sa_sigaction = s2e_ext_sigsegv_handler;
  sigaction(SIGSEGV, &segvAction, &segvActionOld);
  #endif

  memcpy(s2e_cpuExitJmpBuf, cpu->jmp_env, sizeof(cpu->jmp_env));

  if(sigsetjmp(cpu->jmp_env, 0) == 0) {
      if (sigsetjmp(s2e_escapeCallJmpBuf, 0) == 0) {
        std::vector<GenericValue> gvArgs;

        executionEngine->runFunction(f, gvArgs);
        res = true;
      }
      else {
        res = false;
      }
  }
  else {
      memcpy(cpu->jmp_env, s2e_cpuExitJmpBuf, sizeof(cpu->jmp_env));
      throw CpuExitException();
  }

  memcpy(cpu->jmp_env, s2e_cpuExitJmpBuf, sizeof(cpu->jmp_env));

  #ifdef _WIN32
#warning Implement more robust signal handling on windows
  signal(SIGSEGV, SIG_IGN);
#else
  sigaction(SIGSEGV, &segvActionOld, 0);
#endif
  return res;
}

/**
 * Remove all mappings between calls to external functions
 * and the actual external call stub generated by KLEE.
 * Also remove the machine code for the stub and the stub itself.
 * This is used whenever S2E deletes a translation block and its LLVM
 * representation. Failing to do so would leave stale references to
 * machine code in KLEE's external dispatcher.
 */
void S2EExternalDispatcher::removeFunction(llvm::Function *f) {
        ExternalDispatcher::dispatchers_ty::iterator it, itn;

        it = dispatchers.begin();
        while (it != dispatchers.end()) {
            if ((*it).first->getParent()->getParent() == f) {

                llvm::Function *dispatcher = (*it).second;
                executionEngine->freeMachineCodeForFunction(dispatcher);
                dispatcher->eraseFromParent();

                itn = it;
                ++itn;
                dispatchers.erase(it);
                it = itn;
            } else {
                ++it;
            }
        }
    }


S2EHandler::S2EHandler(S2E* s2e)
        : m_s2e(s2e)
{
}

llvm::raw_ostream &S2EHandler::getInfoStream() const
{
    return m_s2e->getInfoStream();
}

std::string S2EHandler::getOutputFilename(const std::string &fileName)
{
    return m_s2e->getOutputFilename(fileName);
}

llvm::raw_fd_ostream *S2EHandler::openOutputFile(const std::string &fileName)
{
    return m_s2e->openOutputFile(fileName);
}

/* klee-related function */
void S2EHandler::incPathsExplored()
{
    m_pathsExplored++;
}

/* klee-related function */
void S2EHandler::processTestCase(const klee::ExecutionState &state,
                     const char *err, const char *suffix)
{
    //XXX: This stuff is not used anymore
    //Use onTestCaseGeneration event instead.
}

void S2EExecutor::handlerTraceMemoryAccess(Executor* executor,
                                     ExecutionState* state,
                                     klee::KInstruction* target,
                                     std::vector<klee::ref<klee::Expr> > &args)
{
    S2EExecutor* self = cast<S2EExecutor>(executor);
    assert(self && "Got KLEE executor instead of S2EExecutor");

    if(!self->m_s2e->getCorePlugin()->onDataMemoryAccess.empty()) {
        S2EExecutionState* s2eState = cast<S2EExecutionState>(state);

        assert(args.size() == 6);

        Expr::Width width = cast<klee::ConstantExpr>(args[3])->getZExtValue();
        bool isWrite = cast<klee::ConstantExpr>(args[4])->getZExtValue();
        bool isIO    = cast<klee::ConstantExpr>(args[5])->getZExtValue();

        klee::ref<Expr> value = klee::ExtractExpr::create(args[2], 0, width);

        klee::ref<Expr> result = self->m_s2e->getCorePlugin()->onDataMemoryAccess.emit(
                s2eState, args[0], args[1], value, isWrite, isIO);

        if (value != result)
        {
            g_s2e->getWarningsStream() << "ERROR: The memory value has been changed by a onDataMemoryAccess handler, but writing back of this value is not yet implemented" << '\n';
        }
    }
}

void S2EExecutor::handlerInstrumentInstruction(klee::Executor* executor,
                                klee::ExecutionState* state,
                                klee::KInstruction* target,
                                std::vector<klee::ref<klee::Expr> > &args)
{
    S2EExecutionState* s2eState = static_cast<S2EExecutionState*>(state);

    //Should definitely be concrete
    ExecutionSignal* signal = reinterpret_cast<ExecutionSignal*>(cast<klee::ConstantExpr>(args.at(1))->getZExtValue());
    uint64_t pc = cast<klee::ConstantExpr>(args.at(2))->getZExtValue();
    signal->emit(static_cast<S2EExecutionState*>(state), pc);
}

void S2EExecutor::handlerOnTlbMiss(Executor* executor,
                                     ExecutionState* state,
                                     klee::KInstruction* target,
                                     std::vector<klee::ref<klee::Expr> > &args)
{
    assert(args.size() == 4);

    S2EExecutionState* s2eState = cast<S2EExecutionState>(state);
    klee::ref<Expr> addr = args[2];
    bool isWrite = cast<klee::ConstantExpr>(args[3])->getZExtValue();

    if(!isa<klee::ConstantExpr>(addr)) {
        /*
        g_s2e->getWarningsStream()
                << "Warning: s2e_on_tlb_miss does not support symbolic addresses"
                << '\n';
                */
        return;
    }

    uint64_t constAddress;
    constAddress = cast<klee::ConstantExpr>(addr)->getZExtValue(64);

    s2e_on_tlb_miss(g_s2e, s2eState, constAddress, isWrite);
}

void S2EExecutor::handlerTracePortAccess(Executor* executor,
                                     ExecutionState* state,
                                     klee::KInstruction* target,
                                     std::vector<klee::ref<klee::Expr> > &args)
{
    S2EExecutor* s2eExecutor = cast<S2EExecutor>(executor);

    if(!s2eExecutor->m_s2e->getCorePlugin()->onPortAccess.empty()) {
        S2EExecutionState* s2eState = cast<S2EExecutionState>(state);

        assert(args.size() == 4);

        Expr::Width width = cast<klee::ConstantExpr>(args[2])->getZExtValue();
        bool isWrite = cast<klee::ConstantExpr>(args[3])->getZExtValue();

        klee::ref<Expr> value = klee::ExtractExpr::create(args[1], 0, width);

        s2eExecutor->m_s2e->getCorePlugin()->onPortAccess.emit(
                s2eState, args[0], value, isWrite);
    }
}

void S2EExecutor::handleForkAndConcretize(Executor* executor,
                                     ExecutionState* state,
                                     klee::KInstruction* target,
                                     std::vector< klee::ref<Expr> > &args)
{
    S2EExecutor* s2eExecutor = cast<S2EExecutor>(executor);

    assert(args.size() == 3);
    klee::ref<Expr> address = args[0];

    address = state->constraints.simplifyExpr(address);

    if (UseExprSimplifier) {
        address = s2eExecutor->simplifyExpr(*state, address);
    }

    if(isa<klee::ConstantExpr>(address)) {
        s2eExecutor->bindLocal(target, *state, address);
        return;
    }

    klee::ref<klee::Expr> concreteAddress;

    if (ConcolicMode) {
        concreteAddress = state->concolics.evaluate(address);
        assert(dyn_cast<klee::ConstantExpr>(concreteAddress) && "Could not evaluate address");
    } else {
        //Not in concolic mode, will have to invoke the constraint solver
        //to compute a concrete value
        klee::ref<klee::ConstantExpr> value;
        bool success = s2eExecutor->getSolver()->getValue(
                Query(state->constraints, address), value);

        if (!success) {
            s2eExecutor->terminateStateEarly(*state, "Could not compute a concrete value for a symbolic address");
            assert(false && "Can't get here");
        }

        concreteAddress = value;
    }

    klee::ref<klee::Expr> condition = EqExpr::create(concreteAddress, address);

    if (!state->forkDisabled) {
        //XXX: may create deep paths!
        StatePair sp = executor->fork(*state, condition, true);

        //The condition is always true in the current state
        //(i.e., expr == concreteAddress holds).
        assert(sp.first == state);

        //It may happen that the simplifier figures out that
        //the condition is always true, in which case, no fork is needed.
        //TODO: find a test case for that
        if (sp.second) {
            //Will have to reexecute handleForkAndConcretize in the speculative state
            sp.second->pc = sp.second->prevPC;
        }
    } else {
        state->addConstraint(condition);
    }

    s2eExecutor->bindLocal(target, *state, concreteAddress);
}

void S2EExecutor::handleMakeSymbolic(Executor* executor,
                                     ExecutionState* state,
                                     klee::KInstruction* target,
                                     std::vector< klee::ref<Expr> > &args)
{
    S2EExecutionState* s2eState = static_cast<S2EExecutionState*>(state);
    s2eState->makeSymbolic(args, false);
}

void S2EExecutor::handleGetValue(klee::Executor* executor,
                                 klee::ExecutionState* state,
                                 klee::KInstruction* target,
                                 std::vector<klee::ref<klee::Expr> > &args) {
    S2EExecutionState* s2eState = cast<S2EExecutionState>(state);
    assert(args.size() == 3 &&
           "Expected three args to tcg_llvm_get_value: addr, size, add_constraint");

    // KLEE address of variable
    klee::ref<klee::ConstantExpr> kleeAddress = cast<klee::ConstantExpr>(args[0]);
    
    // Size in bytes
    uint64_t sizeInBytes = cast<klee::ConstantExpr>(args[1])->getZExtValue();

    // Add a constraint permanently?
    bool add_constraint = cast<klee::ConstantExpr>(args[2])->getZExtValue();

    // Read the value and concretize it.
    // The value will be stored at kleeAddress
    std::vector<klee::ref<Expr> > result;
    s2eState->kleeReadMemory(kleeAddress, sizeInBytes, NULL, false, true, add_constraint);
}

void S2EExecutor::handleBaseInstruction(klee::Executor* executor,
                               klee::ExecutionState* state,
                               klee::KInstruction* target,
                               std::vector<klee::ref<klee::Expr> > &args)
{
	klee::ref<klee::ConstantExpr> env = cast<klee::ConstantExpr>(args[0]);
	assert(!env.isNull() && "env pointer must be a constant expression");
	klee::ref<klee::ConstantExpr> opcode = cast<klee::ConstantExpr>(args[1]);
	assert(!opcode.isNull() && "opcode must be a constant expression");

	g_s2e->getCorePlugin()->onCustomInstruction.emit(static_cast<S2EExecutionState*>(state), opcode->getZExtValue());
}

S2EExecutor::S2EExecutor(S2E* s2e, TCGLLVMContext *tcgLLVMContext,
                    const InterpreterOptions &opts,
                            InterpreterHandler *ie)
        : Executor(opts, ie, tcgLLVMContext->getExecutionEngine()),
          m_s2e(s2e), m_tcgLLVMContext(tcgLLVMContext),
          m_executeAlwaysKlee(false), m_forkProcTerminateCurrentState(false),
          m_inLoadBalancing(false), yieldedState(NULL)
{
    delete externalDispatcher;
    externalDispatcher = new S2EExternalDispatcher(
            tcgLLVMContext->getExecutionEngine());

    LLVMContext& ctx = m_tcgLLVMContext->getLLVMContext();
    Module *M = m_tcgLLVMContext->getModule();

    // XXX: this will not work without creating JIT
    // XXX: how to get data layout without without ExecutionEngine ?
    //TODO: Is it even necessary to set the data layout here? Should be included from the compiled module ...
    m_tcgLLVMContext->getModule()->setDataLayout(
            m_tcgLLVMContext->getExecutionEngine()
                ->getDataLayout()->getStringRepresentation());

    //The symbols registered here are supposed to be called from LLVM IR
	//code generated for a TB. All other functions in the LLVM module will
	//be deleted by optimization passes.
    registerExternalSymbols();

    //Delete body of functions that are supposed to be called in native code
    if (llvm::Function* f = M->getFunction("tlb_fill")) {
        f->deleteBody();
    }

    //Remove all functions from the module that are not contained in a whitelist.
    //This is useful, because tracking down each missing symbol is a pain. Much easier
    //to start from a clean slate and then include successively what is required.
    cleanModule(M);


    if(UseSelectCleaner) {
        m_tcgLLVMContext->getFunctionPassManager()->add(new SelectRemovalPass());
        m_tcgLLVMContext->getFunctionPassManager()->doInitialization();
    }

    //It is not cool to allow KLEE to run the optimize passes,
    //as it will first run an Internalize pass, and then the
    //GlobalOpt pass, which will remove all the internalized functions,
    //leaving us with an empty module.
    //Subsequently, the helper function definitions cannot be found when
    //the external helpers are registered, which makes us :(
    ModuleOptions MOpts = ModuleOptions(
    		/* _LibraryDir = */ "",
			/* _Optimize = */ false,
			/* _CheckDivZero = */ false,
			/* _CheckOvershift = */ false,
			/* _CustomPasses = */ nullptr);

    /* This catches obvious LLVM misconfigurations */
    DataLayout TD(M);
    assert(M->getPointerSize() == Module::Pointer64 && "Something is broken in your LLVM build: LLVM thinks pointers are 32-bits!");

    s2e->getDebugStream() << "Current data layout: " << M->getDataLayout() << '\n';
    s2e->getDebugStream() << "Current target triple: " << M->getTargetTriple() << '\n';

    setModule(M, MOpts, false);

    if (UseFastHelpers) {
        disableConcreteLLVMHelpers();
    }

    /* Add dummy TB function declaration */
    PointerType* tbFunctionArgTy =
            PointerType::get(IntegerType::get(ctx, 64), 0);
    FunctionType* tbFunctionTy = FunctionType::get(
            IntegerType::get(ctx, TCG_TARGET_REG_BITS),
            ArrayRef<llvm::Type*>(vector<llvm::Type*>(1, PointerType::get(
                    IntegerType::get(ctx, 64), 0))),
            false);

    llvm::Function* tbFunction = llvm::Function::Create(
            tbFunctionTy, llvm::Function::PrivateLinkage, "s2e_dummyTbFunction",
            m_tcgLLVMContext->getModule());

    /* Create dummy main function containing just two instructions:
       a call to TB function and ret */
    llvm::Function* dummyMain = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(ctx), false),
            llvm::Function::PrivateLinkage, "s2e_dummyMainFunction",
            m_tcgLLVMContext->getModule());

    BasicBlock* dummyMainBB = BasicBlock::Create(ctx, "entry", dummyMain);

    vector<Value*> tbFunctionArgs(1, ConstantPointerNull::get(tbFunctionArgTy));
    CallInst::Create(tbFunction, ArrayRef<Value*>(tbFunctionArgs),
            "tbFunctionCall", dummyMainBB);
    ReturnInst::Create(m_tcgLLVMContext->getLLVMContext(), dummyMainBB);

    kmodule->updateModuleWithFunction(dummyMain);
    m_dummyMain = kmodule->functionMap[dummyMain];

    llvm::Function* function;

    //Function types don't matter at this point, we simply insert all helper functions as
    //void (*)(void) type, even if they aren't.
    function = kmodule->module->getFunction("helper_s2e_instrument_code");
    assert(function);
    addSpecialFunctionHandler(function, handlerInstrumentInstruction);

    function = kmodule->module->getFunction("helper_s2e_base_instruction");
	assert(function);
	addSpecialFunctionHandler(function, handleBaseInstruction);

//    llvm::errs() << *kmodule->module << '\n';
    llvm::errs() << "WARN - " << __FILE__ << ":" << __LINE__ << ": helper functions stubbed" << '\n';

//    function = kmodule->module->getFunction("tcg_llvm_trace_memory_access");
//    assert(function);
//    addSpecialFunctionHandler(function, handlerTraceMemoryAccess);

// TODO: Find a way to bypass i/o access (when I/O adress is symbolic) for ARM
#ifndef TARGET_ARM
//    function = kmodule->module->getFunction("tcg_llvm_trace_port_access");
//    assert(function);
//    addSpecialFunctionHandler(function, handlerTracePortAccess);
#endif

//    function = kmodule->module->getFunction("s2e_on_tlb_miss");
//    assert(function);
//    addSpecialFunctionHandler(function, handlerOnTlbMiss);

//    function = kmodule->module->getFunction("tcg_llvm_fork_and_concretize");
//    assert(function);
//    addSpecialFunctionHandler(function, handleForkAndConcretize);

// XXX: is this really not needed on ARM?
#ifndef TARGET_ARM
//    function = kmodule->module->getFunction("tcg_llvm_make_symbolic");
//    assert(function);
//    addSpecialFunctionHandler(function, handleMakeSymbolic);
#endif

//    function = kmodule->module->getFunction("tcg_llvm_get_value");
//    assert(function);
//    addSpecialFunctionHandler(function, handleGetValue);


//    llvm::FunctionType *traceInstTy = llvm::FunctionType::get(llvm::Type::getVoidTy(M->getContext()), false);
//    function = dynamic_cast<llvm::Function*>(kmodule->module->getOrInsertFunction("tcg_llvm_trace_instruction", traceInstTy));
//    assert(function);
//    addSpecialFunctionHandler(function, handlerTraceInstruction);

    if (UseFastHelpers) {
        replaceExternalFunctionsWithSpecialHandlers();
    }

    m_tcgLLVMContext->initializeHelpers();

    initializeStatistics();


    searcher = constructUserSearcher(*this);

    m_forceConcretizations = false;

    g_s2e_fork_on_symbolic_address = ForkOnSymbolicAddress;
    g_s2e_concretize_io_addresses = ConcretizeIoAddress;
    g_s2e_concretize_io_writes = ConcretizeIoWrites;

    concolicMode = ConcolicMode;

    if (UseFastHelpers) {
        if (!ForkOnSymbolicAddress) {
            s2e->getWarningsStream()
                    << UseFastHelpers.ArgStr << " can only be used if "
                    << ForkOnSymbolicAddress.ArgStr << " is enabled\n";
            ::exit(-1);
        }
    }

}

const std::string S2EExecutor::CLASS_NAME = "s2e::S2EExecutor";

void S2EExecutor::cleanModule(Module* mod)
{
	static const char * KEEP_FUNCTIONS[] = {
        "helper_s2e_instrument_code",
		"helper_s2e_base_instruction",
        "main",
        "helper_le_ldq_mmu",
        "helper_le_ldsl_mmu",
        "helper_le_ldsw_mmu",
        "helper_le_ldul_mmu",
        "helper_le_lduw_mmu",
        "helper_le_stl_mmu",
        "helper_le_stq_mmu",
        "helper_le_stw_mmu",
        "helper_ret_ldsb_mmu",
        "helper_ret_ldub_mmu",
        "helper_ret_stb_mmu",
		"helper_cpsr_read",
		"helper_cpsr_write",
		"helper_exception_with_syndrome"
	};

	PassManager pm;
	pm.add(createInternalizePass(ArrayRef<const char*>(KEEP_FUNCTIONS, sizeof(KEEP_FUNCTIONS) / sizeof(KEEP_FUNCTIONS[0]))));
	pm.add(createGlobalOptimizerPass());
	pm.run(*mod);
}

void S2EExecutor::registerExternalSymbols(void)
{
#define DEFINE_EXT_SYMBOL(name) \
    llvm::sys::DynamicLibrary::AddSymbol(#name, (void*) &name)

	DEFINE_EXT_SYMBOL(qemu_loglevel);
//    DEFINE_EXT_SYMBOL(S2EExecutionState_ReadRegisterConcrete);
//	DEFINE_EXT_SYMBOL(S2EExecutionState_WriteRegisterConcrete);
	//DEFINE_EXT_SYMBOL(__assert_fail);
	//DEFINE_EXT_SYMBOL(abort);
	//DEFINE_EXT_SYMBOL(address_space_ldl);
	//DEFINE_EXT_SYMBOL(address_space_ldq);
	//DEFINE_EXT_SYMBOL(arm_handle_psci_call);
    //DEFINE_EXT_SYMBOL(arm_is_psci_call);
    //DEFINE_EXT_SYMBOL(armv7m_nvic_acknowledge_irq);
    //DEFINE_EXT_SYMBOL(armv7m_nvic_complete_irq);
    //DEFINE_EXT_SYMBOL(armv7m_nvic_set_pending);
//   DEFINE_EXT_SYMBOL(cpu_abort);
//    DEFINE_EXT_SYMBOL(cpu_breakpoint_insert);
//    DEFINE_EXT_SYMBOL(cpu_breakpoint_remove_all);
//    DEFINE_EXT_SYMBOL(cpu_breakpoint_remove_by_ref);
//    DEFINE_EXT_SYMBOL(cpu_generic_init);
    DEFINE_EXT_SYMBOL(cpu_interrupt_handler);
//    DEFINE_EXT_SYMBOL(cpu_loop_exit);
//    DEFINE_EXT_SYMBOL(cpu_restore_state);
//    DEFINE_EXT_SYMBOL(cpu_resume_from_signal);
//    DEFINE_EXT_SYMBOL(cpu_watchpoint_insert);
//    DEFINE_EXT_SYMBOL(cpu_watchpoint_remove_all);
//    DEFINE_EXT_SYMBOL(cpu_watchpoint_remove_by_ref);
    DEFINE_EXT_SYMBOL(cpus);
    DEFINE_EXT_SYMBOL(object_dynamic_cast_assert);
    DEFINE_EXT_SYMBOL(tlb_fill);
//    //DEFINE_EXT_SYMBOL(crc32);
    //DEFINE_EXT_SYMBOL(crc32c);
    //DEFINE_EXT_SYMBOL(do_arm_semihosting);
//    DEFINE_EXT_SYMBOL(fprintf);
//    DEFINE_EXT_SYMBOL(g_assertion_message_expr);
//    DEFINE_EXT_SYMBOL(g_free);
//    DEFINE_EXT_SYMBOL(g_hash_table_get_keys);
//    DEFINE_EXT_SYMBOL(g_hash_table_insert);
//    DEFINE_EXT_SYMBOL(g_hash_table_lookup);
//    DEFINE_EXT_SYMBOL(g_list_foreach);
//    DEFINE_EXT_SYMBOL(g_list_free);
//    DEFINE_EXT_SYMBOL(g_list_sort);
//    DEFINE_EXT_SYMBOL(g_malloc0);
//    DEFINE_EXT_SYMBOL(g_malloc_n);
//    DEFINE_EXT_SYMBOL(g_memdup);
//    DEFINE_EXT_SYMBOL(g_s2e_state);
//    DEFINE_EXT_SYMBOL(g_slist_foreach);
//    DEFINE_EXT_SYMBOL(g_slist_free);
//    DEFINE_EXT_SYMBOL(g_slist_sort);
//    DEFINE_EXT_SYMBOL(g_strndup);
    //DEFINE_EXT_SYMBOL(gdb_register_coprocessor);
//    DEFINE_EXT_SYMBOL(helper_ret_stb_mmu);
//    DEFINE_EXT_SYMBOL(ldl_phys);

    //The functions in this list are supposed to be called from LLVM IR
    //code generated for a TB. All other functions in the LLVM module will
    //be deleted by optimization passes.
//    __DEFINE_EXT_FUNCTION()
    /* Define globally accessible functions */
//#define __DEFINE_EXT_FUNCTION(name) \
//    llvm::sys::DynamicLibrary::AddSymbol(#name, (void*) name);

//#define __DEFINE_EXT_VARIABLE(name) \
//    llvm::sys::DynamicLibrary::AddSymbol(#name, (void*) &name);

    //__DEFINE_EXT_FUNCTION(raise_exception)
    //__DEFINE_EXT_FUNCTION(raise_exception_err)

//#ifdef _WIN32
//    __DEFINE_EXT_VARIABLE(g_s2e_concretize_io_addresses)
//    __DEFINE_EXT_VARIABLE(g_s2e_concretize_io_writes)
//    __DEFINE_EXT_VARIABLE(g_s2e_fork_on_symbolic_address)
//#endif

//    __DEFINE_EXT_VARIABLE(g_s2e_enable_mmio_checks)

//    __DEFINE_EXT_FUNCTION(fprintf)
//    __DEFINE_EXT_FUNCTION(sprintf)
//    __DEFINE_EXT_FUNCTION(fputc)
//    __DEFINE_EXT_FUNCTION(fwrite)

    DEFINE_EXT_SYMBOL(io_mem_rom);
//    __DEFINE_EXT_VARIABLE(io_mem_rom)
//    __DEFINE_EXT_VARIABLE(io_mem_unassigned)
    DEFINE_EXT_SYMBOL(io_mem_notdirty);
    DEFINE_EXT_SYMBOL(cpu_loop_exit);

//    __DEFINE_EXT_FUNCTION(cpu_io_recompile)
//#ifdef TARGET_ARM
//    llvm::errs() << "WARN - " << __FILE__ << ":" << __LINE__ << ": stubbed" << '\n';
//    //__DEFINE_EXT_FUNCTION(arm_cpu_handle_mmu_fault)
//    __DEFINE_EXT_FUNCTION(cpsr_read)
//    __DEFINE_EXT_FUNCTION(cpsr_write)
//    __DEFINE_EXT_FUNCTION(arm_cpu_list)
//    __DEFINE_EXT_FUNCTION(do_interrupt)
//#endif
//#ifdef TARGET_I386
//    __DEFINE_EXT_FUNCTION(x86_cpu_handle_mmu_fault)
//    __DEFINE_EXT_FUNCTION(cpu_x86_update_cr0)
//    __DEFINE_EXT_FUNCTION(cpu_x86_update_cr3)
//    __DEFINE_EXT_FUNCTION(cpu_x86_update_cr4)
//    __DEFINE_EXT_FUNCTION(cpu_x86_cpuid)
//    llvm::errs() << "WARN - " << __FILE__ << ":" << __LINE__ << ": stubbed" << '\n';
//    __DEFINE_EXT_FUNCTION(cpu_get_apic_base)
//    __DEFINE_EXT_FUNCTION(cpu_set_apic_base)
//    __DEFINE_EXT_FUNCTION(cpu_get_apic_tpr)
//    __DEFINE_EXT_FUNCTION(cpu_set_apic_tpr)
//    __DEFINE_EXT_FUNCTION(cpu_smm_update)
//    llvm::errs() << "WARN - " << __FILE__ << ":" << __LINE__ << ": stubbed" << '\n';
//    __DEFINE_EXT_FUNCTION(hw_breakpoint_insert)
//    __DEFINE_EXT_FUNCTION(hw_breakpoint_remove)
//    __DEFINE_EXT_FUNCTION(check_hw_breakpoints)
//    __DEFINE_EXT_FUNCTION(cpu_get_tsc)
//    helper_register_symbols();
//#endif

//    __DEFINE_EXT_FUNCTION(cpu_outb)
//    __DEFINE_EXT_FUNCTION(cpu_outw)
//    __DEFINE_EXT_FUNCTION(cpu_outl)
//    __DEFINE_EXT_FUNCTION(cpu_inb)
//    __DEFINE_EXT_FUNCTION(cpu_inw)
//    __DEFINE_EXT_FUNCTION(cpu_inl)
//    __DEFINE_EXT_FUNCTION(cpu_restore_state)
//    __DEFINE_EXT_FUNCTION(cpu_abort)
//    __DEFINE_EXT_FUNCTION(cpu_loop_exit)

//    llvm::errs() << "WARN - " << __FILE__ << ":" << __LINE__ << ": stubbed" << '\n';
//    __DEFINE_EXT_FUNCTION(tb_find_pc)

//    __DEFINE_EXT_FUNCTION(qemu_system_reset_request)

//    __DEFINE_EXT_FUNCTION(tlb_flush_page)
//    __DEFINE_EXT_FUNCTION(tlb_flush)

//    llvm::errs() << "WARN - " << __FILE__ << ":" << __LINE__ << ": stubbed" << '\n';
//    __DEFINE_EXT_FUNCTION(io_readb_mmu)
//    __DEFINE_EXT_FUNCTION(io_readw_mmu)
//    __DEFINE_EXT_FUNCTION(io_readl_mmu)
//    __DEFINE_EXT_FUNCTION(io_readq_mmu)

//    __DEFINE_EXT_FUNCTION(io_writeb_mmu)
//    __DEFINE_EXT_FUNCTION(io_writew_mmu)
//    __DEFINE_EXT_FUNCTION(io_writel_mmu)
//    __DEFINE_EXT_FUNCTION(io_writeq_mmu)

//    __DEFINE_EXT_FUNCTION(iotlb_to_region)


//    __DEFINE_EXT_FUNCTION(s2e_ensure_symbolic)

    //__DEFINE_EXT_FUNCTION(s2e_on_tlb_miss)
//    __DEFINE_EXT_FUNCTION(s2e_on_page_fault)
//    __DEFINE_EXT_FUNCTION(s2e_is_port_symbolic)
//    __DEFINE_EXT_FUNCTION(s2e_is_mmio_symbolic_b)
//    __DEFINE_EXT_FUNCTION(s2e_is_mmio_symbolic_w)
//    __DEFINE_EXT_FUNCTION(s2e_is_mmio_symbolic_l)
//    __DEFINE_EXT_FUNCTION(s2e_is_mmio_symbolic_q)

//    __DEFINE_EXT_FUNCTION(s2e_on_privilege_change);
//    __DEFINE_EXT_FUNCTION(s2e_on_page_fault);


//    __DEFINE_EXT_FUNCTION(s2e_ismemfunc)
//    __DEFINE_EXT_FUNCTION(s2e_notdirty_mem_write)

//    __DEFINE_EXT_FUNCTION(cpu_io_recompile)
//    llvm::errs() << "WARN - " << __FILE__ << ":" << __LINE__ << ": stubbed" << '\n';
 //   __DEFINE_EXT_FUNCTION(can_do_io)

//    __DEFINE_EXT_FUNCTION(ldub_phys)
//    __DEFINE_EXT_FUNCTION(stb_phys)

//    __DEFINE_EXT_FUNCTION(lduw_phys)
//    __DEFINE_EXT_FUNCTION(stw_phys)

//    __DEFINE_EXT_FUNCTION(ldl_phys)
//    __DEFINE_EXT_FUNCTION(stl_phys)

//    __DEFINE_EXT_FUNCTION(ldq_phys)
//    __DEFINE_EXT_FUNCTION(stq_phys)

#if defined(TARGET_ARM)
    DEFINE_EXT_SYMBOL(cpsr_read);
    DEFINE_EXT_SYMBOL(cpsr_write);
#endif /* defined(TARGET_ARM) */

#undef DEFINE_EXT_SYMBOL
}

void S2EExecutor::initializeStatistics()
{
    if(StatsTracker::useStatistics()) {
        statsTracker =
                new S2EStatsTracker(*this,
                    interpreterHandler->getOutputFilename("assembly.ll"),
                    userSearcherRequiresMD2U());
        statsTracker->writeHeaders();
    }
}


void S2EExecutor::flushTb() {
    if (g_s2e_state) {
        tb_flush(g_s2e_state->getCPUState());
    }
}

S2EExecutor::~S2EExecutor()
{
	llvm::errs() << __FILE__ << ":" << __LINE__ << ": TODO - S2EExecutor::~S2EExecutor is stubbed" << '\n';
//    if(statsTracker)
//        statsTracker->done();
}

S2EExecutionState* S2EExecutor::createInitialState()
{
    assert(!processTree);

    /* Create initial execution state */
    S2EExecutionState *state =
        new S2EExecutionState(m_dummyMain);

    llvm::errs() << "S2EExecutor::createInitialState: DEBUG - initial state = " << state << ", size = " << sizeof(*state) << '\n';

    state->m_runningConcrete = true;
    state->m_active = true;

    if(pathWriter)
        state->pathOS = pathWriter->open();
    if(symPathWriter)
        state->symPathOS = symPathWriter->open();

    if(statsTracker)
        statsTracker->framePushed(*state, 0);

    states.insert(state);
    searcher->update(0, states, std::set<ExecutionState*>());

    processTree = new PTree(state);
    state->ptreeNode = processTree->root;

    /* Externally accessible global vars */
    /* XXX move away */
    addExternalObject(*state, &tcg_llvm_runtime,
                      sizeof(tcg_llvm_runtime), false,
                      /* isUserSpecified = */ true,
                      /* isSharedConcrete = */ true,
                      /* isValueIgnored = */ true);

    addExternalObject(*state, (void*) tb_function_args,
                      sizeof(tb_function_args), false,
                      /* isUserSpecified = */ true,
                      /* isSharedConcrete = */ true,
                      /* isValueIgnored = */ true);

#define __DEFINE_EXT_OBJECT_RO(name) \
    predefinedSymbols.insert(std::make_pair(#name, (void*) &name)); \
    addExternalObject(*state, (void*) &name, sizeof(name), \
                      true, true, true)->setName(#name);

#define __DEFINE_EXT_OBJECT_RO_SYMB(name) \
    predefinedSymbols.insert(std::make_pair(#name, (void*) &name)); \
    addExternalObject(*state, (void*) &name, sizeof(name), \
                      true, true, false)->setName(#name);

    llvm::errs() << "WARN - " << __FILE__ << ":" << __LINE__ << ": stubbed" << '\n';
    __DEFINE_EXT_OBJECT_RO(g_s2e)
    __DEFINE_EXT_OBJECT_RO(g_s2e_state)
    //__DEFINE_EXT_OBJECT_RO(g_s2e_exec_ret_addr)
//    __DEFINE_EXT_OBJECT_RO(io_mem_read)
//    __DEFINE_EXT_OBJECT_RO(io_mem_write)
    //__DEFINE_EXT_OBJECT_RO(io_mem_opaque)
    __DEFINE_EXT_OBJECT_RO(use_icount)
//    __DEFINE_EXT_OBJECT_RO(cpu_single_env)
//    __DEFINE_EXT_OBJECT_RO(loglevel)
//    __DEFINE_EXT_OBJECT_RO(logfile)
#ifdef TARGET_I386
    __DEFINE_EXT_OBJECT_RO_SYMB(parity_table)
//    __DEFINE_EXT_OBJECT_RO_SYMB(rclw_table)
//    __DEFINE_EXT_OBJECT_RO_SYMB(rclb_table)
#endif

    m_s2e->getMessagesStream(state)
            << "Created initial state" << '\n';

    return state;
}

void S2EExecutor::initializeExecution(S2EExecutionState* state,
                                      bool executeAlwaysKlee)
{
#if 0
    typedef std::pair<uint64_t, uint64_t> _UnusedMemoryRegion;
    foreach(_UnusedMemoryRegion p, m_unusedMemoryRegions) {
        /* XXX */
        /* XXX : use qemu_virtual* */
#ifdef WIN32
        VirtualFree((void*) p.first, p.second, MEM_FREE);
#else
        munmap((void*) p.first, p.second);
#endif
    }
#endif

    m_executeAlwaysKlee = executeAlwaysKlee;

    //TODO: lock rcu list before doing that?
    registerDirtyMask(state, reinterpret_cast<uint64_t>(ram_list.dirty_memory), last_ram_offset() >> TARGET_PAGE_BITS);

    initializeGlobals(*state);
    bindModuleConstants();

    initTimers();
    initializeStateSwitchTimer();
}

void S2EExecutor::registerCpu(S2EExecutionState *initialState,
                              CPUState* cpu)
{
    CPUArchState* env = static_cast<CPUArchState*>(cpu->env_ptr);

    std::cout << std::hex
            << "Adding CPU (addr = " << std::hex << cpu
              << ", size = 0x" << sizeof(*env) << ")"
              << std::dec << '\n';

    /* Add registers and eflags area as a true symbolic area */

    initialState->m_cpuRegistersState =
        addExternalObject(*initialState, env, CPU_CONC_LIMIT,
                      /* isReadOnly = */ false,
                      /* isUserSpecified = */ false,
                      /* isSharedConcrete = */ false);


    initialState->m_cpuRegistersState->setName("CpuRegistersState");

    /* Add the rest of the structure as concrete-only area */
    initialState->m_cpuSystemState =
        addExternalObject(*initialState,
                      ((uint8_t*)env) + CPU_CONC_LIMIT,
                      sizeof(CPUArchState) - CPU_CONC_LIMIT,
                      /* isReadOnly = */ false,
                      /* isUserSpecified = */ true,
                      /* isSharedConcrete = */ true);

    initialState->m_cpuSystemState->setName("CpuSystemState");

    initialState->m_cpuState =
    	addExternalObject(*initialState,
    			          cpu,
						  /* The following works because env is embedded in a superstructure behind cpu */
						  reinterpret_cast<uint8_t *>(env) - reinterpret_cast<uint8_t *>(cpu),
						  /* isReadOnly = */ false,
						  /* isUserSpecified = */ true,
						  /* isSharedConcrete = */ true);
    initialState->m_cpuState->setName("CpuState");

    m_saveOnContextSwitch.push_back(initialState->m_cpuSystemState);
    m_saveOnContextSwitch.push_back(initialState->m_cpuState);

    const ObjectState *cpuSystemObject = initialState->addressSpace
                                .findObject(initialState->m_cpuSystemState);
    const ObjectState *cpuRegistersObject = initialState->addressSpace
                                .findObject(initialState->m_cpuRegistersState);
    const ObjectState *cpuObject = initialState->addressSpace
    		                    .findObject(initialState->m_cpuState);

    initialState->m_cpuRegistersObject = initialState->addressSpace
        .getWriteable(initialState->m_cpuRegistersState, cpuRegistersObject);
    initialState->m_cpuSystemObject = initialState->addressSpace
        .getWriteable(initialState->m_cpuSystemState, cpuSystemObject);
    initialState->m_cpuObject = initialState->addressSpace
            .getWriteable(initialState->m_cpuState, cpuObject);
}


void S2EExecutor::registerRam(S2EExecutionState *initialState,
                        uint64_t startAddress, uint64_t size,
                        uint64_t hostAddress, bool isSharedConcrete,
                        bool saveOnContextSwitch, const char *name)
{
    assert(isSharedConcrete || !saveOnContextSwitch);
    assert(startAddress == (uint64_t) -1 ||
           (startAddress & ~TARGET_PAGE_MASK) == 0);
    assert((size & ~TARGET_PAGE_MASK) == 0);
    assert((hostAddress & ~TARGET_PAGE_MASK) == 0);

    m_s2e->getDebugStream()
              << "Adding memory block (startAddr = " << hexval(startAddress)
              << ", size = " << hexval(size) << ", hostAddr = " << hexval(hostAddress)
              << ", isSharedConcrete=" << isSharedConcrete << ", name=" << name << ")\n";

#ifdef DEBUG_TLB
    qemu_log("ALLOCATE RAM of size=%" PRIu64 "\n",size);
    qemu_log("\t start address: %" PRIx64 ".\n", startAddress);
    qemu_log("\t host_address: %" PRIx64 ".\n", hostAddress);
#endif

    for(uint64_t addr = hostAddress; addr < hostAddress+size;
                 addr += S2E_RAM_OBJECT_SIZE) {
        std::stringstream ss;

        ss << name << "_" << std::hex << (addr-hostAddress);

        MemoryObject *mo = addExternalObject(
                *initialState, (void*) addr, S2E_RAM_OBJECT_SIZE, false,
                /* isUserSpecified = */ true, isSharedConcrete,
                isSharedConcrete && !saveOnContextSwitch && StateSharedMemory);

#ifdef DEBUG_TLB
        qemu_log("\t mo address: 0x%p.\n", mo);

        //get concrete store just for debugging
        ObjectPair op = initialState->addressSpace.findObject(addr);
        ObjectState* wos =
                initialState->addressSpace.getWriteable(op.first, op.second);

        qemu_log("\t wos->concreteStore  address: 0x%p.\n", wos->getConcreteStore());
#endif

        mo->setName(ss.str());

        if (isSharedConcrete && (saveOnContextSwitch || !StateSharedMemory)) {
            m_saveOnContextSwitch.push_back(mo);
        }
    }

    if(!isSharedConcrete) {
        /* XXX */
        /* XXX : use qemu_mprotect */
#ifdef WIN32
        DWORD OldProtect;
        if (!VirtualProtect((void*) hostAddress, size, PAGE_NOACCESS, &OldProtect)) {
            assert(false);
        }
#else
        mprotect((void*) hostAddress, size, PROT_NONE);
#endif
        m_unusedMemoryRegions.push_back(make_pair(hostAddress, size));
    }

    initialState->m_memcache.registerPool(hostAddress, size);

}

void S2EExecutor::registerDirtyMask(S2EExecutionState *initial_state, uint64_t host_address, uint64_t size)
{
    //Assume that dirty mask is small enough, so no need to split it in small pages
    assert(!initial_state->m_dirtyMask);
    initial_state->m_dirtyMask = g_s2e->getExecutor()->addExternalObject(
            *initial_state, (void*) host_address, size, false,
            /* isUserSpecified = */ true, true, false);

    initial_state->m_dirtyMask->setName("dirtyMask");

    m_saveOnContextSwitch.push_back(initial_state->m_dirtyMask);

    const ObjectState *dirtyMaskObject = initial_state->addressSpace
                                .findObject(initial_state->m_dirtyMask);

    initial_state->m_dirtyMaskObject = initial_state->addressSpace
        .getWriteable(initial_state->m_dirtyMask, dirtyMaskObject);
}

void S2EExecutor::terminateStateOnError(klee::ExecutionState &kleeState,
                                     const llvm::Twine &messaget,
                                     const char *suffix,
                                     const llvm::Twine &info)
{
	S2EExecutionState* state = cast<S2EExecutionState>(&kleeState);
	std::string message = messaget.str();
	static std::set< std::pair<Instruction*, std::string> > emittedErrors;
	Instruction * lastInst;
	const InstructionInfo &ii = getLastNonKleeInternalInstruction(kleeState, &lastInst);

	if (ii.file != "") {
		m_s2e->getWarningsStream(state) << "ERROR: " << ii.file
				<< ":" << ii.line << ": " << message << '\n';
	} else {
		m_s2e->getWarningsStream(state) << "ERROR: (location information missing) "
						<< message << '\n';
	}

	std::string MsgString;
	llvm::raw_string_ostream msg(MsgString);
	msg << "Error: " << message << "\n";
	if (ii.file != "") {
	  msg << "File: " << ii.file << "\n";
	  msg << "Line: " << ii.line << "\n";
	  msg << "assembly.ll line: " << ii.assemblyLine << "\n";
	}
	msg << "Stack: \n";
	kleeState.dumpStack(msg);

	std::string info_str = info.str();
	if (info_str != "")
		msg << "Info: \n" << info_str;

	m_s2e->getWarningsStream(state) << msg.str() << '\n';

	terminateState(kleeState);
}


void S2EExecutor::switchToConcrete(S2EExecutionState *state)
{
    assert(!state->m_runningConcrete);

    /* Concretize any symbolic registers */
    ObjectState* wos = state->m_cpuRegistersObject;
    assert(wos);

    if (m_forceConcretizations) {
    	assert(false && "stubbed");
//        //XXX: Find a adhoc dirty way to implement overconstrained consistency model
//        //There should be a consistency plugin somewhere else
//        s2e::plugins::ModuleExecutionDetector *md =
//                dynamic_cast<s2e::plugins::ModuleExecutionDetector*>(m_s2e->getPlugin("ModuleExecutionDetector"));
//        if (md && !md->getCurrentDescriptor(state)) {
//
//            if(!wos->isAllConcrete()) {
//                /* The object contains symbolic values. We have to
//               concretize it */
//
//                for(unsigned i = 0; i < wos->size; ++i) {
//                    ref<Expr> e = wos->read8(i);
//                    if(!isa<klee::ConstantExpr>(e)) {
//                        uint8_t ch = toConstant(*state, e,
//                                                "switching to concrete execution")->getZExtValue(8);
//                        wos->write8(i, ch);
//                    }
//                }
//            }
//        }
    }

    //assert(os->isAllConcrete());
    memcpy((void*) state->m_cpuRegistersState->address,
           wos->getConcreteStore(true), wos->size);
    static_cast<S2EExecutionState*>(state)->m_runningConcrete = true;

    if (PrintModeSwitch) {
        m_s2e->getMessagesStream(state)
                << "Switched to concrete execution at pc = "
                << hexval(state->getPc()) << '\n';
    }
}

void S2EExecutor::switchToSymbolic(S2EExecutionState *state)
{
    assert(state->m_runningConcrete);

    //assert(os && os->isAllConcrete());

    // TODO: check that symbolic registers were not accessed
    // in shared location ! Ideas: use hw breakpoints, or instrument
    // translated code.

    ObjectState *wos = state->m_cpuRegistersObject;
    memcpy(wos->getConcreteStore(true),
           (void*) state->m_cpuRegistersState->address, wos->size);
    state->m_runningConcrete = false;

    if (PrintModeSwitch) {
        m_s2e->getMessagesStream(state)
                << "Switched to symbolic execution at pc = "
                << hexval(state->getPc()) << '\n';
    }
}



void S2EExecutor::copyOutConcretes(ExecutionState &state)
{
    return;
}

bool S2EExecutor::copyInConcretes(klee::ExecutionState &state)
{
    return true;
}

void S2EExecutor::doLoadBalancing()
{
    if (states.size() < 2) {
        return;
    }

    //Don't bother copying stuff if it's obvious that it'll very likely fail
    if (m_s2e->getCurrentProcessCount() == m_s2e->getMaxProcesses()) {
        return;
    }

    std::vector<ExecutionState*> allStates;

    for (auto state : states) {
        S2EExecutionState *s2estate = static_cast<S2EExecutionState*>(state);
        if (!s2estate->isZombie()) {
            allStates.push_back(s2estate);
        }
    }

    if (allStates.size() < 2) {
        return;
    }

    g_s2e->getDebugStream() << "LoadBalancing: starting\n";

    m_inLoadBalancing = true;

    vm_stop(RUN_STATE_SAVE_VM);

    unsigned parentId = m_s2e->getCurrentProcessIndex();
    m_s2e->getCorePlugin()->onProcessFork.emit(true, false, -1);
    int child = m_s2e->fork();
    if (child < 0) {
        //Fork did not succeed
        m_s2e->getCorePlugin()->onProcessFork.emit(false, false, -1);
        m_inLoadBalancing = false;
        vm_start();
        return;
    }

    unsigned size = allStates.size();
    unsigned n = size / 2;
    unsigned lower = child ? 0 : n;
    unsigned upper = child ? n : size;

    m_s2e->getCorePlugin()->onProcessFork.emit(false, child, parentId);

    g_s2e->getDebugStream() << "LoadBalancing: terminating states\n";

    for (unsigned i=lower; i<upper; ++i) {
        S2EExecutionState *s2estate = static_cast<S2EExecutionState*>(allStates[i]);
        terminateStateAtFork(*s2estate);
    }

    m_s2e->getCorePlugin()->onProcessForkComplete.emit(child);

    m_inLoadBalancing = false;
    vm_start();
}

void S2EExecutor::stateSwitchTimerCallback(void *opaque)
{
    S2EExecutor *c = (S2EExecutor*)opaque;

    if (g_s2e_state) {
        c->doLoadBalancing();
        S2EExecutionState *nextState = c->selectNextState(g_s2e_state);
        if (nextState) {
            g_s2e_state = nextState;
        } else {
            //Do not reschedule the timer anymore
            return;
        }
    }

    timer_mod(c->m_stateSwitchTimer, qemu_clock_get_ms(QEMU_CLOCK_HOST) + 100);
}

void S2EExecutor::initializeStateSwitchTimer()
{
    m_stateSwitchTimer = timer_new_ms(QEMU_CLOCK_HOST, &stateSwitchTimerCallback, this);
    timer_mod(m_stateSwitchTimer, qemu_clock_get_ms(QEMU_CLOCK_HOST) + 100);
}

void S2EExecutor::doStateSwitch(S2EExecutionState* oldState,
                                S2EExecutionState* newState)
{
	llvm::errs() << __FILE__ << ":" << __LINE__ << ": TODO: S2EExecutor::doStateSwitch is partially stubbed" << '\n';
    assert(oldState || newState);
    assert(!oldState || oldState->m_active);
    assert(!newState || !newState->m_active);
    assert(!newState || !newState->m_runningConcrete);
//
//    //Some state save/restore logic in QEMU flushes the cache.
//    //This can have bad effects in case of saving/restoring states
//    //that were in the middle of a memory operation. Therefore,
//    //we disable it here and re-enable after the new state has been activated.
//    g_s2e_disable_tlb_flush = 1;
//
//    //Clear the asynchronous request queue, which is not saved as part of
//    //the snapshots by QEMU. This is the same mechanism as used by
//    //load/save_vmstate, so it should work reliably
//    assert(false && "stubbed");
////    qemu_aio_flush();
//    bdrv_flush_all();
//    cpu_disable_ticks();
//
//    m_s2e->getMessagesStream(oldState)
//            << "Switching from state " << (oldState ? oldState->getID() : -1)
//            << " to state " << (newState ? newState->getID() : -1) << '\n';
//
    const MemoryObject* cpuSysMo = oldState ? oldState->m_cpuSystemState :
                                            newState->m_cpuSystemState;
    const MemoryObject* cpuMo = oldState ? oldState->m_cpuState :
                                                newState->m_cpuState;

    if(oldState) {
        if(oldState->m_runningConcrete)
            switchToSymbolic(oldState);

//        /*
//        if(use_icount) {
//            assert(env->s2e_icount == (uint64_t) (qemu_icount -
//                            (env->icount_decr.u16.low + env->icount_extra)));
//        }
//        */
//
        //copyInConcretes(*oldState);
//        oldState->getDeviceState()->saveDeviceState();
        //oldState->m_qemuIcount = qemu_icount;
//
//        assert(false && "stubbed");
////        *oldState->m_timersState = timers_state;
//
//        uint8_t *oldStore = oldState->m_cpuSystemObject->getConcreteStore();
//        memcpy(oldStore, (uint8_t*) cpuMo->address, cpuMo->size);
//
        oldState->m_active = false;
    }

    if(newState) {
//        assert(false && "stubbed");
////        timers_state = *newState->m_timersState;
//        //qemu_icount = newState->m_qemuIcount;
//
//        jmp_buf jmp_env;
//        memcpy(&jmp_env, &env->jmp_env, sizeof(jmp_buf));

        const uint8_t *newStore = newState->m_cpuSystemObject->getConcreteStore();
        memcpy((uint8_t*) cpuSysMo->address, newStore, cpuSysMo->size);

        newStore = newState->m_cpuObject->getConcreteStore();
        memcpy((uint8_t*) cpuMo->address, newStore, cpuMo->size);

//        memcpy(&env->jmp_env, &jmp_env, sizeof(jmp_buf));

        newState->m_active = true;
//
//        //Devices may need to write to memory, which can be done
//        //after the state is activated
//        //XXX: assigning g_s2e_state here is ugly but is required for restoreDeviceState...
//        g_s2e_state = newState;
//        newState->getDeviceState()->restoreDeviceState();
    }

    uint64_t totalCopied = 0;
    uint64_t objectsCopied = 0;
    for(MemoryObject* mo : m_saveOnContextSwitch) {
        if(mo == cpuMo)
            continue;

        if(oldState) {
            const ObjectState *oldOS = oldState->addressSpace.findObject(mo);
            ObjectState *oldWOS = oldState->addressSpace.getWriteable(mo, oldOS);
            uint8_t *oldStore = oldWOS->getConcreteStore();
            assert(oldStore);
            memcpy(oldStore, (uint8_t*) mo->address, mo->size);
        }

        if(newState) {
            const ObjectState *newOS = newState->addressSpace.findObject(mo);
            const uint8_t *newStore = newOS->getConcreteStore();
            assert(newStore);
            memcpy((uint8_t*) mo->address, newStore, mo->size);
        }

        totalCopied += mo->size;
        objectsCopied++;
    }

    cpu_enable_ticks();

    if (VerboseStateSwitching) {
        s2e_debug_print("Copied %d (count=%d)\n", totalCopied, objectsCopied);
    }

//    if(FlushTBsOnStateSwitch) {
//        assert(false && "stubbed");
////        tb_flush(env);
//    }
//
//    g_s2e_disable_tlb_flush = 0;
//
    m_s2e->getCorePlugin()->onStateSwitch.emit(oldState, newState);
}

ExecutionState* S2EExecutor::selectNonSpeculativeState(S2EExecutionState *state)
{
    ExecutionState *newState;
    std::set<ExecutionState*> empty;

    do {
        if (searcher->empty()) {
            newState = NULL;
            break;
        }

        newState = &searcher->selectState();

        if (newState->isSpeculative()) {
            //The searcher wants us to execute a speculative state.
            //The engine must make sure that such a state
            //satisfies all the path constraints.
            if (!resolveSpeculativeState(*newState)) {
                terminateState(*newState);
                updateStates(state);
                continue;
            }

            //Give a chance to the searcher to update
            //the status of the state.
            searcher->update(newState, empty, empty);
        }
    } while(newState->isSpeculative());

    if (!newState) {
        m_s2e->getWarningsStream() << "All states were terminated" << '\n';
        for (S2EExecutionState* s : m_deletedStates) {
            //Leave the current state in a zombie form to let QEMU exit gracefully.
            if (s != g_s2e_state) {
                unrefS2ETb(s->m_lastS2ETb);
                s->m_lastS2ETb = NULL;
                delete s;
            }
        }
        m_deletedStates.clear();
        qemu_system_shutdown_request();
    }

    return newState;
}

void S2EExecutor::restoreYieldedState(void) {
    if (yieldedState) {
        S2EExecutionState* s2eYieldedState =
            static_cast<S2EExecutionState*>(yieldedState);
        s2eYieldedState->yield(false);
        resumeState(yieldedState);
        yieldedState = NULL;
        // Yielded state is now available for scheduling
    }
}

S2EExecutionState* S2EExecutor::selectNextState(S2EExecutionState *state)
{
    assert(state->m_active);

    //If no more states are there, updateStates will fail
//    if (!searcher->empty() || !addedStates.empty()) {
//    	updateStates(state);
//    }
    assert(addedStates.empty() && removedStates.empty() && "Simple check that call to updateStates is not needed");

    ExecutionState *nstate = selectNonSpeculativeState(state);
    if (nstate == NULL) {
        return NULL;
    }

    ExecutionState& newKleeState = *nstate;

    S2EExecutionState* newState = cast<S2EExecutionState>(&newKleeState);
    assert(states.find(newState) != states.end());

    if(!state->m_active) {
        /* Current state might be switched off by merge method */
        state = NULL;
    }

    // Now that we've rescheduled, put the yielded state back
    // so that we can schedule it again.
    restoreYieldedState();

    if(newState != state) {
        g_s2e->getCorePlugin()->onStateSwitch.emit(state, newState);
        vm_stop(RUN_STATE_SAVE_VM);
        doStateSwitch(state, newState);
        vm_start();
    }

    //We can't free the state immediately if it is the current state.
    //Do it now.
    for (S2EExecutionState* s : m_deletedStates) {
        assert(s != newState);
        unrefS2ETb(s->m_lastS2ETb);
        s->m_lastS2ETb = NULL;
        llvm::errs() << "S2EExecutor::selectNextState: DEBUG - deleting state " << s << '\n';
        delete s;
    }
    m_deletedStates.clear();

    return newState;
}

/** Simulate start of function execution, creating KLEE structs of required */
void S2EExecutor::prepareFunctionExecution(S2EExecutionState *state,
                            llvm::Function *function,
                            const std::vector<klee::ref<klee::Expr> > &args)
{
    KFunction *kf;
    auto func_itr = kmodule->functionMap.find(function);
    if(func_itr != kmodule->functionMap.end()) {
        kf = func_itr->second;
    } else {

        unsigned cIndex = kmodule->constants.size();
        kf = kmodule->updateModuleWithFunction(function);

        for(unsigned i = 0; i < kf->numInstructions; ++i)
            bindInstructionConstants(kf->instructions[i]);

        /* Update global functions (new functions can be added
           while creating added function) */
        for (Module::iterator i = kmodule->module->begin(),
                              ie = kmodule->module->end(); i != ie; ++i) {
            llvm::Function *f = i;
            klee::ref<klee::ConstantExpr> addr(0);

            // If the symbol has external weak linkage then it is implicitly
            // not defined in this module; if it isn't resolvable then it
            // should be null.
            if (f->hasExternalWeakLinkage() &&
                    !externalDispatcher->resolveSymbol(f->getName())) {
                addr = Expr::createPointer(0);
            } else {
                addr = Expr::createPointer((uintptr_t) (void*) f);
                legalFunctions.insert((uint64_t) (uintptr_t) (void*) f);
            }

            globalAddresses.insert(std::make_pair(f, addr));
        }

        kmodule->constantTable.resize(kmodule->constants.size());

        for(unsigned i = cIndex; i < kmodule->constants.size(); ++i) {
            Cell &c = kmodule->constantTable[i];
            c.value = evalConstant(kmodule->constants[i]);
        }
    }

    /* Emulate call to a TB function */
    state->prevPC = state->pc;

    state->pushFrame(state->pc, kf);
    state->pc = kf->instructions;

    if(statsTracker)
        statsTracker->framePushed(*state,
            &state->stack[state->stack.size()-2]);

    /* Pass argument */
    for(unsigned i = 0; i < args.size(); ++i)
        bindArgument(kf, i, *state, args[i]);
}

inline bool S2EExecutor::executeInstructions(S2EExecutionState *state, unsigned callerStackSize)
{
	try {
    	//TODO: Why don't we check !states.empty() && !haltExecution like in KLEE here?
        while(state->stack.size() != callerStackSize) {
            ++state->m_stats.m_statInstructionCountSymbolic;

            KInstruction *ki = state->pc;

            if ( S2EDebugInstructions ) {
                m_s2e->getDebugStream(state) << "executing "
                      << ki->inst->getParent()->getParent()->getName().str()
                      << ": " << *ki->inst << '\n';
            }

            stepInstruction(*state);
            executeInstruction(*state, ki);

            updateStates(state);

            //S2E doesn't know if the current state can be run
            //if concolic fork marks it as speculative.
            //XXX: Current state should *never* be speculative.
            assert(!state->isSpeculative());
            /*if (state->isSpeculative()) {
                return true;
            }*/

            //Handle the case where we killed the current state inside processFork
            if (m_forkProcTerminateCurrentState) {
                assert(false && "stubbed");
//                state->writeCpuState(CPU_OFFSET(exception_index), EXCP_S2E, 8*sizeof(int));
                state->zombify();
                m_forkProcTerminateCurrentState = false;
                return true;
            }
        }
    } catch (CpuExitException &) {
        assert(addedStates.empty());
        return true;
    }

    //The TB finished executing normally
    if (callerStackSize == 1) {
        state->prevPC = 0;
        state->pc = m_dummyMain->instructions;
    }

    return false;
}

bool S2EExecutor::finalizeTranslationBlockExec(S2EExecutionState *state)
{
    if(!state->m_needFinalizeTBExec)
        return false;

    state->m_needFinalizeTBExec = false;
    assert(state->stack.size() != 1);

    assert(!state->m_runningConcrete);

    if (VerboseTbFinalize) {
        m_s2e->getDebugStream() << "Finalizing TB execution " << state->getID() << '\n';
        foreach(const StackFrame& fr, state->stack) {
            m_s2e->getDebugStream() << fr.kf->function->getName().str() << '\n';
        }
    }

    /* Information for GETPC() macro */
    /* XXX: tc_ptr could be already freed at this moment */
    /*      however, GETPC is not used in S2E anyway */
    //g_s2e_exec_ret_addr = 0; //state->getTb()->tc_ptr;


    /**
     * TBs can fork anywhere and the remainder can also throw exceptions.
     * Should exit the CPU loop in this case.
     */
    bool ret = executeInstructions(state);

    if (VerboseTbFinalize) {
        m_s2e->getDebugStream(state) << "Done finalizing TB execution\n";
    }

    return ret;
}

void S2EExecutor::updateStates(klee::ExecutionState *current)
{
	assert((!states.empty() || !addedStates.empty()) && "No more states to schedule");
	klee::Executor::updateStates(current);
}

#ifdef _WIN32

extern "C" volatile LONG g_signals_enabled;

typedef int sigset_t;

static void s2e_disable_signals(sigset_t *oldset)
{
    while(InterlockedCompareExchange(&g_signals_enabled, 0, 1) == 0)
       ;
}

static void s2e_enable_signals(sigset_t *oldset)
{
    g_signals_enabled = 1;
}

#else

static void s2e_disable_signals(sigset_t *oldset)
{
    sigset_t set;
    sigfillset(&set);
    sigprocmask(SIG_BLOCK, &set, oldset);
}

static void s2e_enable_signals(sigset_t *oldset)
{
    sigprocmask(SIG_SETMASK, oldset, NULL);
}

#endif

uintptr_t S2EExecutor::executeTranslationBlockKlee(
        S2EExecutionState* state,
        TranslationBlock* tb)
{
    assert(state->m_active && !state->m_runningConcrete);
    assert(state->stack.size() == 1);
    assert(state->pc == m_dummyMain->instructions);


    tb_function_args[0] = state->getConcreteCpuState();
    tb_function_args[1] = 0;
    tb_function_args[2] = 0;

    ++state->m_stats.m_statTranslationBlockSymbolic;

    /* Generate LLVM code if necessary */
    if(!tb->llvm_function) {
        cpu_gen_llvm(state->getConcreteCpuState(), tb);
        assert(tb->llvm_function);
    }

    if(tb->s2e_tb != state->m_lastS2ETb) {
        unrefS2ETb(state->m_lastS2ETb);
        state->m_lastS2ETb = tb->s2e_tb;
        state->m_lastS2ETb->refCount += 1;
    }

    /* Prepare function execution */
    prepareFunctionExecution(state,
            tb->llvm_function, std::vector<klee::ref<Expr> >(1,
                Expr::createPointer((uint64_t) tb_function_args)));

    if (executeInstructions(state)) {
        throw CpuExitException();
    }

    /* Get return value */
    klee::ref<Expr> resExpr =
            getDestCell(*state, state->pc).value;
    assert(isa<klee::ConstantExpr>(resExpr));

    return cast<klee::ConstantExpr>(resExpr)->getZExtValue();
}

tcg_target_ulong S2EExecutor::executeTranslationBlockConcrete(S2EExecutionState *state,
                                                       TranslationBlock *tb,
                                                       CPUState* cpu)
{
	sigjmp_buf jmp_env;
    assert(state->m_active && state->m_runningConcrete);
    assert(sizeof(jmp_env) == sizeof(cpu->jmp_env));
    state->m_stats.m_statTranslationBlockConcrete += 1;

    tcg_target_ulong ret = 0;

    memcpy(jmp_env, cpu->jmp_env, sizeof(jmp_env));
    if(sigsetjmp(cpu->jmp_env, 0) == 0) {
    	ret = tcg_qemu_tb_exec(cpu->env_ptr, tb->tc_ptr);
    	memcpy(cpu->jmp_env, jmp_env, sizeof(jmp_env));
    	return ret;
    } else {
    	memcpy(cpu->jmp_env, jmp_env, sizeof(jmp_env));
        throw CpuExitException();
    }
}

static inline void s2e_tb_reset_jump(TranslationBlock *tb, unsigned int n)
{
    TranslationBlock *tb1, *tb_next, **ptb;
    unsigned int n1;

    tb1 = tb->jmp_next[n];
    if (tb1 != NULL) {
        /* find head of list */
        for(;;) {
            n1 = (intptr_t)tb1 & 3;
            tb1 = (TranslationBlock *)((intptr_t)tb1 & ~3);
            if (n1 == 2)
                break;
            tb1 = tb1->jmp_next[n1];
        }
        /* we are now sure now that tb jumps to tb1 */
        tb_next = tb1;

        /* remove tb from the jmp_first list */
        ptb = &tb_next->jmp_first;
        for(;;) {
            tb1 = *ptb;
            n1 = (intptr_t)tb1 & 3;
            tb1 = (TranslationBlock *)((intptr_t)tb1 & ~3);
            if (n1 == n && tb1 == tb)
                break;
            ptb = &tb1->jmp_next[n1];
        }
        *ptb = tb->jmp_next[n];
        tb->jmp_next[n] = NULL;

        /* suppress the jump to next tb in generated code */
        tb_set_jmp_target(tb, n, (uintptr_t)(static_cast<uint8_t *>(tb->tc_ptr) + tb->tb_next_offset[n]));
        tb->s2e_tb_next[n] = NULL;
    }
}


//XXX: inline causes compiler internal errors
static void s2e_tb_reset_jump_smask(TranslationBlock* tb, unsigned int n,
                                           uint64_t smask, int depth = 0)
{
    TranslationBlock *tb1 = tb->s2e_tb_next[n];
    sigset_t oldset;
    if (depth == 0) {
        s2e_disable_signals(&oldset);
    }

    if(tb1) {
        if(depth > 2 || (smask & tb1->accesses.reg_rmask) || (smask & tb1->accesses.reg_wmask)
                             || (tb1->accesses.accesses_memory & 4)) {
            s2e_tb_reset_jump(tb, n);
        } else if(tb1 != tb) {
            s2e_tb_reset_jump_smask(tb1, 0, smask, depth + 1);
            s2e_tb_reset_jump_smask(tb1, 1, smask, depth + 1);
        }
    }

    if (depth == 0) {
        s2e_enable_signals(&oldset);
    }
}

uintptr_t S2EExecutor::executeTranslationBlock(
        S2EExecutionState* state,
        TranslationBlock* tb,
        CPUState* cpu)
{
    //Avoid incrementing stats every time, very expensive.
    static unsigned doStatsIncrementCount= 0;
    assert(state->isActive());

    bool executeKlee = m_executeAlwaysKlee;

    //m_startSymbexAtPC is set when code jumps to symbolic execution
    if(state->m_startSymbexAtPC != (uint64_t) -1) {
        executeKlee |= (state->getPc() == state->m_startSymbexAtPC);
        state->m_startSymbexAtPC = (uint64_t) -1;
    }

    //XXX: hack to run code symbolically that may be delayed because of interrupts.
    //Size check is important to avoid expensive calls to getPc/getPid in the common case
    if (state->m_toRunSymbolically.size() > 0 &&  state->m_toRunSymbolically.find(std::make_pair(state->getPc(), state->getPid()))
        != state->m_toRunSymbolically.end()) {
        executeKlee = true;
        state->m_toRunSymbolically.erase(std::make_pair(state->getPc(), state->getPid()));
    }

    if(!executeKlee) {
        //XXX: This should be fixed to make sure that helpers do not read/write corrupted data
        //because they think that execution is concrete while it should be symbolic (see issue #30).

        /* We can not execute TB natively if it reads any symbolic regs */
        uint64_t smask = state->getSymbolicRegistersMask();
        if(smask || tb->accesses.accesses_memory) {
            if((smask & tb->accesses.reg_rmask) || (smask & tb->accesses.reg_wmask)
                     || tb->accesses.accesses_memory) {
                /* TB reads symbolic variables */
                executeKlee = true;

            } else {
                s2e_tb_reset_jump_smask(tb, 0, smask);
                s2e_tb_reset_jump_smask(tb, 1, smask);
            }
        }
    }

    //The following code is taken from cpu_tb_exit in cpu-exec.c
    /* Execute a TB, and fix up the CPU state afterwards if necessary */
    uintptr_t next_tb;

#if defined(DEBUG_DISAS)
    if (qemu_loglevel_mask(CPU_LOG_TB_CPU)) {
#if defined(TARGET_I386)
        log_cpu_state(cpu, CPU_DUMP_CCOP);
#elif defined(TARGET_M68K)
        /* ??? Should not modify env state for dumping.  */
        cpu_m68k_flush_flags(env, env->cc_op);
        env->cc_op = CC_OP_FLAGS;
        env->sr = (env->sr & 0xffe0) | env->cc_dest | (env->cc_x << 4);
        log_cpu_state(cpu, 0);
#else
        log_cpu_state(cpu, 0);
#endif
    }
#endif /* DEBUG_DISAS */

    cpu->can_do_io = !use_icount;

    if(executeKlee) {
        if(state->m_runningConcrete) {
            TimerStatIncrementer t(stats::concreteModeTime);
            switchToSymbolic(state);
        }

        TimerStatIncrementer t(stats::symbolicModeTime);

        //XXX: adapt scaling dynamically.
        int slowdown = UseFastHelpers ? ClockSlowDownFastHelpers : ClockSlowDown;
        cpu_enable_scaling(slowdown);

        next_tb = executeTranslationBlockKlee(state, tb);

        if ((next_tb & TB_EXIT_MASK) == TB_EXIT_REQUESTED) {
            state->m_startSymbexAtPC = state->getPc();
        }

    } else {
        //g_s2e_exec_ret_addr = 0;
        if(!state->m_runningConcrete)
            switchToConcrete(state);

        if (!((++doStatsIncrementCount) & 0xFFF)) {
            TimerStatIncrementer t(stats::concreteModeTime);
        }

        llvm::errs() << "WARN - " << __FILE__ << ":" << __LINE__ << ": CPU timer scaling stubbed" << '\n';
//        int new_scaling = timers_state.clock_scale / 2;
//        if (new_scaling == 0) {
//            new_scaling = 1;
//        }
//        cpu_enable_scaling(new_scaling);

        next_tb = executeTranslationBlockConcrete(state, tb, cpu);
    }

    cpu->can_do_io = 1;
    trace_exec_tb_exit((void *) (next_tb & ~TB_EXIT_MASK),
                       next_tb & TB_EXIT_MASK);

    if ((next_tb & TB_EXIT_MASK) > TB_EXIT_IDX1) {
        /* We didn't start executing this TB (eg because the instruction
         * counter hit zero); we must restore the guest PC to the address
         * of the start of the TB.
         */
        CPUClass *cc = CPU_GET_CLASS(cpu);
        TranslationBlock *tb = (TranslationBlock *)(next_tb & ~TB_EXIT_MASK);
        if (cc->synchronize_from_tb) {
            cc->synchronize_from_tb(cpu, tb);
        } else {
            assert(cc->set_pc);
            cc->set_pc(cpu, tb->pc);
        }
    }
    if ((next_tb & TB_EXIT_MASK) == TB_EXIT_REQUESTED) {
        /* We were asked to stop executing TBs (probably a pending
         * interrupt. We've now stopped, so clear the flag.
         */
        cpu->tcg_exit_req = 0;
    }
        

    return next_tb;
}

void S2EExecutor::cleanupTranslationBlock(S2EExecutionState* state)
{
    assert(state->m_active);

    //g_s2e_exec_ret_addr = 0;

    while(state->stack.size() != 1)
        state->popFrame();

    state->prevPC = 0;
    state->pc = m_dummyMain->instructions;

#if 0
    if(!state->m_runningConcrete) {
        /* If we was interupted while symbexing, we can be resumed
           for concrete execution */
        copyOutConcretes(*state);
    }
#endif
}

klee::ref<klee::Expr> S2EExecutor::executeFunction(S2EExecutionState *state,
                            llvm::Function *function,
                            const std::vector<klee::ref<klee::Expr> >& args)
{
    assert(!state->m_runningConcrete);
    assert(!state->prevPC);
    assert(state->stack.size() == 1);

    /* Update state */
    //if (!copyInConcretes(*state)) {
    //    std::cerr << "external modified read-only object" << '\n';
    //    exit(1);
    //}

    KInstIterator callerPC = state->pc;
    uint32_t callerStackSize = state->stack.size();

    /* Prepare function execution */
    prepareFunctionExecution(state, function, args);

    /* Execute */
    if (executeInstructions(state, callerStackSize)) {
        throw CpuExitException();
    }

    if(callerPC == m_dummyMain->instructions) {
        assert(state->stack.size() == 1);
        state->prevPC = 0;
        state->pc = callerPC;
    }

    klee::ref<Expr> resExpr(0);
    if(function->getReturnType()->getTypeID() != llvm::Type::VoidTyID)
        resExpr = getDestCell(*state, state->pc).value;

    //copyOutConcretes(*state);

    return resExpr;
}

klee::ref<klee::Expr> S2EExecutor::executeFunction(S2EExecutionState *state,
                            const std::string& functionName,
                            const std::vector<klee::ref<klee::Expr> >& args)
{
    llvm::Function *function = kmodule->module->getFunction(functionName);
    assert(function && "function with given name do not exists in LLVM module");
    return executeFunction(state, function, args);
}

void S2EExecutor::deleteState(klee::ExecutionState *state)
{
    processTree->remove(state->ptreeNode);
    m_deletedStates.push_back(cast<S2EExecutionState>(state));
}

void S2EExecutor::doStateFork(S2EExecutionState *originalState,
                 const vector<S2EExecutionState*>& newStates,
                 const vector<klee::ref<Expr> >& newConditions)
{
    assert(originalState->m_active && !originalState->m_runningConcrete);

    llvm::raw_ostream& out = m_s2e->getMessagesStream(originalState);
    out << "Forking state " << originalState->getID()
            << " at pc = " << hexval(originalState->getPc()) << '\n';

    for(unsigned i = 0; i < newStates.size(); ++i) {
        S2EExecutionState* newState = newStates[i];

        if (VerboseFork) {
            out << "    state " << newState->getID() << " with condition "
            << newConditions[i] << '\n';
        }

        if(newState != originalState) {
            newState->m_needFinalizeTBExec = true;
            newState->m_active = false;
        }
    }

    if (VerboseFork) {
        m_s2e->getDebugStream() << "Stack frame at fork:" << '\n';
        for (const StackFrame& fr : originalState->stack) {
            m_s2e->getDebugStream() << fr.kf->function->getName().str() << '\n';
        }
    }

    m_s2e->getCorePlugin()->onStateFork.emit(originalState,
                                             newStates, newConditions);
}

S2EExecutor::StatePair S2EExecutor::fork(ExecutionState &current,
                            klee::ref<Expr> condition, bool isInternal)
{
	assert(!cast<S2EExecutionState>(&current)->m_runningConcrete &&
			"Cannot fork while executing in concrete mode");

    StatePair res;

    if (ConcolicMode) {
        res = Executor::concolicFork(current, condition, isInternal);
    } else {
        res = Executor::fork(current, condition, isInternal);
    }

    if(res.first && res.second) {
        std::vector<S2EExecutionState*> newStates(2);
        std::vector<klee::ref<Expr> > newConditions(2);

        newStates[0] = cast<S2EExecutionState>(res.first);
        newStates[1] = cast<S2EExecutionState>(res.second);

        newConditions[0] = condition;
        newConditions[1] = klee::NotExpr::create(condition);

        doStateFork(cast<S2EExecutionState>(&current),
                       newStates, newConditions);
    }
    return res;
}

/**
 * Called from klee::Executor when the engine is about to fork
 * the current state.
 */
void S2EExecutor::notifyBranch(ExecutionState &state)
{
    S2EExecutionState *s2eState = cast<S2EExecutionState>(&state);

    /* Checkpoint the device state before branching */
    llvm::errs() << __FILE__ << ":" << __LINE__ << ": TODO: Checkpoint device state before branching" << '\n';
//    assert(false && "stubbed");
//    qemu_aio_flush();
//    bdrv_flush_all();
    //s2eState->m_tlb.clearTlbOwnership();

    /* Save CPU state */
    const MemoryObject* cpuSysMo = s2eState->m_cpuSystemState;
    uint8_t *cpuSysStore = s2eState->m_cpuSystemObject->getConcreteStore();
    memcpy(cpuSysStore, (uint8_t*) cpuSysMo->address, cpuSysMo->size);

    const MemoryObject* cpuMo = s2eState->m_cpuState;
        uint8_t *cpuStore = s2eState->m_cpuObject->getConcreteStore();
        memcpy(cpuStore, (uint8_t*) cpuMo->address, cpuMo->size);

    cpu_disable_ticks();
    s2eState->getDeviceState()->saveDeviceState();
    llvm::errs() << __FILE__ << ":" << __LINE__ << ": TODO: Save timers state in branching" << '\n';
//    *s2eState->m_timersState = timers_state;
    cpu_enable_ticks();

    for(MemoryObject* mo : m_saveOnContextSwitch) {
        const ObjectState *os = s2eState->addressSpace.findObject(mo);
        ObjectState *wos = s2eState->addressSpace.getWriteable(mo, os);
        uint8_t *store = wos->getConcreteStore();
        assert(store);
        memcpy(store, (uint8_t*) mo->address, mo->size);
    }
}

void S2EExecutor::branch(klee::ExecutionState &state,
          const vector<klee::ref<Expr> > &conditions,
          vector<ExecutionState*> &result)
{
    S2EExecutionState *s2eState = cast<S2EExecutionState>(&state);
    assert(!s2eState->m_runningConcrete);

    Executor::branch(state, conditions, result);

    unsigned n = conditions.size();

    vector<S2EExecutionState*> newStates;
    vector<klee::ref<Expr> > newConditions;

    newStates.reserve(n);
    newConditions.reserve(n);

    for(unsigned i = 0; i < n; ++i) {
        if(result[i]) {
            assert(cast<S2EExecutionState>(result[i]));
            newStates.push_back(cast<S2EExecutionState>(result[i]));
            newConditions.push_back(conditions[i]);
        }
    }

    if(newStates.size() > 1) {
        doStateFork(cast<S2EExecutionState>(&state),
                       newStates, newConditions);
    }
}

bool S2EExecutor::merge(klee::ExecutionState &_base, klee::ExecutionState &_other)
{
    S2EExecutionState* base = cast<S2EExecutionState>(&_base);
    S2EExecutionState* other = cast<S2EExecutionState>(&_other);

    /* Ensure that both states are inactive, otherwise merging will not work */
    if(base->m_active)
        doStateSwitch(base, NULL);
    else if(other->m_active)
        doStateSwitch(other, NULL);

    if(base->merge(*other)) {
        m_s2e->getMessagesStream(base)
                << "Merged with state " << other->getID() << '\n';
        return true;
    } else {
        m_s2e->getDebugStream(base)
                << "Merge with state " << other->getID() << " failed" << '\n';
        return false;
    }
}

void S2EExecutor::terminateStateEarly(klee::ExecutionState &state, const llvm::Twine &message)
{
    S2EExecutionState  *s2estate = static_cast<S2EExecutionState*>(&state);
    m_s2e->getMessagesStream(s2estate) << message << '\n';
    std::vector<std::string> cfg = m_s2e->getConfig()->getListKeys("s2e");
    bool gen_testcase = true;

    // Whether to generate testcase
    if (std::find(cfg.begin(), cfg.end(), "generate_testcase_on_kill") != cfg.end())	{
        gen_testcase = m_s2e->getConfig()->getBool("s2e.generate_testcase_on_kill", gen_testcase);
    }
    if (gen_testcase)
        m_s2e->getCorePlugin()->onTestCaseGeneration.emit(s2estate, message.str());
    terminateState(state);
}

void S2EExecutor::terminateState(ExecutionState &s)
{
    S2EExecutionState& state = static_cast<S2EExecutionState&>(s);
    m_s2e->getCorePlugin()->onStateKill.emit(&state);

    terminateStateAtFork(state);
    state.zombify();

    g_s2e->getWarningsStream().flush();
    g_s2e->getDebugStream().flush();

    //No need for exiting the loop if we kill another state.
    if (!m_inLoadBalancing && (&state == g_s2e_state)) {
        state.getCPUState()->exception_index = EXCP_S2E;
        timer_mod(m_stateSwitchTimer, qemu_clock_get_ms(QEMU_CLOCK_HOST));
        throw CpuExitException();
    }
}

/* Yield the current state.  Once a new state is scheduled,
   this yielded state will again be schedulable.  Only one
   state can yield at a time.  If after a state X yields,
   another state Y yields, state X will again be schedulable.
   Yielding requires other states to be available:  if there
   is only one state, yield is a no-op.
*/
void S2EExecutor::yieldState(ExecutionState &s)
{
    S2EExecutionState& state = static_cast<S2EExecutionState&>(s);
    if (states.size() <= 1) {
        m_s2e->getMessagesStream(&state)
            << "Not yielding because there's only one state.\n";
        return;
    }

    m_s2e->getMessagesStream(&state)
        << "Yielding state " << state.getID() << "\n";

    // Check if the state we're yielding has reached the searcher
    std::set<klee::ExecutionState*>::iterator it = addedStates.find(&state);
    assert (yieldedState == NULL);
    yieldedState = &state;
    if (it != addedStates.end()) {
        // never reached searcher
        addedStates.erase(it);
    }

    // Suspend the state
    bool result = suspendState(yieldedState);
    assert (result && "Searcher required to use yield");
    state.yield(true);

    g_s2e->getWarningsStream().flush();
    g_s2e->getDebugStream().flush();

    // Skip the opcode
    state.writeCpuState(CPU_CONC_LIMIT, state.getPc() + 10, CPU_REG_SIZE << 3);

    // Stop current execution
    assert(false && "stubbed");
//    state.writeCpuState(CPU_OFFSET(exception_index), EXCP_S2E, 8*sizeof(int));
    throw CpuExitException();
}

void S2EExecutor::terminateStateAtFork(S2EExecutionState &state)
{
    Executor::terminateState(state);
}

inline void S2EExecutor::setCCOpEflags(S2EExecutionState *state)
{
#ifdef TARGET_I386
	uint32_t cc_op = 0;

    // Check wether any of cc_op, cc_src, cc_dst or cc_tmp are symbolic
    if((state->getSymbolicRegistersMask() & (0xf<<1)) || m_executeAlwaysKlee) {
        // call set_cc_op_eflags only if cc_op is symbolic or cc_op != CC_OP_EFLAGS
        bool ok = state->readCpuRegisterConcrete(CPU_OFFSET(cc_op),
                                                 &cc_op, sizeof(cc_op));
        if(!ok || cc_op != CC_OP_EFLAGS) {
            try {
                if(state->m_runningConcrete)
                    switchToSymbolic(state);
                TimerStatIncrementer t(stats::symbolicModeTime);
                executeFunction(state, "helper_set_cc_op_eflags");
            } catch(s2e::CpuExitException&) {
                updateStates(state);
                siglongjmp(state->getCPUState()->jmp_env, 1);
            }
        }
    } else {
        bool ok = state->readCpuRegisterConcrete(CPU_OFFSET(cc_op),
                                                 &cc_op, sizeof(cc_op));
        assert(ok);
        if(cc_op != CC_OP_EFLAGS) {
            if(!state->m_runningConcrete)
                switchToConcrete(state);
            //TimerStatIncrementer t(stats::concreteModeTime);
            helper_set_cc_op_eflags();
        }
    }
#elif defined(TARGET_ARM)
    //not needed for ARM
#endif
}

/* Note that each QEMU target have different handlers naming and signature:
 * on ARM - do_interrupt()/s2e_do_interrupt() - 0 args
 * on X86 - do_interrupt_all()/s2e_do_interrupt_all() - 5 args
 */

#ifdef TARGET_ARM
inline void S2EExecutor::doInterrupt(S2EExecutionState *state)
{
    assert(false && "stubbed");
//    if(state->m_cpuRegistersObject->isAllConcrete() && !m_executeAlwaysKlee) {
//        if(!state->m_runningConcrete)
//            switchToConcrete(state);
//        //TimerStatIncrementer t(stats::concreteModeTime);
//        s2e_do_interrupt();
//    } else {
//        if(state->m_runningConcrete)
//            switchToSymbolic(state);
//        std::vector<klee::ref<klee::Expr> > args(0);
//        try {
//            TimerStatIncrementer t(stats::symbolicModeTime);
//            executeFunction(state, "s2e_do_interrupt", args);
//        } catch(s2e::CpuExitException&) {
//            updateStates(state);
//            s2e_longjmp(env->jmp_env, 1);
//        }
//    }
//
}
#elif defined(TARGET_I386)
inline void S2EExecutor::doInterrupt(S2EExecutionState *state, int intno,
                                     int is_int, int error_code,
                                     uint64_t next_eip, int is_hw)
{
    assert(false && "stubbed");
//    if(state->m_cpuRegistersObject->isAllConcrete() && !m_executeAlwaysKlee) {
//        if(!state->m_runningConcrete)
//            switchToConcrete(state);
//        //TimerStatIncrementer t(stats::concreteModeTime);
//        s2e_do_interrupt_all(intno, is_int, error_code, next_eip, is_hw);
//    } else {
//        if(state->m_runningConcrete)
//            switchToSymbolic(state);
//        std::vector<klee::ref<klee::Expr> > args(5);
//        args[0] = klee::ConstantExpr::create(intno, sizeof(int)*8);
//        args[1] = klee::ConstantExpr::create(is_int, sizeof(int)*8);
//        args[2] = klee::ConstantExpr::create(error_code, sizeof(int)*8);
//        args[3] = klee::ConstantExpr::create(next_eip, sizeof(target_ulong)*8);
//        args[4] = klee::ConstantExpr::create(is_hw, sizeof(int)*8);
//        try {
//            TimerStatIncrementer t(stats::symbolicModeTime);
//            executeFunction(state, "s2e_do_interrupt_all", args);
//        } catch(s2e::CpuExitException&) {
//            updateStates(state);
//            s2e_longjmp(env->jmp_env, 1);
//        }
//    }
}
#endif

void S2EExecutor::setupTimersHandler()
{
	llvm::errs() << __FILE__ << ":" << __LINE__ << ": S2EExecutor::setupTimersHandler has been stubbed" << '\n';
//    m_s2e->getCorePlugin()->onTimer.connect(
//            sigc::bind(sigc::ptr_fun(&onAlarm), 0));
}

/** Suspend the given state (does not kill it) */
bool S2EExecutor::suspendState(S2EExecutionState *state, bool onlyRemoveFromPtree)
{
    if (onlyRemoveFromPtree) {
        processTree->deactivate(state->ptreeNode);
        return true;
    }

    if (searcher)  {
        searcher->removeState(state, NULL);
        size_t r = states.erase(state);
        assert(r == 1);
        processTree->deactivate(state->ptreeNode);
        return true;
    }
    return false;
}

bool S2EExecutor::resumeState(S2EExecutionState *state, bool onlyAddToPtree)
{
    if (onlyAddToPtree) {
        processTree->activate(state->ptreeNode);
        return true;
    }

    if (searcher)  {
        if (states.find(state) != states.end()) {
            return false;
        }
        processTree->activate(state->ptreeNode);
        states.insert(state);
        searcher->addState(state, NULL);
        return true;
    }
    return false;
}


void S2EExecutor::unrefS2ETb(S2ETranslationBlock* s2e_tb)
{
    if(s2e_tb && 0 == --s2e_tb->refCount) {
        if(s2e_tb->llvm_function && !KeepLLVMFunctions) {
            S2EExternalDispatcher *s2eDispatcher = static_cast<S2EExternalDispatcher*>(externalDispatcher);
            s2eDispatcher->removeFunction(s2e_tb->llvm_function);
            kmodule->removeFunction(s2e_tb->llvm_function);
        }
        for (void* s : s2e_tb->executionSignals) {
            delete static_cast<ExecutionSignal*>(s);
        }
    }
}

void S2EExecutor::queueStateForMerge(S2EExecutionState *state)
{
    if(!isa<MergingSearcher>(searcher)) {
        m_s2e->getWarningsStream(state)
                << "State merging request is ignored because"
                   " MergingSearcher is not activated\n";
        return;
    }
    assert(state->m_active && !state->m_runningConcrete && state->pc);

    /* Ignore attempt to merge states immediately after previous attempt */
    if(state->m_lastMergeICount == state->getTotalInstructionCount() - 1)
        return;

    state->m_lastMergeICount = state->getTotalInstructionCount();

    target_ulong mergePoint = 0;
#ifdef TARGET_ARM
    if(!state->readCpuRegisterConcrete(CPU_OFFSET(regs[13]), &mergePoint, CPU_REG_SIZE)) {
        m_s2e->getWarningsStream(state)
                << "Warning: merge request for a state with symbolic SP" << "\n";
    }
#elif defined(TARGET_I386)
    if(!state->readCpuRegisterConcrete(CPU_OFFSET(regs[R_ESP]), &mergePoint,
                                                               CPU_REG_SIZE)) {
        m_s2e->getWarningsStream(state)
                << "Warning: merge request for a state with symbolic ESP" << '\n';
    }
#endif
    mergePoint = hash64(mergePoint);
    mergePoint = hash64(state->getPc(), mergePoint);

    m_s2e->getMessagesStream(state) << "Queueing state for merging" << '\n';

    static_cast<S2EMergingSearcher*>(searcher)->queueStateForMerge(*state, mergePoint);
    throw CpuExitException();
}

void S2EExecutor::updateStats(S2EExecutionState *state)
{
    state->m_stats.updateStats(state);
    processTimers(state, 0);
}

Solver* S2EExecutor::getSolver() const {
	return solver->solver;
}

} // namespace s2e

/******************************/
/* Functions called from QEMU */

S2EExecutionState* s2e_create_initial_state(S2E *s2e)
{
    return s2e->getExecutor()->createInitialState();
}

void S2EExecutor_InitializeExecution(S2EExecutor* self, S2EExecutionState* initial_state, bool always_execute_klee)
{
    self->initializeExecution(initial_state, always_execute_klee);
}

void s2e_register_dirty_mask(S2E *s2e, S2EExecutionState *initial_state,
                            uint64_t host_address, uint64_t size)
{
    s2e->getExecutor()->registerDirtyMask(initial_state, host_address, size);
}


int s2e_qemu_finalize_tb_exec(S2E *s2e, S2EExecutionState* state)
{
    return s2e->getExecutor()->finalizeTranslationBlockExec(state);
}

void s2e_qemu_cleanup_tb_exec()
{
    return g_s2e->getExecutor()->cleanupTranslationBlock(g_s2e_state);
}

void s2e_set_cc_op_eflags(CPUArchState *env1)
{
    assert(false && "stubbed");
//    env = env1;
//    g_s2e->getExecutor()->setCCOpEflags(g_s2e_state);
}

/**
 *  We also need to track when execution enters/exits emulation code.
 *  Some plugins do not care about what memory accesses the emulation
 *  code performs internally, therefore, there must be a means for such
 *  plugins to enable/disable tracing upon exiting/entering
 *  the emulation code.
 *  The interrupt-handling methods here below are target-specific, singature
 *  must match the one in QEMU target.
 */

#ifdef TARGET_ARM
void do_interrupt(CPUArchState *env)
{
    g_s2e_state->setRunningExceptionEmulationCode(true);
    g_s2e->getExecutor()->doInterrupt(g_s2e_state);
    g_s2e_state->setRunningExceptionEmulationCode(false);
}
#elif defined(TARGET_I386)
void do_interrupt_all(int intno, int is_int, int error_code,
                      target_ulong next_eip, int is_hw)
{
    g_s2e_state->setRunningExceptionEmulationCode(true);
    s2e_on_exception(intno);
    g_s2e->getExecutor()->doInterrupt(g_s2e_state, intno, is_int, error_code,
                                    next_eip, is_hw);
    g_s2e_state->setRunningExceptionEmulationCode(false);
}
#endif

/**
 *  Checks whether we are trying to access an I/O port that returns a symbolic value.
 */
void s2e_switch_to_symbolic(S2E *s2e, S2EExecutionState *state)
{
    //XXX: For now, we assume that symbolic hardware, when triggered,
    //will want to start symbexec.
    state->enableSymbolicExecution();
    state->jumpToSymbolic();
}

void s2e_ensure_symbolic(S2E *s2e, S2EExecutionState *state)
{
    state->jumpToSymbolic();
}

/** Tlb cache helpers */

void s2e_dma_read(uint64_t hostAddress, uint8_t *buf, unsigned size)
{
    return g_s2e_state->dmaRead(hostAddress, buf, size);
}

void s2e_dma_write(uint64_t hostAddress, uint8_t *buf, unsigned size)
{
    return g_s2e_state->dmaWrite(hostAddress, buf, size);
}

void s2e_tb_alloc(S2E*, TranslationBlock *tb)
{
    tb->s2e_tb = new S2ETranslationBlock;
    tb->s2e_tb->llvm_function = NULL;
    tb->s2e_tb->refCount = 1;

    /* Push one copy of a signal to use it as a cache */
    tb->s2e_tb->executionSignals.push_back(new s2e::ExecutionSignal);

    tb->s2e_tb_next[0] = 0;
    tb->s2e_tb_next[1] = 0;
}

void s2e_set_tb_function(S2E*, TranslationBlock *tb)
{
    tb->s2e_tb->llvm_function = tb->llvm_function;
}

void s2e_tb_free(S2E* s2e, TranslationBlock *tb)
{
    s2e->getExecutor()->unrefS2ETb(tb->s2e_tb);
}

void s2e_flush_tlb_cache()
{
    g_s2e_state->flushTlbCache();
}

void s2e_flush_tb_cache()
{
    if (g_s2e && g_s2e->getExecutor()->getStatesCount() > 1) {
        if (!FlushTBsOnStateSwitch) {
            g_s2e->getWarningsStream() << "Flushing TB cache with more than 1 state. Dangerous. Expect crashes.\n";
        }
    }
}

void s2e_flush_tlb_cache_page(void *objectState, int mmu_idx, int index)
{
    g_s2e_state->flushTlbCachePage(static_cast<klee::ObjectState*>(objectState), mmu_idx, index);
}

int s2e_is_load_balancing()
{
    return g_s2e->getExecutor()->isLoadBalancing();
}

void helper_register_symbol(const char *name, void *address)
{
    llvm::sys::DynamicLibrary::AddSymbol(name, address);
}

#ifdef S2E_DEBUG_MEMORY
#ifdef __linux__

void *operator new(size_t s) throw(std::bad_alloc)
{
    void *ret = malloc(s);
    if (!ret) {
        throw std::bad_alloc();
    }

    memset(ret, 0xAA, s);
    return ret;
}

void* operator new[](size_t s) throw(std::bad_alloc)
{
    void *ret = malloc(s);
    if (!ret) {
        throw std::bad_alloc();
    }

    memset(ret, 0xAA, s);
    return ret;
}



void operator delete( void *pvMem ) throw()
{
   size_t s =  malloc_usable_size(pvMem);
   memset(pvMem, 0xBB, s);
   free(pvMem);
}

void operator delete[](void *pvMem) throw()
{
    size_t s =  malloc_usable_size(pvMem);
    memset(pvMem, 0xBB, s);
    free(pvMem);
}
#endif /* defined(__linux__) */

#endif /* defined(S2E_DEBUG_MEMORY) */

S2EExecutionState* S2EExecutor_CreateInitialState(S2EExecutor* self)
{
	if (g_s2e_state != NULL) {
        llvm::errs() << "ERROR - " << __FILE__ << ":" << __LINE__ << ": g_s2e_state is already initialized" << '\n';
        ::exit(1);
    }
    
    S2EExecutionState *state = self->createInitialState();
    g_s2e_state = state;
    return state;
}

S2EExecutionState* S2EExecutor_GetCurrentState(S2EExecutor* self)
{
    assert(g_s2e_state && "Initial state should have been initialized by now");
    return g_s2e_state;
}

void S2EExecutor_RegisterCpu(S2EExecutor* self, S2EExecutionState* initial_state, struct CPUState* cpu)
{
    self->registerCpu(initial_state, cpu);
}


uintptr_t S2EExecutor_ExecuteTranslationBlock(S2EExecutor* self, struct CPUState* cpu, struct TranslationBlock* tb)
{
    S2EExecutionState* cur_state = S2EExecutor_GetCurrentState(self);
    cur_state->setRunningExceptionEmulationCode(false);

    try {
        return self->executeTranslationBlock(cur_state, tb, cpu);
    } 
    catch(s2e::CpuExitException&) {
        self->updateStates(cur_state);
        siglongjmp(cpu->jmp_env, 1);
    }
}

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
    self->registerRam(initial_state,
        address, size, hostAddress, isSharedConcrete,
        saveOnContextSwitch, name);
}
