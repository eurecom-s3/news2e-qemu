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

class TestOnTranslateBlockEndPlugin : public Plugin
{
	S2E_PLUGIN

public:
	TestOnTranslateBlockEndPlugin(S2E* s2e): Plugin(s2e) {}
	void initialize();
private:
	void slotTranslateTBEnd(ExecutionSignal* signal, S2EExecutionState* state, TranslationBlock* tb, uint64_t pc, bool hasNextPc, uint64_t nextPc);
};

S2E_DEFINE_PLUGIN(TestOnTranslateBlockEndPlugin, "Print when the OnTranslateBlockEnd event is received", "TestOnTranslateBlockEndPlugin");

void TestOnTranslateBlockEndPlugin::initialize()
{
    s2e()->getCorePlugin()->onTranslateBlockEnd.connect(sigc::mem_fun(*this, &TestOnTranslateBlockEndPlugin::slotTranslateTBEnd));
}

void TestOnTranslateBlockEndPlugin::slotTranslateTBEnd(ExecutionSignal *signal,
                                    S2EExecutionState *state,
                                    TranslationBlock *tb,
                                    uint64_t pc,
                                    bool hasNextPc,
                                    uint64_t nextPc)
{
    s2e()->getWarningsStream() << "OnTranslateBlockEnd called at pc " << hexval(pc) << '\n';
}

} // namespace plugins
} // namespace s2e
