#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>
#include <s2e/s2e_qemu.h>

#include "StateManager.h"
#include <klee/Searcher.h>

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(StateManager, "Control the deletion/suspension of states", "StateManager",
                  "ModuleExecutionDetector");

static void sm_callback(S2EExecutionState *s, bool killingState)
{
    StateManager *sm = static_cast<StateManager*>(g_s2e->getPlugin("StateManager"));
    assert(sm);

    if (killingState && s) {
        sm->resumeSucceededState(s);
        return;
    }

    //If there are no states, try to resume some successful ones
    if (g_s2e->getExecutor()->getStatesCount() == 0) {
        sm->killAllButOneSuccessful(false);
        return;
    }

    //Check for timeout conditions
    sm->killOnTimeOut();
}

void StateManager::resumeSucceeded()
{
    foreach2(it, m_succeeded.begin(), m_succeeded.end()) {
        m_executor->resumeState(*it);
    }
    m_succeeded.clear();
}

bool StateManager::resumeSucceededState(S2EExecutionState *s)
{
    if (m_succeeded.find(s) != m_succeeded.end()) {
        m_succeeded.erase(s);
        m_executor->resumeState(s);
        return true;
    }
    return false;

}

void StateManager::initialize()
{
    ConfigFile *cfg = s2e()->getConfig();

    m_timeout = cfg->getInt(getConfigKey() + ".timeout");
    m_timerTicks = 0;

    m_detector = static_cast<ModuleExecutionDetector*>(s2e()->getPlugin("ModuleExecutionDetector"));

    m_detector->onModuleTranslateBlockStart.connect(
            sigc::mem_fun(*this,
                    &StateManager::onNewBlockCovered)
            );

    m_executor = s2e()->getExecutor();

    if (m_timeout > 0) {
        s2e()->getCorePlugin()->onTimer.connect(
                sigc::mem_fun(*this,
                        &StateManager::onTimer)
                );
    }

    s2e()->getExecutor()->setStateManagerCb(sm_callback);
}

//Reset the timeout every time a new block of the module is translated.
//XXX: this is an approximation. The cache could be flushed in between.
void StateManager::onNewBlockCovered(
        ExecutionSignal *signal,
        S2EExecutionState* state,
        const ModuleDescriptor &module,
        TranslationBlock *tb,
        uint64_t pc)
{
    s2e()->getDebugStream() << "New block " << std::hex << pc << " discovered" << std::endl;
    m_timerTicks = 0;
}

void StateManager::killOnTimeOut()
{
    if (m_timerTicks < m_timeout) {
        return;
    }

    s2e()->getDebugStream() << "No more blocks found in " <<
            std::dec << m_timerTicks << " seconds, killing states."
            << std::endl;

    //Reset the counter here to avoid being called again
    //(killAllButOneSuccessful will throw an exception if it deletes the current state).
    m_timerTicks = 0;

    if (!killAllButOneSuccessful(true)) {
        s2e()->getDebugStream() << "There are no successful states to kill..."  << std::endl;
        //killAllButCurrent();
    }

}

void StateManager::onTimer()
{
    ++m_timerTicks;
}

bool StateManager::succeedState(S2EExecutionState *s)
{
    s2e()->getDebugStream() << "Succeeding state " << std::dec << s->getID() << std::endl;

    if (m_succeeded.find(s) != m_succeeded.end()) {
        //Avoid suspending states that were consecutively succeeded.
        s2e()->getDebugStream() << "State " << std::dec << s->getID() <<
                " was already marked as succeeded" << std::endl;
        return false;
    }
    m_succeeded.insert(s);

    return s2e()->getExecutor()->suspendState(s);
}

bool StateManager::killAllButCurrent()
{
    s2e()->getDebugStream() << "Killing all but current " << std::dec << g_s2e_state->getID() << std::endl;
    const std::set<klee::ExecutionState*> &states = s2e()->getExecutor()->getStates();
    std::set<klee::ExecutionState*>::const_iterator it1, it2;

    foreach2(it, m_succeeded.begin(), m_succeeded.end()) {
        m_executor->resumeState(*it);
    }
    m_succeeded.clear();

    it1 = states.begin();
    while(it1 != states.end()) {
        if (*it1 == g_s2e_state) {
            ++it1;
            continue;
        }
        it2 = it1;
        ++it2;
        s2e()->getExecutor()->terminateStateOnExit(**it1);
        it1 = it2;
    }
    return true;
}

bool StateManager::killAllButOneSuccessful(bool killCurrent)
{
    if (m_succeeded.size() < 1) {
        return false;
    }

    s2e()->getDebugStream() << "Killing all but one successful " << std::endl;

    foreach2(it, m_succeeded.begin(), m_succeeded.end()) {
        m_executor->resumeState(*it);
    }

    S2EExecutionState *one =  *m_succeeded.begin();
    const std::set<klee::ExecutionState*> &states = m_executor->getStates();
    m_succeeded.clear();

    bool doKillCurrent = false;

    foreach2(it, states.begin(), states.end()) {
        if (*it == one) {
            s2e()->getDebugStream() << "Keeping state " << std::dec << one->getID() << std::endl;
            continue;
        }else {
            if (*it != g_s2e_state) {
                S2EExecutionState* s2eState = static_cast<S2EExecutionState*>(*it);
                s2e()->getDebugStream() << "Killing state  " << std::dec << (s2eState)->getID() << std::endl;
                //XXX: the state will be lost
                //m_executor->suspendState(*it);
                s2e()->getExecutor()->terminateStateOnExit(**it);
            }else {
                if (killCurrent) {
                    doKillCurrent = true;
                }
            }
        }
    }

    //In case we need to kill the current state, do it last, because it will throw and exception
    //and return to the state scheduler.
    if (doKillCurrent) {
        s2e()->getExecutor()->terminateStateOnExit(*g_s2e_state);
    }

    return true;
}

bool StateManager::empty()
{
    assert(s2e()->getExecutor()->getSearcher());
    return s2e()->getExecutor()->getSearcher()->empty();
}

}
}
