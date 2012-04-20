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
 *    Vitaly Chipounov (vitaly.chipounov@epfl.ch)
 *    Volodymyr Kuznetsov (vova.kuznetsov@epfl.ch)
 *
 * All contributors listed in S2E-AUTHORS.
 *
 */

extern "C" {
#include <qemu-common.h>
#include <cpu-all.h>
#include <cpu.h>
#include <exec-all.h>
extern struct CPUX86State *env;
}

#include "S2EExecutor.h"
#include "S2EExecutionState.h"
#include "S2E.h"
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/s2e_qemu.h>

using namespace klee;

namespace s2e {

#define S2E_RAM_OBJECT_DIFF (TARGET_PAGE_BITS - S2E_RAM_OBJECT_BITS)
#ifdef SOFTMMU_CODE_ACCESS
#define READ_ACCESS_TYPE 2
#define ADDR_READ addr_code
#else
#define READ_ACCESS_TYPE 0
#define ADDR_READ addr_read
#endif

//XXX: Fix this
#define CPU_MMU_INDEX 0

//This is an io_write_chkX_mmu function
static void io_write_chk(S2EExecutionState *state,
                             target_phys_addr_t physaddr,
                             ref<Expr> val,
                             target_ulong addr,
                             void *retaddr, Expr::Width width)
{
    target_phys_addr_t origaddr = physaddr;
    MemoryRegion *mr = iotlb_to_region(physaddr);

    physaddr = (physaddr & TARGET_PAGE_MASK) + addr;
    if (mr != &io_mem_ram && mr != &io_mem_rom
        && mr != &io_mem_unassigned
        && mr != &io_mem_notdirty
            && !can_do_io(env)) {
        cpu_io_recompile(env, retaddr);
    }


    env->mem_io_vaddr = addr;
    env->mem_io_pc = (uintptr_t)retaddr;
#ifdef TARGET_WORDS_BIGENDIAN
    #error This is not implemented yet.
#else
    if (s2e_ismemfunc(mr, 1)) {
        uintptr_t pa = s2e_notdirty_mem_write(physaddr);
        state->writeMemory(pa, val, S2EExecutionState::HostAddress);
        return;
    }
#endif

    //XXX: Check if MMIO is symbolic, and add corresponding trace entry
    ConstantExpr* csteVal = dyn_cast<ConstantExpr>(val);
    if (!csteVal) {
        val = g_s2e->getExecutor()->toConstant(*state, val, "concretizing symbolic value written to HW");
        csteVal = dyn_cast<ConstantExpr>(val);
        assert(csteVal);
    }

    //By default, call the original io_write function, which is external
    switch(width){
        case Expr::Int8:  io_writeb_mmu(origaddr, csteVal->getZExtValue(), addr, retaddr); break;
        case Expr::Int16: io_writew_mmu(origaddr, csteVal->getZExtValue(), addr, retaddr); break;
        case Expr::Int32: io_writel_mmu(origaddr, csteVal->getZExtValue(), addr, retaddr); break;
        case Expr::Int64: io_writeq_mmu(origaddr, csteVal->getZExtValue(), addr, retaddr); break;
        default: assert(false);
    }
}

//This is an io_read_chkX_mmu function
static ref<Expr> io_read_chk(S2EExecutionState *state,
                             target_phys_addr_t physaddr,
                             target_ulong addr,
                             void *retaddr, Expr::Width width)
{
    ref<Expr> res;
    target_phys_addr_t origaddr = physaddr;
    MemoryRegion *mr = iotlb_to_region(physaddr);

    target_ulong naddr = (physaddr & TARGET_PAGE_MASK)+addr;
    int isSymb = g_s2e->getCorePlugin()->isMmioSymbolic(naddr, width / 8);;
    std::stringstream ss;
    if (isSymb) {
        //If at least one byte is symbolic, generate a label
        ss << "iommuread_" << hexval(naddr) << "@" << hexval(env->eip);
    }

    //If it is not DMA, then check if it is normal memory
    env->mem_io_pc = (uintptr_t)retaddr;
    if (mr != &io_mem_ram && mr != &io_mem_rom
        && mr != &io_mem_unassigned
        && mr != &io_mem_notdirty
            && !can_do_io(env)) {
        cpu_io_recompile(env, retaddr);
    }

    env->mem_io_vaddr = addr;
    if (s2e_ismemfunc(mr, 0)) {
        uintptr_t pa = s2e_notdirty_mem_write(physaddr);
        if (isSymb) {
            return state->createSymbolicValue(ss.str(), width);
        }
        return state->readMemory(pa, width, S2EExecutionState::HostAddress);
    }

    //By default, call the original io_read function, which is external
    switch(width){
        case Expr::Int8: return  ConstantExpr::create(io_readb_mmu(origaddr, addr, retaddr), width);
        case Expr::Int16: return ConstantExpr::create(io_readw_mmu(origaddr, addr, retaddr), width);
        case Expr::Int32: return ConstantExpr::create(io_readl_mmu(origaddr, addr, retaddr), width);
        case Expr::Int64: return ConstantExpr::create(io_readq_mmu(origaddr, addr, retaddr), width);
        default: assert(false);
    }
}

void S2EExecutor::handle_ldl_mmu(Executor* executor,
                                     ExecutionState* state,
                                     klee::KInstruction* target,
                                     std::vector< ref<Expr> > &args)
{
    assert(args.size() == 2);
    handle_ldstl_mmu(executor, state, target, args, false);
}

void S2EExecutor::handle_stl_mmu(Executor* executor,
                                     ExecutionState* state,
                                     klee::KInstruction* target,
                                     std::vector< ref<Expr> > &args)
{
    assert(args.size() == 3);
    handle_ldstl_mmu(executor, state, target, args, true);
}

/* Replacement for __ldl_mmu / __stl_mmu */
/* Params: ldl: addr, mmu_idx */
/* Params: stl: addr, val, mmu_idx */
void S2EExecutor::handle_ldstl_mmu(Executor* executor,
                                     ExecutionState* state,
                                     klee::KInstruction* target,
                                     std::vector< ref<Expr> > &args,
                                     bool isWrite)
{
    S2EExecutionState *s2estate = static_cast<S2EExecutionState*>(state);
    S2EExecutor* s2eExecutor = static_cast<S2EExecutor*>(executor);

    ref<Expr> symbAddress = args[0];
    unsigned mmu_idx = dyn_cast<ConstantExpr>(args[isWrite ? 2 : 1])->getZExtValue();

    //XXX: for now, concretize symbolic addresses
    ref<ConstantExpr> constantAddress = dyn_cast<ConstantExpr>(symbAddress);
    if (constantAddress.isNull()) {
        constantAddress = executor->toConstant(*state, symbAddress, "handle_ldl_mmu symbolic address");
        constantAddress = dyn_cast<ConstantExpr>(args[0]);
        assert(!constantAddress.isNull());
    }

    //XXX: determine this by looking at the instruction that called us
    unsigned data_size = 4;
    Expr::Width width = data_size * 8;

    target_ulong addr = constantAddress->getZExtValue();
    int object_index, index;
    ref<Expr> value;
    target_ulong tlb_addr, addr1, addr2;
    target_phys_addr_t addend, ioaddr;
    void *retaddr = NULL;

    if (isWrite) {
        value = args[1];
    }

    object_index = addr >> S2E_RAM_OBJECT_BITS;
    index = (object_index >> S2E_RAM_OBJECT_DIFF) & (CPU_TLB_SIZE - 1);

    redo:

    if (isWrite) {
        tlb_addr = env->tlb_table[mmu_idx][index].addr_write;
    } else {
        tlb_addr = env->tlb_table[mmu_idx][index].ADDR_READ;
    }

    if (likely((addr & TARGET_PAGE_MASK) == (tlb_addr & (TARGET_PAGE_MASK | TLB_INVALID_MASK)))) {
        if (unlikely(tlb_addr & ~TARGET_PAGE_MASK)) {
           /* IO access */
            if ((addr & (data_size - 1)) != 0)
                goto do_unaligned_access;

            ioaddr = env->iotlb[mmu_idx][index];

            if (!isWrite)
                value = io_read_chk(s2estate, ioaddr, addr, retaddr, width);

            //Trace the access
            std::vector<ref<Expr> > traceArgs;
            traceArgs.push_back(symbAddress);
            traceArgs.push_back(ConstantExpr::create(addr + ioaddr, Expr::Int64));
            traceArgs.push_back(value);
            traceArgs.push_back(ConstantExpr::create(isWrite, Expr::Int64)); //isWrite
            traceArgs.push_back(ConstantExpr::create(1, Expr::Int64)); //isIO
            handlerTraceMemoryAccess(executor, state, target, args);

           if (isWrite)
               io_write_chk(s2estate, ioaddr, value, addr, retaddr, width);

        } else if (unlikely(((addr & ~S2E_RAM_OBJECT_MASK) + data_size - 1) >= S2E_RAM_OBJECT_SIZE)) {
            /* slow unaligned access (it spans two pages or IO) */
        do_unaligned_access:
            addr1 = addr & ~(data_size - 1);
            addr2 = addr1 + data_size;

            std::vector<ref<Expr> > traceArgs;
            traceArgs.push_back(ConstantExpr::create(addr1, Expr::Int64));

            if (isWrite)
                traceArgs.push_back(value);

            traceArgs.push_back(ConstantExpr::create(mmu_idx, Expr::Int64));
            handle_ldstl_mmu(executor, state, target, args, isWrite);

            traceArgs[0] = ConstantExpr::create(addr2, Expr::Int64);
            handle_ldstl_mmu(executor, state, target, args, isWrite);
       } else {
            /* unaligned/aligned access in the same page */
#ifdef ALIGNED_ONLY
            if ((addr & (DATA_SIZE - 1)) != 0) {
                do_unaligned_access(ENV_VAR addr, READ_ACCESS_TYPE, mmu_idx, retaddr);
            }
#endif
            addend = env->tlb_table[mmu_idx][index].addend;

            if (isWrite) {
                s2estate->writeMemory(addr + addend, value, S2EExecutionState::HostAddress);
            } else {
                value = s2estate->readMemory(addr + addend, width, S2EExecutionState::HostAddress);
            }

            //Trace the access
            std::vector<ref<Expr> > traceArgs;
            traceArgs.push_back(symbAddress);
            traceArgs.push_back(ConstantExpr::create(addr + addend, Expr::Int64));
            traceArgs.push_back(value);
            traceArgs.push_back(ConstantExpr::create(isWrite, Expr::Int64)); //isWrite
            traceArgs.push_back(ConstantExpr::create(0, Expr::Int64)); //isIO
            handlerTraceMemoryAccess(executor, state, target, args);
       }
    } else {
        /* the page is not in the TLB : fill it */
#ifdef ALIGNED_ONLY
        if ((addr & (data_size - 1)) != 0)
            do_unaligned_access(ENV_VAR addr, READ_ACCESS_TYPE, mmu_idx, retaddr);
#endif
        tlb_fill(env, addr, object_index << S2E_RAM_OBJECT_BITS,
                 READ_ACCESS_TYPE, mmu_idx, retaddr);
        goto redo;
    }

    if (!isWrite) {
        s2eExecutor->bindLocal(target, *state, value);
    }
}

/* Replacement for ldl_kernel */
void S2EExecutor::handle_ldl_kernel(Executor* executor,
                                     ExecutionState* state,
                                     klee::KInstruction* target,
                                     std::vector< ref<Expr> > &args)
{
    assert(args.size() == 1);
    handle_ldstl_kernel(executor, state, target, args, false);

}

/* Replacement for stl_kernel */
void S2EExecutor::handle_stl_kernel(Executor* executor,
                                     ExecutionState* state,
                                     klee::KInstruction* target,
                                     std::vector< ref<Expr> > &args)
{
    assert(args.size() == 2);
    handle_ldstl_kernel(executor, state, target, args, true);
}

void S2EExecutor::handle_ldstl_kernel(Executor* executor,
                                     ExecutionState* state,
                                     klee::KInstruction* target,
                                     std::vector< ref<Expr> > &args,
                                     bool isWrite)
{
    S2EExecutionState *s2estate = static_cast<S2EExecutionState*>(state);
    S2EExecutor* s2eExecutor = static_cast<S2EExecutor*>(executor);
    ref<Expr> symbAddress = args[0];
    unsigned mmu_idx = CPU_MMU_INDEX;

    //XXX: for now, concretize symbolic addresses
    ref<ConstantExpr> constantAddress = dyn_cast<ConstantExpr>(symbAddress);
    if (constantAddress.isNull()) {
        constantAddress = executor->toConstant(*state, symbAddress, "handle_ldl_mmu symbolic address");
        constantAddress = dyn_cast<ConstantExpr>(args[0]);
        assert(!constantAddress.isNull());
    }

    //XXX: determine this by looking at the instruction that called us
    unsigned data_size = 4;
    Expr::Width width = data_size * 8;

    target_ulong addr = constantAddress->getZExtValue();
    int object_index, page_index;
    ref<Expr> value;
    uintptr_t physaddr;

    object_index = addr >> S2E_RAM_OBJECT_BITS;
    page_index = (object_index >> S2E_RAM_OBJECT_DIFF) & (CPU_TLB_SIZE - 1);

    //////////////////////////////////////////

    if (isWrite) {
        value = args[1];
    }

    if (unlikely(env->tlb_table[mmu_idx][page_index].ADDR_READ !=
                 (addr & (TARGET_PAGE_MASK | (data_size - 1))))) {

        std::vector<ref<Expr> > slowArgs;


        if (isWrite) {
            slowArgs.push_back(constantAddress);
            slowArgs.push_back(value);
            slowArgs.push_back(ConstantExpr::create(mmu_idx, Expr::Int64));
            handle_stl_mmu(executor, state, target, slowArgs);
        } else {
            slowArgs.push_back(constantAddress);
            slowArgs.push_back(ConstantExpr::create(mmu_idx, Expr::Int64));
            handle_ldl_mmu(executor, state, target, slowArgs);
        }
        return;

    } else {
        //When we get here, the address is aligned with the size of the access,
        //which by definition means that it will fall inside the small page, without overflowing.
        physaddr = addr + env->tlb_table[mmu_idx][page_index].addend;


        if (isWrite) {
            s2estate->writeMemory(physaddr, value, S2EExecutionState::HostAddress);
        } else {
            value = s2estate->readMemory(physaddr, width, S2EExecutionState::HostAddress);
            s2eExecutor->bindLocal(target, *state, value);
        }

        //Trace the access
        std::vector<ref<Expr> > traceArgs;
        traceArgs.push_back(constantAddress);
        traceArgs.push_back(ConstantExpr::create(physaddr, Expr::Int64));
        traceArgs.push_back(value);
        traceArgs.push_back(ConstantExpr::create(0, Expr::Int64)); //isWrite
        traceArgs.push_back(ConstantExpr::create(0, Expr::Int64)); //isIO
        handlerTraceMemoryAccess(executor, state, target, args);
    }
}

}
