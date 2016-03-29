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

class TestOnTranslateInstructionStartPlugin : public Plugin
{
	S2E_PLUGIN

public:
	TestOnTranslateInstructionStartPlugin(S2E* s2e): Plugin(s2e) {}
	void initialize();
private:
	void slotTranslateInsnStart(ExecutionSignal* signal, S2EExecutionState* state, TranslationBlock* tb, uint64_t pc);
};

S2E_DEFINE_PLUGIN(TestOnTranslateInstructionStartPlugin, "Print when the OnTranslateInstructionStart event is received", "TestOnTranslateInstructionStartPlugin");

void TestOnTranslateInstructionStartPlugin::initialize()
{
    s2e()->getCorePlugin()->onTranslateInstructionStart.connect(sigc::mem_fun(*this, &TestOnTranslateInstructionStartPlugin::slotTranslateInsnStart));
}

void TestOnTranslateInstructionStartPlugin::slotTranslateInsnStart(ExecutionSignal *signal,
                                    S2EExecutionState *state,
                                    TranslationBlock *tb,
                                    uint64_t pc)
{
    s2e()->getWarningsStream() << "OnTranslateInstructionStart called at pc " << hexval(pc) << '\n';
}

} // namespace plugins
} // namespace s2e
