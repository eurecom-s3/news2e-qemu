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

class TestOnExecuteBlockStartPlugin : public Plugin
{
	S2E_PLUGIN

public:
	TestOnExecuteBlockStartPlugin(S2E* s2e): Plugin(s2e) {}
	void initialize();
private:
	void slotTranslateTBStart(ExecutionSignal* signal, S2EExecutionState* state, TranslationBlock* tb, uint64_t pc);
	void slotExecuteTBStart(S2EExecutionState* state, uint64_t pc);
};

S2E_DEFINE_PLUGIN(TestOnExecuteBlockStartPlugin, "Print when the OnExecuteBlockStart event is received", "TestOnExecuteBlockStart");

void TestOnExecuteBlockStartPlugin::initialize()
{
    s2e()->getCorePlugin()->onTranslateBlockStart.connect(sigc::mem_fun(*this, &TestOnExecuteBlockStartPlugin::slotTranslateTBStart));
}

void TestOnExecuteBlockStartPlugin::slotTranslateTBStart(ExecutionSignal *signal,
                                    S2EExecutionState *state,
                                    TranslationBlock *tb,
                                    uint64_t pc)
{
    signal->connect(sigc::mem_fun(*this, &TestOnExecuteBlockStartPlugin::slotExecuteTBStart));
}

void TestOnExecuteBlockStartPlugin::slotExecuteTBStart(S2EExecutionState* state, uint64_t pc)
{
    s2e()->getWarningsStream() << "OnExecuteBlockStart called at pc " << hexval(pc) << '\n';
}

} // namespace plugins
} // namespace s2e
