#ifndef S2E_PLUGINS_SYMBHW_H
#define S2E_PLUGINS_SYMBHW_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>
#include <s2e/ConfigFile.h>

#include <string>
#include <set>
#include <map>

namespace s2e {
namespace plugins {

class SymbolicHardware;

class DeviceDescriptor {
protected:
    std::string m_id;
    void *m_qemuIrq;
    void *m_qemuDev;
    bool m_active;

public:
    DeviceDescriptor(const std::string &id);

    static DeviceDescriptor *create(SymbolicHardware *plg, ConfigFile *cfg, const std::string &key);
    virtual ~DeviceDescriptor();

    struct comparator {
    bool operator()(const DeviceDescriptor *dd1, const DeviceDescriptor *dd2) const {
        return dd1->m_id < dd2->m_id;
    }
    };

    bool isActive() const {
        return m_active;
    }

    void setActive(bool b) {
        m_active = true;
    }

    void setDevice(void *qemuDev) {
        m_qemuDev = qemuDev;
    }

    virtual void print(std::ostream &os) const {}
    virtual void initializeQemuDevice() {assert(false);}
    virtual void activateQemuDevice(struct PCIBus *bus) { assert(false);}
    virtual void setInterrupt(bool state) {assert(false);};
    virtual void assignIrq(void *irq) {assert(false);}
    virtual bool readPciAddressSpace(void *buffer, uint32_t offset, uint32_t size) {
        return false;
    }
};

class IsaDeviceDescriptor:public DeviceDescriptor {
public:
    struct IsaResource {
        uint16_t portBase;
        uint16_t portSize;
        uint8_t irq;        
    };

private:
    IsaResource m_isaResource;

    struct ISADeviceInfo *m_isaInfo;
    struct Property *m_isaProperties;
public:
    IsaDeviceDescriptor(const std::string &id, const IsaResource &res);

    static IsaDeviceDescriptor* create(SymbolicHardware *plg, ConfigFile *cfg, const std::string &key);
    virtual ~IsaDeviceDescriptor();
    virtual void print(std::ostream &os) const;
    virtual void initializeQemuDevice();
    virtual void activateQemuDevice(struct PCIBus *bus);

    const IsaResource& getResource() const {
        return m_isaResource;
    }

    virtual void setInterrupt(bool state);
    virtual void assignIrq(void *irq);
};

class PciDeviceDescriptor:public DeviceDescriptor {
public:
    struct PciResource{
        bool isIo;
        uint32_t size;
        bool prefetchable;
    };

    typedef std::vector<PciResource> PciResources;
private:
    uint16_t m_vid;
    uint16_t m_pid;
    uint32_t m_classCode;
    uint8_t m_revisionId;
    uint8_t m_interruptPin;
    PciResources m_resources;

    struct _PCIDeviceInfo *m_pciInfo;
    struct Property *m_pciInfoProperties;
    struct VMStateDescription *m_vmState;
    struct _VMStateField *m_vmStateFields;

    PciDeviceDescriptor(const std::string &id);
    virtual void print(std::ostream &os) const;

public:
    int mmio_io_addr;

    uint16_t getVid() const { return m_vid; }
    uint16_t getPid() const { return m_pid; }
    uint32_t getClassCode() const { return m_classCode; }
    uint8_t getRevisionId() const { return m_revisionId; }
    uint8_t getInterruptPin() const { return m_interruptPin; }

    const PciResources& getResources() const { return m_resources; }
    static PciDeviceDescriptor* create(SymbolicHardware *plg, ConfigFile *cfg, const std::string &key);

    virtual void initializeQemuDevice();
    virtual void activateQemuDevice(struct PCIBus *bus);

    virtual void setInterrupt(bool state);
    virtual void assignIrq(void *irq);

    virtual bool readPciAddressSpace(void *buffer, uint32_t offset, uint32_t size);

    virtual ~PciDeviceDescriptor();
};

class SymbolicHardware : public Plugin
{
    S2E_PLUGIN
public:

            typedef std::set<DeviceDescriptor *,DeviceDescriptor::comparator > DeviceDescriptors;


public:
    SymbolicHardware(S2E* s2e): Plugin(s2e) {}
    virtual ~SymbolicHardware();
    void initialize();

    DeviceDescriptor *findDevice(const std::string &name) const;

    void setSymbolicPortRange(uint16_t start, unsigned size, bool isSymbolic);
    bool isSymbolic(uint16_t port) const;

    bool isMmioSymbolic(uint64_t physaddress) const;
    bool setSymbolicMmioRange(S2EExecutionState *state, uint64_t physaddr, uint64_t size);
    bool resetSymbolicMmioRange(S2EExecutionState *state, uint64_t physaddr);
private:
    uint32_t m_portMap[65536/(sizeof(uint32_t)*8)];
    DeviceDescriptors m_devices;

    void onDeviceRegistration();
    void onDeviceActivation(struct PCIBus* pci);

};

class SymbolicHardwareState : public PluginState
{
public:
    struct MemoryRange {
        uint64_t base, size;
        bool operator()(const MemoryRange &r1, const MemoryRange &r2) const {
            return r1.base + r1.size <= r2.base;
        }
        MemoryRange() {
            base = size = 0;
        }
    };

    typedef std::set<MemoryRange, MemoryRange> MemoryRanges;
private:

    MemoryRanges m_MmioMemory;


public:

    SymbolicHardwareState();
    virtual ~SymbolicHardwareState();
    virtual SymbolicHardwareState* clone() const;
    static PluginState *factory(Plugin *p, S2EExecutionState *s);

    bool addMmioRange(uint64_t physbase, uint64_t size);
    bool delMmioRange(uint64_t physbase);
    bool isMmio(uint64_t physaddr, uint64_t size) const;

    friend class SymbolicHardware;

};

} // namespace plugins
} // namespace s2e

#endif // S2E_PLUGINS_EXAMPLE_H
