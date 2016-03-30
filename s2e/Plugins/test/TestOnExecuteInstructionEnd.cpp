#include "s2e/cxx/Plugin.h" 
#include "s2e/Plugins/CorePlugin.h"
#include "s2e/cxx/S2EExecutionState.h"
#include "s2e/cxx/S2EExecutor.h"
#include "s2e/cxx/ConfigFile.h"
#include "s2e/cxx/Utils.h"
#include "s2e/cxx/S2E.h"

#include <iostream>
#include <sstream>

using namespace std;

namespace s2e {
namespace plugins {

class TestOnExecuteInstructionEndPlugin : public Plugin
{
	S2E_PLUGIN

public:
	TestOnExecuteInstructionEndPlugin(S2E* s2e): Plugin(s2e) {}
	void initialize();
private:
	void slotTranslateInsnEnd(ExecutionSignal* signal, S2EExecutionState* state, TranslationBlock* tb, uint64_t pc);
        void slotExecuteInsnEnd(S2EExecutionState* state, uint64_t pc);
};

S2E_DEFINE_PLUGIN(TestOnExecuteInstructionEndPlugin, "Print when the OnExecuteInstructionEnd event is received", "TestOnExecuteInstructionEndPlugin");

void TestOnExecuteInstructionEndPlugin::initialize()
{
    s2e()->getCorePlugin()->onTranslateInstructionEnd.connect(sigc::mem_fun(*this, &TestOnExecuteInstructionEndPlugin::slotTranslateInsnEnd));
}

void TestOnExecuteInstructionEndPlugin::slotTranslateInsnEnd(ExecutionSignal *signal,
                                    S2EExecutionState *state,
                                    TranslationBlock *tb,
                                    uint64_t pc)
{
    signal->connect(sigc::mem_fun(*this, &TestOnExecuteInstructionEndPlugin::slotExecuteInsnEnd));
}

void TestOnExecuteInstructionEndPlugin::slotExecuteInsnEnd(S2EExecutionState* state, uint64_t pc)
{
    s2e()->getWarningsStream() << "OnExecuteInstructionEnd called at pc " << hexval(pc) << '\n';
}

} // namespace plugins
} // namespace s2e
