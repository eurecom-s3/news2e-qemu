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

class TestOnExecuteBlockEndPlugin : public Plugin
{
	S2E_PLUGIN

public:
	TestOnExecuteBlockEndPlugin(S2E* s2e): Plugin(s2e) {}
	void initialize();
private:
	void slotTranslateTBEnd(ExecutionSignal* signal, S2EExecutionState* state, TranslationBlock* tb, uint64_t pc, bool hasNextPc, uint64_t nextPc);
        void slotExecuteTBEnd(S2EExecutionState* state, uint64_t pc);
};

S2E_DEFINE_PLUGIN(TestOnExecuteBlockEndPlugin, "Print when the OnExecuteBlockEnd event is received", "TestOnExecuteBlockEndPlugin");

void TestOnExecuteBlockEndPlugin::initialize()
{
    s2e()->getCorePlugin()->onTranslateBlockEnd.connect(sigc::mem_fun(*this, &TestOnExecuteBlockEndPlugin::slotTranslateTBEnd));
}

void TestOnExecuteBlockEndPlugin::slotTranslateTBEnd(ExecutionSignal *signal,
                                    S2EExecutionState *state,
                                    TranslationBlock *tb,
                                    uint64_t pc,
                                    bool hasNextPc,
                                    uint64_t nextPc)
{
    signal->connect(sigc::mem_fun(*this, &TestOnExecuteBlockEndPlugin::slotExecuteTBEnd));
}

void TestOnExecuteBlockEndPlugin::slotExecuteTBEnd(S2EExecutionState* state, uint64_t pc)
{
    s2e()->getWarningsStream() << "OnExecuteBlockEnd called at pc " << hexval(pc) << '\n';
}

} // namespace plugins
} // namespace s2e
