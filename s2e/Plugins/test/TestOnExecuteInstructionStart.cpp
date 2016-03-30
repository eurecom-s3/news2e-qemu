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

class TestOnExecuteInstructionStartPlugin : public Plugin
{
	S2E_PLUGIN

public:
	TestOnExecuteInstructionStartPlugin(S2E* s2e): Plugin(s2e) {}
	void initialize();
private:
	void slotTranslateInsnStart(ExecutionSignal* signal, S2EExecutionState* state, TranslationBlock* tb, uint64_t pc);
        void slotExecuteInsnStart(S2EExecutionState* state, uint64_t pc);
};

S2E_DEFINE_PLUGIN(TestOnExecuteInstructionStartPlugin, "Print when the OnExecuteInstructionStart event is received", "TestOnExecuteInstructionStartPlugin");

void TestOnExecuteInstructionStartPlugin::initialize()
{
    s2e()->getCorePlugin()->onTranslateInstructionStart.connect(sigc::mem_fun(*this, &TestOnExecuteInstructionStartPlugin::slotTranslateInsnStart));
}

void TestOnExecuteInstructionStartPlugin::slotTranslateInsnStart(ExecutionSignal *signal,
                                    S2EExecutionState *state,
                                    TranslationBlock *tb,
                                    uint64_t pc)
{
    signal->connect(sigc::mem_fun(*this, &TestOnExecuteInstructionStartPlugin::slotExecuteInsnStart));
}

void TestOnExecuteInstructionStartPlugin::slotExecuteInsnStart(S2EExecutionState* state, uint64_t pc)
{
    s2e()->getWarningsStream() << "OnExecuteInstructionStart called at pc " << hexval(pc) << '\n';
}

} // namespace plugins
} // namespace s2e
