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

class TestOnTranslationBlockStartPlugin : public Plugin
{
	S2E_PLUGIN

public:
	TestOnTranslationBlockStartPlugin(S2E* s2e): Plugin(s2e) {}
	void initialize();

	/** Signal emited when first instruction is executed */
	sigc::signal<void, S2EExecutionState * >
			   onInitialize;

private:
	sigc::connection connExecuteInstr;
	
	void slotTranslateTBStart(ExecutionSignal* signal, S2EExecutionState* state, TranslationBlock* tb, uint64_t pc);
};

S2E_DEFINE_PLUGIN(TestOnTranslationBlockStartPlugin, "Print when the OnTranslationBlockStart event is received", "TestOnTranslationBlockStart", );

void TestOnTranslationBlockStartPlugin::initialize()
{
    s2e()->getCorePlugin()->onTranslateBlockStart.connect(sigc::mem_fun(*this, &TestOnTranslationBlockStartPlugin::slotTranslateTBStart));
}

void TestOnTranslationBlockStartPlugin::slotTranslateTBStart(ExecutionSignal *signal,
                                    S2EExecutionState *state,
                                    TranslationBlock *tb,
                                    uint64_t pc)
{
    s2e()->getWarningsStream() << "OnTranslationBlockStart called at pc 0x" << hexval(pc) << '\n';
}

} // namespace plugins
} // namespace s2e
