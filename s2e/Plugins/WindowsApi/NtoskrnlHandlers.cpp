/*
 * S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Dependable Systems Laboratory, EPFL nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE DEPENDABLE SYSTEMS LABORATORY, EPFL BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Currently maintained by:
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <exec-all.h>
}

#include <s2e/S2E.h>
#include <s2e/S2EExecutor.h>
#include <s2e/ConfigFile.h>
#include <s2e/Utils.h>

#include <s2e/Plugins/FunctionMonitor.h>

#define CURRENT_CLASS NtoskrnlHandlers

#include "NtoskrnlHandlers.h"

#include <s2e/Plugins/WindowsInterceptor/WindowsImage.h>
#include <klee/Solver.h>

#include <iostream>
#include <sstream>

using namespace s2e::windows;

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(NtoskrnlHandlers, "Basic collection of NT Kernel API functions.", "NtoskrnlHandlers",
                  "FunctionMonitor", "Interceptor");

const WindowsApiHandler<NtoskrnlHandlers::EntryPoint> NtoskrnlHandlers::s_handlers[] = {

    DECLARE_EP_STRUC(NtoskrnlHandlers, DebugPrint),
    DECLARE_EP_STRUC(NtoskrnlHandlers, IoCreateSymbolicLink),
    DECLARE_EP_STRUC(NtoskrnlHandlers, IoCreateDevice),
    DECLARE_EP_STRUC(NtoskrnlHandlers, IoIsWdmVersionAvailable),
    DECLARE_EP_STRUC(NtoskrnlHandlers, IoFreeMdl),

    DECLARE_EP_STRUC(NtoskrnlHandlers, RtlEqualUnicodeString),
    DECLARE_EP_STRUC(NtoskrnlHandlers, RtlAddAccessAllowedAce),
    DECLARE_EP_STRUC(NtoskrnlHandlers, GetSystemUpTime),
    DECLARE_EP_STRUC(NtoskrnlHandlers, KeStallExecutionProcessor),

    DECLARE_EP_STRUC(NtoskrnlHandlers, ExAllocatePoolWithTag),

    DECLARE_EP_STRUC(NtoskrnlHandlers, DebugPrint)
};

const NtoskrnlHandlers::NtoskrnlHandlersMap NtoskrnlHandlers::s_handlersMap =
        WindowsApi::initializeHandlerMap<NtoskrnlHandlers, NtoskrnlHandlers::EntryPoint>();

void NtoskrnlHandlers::initialize()
{
    WindowsApi::initialize();

    m_loaded = false;

    m_windowsMonitor->onModuleLoad.connect(
            sigc::mem_fun(*this,
                    &NtoskrnlHandlers::onModuleLoad)
            );

    m_windowsMonitor->onModuleUnload.connect(
            sigc::mem_fun(*this,
                    &NtoskrnlHandlers::onModuleUnload)
            );

}

void NtoskrnlHandlers::onModuleLoad(
        S2EExecutionState* state,
        const ModuleDescriptor &module
        )
{
    if (module.Name != "ntoskrnl.exe") {
        return;
    }

    if (m_loaded) {
        return;
    }

    m_loaded = true;
    m_module = module;
}

void NtoskrnlHandlers::onModuleUnload(
    S2EExecutionState* state,
    const ModuleDescriptor &module
    )
{
    if (module.Name != "ntoskrnl.exe") {
        return;
    }

    //If we get here, Windows is broken.
    m_loaded = false;

    m_functionMonitor->disconnect(state, module);
}

void NtoskrnlHandlers::DebugPrint(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }

    //Invoke this function in all contexts
     uint32_t strptr;
     bool ok = true;
     ok &= readConcreteParameter(state, 1, &strptr);

     if (!ok) {
         s2e()->getDebugStream() << "Could not read string in DebugPrint" << std::endl;
         return;
     }

     std::string message;
     ok = state->readString(strptr, message, 255);
     if (!ok) {
         s2e()->getDebugStream() << "Could not read string in DebugPrint at address 0x" << std::hex << strptr <<  std::endl;
         return;
     }

     s2e()->getMessagesStream(state) << "DebugPrint: " << message << std::endl;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////
void NtoskrnlHandlers::IoCreateSymbolicLink(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    if (getConsistency(__FUNCTION__) < LOCAL) {
        return;
    }

    state->undoCallAndJumpToSymbolic();

    S2EExecutionState *normalState = forkSuccessFailure(state, true, 2, getVariableName(state, __FUNCTION__));
    FUNCMON_REGISTER_RETURN(normalState, fns, NtoskrnlHandlers::IoCreateSymbolicLinkRet);
}

void NtoskrnlHandlers::IoCreateSymbolicLinkRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();

    if (!NtSuccess(g_s2e, state)) {
        HANDLER_TRACE_FCNFAILED();
        return;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NtoskrnlHandlers::IoCreateDevice(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    if (getConsistency(__FUNCTION__) < LOCAL) {
        return;
    }

    state->jumpToSymbolicCpp();
    S2EExecutionState *normalState = forkSuccessFailure(state, true, 7, getVariableName(state, __FUNCTION__));
    FUNCMON_REGISTER_RETURN(normalState, fns, NtoskrnlHandlers::IoCreateDeviceRet);
}

void NtoskrnlHandlers::IoCreateDeviceRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();

    if (!NtSuccess(g_s2e, state)) {
        HANDLER_TRACE_FCNFAILED();
        return;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NtoskrnlHandlers::IoIsWdmVersionAvailable(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    if (getConsistency(__FUNCTION__) < LOCAL) {
        return;
    }

    FUNCMON_REGISTER_RETURN(state, fns, NtoskrnlHandlers::IoIsWdmVersionAvailableRet);
}

void NtoskrnlHandlers::IoIsWdmVersionAvailableRet(S2EExecutionState* state)
{
    HANDLER_TRACE_RETURN();

    state->jumpToSymbolicCpp();

    uint32_t isAvailable;
    if (!state->readCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &isAvailable, sizeof(isAvailable))) {
        return;
    }
    if (!isAvailable) {
        HANDLER_TRACE_FCNFAILED();
    }

    //Fork a state with success and failure
    std::vector<uint32_t> values;
    values.push_back(1);
    values.push_back(0);
    forkRange(state, __FUNCTION__, values);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NtoskrnlHandlers::IoFreeMdl(S2EExecutionState *state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    uint32_t Buffer;
    bool ok = true;
    ok &= readConcreteParameter(state, 0, &Buffer);

    if (!ok) {
        s2e()->getDebugStream(state) << "Could not read parameters" << std::endl;
        return;
    }

    if(m_memoryChecker) {
        m_memoryChecker->revokeMemory(state, NULL, Buffer, uint64_t(-1));
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NtoskrnlHandlers::GetSystemUpTime(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();
    state->undoCallAndJumpToSymbolic();

    s2e()->getDebugStream(state) << "Bypassing function " << __FUNCTION__ << std::endl;

    klee::ref<klee::Expr> ret = state->createSymbolicValue(klee::Expr::Int32, getVariableName(state, __FUNCTION__));

    uint32_t valPtr;
    if (readConcreteParameter(state, 0, &valPtr)) {
        state->writeMemory(valPtr, ret);
        state->bypassFunction(1);
        throw CpuExitException();
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NtoskrnlHandlers::KeStallExecutionProcessor(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();
    s2e()->getDebugStream(state) << "Bypassing function " << __FUNCTION__ << std::endl;

    state->bypassFunction(1);
    throw CpuExitException();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NtoskrnlHandlers::RtlEqualUnicodeString(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    if (getConsistency(__FUNCTION__) == STRICT) {
        return;
    }

    state->undoCallAndJumpToSymbolic();

    //XXX: local assumes the stuff comes from the registry
    //XXX: local consistency is broken, because each time gets a new symbolic value,
    //disregarding the string.
    if (getConsistency(__FUNCTION__) == OVERAPPROX || getConsistency(__FUNCTION__) == LOCAL) {
        klee::ref<klee::Expr> eax = state->createSymbolicValue(klee::Expr::Int32, __FUNCTION__);
        state->writeCpuRegister(offsetof(CPUState, regs[R_EAX]), eax);
        state->bypassFunction(3);
        throw CpuExitException();
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////////
void NtoskrnlHandlers::RtlAddAccessAllowedAce(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    if (getConsistency(__FUNCTION__) < LOCAL) {
        return;
    }

    state->undoCallAndJumpToSymbolic();

    //Fork one successful state and one failed state (where the function is bypassed)
    std::vector<S2EExecutionState *> states;
    forkStates(state, states, 1, getVariableName(state, __FUNCTION__) + "_failure");

    //Skip the call in the current state
    state->bypassFunction(4);

    state->writeCpuRegister(offsetof(CPUState, regs[R_EAX]),
                            createFailure(state, getVariableName(state, __FUNCTION__) + "_result"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////
void NtoskrnlHandlers::ExAllocatePoolWithTag(S2EExecutionState* state, FunctionMonitorState *fns)
{
    if (!calledFromModule(state)) { return; }
    HANDLER_TRACE_CALL();

    state->undoCallAndJumpToSymbolic();

    bool ok = true;
    uint32_t poolType, size;
    ok &= readConcreteParameter(state, 0, &poolType);
    ok &= readConcreteParameter(state, 1, &size);
    if(!ok) {
        s2e()->getDebugStream(state) << "Can not read pool type and length of memory allocation" << std::endl;
        return;
    }

    if (getConsistency(__FUNCTION__) < LOCAL) {
        //We'll have to grant access to the memory array
        FUNCMON_REGISTER_RETURN_A(state, fns, NtoskrnlHandlers::ExAllocatePoolWithTagRet, poolType, size);
        return;
    }

    //Fork one successful state and one failed state (where the function is bypassed)
    std::vector<S2EExecutionState *> states;
    forkStates(state, states, 1, getVariableName(state, __FUNCTION__) + "_failure");

    //Skip the call in the current state
    state->bypassFunction(4);
    uint32_t failValue = 0;
    state->writeCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &failValue, sizeof(failValue));

    //Register the return handler
    S2EExecutionState *otherState = states[0] == state ? states[1] : states[0];
    FUNCMON_REGISTER_RETURN_A(otherState, m_functionMonitor, NtoskrnlHandlers::ExAllocatePoolWithTagRet, poolType, size);
}

void NtoskrnlHandlers::ExAllocatePoolWithTagRet(S2EExecutionState* state, uint32_t poolType, uint32_t size)
{
    HANDLER_TRACE_RETURN();

    uint32_t address;
    if (!state->readCpuRegisterConcrete(offsetof(CPUState, regs[R_EAX]), &address, sizeof(address))) {
        return;
    }
    if (!address) {
        HANDLER_TRACE_FCNFAILED();
        return;
    }

    //XXX: grant memory access
}


}
}
