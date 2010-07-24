#define NDEBUG

// XXX: qemu stuff should be included before anything from KLEE or LLVM !
extern "C" {
#include "config.h"
#include "qemu-common.h"
}

#include "UserModeInterceptor.h"
#include "WindowsImage.h"

#include <s2e/Utils.h>

#include <string>

using namespace s2e;
using namespace plugins;

WindowsUmInterceptor::WindowsUmInterceptor(WindowsMonitor *Os)
{

    m_Os = Os;
    m_TracingState = SEARCH_PROCESS;
    m_PrevCr3 = 0;

    m_ASBase = 0;
    m_ASSize = Os->GetUserAddressSpaceSize();
}


WindowsUmInterceptor::~WindowsUmInterceptor()
{

}

#if 0
/**
*  Cycle through the list of all loaded processes and notify the listeners
*/
bool WindowsUmInterceptor::NotifyLoadedProcesses(S2EExecutionState *state)
{
    s2e::windows::LIST_ENTRY32 ListHead;
    uint64_t ActiveProcessList = m_Os->GetPsActiveProcessListPtr();
    CPUState *cpuState = (CPUState *)state->getCpuState();

    uint64_t pListHead = PsLoadedModuleList;
    if (!state->readMemoryConcrete(ActiveProcessList, &ListHead, sizeof(ListHead))) {
        return false;
    }

    for (pItem = ListHead.Flink; pItem != pListHead; ) {
        uint32_t pProcessEntry = CONTAINING_RECORD32(pItem, s2e::windows::EPROCESS32, ActiveProcessLinks);
        s2e::windows::EPROCESS32 ProcessEntry;

        if (!state->readMemoryConcrete(pProcessEntry, &ProcessEntry, sizeof(ProcessEntry))) {
            return false;
        }

        ModuleDescriptor desc;
        state->readString(ProcessEntry.ImageFileName, desc.Name, sizeof(ProcessEntry.ImageFileName));
        desc.Pid = ProcessEntry.Pcb.DirectoryTableBase;
        desc.LoadBase = ProcessEntry.Pcb. LdrEntry.DllBase;
        desc.Size = LdrEntry.SizeOfImage;


    }
}
#endif

bool WindowsUmInterceptor::FindModules(S2EExecutionState *state)
{
    s2e::windows::LDR_DATA_TABLE_ENTRY32 LdrEntry;
    s2e::windows::PEB_LDR_DATA32 LdrData;

    if (!WaitForProcessInit(state)) {
        return false;
    }

    if (!state->readMemoryConcrete(m_LdrAddr, &LdrData, sizeof(s2e::windows::PEB_LDR_DATA32))) {
        return false;
    }

    uint32_t CurLib = CONTAINING_RECORD32(LdrData.InLoadOrderModuleList.Flink,
        s2e::windows::LDR_DATA_TABLE_ENTRY32, InLoadOrderLinks);

    uint32_t HeadOffset = m_LdrAddr + offsetof(s2e::windows::PEB_LDR_DATA32, InLoadOrderModuleList);
    if (LdrData.InLoadOrderModuleList.Flink == HeadOffset) {
        return false;
    }

    do {
        if (!state->readMemoryConcrete(CurLib, &LdrEntry, sizeof(s2e::windows::LDR_DATA_TABLE_ENTRY32))) {
            DPRINTF("Could not read LDR_DATA_TABLE_ENTRY (%#x)\n", CurLib);
            return false;
        }

        std::string s;
        state->readUnicodeString(LdrEntry.BaseDllName.Buffer, s, LdrEntry.BaseDllName.Length);
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);

        if (s.length() == 0) {
            if (LdrEntry.DllBase == 0x7c900000) {
                //XXX
                s = "ntdll.dll";
            }else {
                s = "<unnamed>";
            }
        }

        //if (m_SearchedModules.find(s) != m_SearchedModules.end()) {
        //Update the information about the library
        ModuleDescriptor Desc;
        Desc.Pid = state->getPid();
        Desc.Name = s;
        Desc.LoadBase = LdrEntry.DllBase;
        Desc.Size = LdrEntry.SizeOfImage;

        //XXX: this must be state-local
        if (m_LoadedLibraries.find(Desc) == m_LoadedLibraries.end()) {
            DPRINTF("  MODULE %s Base=%#x Size=%#x\n", s.c_str(), LdrEntry.DllBase, LdrEntry.SizeOfImage);
            m_LoadedLibraries.insert(Desc);
            NotifyModuleLoad(state, Desc);
        }

        CurLib = CONTAINING_RECORD32(LdrEntry.InLoadOrderLinks.Flink,
            s2e::windows::LDR_DATA_TABLE_ENTRY32, InLoadOrderLinks);
    }while(LdrEntry.InLoadOrderLinks.Flink != HeadOffset);

    return true;
}


bool WindowsUmInterceptor::WaitForProcessInit(S2EExecutionState* state)
{
    s2e::windows::PEB_LDR_DATA32 LdrData;
    s2e::windows::PEB32 PebBlock;
    uint32_t Peb = (uint32_t)-1;


    uint64_t fsBase = state->readCpuState(CPU_OFFSET(segs[R_FS].base), 8*sizeof(target_ulong));


    if (state->getPc() < 0x80000000) {
        if(!state->readMemoryConcrete(fsBase + 0x18, &Peb, 4)) {
            return false;
        }

        if(!state->readMemoryConcrete(Peb+0x30, &Peb, 4)) {
            return false;
        }
    }else {
        //We are in kernel mode, do it by reading kernel-mode struc
        uint32_t v, curProcess = -1;
        if(!state->readMemoryConcrete(fsBase + 0x124, &v, (sizeof(v)))) {
            return false;
        }

        if(!state->readMemoryConcrete(v + KPCR_EPROCESS, &curProcess, (sizeof(curProcess)))) {
            return false;
        }

        /*s2e::windows::EPROCESS32 epr;

        if(!state->readMemoryConcrete(curProcess, &epr, (sizeof(epr)))) {
            return false;
        }*/

        if(!state->readMemoryConcrete(curProcess + offsetof(s2e::windows::EPROCESS32,Peb), &Peb, (sizeof(Peb)))) {
            return false;
        }
    }

    if (Peb == 0xFFFFFFFF) {
        return false;
    }

    if (!state->readMemoryConcrete(Peb, &PebBlock, sizeof(PebBlock))) {
        return false;
    }

    /* Check that the entries are inited */
    if (!state->readMemoryConcrete(PebBlock.Ldr, &LdrData,
        sizeof(s2e::windows::PEB_LDR_DATA32))) {
            return false;
    }

    /* Check that the structure is correctly initialized */
    if (LdrData.Length != 0x28)
        return false;

    if (!LdrData.InLoadOrderModuleList.Flink || !LdrData.InLoadOrderModuleList.Blink )
        return false;

    if (!LdrData.InMemoryOrderModuleList.Flink || !LdrData.InMemoryOrderModuleList.Blink )
        return false;

    m_LdrAddr = PebBlock.Ldr;
    m_ProcBase = PebBlock.ImageBaseAddress;

    DPRINTF("Process %#"PRIx64" %#x %#x\n", m_ProcBase, LdrData.Initialized, LdrData.EntryInProgress);
    return true;

}


void WindowsUmInterceptor::NotifyModuleLoad(S2EExecutionState *state, ModuleDescriptor &Library)
{
    WindowsImage Image(state, Library.LoadBase);
    Library.NativeBase = Image.GetImageBase();
    Library.EntryPoint = Image.GetEntryPoint();
    m_Os->onModuleLoad.emit(state, Library);
}

bool WindowsUmInterceptor::CatchModuleLoad(S2EExecutionState *State)
{
    FindModules(State);
    return true;
}

bool WindowsUmInterceptor::CatchProcessTermination(S2EExecutionState *State)
{
    uint64_t pEProcess;

    assert(m_Os->GetVersion() == WindowsMonitor::SP3);

    pEProcess = cast<klee::ConstantExpr>(
        State->readCpuRegister(CPU_OFFSET(regs[R_EBX]), 8*sizeof(target_ulong)))
            ->getZExtValue();
    s2e::windows::EPROCESS32 EProcess;

    if (!State->readMemoryConcrete(pEProcess, &EProcess, sizeof(EProcess))) {
        TRACE("Could not read EProcess data structure at %#"PRIx64"!\n", pEProcess);
        return false;
    }

    DPRINTF("Process %#"PRIx32" %16s unloaded\n", EProcess.Pcb.DirectoryTableBase,
        EProcess.ImageFileName);
    m_Os->onProcessUnload.emit(State, EProcess.Pcb.DirectoryTableBase);

    return true;
}

bool WindowsUmInterceptor::CatchModuleUnload(S2EExecutionState *State)
{
    //XXX: This register is hard coded for XP SP3
    assert(m_Os->GetVersion() == WindowsMonitor::SP3);
    uint64_t pLdrEntry = cast<klee::ConstantExpr>(
        State->readCpuRegister(CPU_OFFSET(regs[R_ESI]), 8*sizeof(target_ulong)))
            ->getZExtValue();
    s2e::windows::LDR_DATA_TABLE_ENTRY32 LdrEntry;

    if (!State->readMemoryConcrete(pLdrEntry, &LdrEntry, sizeof(LdrEntry))) {
        TRACE("Could not read pLdrEntry data structure at %#"PRIx64"!\n", pLdrEntry);
        return false;
    }


    ModuleDescriptor Desc;
    Desc.Pid = State->getPc();

    State->readUnicodeString(LdrEntry.BaseDllName.Buffer, Desc.Name, LdrEntry.BaseDllName.Length);;
    std::transform(Desc.Name.begin(), Desc.Name.end(), Desc.Name.begin(), ::tolower);

    Desc.LoadBase = LdrEntry.DllBase;
    Desc.Size = LdrEntry.SizeOfImage;

    DPRINTF("Detected module unload %s pid=%#"PRIx64" LoadBase=%#"PRIx64"\n",
        Desc.Name.c_str(), Desc.Pid, Desc.LoadBase);

    m_Os->onModuleUnload.emit(State, Desc);

    return true;
}

bool WindowsUmInterceptor::GetPids(S2EExecutionState *State, PidSet &out)
{
    s2e::windows::LIST_ENTRY32 ListHead;
    uint64_t ActiveProcessList = m_Os->GetPsActiveProcessListPtr();
    uint64_t pItem;

    //Read the head of the list
    if (!State->readMemoryConcrete(ActiveProcessList, &ListHead, sizeof(ListHead))) {
        return false;
    }

    //Check for empty list
    if (ListHead.Flink == ActiveProcessList) {
        return true;
    }

    for (pItem = ListHead.Flink; pItem != ActiveProcessList; pItem = ListHead.Flink) {
        if (!State->readMemoryConcrete(pItem, &ListHead, sizeof(ListHead))) {
            return false;
        }

        uint32_t pProcessEntry = CONTAINING_RECORD32(pItem, s2e::windows::EPROCESS32, ActiveProcessLinks);
        s2e::windows::EPROCESS32 ProcessEntry;

        if (!State->readMemoryConcrete(pProcessEntry, &ProcessEntry, sizeof(ProcessEntry))) {
            return false;
        }

        out.insert(ProcessEntry.Pcb.DirectoryTableBase);
    }
    return true;
}


