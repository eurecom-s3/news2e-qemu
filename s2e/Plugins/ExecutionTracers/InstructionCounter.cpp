///XXX: Do not use, deprecated

extern "C" {
#include "config.h"
#include "qemu-common.h"
}

#include "InstructionCounter.h"
#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>
#include <s2e/Database.h>

#include <llvm/System/TimeValue.h>

#include <iostream>
#include <sstream>



namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(InstructionCounter, "Instruction counter plugin", "InstructionCounter", "ExecutionTracer", "ModuleExecutionDetector");

void InstructionCounter::initialize()
{
    m_tb = NULL;

    m_executionTracer = static_cast<ExecutionTracer*>(s2e()->getPlugin("ExecutionTracer"));
    assert(m_executionTracer);

    m_executionDetector = static_cast<ModuleExecutionDetector*>(s2e()->getPlugin("ModuleExecutionDetector"));
    assert(m_executionDetector);

    //TODO: whole-system counting
    startCounter();
}




/////////////////////////////////////////////////////////////////////////////////////
void InstructionCounter::startCounter()
{
    m_executionDetector->onModuleTranslateBlockStart.connect(
            sigc::mem_fun(*this, &InstructionCounter::onTranslateBlockStart)
            );
}


/////////////////////////////////////////////////////////////////////////////////////

/**
 *  Instrument only the blocks where we want to count the instructions.
 */
void InstructionCounter::onTranslateBlockStart(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        const ModuleDescriptor &module,
        TranslationBlock *tb,
        uint64_t pc)
{
    if (m_tb) {
        m_tbConnection.disconnect();
    }
    m_tb = tb;

    CorePlugin *plg = s2e()->getCorePlugin();
    m_tbConnection = plg->onTranslateInstructionStart.connect(
            sigc::mem_fun(*this, &InstructionCounter::onTranslateInstructionStart)
    );

    //This function will flush the number of executed instructions
    signal->connect(
        sigc::mem_fun(*this, &InstructionCounter::onTraceTb)
    );
}


void InstructionCounter::onTranslateInstructionStart(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        TranslationBlock *tb,
        uint64_t pc)
{
    if (tb != m_tb) {
        //We've been suddenly interrupted by some other module
        m_tb = NULL;
        m_tbConnection.disconnect();
        return;
    }

    //Connect a function that will increment the number of executed
    //instructions.
    signal->connect(
        sigc::mem_fun(*this, &InstructionCounter::onTraceInstruction)
    );

}

void InstructionCounter::onModuleTranslateBlockEnd(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        const ModuleDescriptor &module,
        TranslationBlock *tb,
        uint64_t endPc,
        bool staticTarget,
        uint64_t targetPc)
{
    //TRACE("%"PRIx64" StaticTarget=%d TargetPc=%"PRIx64"\n", endPc, staticTarget, targetPc);

    //Done translating the blocks, no need to instrument anymore.
    m_tb = NULL;
    m_tbConnection.disconnect();
}

/////////////////////////////////////////////////////////////////////////////////////

void InstructionCounter::onTraceTb(S2EExecutionState* state, uint64_t pc)
{
    //Get the plugin state for the current path
    DECLARE_PLUGINSTATE(InstructionCounterState, state);

    if (plgState->m_lastTbPc == pc) {
        //Avoid repeateadly tracing tight loops.
        return;
    }

    //Flush the counter
    ExecutionTraceICount e;
    e.count = plgState->m_iCount;
    m_executionTracer->writeData(state, &e, sizeof(e), TRACE_ICOUNT);
}


void InstructionCounter::onTraceInstruction(S2EExecutionState* state, uint64_t pc)
{
    //Get the plugin state for the current path
    DECLARE_PLUGINSTATE(InstructionCounterState, state);

    //Increment the instruction count
    plgState->m_iCount++;
}


/////////////////////////////////////////////////////////////////////////////////////
InstructionCounterState::InstructionCounterState()
{
    m_iCount = 0;
    m_lastTbPc = 0;
}

InstructionCounterState::InstructionCounterState(S2EExecutionState *s, Plugin *p)
{
    m_iCount = 0;
    m_lastTbPc = 0;
}

InstructionCounterState::~InstructionCounterState()
{

}

PluginState *InstructionCounterState::clone() const
{
    return new InstructionCounterState(*this);
}

PluginState *InstructionCounterState::factory(Plugin *p, S2EExecutionState *s)
{
    return new InstructionCounterState(s, p);
}

} // namespace plugins
} // namespace s2e


