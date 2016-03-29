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

class TestOnTranslateBlockStartPlugin : public Plugin
{
	S2E_PLUGIN

public:
	TestOnTranslateBlockStartPlugin(S2E* s2e): Plugin(s2e) {}
	void initialize();

private:
	void slotTranslateTBStart(ExecutionSignal* signal, S2EExecutionState* state, TranslationBlock* tb, uint64_t pc);
};

S2E_DEFINE_PLUGIN(TestOnTranslateBlockStartPlugin, "Print when the OnTranslateBlockStart event is received", "TestOnTranslateBlockStartPlugin", );

void TestOnTranslateBlockStartPlugin::initialize()
{
    s2e()->getCorePlugin()->onTranslateBlockStart.connect(sigc::mem_fun(*this, &TestOnTranslateBlockStartPlugin::slotTranslateTBStart));
}

void TestOnTranslateBlockStartPlugin::slotTranslateTBStart(ExecutionSignal *signal,
                                    S2EExecutionState *state,
                                    TranslationBlock *tb,
                                    uint64_t pc)
{
    s2e()->getWarningsStream() << "OnTranslateBlockStart called at pc " << hexval(pc) << '\n';
}

} // namespace plugins
} // namespace s2e
