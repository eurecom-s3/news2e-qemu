#ifndef S2E_EXECUTIONSTATE_H
#define S2E_EXECUTIONSTATE_H

#include <klee/ExecutionState.h>
#include <klee/Memory.h>

extern "C" {
    struct TranslationBlock;
    struct TimersState;
}

// XXX
struct CPUX86State;
#define CPU_OFFSET(field) offsetof(CPUX86State, field)

//#include <tr1/unordered_map>

namespace s2e {

class Plugin;
class PluginState;
class S2EDeviceState;
struct S2ETranslationBlock;

//typedef std::tr1::unordered_map<const Plugin*, PluginState*> PluginStateMap;
typedef std::map<const Plugin*, PluginState*> PluginStateMap;
typedef PluginState* (*PluginStateFactory)(Plugin *p, S2EExecutionState *s);

class S2EExecutionState : public klee::ExecutionState
{
protected:
    friend class S2EExecutor;

    static int s_lastStateID;

    /** Unique numeric ID for the state */
    int m_stateID;

    PluginStateMap m_PluginState;

    /** True value means forking is enabled. */
    bool m_symbexEnabled;

    /* Internal variable - set to PC where execution should be
       switched to symbolic (e.g., due to access to symbolic memory */
    uint64_t m_startSymbexAtPC;

    /** Set to true when the state is active (i.e., currently selected).
        NOTE: for active states, SharedConcrete memory objects are stored
              in shared locations, for inactive - in ObjectStates. */
    bool m_active;

    /** Set to true when the state executes code in concrete mode.
        NOTE: When m_runningConcrete is true, CPU registers that contain
              concrete values are stored in the shared region (env global
              variable), all other CPU registers are stored in ObjectState.
    */
    bool m_runningConcrete;

    /* Move the following to S2EExecutor */
    klee::MemoryObject* m_cpuRegistersState;
    klee::MemoryObject* m_cpuSystemState;

    klee::ObjectState *m_cpuRegistersObject;
    klee::ObjectState *m_cpuSystemObject;

    S2EDeviceState *m_deviceState;

    /* The following structure is used to store QEMU time accounting
       variables while the state is inactive */
    TimersState* m_timersState;

    S2ETranslationBlock* m_lastS2ETb;

    ExecutionState* clone();
    void addressSpaceChange(const klee::MemoryObject *mo,
                            const klee::ObjectState *oldState,
                            klee::ObjectState *newState);

public:
    enum AddressType {
        VirtualAddress, PhysicalAddress, HostAddress
    };

    S2EExecutionState(klee::KFunction *kf);
    ~S2EExecutionState();

    int getID() const { return m_stateID; }

    S2EDeviceState *getDeviceState() const {
        return m_deviceState;
    }

    TranslationBlock *getTb() const;

    uint64_t getTotalInstructionCount();

    /*************************************************/

    PluginState* getPluginState(Plugin *plugin, PluginStateFactory factory) {
        PluginStateMap::iterator it = m_PluginState.find(plugin);
        if (it == m_PluginState.end()) {
            PluginState *ret = factory(plugin, this);
            assert(ret);
            m_PluginState[plugin] = ret;
            return ret;
        }
        return (*it).second;
    }

    /** Returns true is this is the active state */
    bool isActive() const { return m_active; }

    /** Returns true if this state is currently running in concrete mode */
    bool isRunningConcrete() const { return m_runningConcrete; }

    /** Returns a mask of registers that contains symbolic values */
    uint64_t getSymbolicRegistersMask() const;

    /** Read CPU general purpose register */
    klee::ref<klee::Expr> readCpuRegister(unsigned offset,
                                          klee::Expr::Width width) const;

    /** Write CPU general purpose register */
    void writeCpuRegister(unsigned offset, klee::ref<klee::Expr> value);

    /** Read concrete value from general purpose CPU register */
    bool readCpuRegisterConcrete(unsigned offset, void* buf, unsigned size);

    /** Write concrete value to general purpose CPU register */
    void writeCpuRegisterConcrete(unsigned offset, const void* buf, unsigned size);

    /** Read CPU system state */
    uint64_t readCpuState(unsigned offset, unsigned width) const;

    /** Write CPU system state */
    void writeCpuState(unsigned offset, uint64_t value, unsigned width);

    uint64_t getPc() const;
    uint64_t getPid() const;
    uint64_t getSp() const;

    void setPc(uint64_t pc);
    void setSp(uint64_t sp);

    bool getReturnAddress(uint64_t *retAddr);
    bool bypassFunction(unsigned paramCount);
    void undoCallAndJumpToSymbolic();

    void dumpStack(unsigned count);

    /** Returns true if symbex is currently enabled for this state */
    bool isSymbolicExecutionEnabled() const { return m_symbexEnabled; }

    /** Read value from memory, returning false if the value is symbolic */
    bool readMemoryConcrete(uint64_t address, void *buf, uint64_t size,
                            AddressType addressType = VirtualAddress);

    /** Write concrete value to memory */
    bool writeMemoryConcrete(uint64_t address, void *buf,
                             uint64_t size, AddressType addressType=VirtualAddress);

    /** Read an ASCIIZ string from memory */
    bool readString(uint64_t address, std::string &s, unsigned maxLen=256);
    bool readUnicodeString(uint64_t address, std::string &s, unsigned maxLen=256);

    /** Virtual address translation (debug mode). Returns -1 on failure. */
    uint64_t getPhysicalAddress(uint64_t virtualAddress) const;

    /** Address translation (debug mode). Returns host address or -1 on failure */
    uint64_t getHostAddress(uint64_t address,
                            AddressType addressType = VirtualAddress) const;

    /** Access to state's memory. Address is virtual or physical,
        depending on 'physical' argument. Returns NULL or false in
        case of failure (can't resolve virtual address or physical
        address is invalid) */
    klee::ref<klee::Expr> readMemory(uint64_t address,
                             klee::Expr::Width width,
                             AddressType addressType = VirtualAddress) const;
    klee::ref<klee::Expr> readMemory8(uint64_t address,
                              AddressType addressType = VirtualAddress) const;

    bool writeMemory(uint64_t address,
                     klee::ref<klee::Expr> value,
                     AddressType addressType = VirtualAddress);
    bool writeMemory(uint64_t address,
                     uint8_t* buf,
                     klee::Expr::Width width,
                     AddressType addressType = VirtualAddress);

    bool writeMemory8(uint64_t address,
                      klee::ref<klee::Expr> value,
                      AddressType addressType = VirtualAddress);
    bool writeMemory8 (uint64_t address, uint8_t  value,
                       AddressType addressType = VirtualAddress);
    bool writeMemory16(uint64_t address, uint16_t value,
                       AddressType addressType = VirtualAddress);
    bool writeMemory32(uint64_t address, uint32_t value,
                       AddressType addressType = VirtualAddress);
    bool writeMemory64(uint64_t address, uint64_t value,
                       AddressType addressType = VirtualAddress);

    /** Creates new unconstrained symbolic value */
    klee::ref<klee::Expr> createSymbolicValue(klee::Expr::Width width,
                              const std::string& name = std::string());

    std::vector<klee::ref<klee::Expr> > createSymbolicArray(
            unsigned size, const std::string& name = std::string());

    /** Debug functions **/
    void dumpX86State(std::ostream &os) const;
};

//Some convenience macros
#define SREAD(state, addr, val) if (!state->readMemoryConcrete(addr, &val, sizeof(val))) { return; }
#define SREADR(state, addr, val) if (!state->readMemoryConcrete(addr, &val, sizeof(val))) { return false; }

}

#endif // S2E_EXECUTIONSTATE_H
