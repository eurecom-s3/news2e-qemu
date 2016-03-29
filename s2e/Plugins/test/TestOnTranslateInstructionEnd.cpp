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

class TestOnTranslateInstructionEndPlugin : public Plugin
{
	S2E_PLUGIN

public:
	TestOnTranslateInstructionEndPlugin(S2E* s2e): Plugin(s2e) {}
	void initialize();
private:
	void slotTranslateInsnEnd(ExecutionSignal* signal, S2EExecutionState* state, TranslationBlock* tb, uint64_t pc);
};

S2E_DEFINE_PLUGIN(TestOnTranslateInstructionEndPlugin, "Print when the OnTranslateInstructionEnd event is received", "TestOnTranslateInstructionEndPlugin");

void TestOnTranslateInstructionEndPlugin::initialize()
{
    s2e()->getCorePlugin()->onTranslateInstructionEnd.connect(sigc::mem_fun(*this, &TestOnTranslateInstructionEndPlugin::slotTranslateInsnEnd));
}

void TestOnTranslateInstructionEndPlugin::slotTranslateInsnEnd(ExecutionSignal *signal,
                                    S2EExecutionState *state,
                                    TranslationBlock *tb,
                                    uint64_t pc)
{
    s2e()->getWarningsStream() << "OnTranslateInstructionEnd called at pc " << hexval(pc) << '\n';
}

} // namespace plugins
} // namespace s2e
