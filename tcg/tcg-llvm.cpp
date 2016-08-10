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
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *
 * All contributors are listed in the S2E-AUTHORS file.
 */

extern "C" {
#include "tcg.h"
}

#include "s2e/target/tcg-llvm.h"
#include "s2e/cxx/TCGLLVMContext.h"
#include "s2e/TCGLLVMRuntime.h"
#include "s2e/cxx/S2EExecutor.h"

extern "C" {
#include "config.h"
#include "qemu-common.h"
#include "disas/disas.h"

#if defined(CONFIG_SOFTMMU)
//TODO: What about endianness? Is it correct to always use little endian here as long as the host is le?
static void *qemu_ld_helpers[5] = {
    (void*) helper_ret_ldub_mmu,
    (void*) helper_le_lduw_mmu,
    (void*) helper_le_ldul_mmu,
    (void*) helper_le_ldq_mmu,
    (void*) helper_le_ldq_mmu,
};

static void *qemu_st_helpers[5] = {
    (void*) helper_ret_stb_mmu,
    (void*) helper_le_stw_mmu,
    (void*) helper_le_stl_mmu,
    (void*) helper_le_stq_mmu,
    (void*) helper_le_stq_mmu,
};

#endif

}

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/JITMemoryManager.h>
#include <llvm/ExecutionEngine/JIT.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/DynamicLibrary.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/Threading.h>

#include <iostream>
#include <sstream>

//#undef NDEBUG

TCGLLVMContext* tcg_llvm_ctx = 0;

/* These data is accessible from generated code */
TCGLLVMRuntime tcg_llvm_runtime = {
    0, 0, {0,0,0}
    , 0
};

using llvm::legacy::FunctionPassManager;
using llvm::LLVMContext;
using llvm::IRBuilder;
using llvm::ExecutionEngine;
using llvm::Module;
using llvm::Function;
using llvm::BasicBlock;
using llvm::Value;
using llvm::getGlobalContext;
using llvm::InitializeNativeTarget;
using llvm::GlobalValue;
using llvm::DataLayout;
using llvm::createReassociatePass;
using llvm::createDeadStoreEliminationPass;
using llvm::createPromoteMemoryToRegisterPass;
using llvm::createCFGSimplificationPass;
using llvm::createGVNPass;
using llvm::createInstructionCombiningPass;
using llvm::createConstantPropagationPass;
using llvm::JITMemoryManager;
using llvm::IntegerType;
using llvm::ConstantInt;
using llvm::PointerType;
using llvm::Constant;
using llvm::toStringRef;
using llvm::SmallVector;
using llvm::FunctionType;
using llvm::sys::DynamicLibrary;
using llvm::ArrayRef;
using llvm::StringRef;
using llvm::APInt;
using llvm::SMDiagnostic;
using llvm::ParseIR;
using llvm::MemoryBuffer;
using llvm::ReturnInst;

namespace Intrinsic = llvm::Intrinsic;


class TJITMemoryManager;

class TCGLLVMContextPrivate {
    friend class TCGLLVMContext;
    LLVMContext& m_context;
    IRBuilder<> m_builder;

    /* Current m_module */
    Module *m_module;

    /* JIT engine */
    TJITMemoryManager *m_jitMemoryManager;
    ExecutionEngine *m_executionEngine;

    /* Function pass manager (used for optimizing the code) */
    FunctionPassManager *m_functionPassManager;

#ifdef CONFIG_S2E
    /* Declaration of a wrapper function for helpers */
    Function *m_helperTraceMemoryAccess;
    Function *m_helperTraceInstruction;
    Function *m_helperForkAndConcretize;
    Function *m_helperMakeSymbolic;
    Function *m_helperGetValue;
    Function* m_qemu_ld_helpers[4];
    Function* m_qemu_st_helpers[4];
#endif

    /* Count of generated translation blocks */
    int m_tbCount;

    /* XXX: The following members are "local" to generateCode method */

    /* TCGContext for current translation block */
    TCGContext* m_tcgContext;

    /* Function for current translation block */
    Function *m_tbFunction;

    /* Current temp m_values */
    Value* m_values[TCG_MAX_TEMPS];

    /* Pointers to in-memory versions of globals or local temps */
    Value* m_memValuesPtr[TCG_MAX_TEMPS];

    /* For reg-based globals, store argument number,
     * for mem-based globals, store base value index */
    int m_globalsIdx[TCG_MAX_TEMPS];

    std::map<TCGLabel *, BasicBlock*> m_labels;

public:
    TCGLLVMContextPrivate();
    ~TCGLLVMContextPrivate();

    void deleteExecutionEngine() {
        if (m_executionEngine) {
            delete m_executionEngine;
            m_executionEngine = NULL;
        }
    }

    FunctionPassManager *getFunctionPassManager() const {
        return m_functionPassManager;
    }

    /* Shortcuts */
    llvm::Type* intType(int w) { return IntegerType::get(m_context, w); }
    llvm::Type* intPtrType(int w) { return PointerType::get(intType(w), 0); }
    llvm::Type* wordType() { return intType(TCG_TARGET_REG_BITS); }
    llvm::Type* wordType(int bits) { return intType(bits); }
    llvm::Type* wordPtrType() { return intPtrType(TCG_TARGET_REG_BITS); }

    void adjustTypeSize(unsigned target, Value **v1) {
        Value *va = *v1;
        if (target == 32) {
            if (va->getType() == intType(64)) {
                *v1 = m_builder.CreateTrunc(va, intType(target));
            } else if (va->getType() != intType(32)) {
                assert(false);
            }
        }
    }

    //This handles the special case of symbolic values
    //assigned to the program counter register
    Value* handleSymbolicPcAssignment(Value *orig) {
#ifdef CONFIG_S2E
        if (isa<ConstantInt>(orig)) {
            return orig;
        }

        Value *valueToStore = m_builder.CreateCall3(m_helperForkAndConcretize,
                            m_builder.CreateZExt(orig, intType(64)),
                            ConstantInt::get(intType(64), 0),
                            ConstantInt::get(intType(64), (target_ulong)-1));
        valueToStore = m_builder.CreateTrunc(valueToStore,
                                             intType(TARGET_LONG_BITS));
        return valueToStore;
#else
        return orig;
#endif
    }

    void adjustTypeSize(unsigned target, Value **v1, Value **v2) {
        adjustTypeSize(target, v1);
        adjustTypeSize(target, v2);
    }

    llvm::Type* tcgType(int type) {
        return type == TCG_TYPE_I64 ? intType(64) : intType(32);
    }

    llvm::Type* tcgPtrType(int type) {
        return type == TCG_TYPE_I64 ? intPtrType(64) : intPtrType(32);
    }

    /* Helpers */
    Value* getValue(int idx);
    void setValue(int idx, Value *v);
    void delValue(int idx);

    Value* getPtrForValue(int idx);
    void delPtrForValue(int idx);
    void initGlobalsAndLocalTemps();
    unsigned getValueBits(int idx);

    void invalidateCachedMemory();

    uint64_t toInteger(Value *v) const {
        if (ConstantInt *cste = dyn_cast<ConstantInt>(v)) {
            return *cste->getValue().getRawData();
        }
        llvm::errs() << *v << '\n';
        assert(false && "Not a constant");
    }
#ifdef CONFIG_S2E
    void initializeHelpers();
#endif

    BasicBlock* getLabel(TCGLabel *lbl);
    void delLabel(TCGLabel *lbl);
    void startNewBasicBlock(BasicBlock *bb = NULL);

    /* Code generation */
    Value* generateQemuMemOp(bool ld, Value *value, Value *addr,
                             int mem_index, int bits);

    void generateTraceCall(uintptr_t pc);
    Value* generateCondition(TCGCond cond, Value *arg1, Value *arg2);
    void generateOperation(TCGOp* op, const TCGArg *args);

    void generateCode(TCGContext *s, TranslationBlock *tb);
};

/* Custom JITMemoryManager in order to capture the size of
 * the last generated function */
class TJITMemoryManager: public JITMemoryManager {
    JITMemoryManager* m_base;
    ptrdiff_t m_lastFunctionSize;
public:
    TJITMemoryManager():
        m_base(JITMemoryManager::CreateDefaultMemManager()),
        m_lastFunctionSize(0) {}
    ~TJITMemoryManager() { delete m_base; }

    ptrdiff_t getLastFunctionSize() const { return m_lastFunctionSize; }

    uint8_t *startFunctionBody(const Function *F, uintptr_t &ActualSize) {
        m_lastFunctionSize = 0;
        return m_base->startFunctionBody(F, ActualSize);
    }
    void endFunctionBody(const Function *F, uint8_t *FunctionStart,
                                uint8_t *FunctionEnd) {
        m_lastFunctionSize = FunctionEnd - FunctionStart;
        m_base->endFunctionBody(F, FunctionStart, FunctionEnd);
    }

    void setMemoryWritable() { m_base->setMemoryWritable(); }
    void setMemoryExecutable() { m_base->setMemoryExecutable(); }
    void setPoisonMemory(bool poison) { m_base->setPoisonMemory(poison); }
    void AllocateGOT() { m_base->AllocateGOT(); }
    uint8_t *getGOTBase() const { return m_base->getGOTBase(); }
    //void SetDlsymTable(void *ptr) { m_base->SetDlsymTable(ptr); }
    //void *getDlsymTable() const { return m_base->getDlsymTable(); }
    uint8_t *allocateStub(const GlobalValue* F, unsigned StubSize,
                                unsigned Alignment) {
        return m_base->allocateStub(F, StubSize, Alignment);
    }
    uint8_t *allocateSpace(intptr_t Size, unsigned Alignment) {
        return m_base->allocateSpace(Size, Alignment);
    }
    uint8_t *allocateGlobal(uintptr_t Size, unsigned Alignment) {
        return m_base->allocateGlobal(Size, Alignment);
    }
    uint8_t *allocateCodeSection(uintptr_t Size, unsigned Alignment,
                                 unsigned SectionID,
								 StringRef SectionName) override {
        return m_base->allocateCodeSection(Size,
        		Alignment, SectionID, SectionName);
    }
    uint8_t *allocateDataSection(uintptr_t Size, unsigned Alignment,
                                 unsigned SectionID,
								 StringRef SectionName,
								 bool isReadOnly) override {
        return m_base->allocateDataSection(Size,
        		Alignment, SectionID, SectionName, isReadOnly);
    }
    void *getPointerToNamedFunction(const std::string &Name,
                                    bool AbortOnFailure = true) {
        return m_base->getPointerToNamedFunction(Name, AbortOnFailure);
    }

    void deallocateFunctionBody(void *Body) override {
        m_base->deallocateFunctionBody(Body);
    }

    bool finalizeMemory(std::string *ErrMsg) override {
    	return m_base->finalizeMemory(ErrMsg);
    }

    bool CheckInvariants(std::string &ErrorStr) override {
            return m_base->CheckInvariants(ErrorStr);
    }

    size_t GetDefaultCodeSlabSize() {
        return m_base->GetDefaultCodeSlabSize();
    }
    size_t GetDefaultDataSlabSize() {
        return m_base->GetDefaultDataSlabSize();
    }
    size_t GetDefaultStubSlabSize() {
        return m_base->GetDefaultStubSlabSize();
    }
    unsigned GetNumCodeSlabs() { return m_base->GetNumCodeSlabs(); }
    unsigned GetNumDataSlabs() { return m_base->GetNumDataSlabs(); }
    unsigned GetNumStubSlabs() { return m_base->GetNumStubSlabs(); }
};

///Created by objcopy when the binary contents of op_helper.bc is copied into an object file
extern "C" const char _binary_op_helpers_bca_start;
///Created by objcopy when the binary contents of op_helper.bc is copied into an object file
extern "C" const char _binary_op_helpers_bca_end;

llvm::Module* TCGLLVMContext::getCompiledBitcode(llvm::LLVMContext& context)
{
	SMDiagnostic smerror;
	Module* module  = ParseIR(
	        MemoryBuffer::getMemBuffer(
	            StringRef(&_binary_op_helpers_bca_start, &_binary_op_helpers_bca_end - &_binary_op_helpers_bca_start),
	            "op_helpers.bc",
	            false),
	        smerror,
			context);
	//TODO: Check for errors
	return module;
}


TCGLLVMContextPrivate::TCGLLVMContextPrivate()
    : m_context(getGlobalContext()), m_builder(m_context), m_tbCount(0),
      m_tcgContext(NULL), m_tbFunction(NULL)
{
    std::memset(m_values, 0, sizeof(m_values));
    std::memset(m_memValuesPtr, 0, sizeof(m_memValuesPtr));
    std::memset(m_globalsIdx, 0, sizeof(m_globalsIdx));

    InitializeNativeTarget();

    m_module = TCGLLVMContext::getCompiledBitcode(m_context);

    m_jitMemoryManager = new TJITMemoryManager();

    std::string error;
    m_executionEngine = ExecutionEngine::createJIT(
            m_module, &error, m_jitMemoryManager);
    if(m_executionEngine == NULL) {
        std::cerr << "Unable to create LLVM JIT: " << error << std::endl;
        exit(1);
    }

    m_functionPassManager = new FunctionPassManager(m_module);
    m_functionPassManager->add(
            new DataLayout(*m_executionEngine->getDataLayout()));

    m_functionPassManager->add(createReassociatePass());
    m_functionPassManager->add(createConstantPropagationPass());
    m_functionPassManager->add(createInstructionCombiningPass());
    m_functionPassManager->add(createGVNPass());
    m_functionPassManager->add(createDeadStoreEliminationPass());
    m_functionPassManager->add(createCFGSimplificationPass());
    m_functionPassManager->add(createPromoteMemoryToRegisterPass());

    //m_functionPassManager->add(new SelectRemovalPass());

    m_functionPassManager->doInitialization();
}

TCGLLVMContextPrivate::~TCGLLVMContextPrivate()
{
    delete m_functionPassManager;

    // the following line will also delete
    // m_moduleProvider, m_module and all its functions
    if (m_executionEngine) {
        delete m_executionEngine;
    }
}

#ifdef CONFIG_S2E
void TCGLLVMContextPrivate::initializeHelpers()
{
    llvm::errs() << "WARN - " << __FILE__ << ":" << __LINE__ << ": stubbed" << '\n';

//    m_helperTraceMemoryAccess =
//            m_module->getFunction("tcg_llvm_trace_memory_access");
//
//    m_helperTraceInstruction =
//            m_module->getFunction("tcg_llvm_trace_instruction");
//
//    m_helperForkAndConcretize =
//            m_module->getFunction("tcg_llvm_fork_and_concretize");
//
//    m_helperMakeSymbolic =
//            m_module->getFunction("tcg_llvm_make_symbolic");
//    m_helperGetValue =
//            m_module->getFunction("tcg_llvm_get_value");
//
    //TODO: This only references LE helpers, what if the target is BE?
    static const char* mmu_ld_helpers[] = {
    	"helper_ret_ldub_mmu",
		"helper_le_lduw_mmu",
		"helper_le_ldul_mmu",
		"helper_le_ldq_mmu"
    };

    for (unsigned i = 0; i < sizeof(mmu_ld_helpers) / sizeof(mmu_ld_helpers[0]); ++i) {
    	m_qemu_ld_helpers[i] = m_module->getFunction(mmu_ld_helpers[i]);
    }

    //TODO: This only references LE helpers, what if the target is BE?
    static const char* mmu_st_helpers[] = {
		"helper_ret_stb_mmu",
		"helper_le_stw_mmu",
		"helper_le_stl_mmu",
		"helper_le_stq_mmu"
	};

    for (unsigned i = 0; i < sizeof(mmu_st_helpers) / sizeof(mmu_st_helpers[0]); ++i) {
		m_qemu_st_helpers[i] = m_module->getFunction(mmu_st_helpers[i]);
	}

//    m_qemu_ld_helpers[0] = m_module->getFunction("__ldb_mmu");
//    m_qemu_ld_helpers[1] = m_module->getFunction("__ldw_mmu");
//    m_qemu_ld_helpers[2] = m_module->getFunction("__ldl_mmu");
//    m_qemu_ld_helpers[3] = m_module->getFunction("__ldq_mmu");
//    m_qemu_ld_helpers[4] = m_module->getFunction("__ldq_mmu");
//
//    m_qemu_st_helpers[0] = m_module->getFunction("__stb_mmu");
//    m_qemu_st_helpers[1] = m_module->getFunction("__stw_mmu");
//    m_qemu_st_helpers[2] = m_module->getFunction("__stl_mmu");
//    m_qemu_st_helpers[3] = m_module->getFunction("__stq_mmu");
//    m_qemu_st_helpers[4] = m_module->getFunction("__stq_mmu");
//
//    assert(m_helperTraceMemoryAccess);
//// XXX: is this really not needed on ARM?
//#ifndef TARGET_ARM
//    assert(m_helperMakeSymbolic);
//#endif
//    assert(m_helperGetValue);
//    for(int i = 0; i < 5; ++i) {
//        assert(m_qemu_ld_helpers[i]);
//        assert(m_qemu_st_helpers[i]);
//    }
}
#endif

Value* TCGLLVMContextPrivate::getPtrForValue(int idx)
{
    TCGContext *s = m_tcgContext;
    TCGTemp &temp = s->temps[idx];

    assert(idx < s->nb_globals || s->temps[idx].temp_local);
    
    if(m_memValuesPtr[idx] == NULL) {
        assert(idx < s->nb_globals);

        if(temp.fixed_reg) {
            Value *v = m_builder.CreateConstGEP1_32(
                    m_tbFunction->arg_begin(), m_globalsIdx[idx]);
            m_memValuesPtr[idx] = m_builder.CreatePointerCast(
                    v, tcgPtrType(temp.type)
#ifndef NDEBUG
                    , StringRef(temp.name) + "_ptr"
#endif
                    );

        } else {
            Value *v = getValue(m_globalsIdx[idx]);
            assert(v->getType() == wordType());

            v = m_builder.CreateAdd(v, ConstantInt::get(
                            wordType(), temp.mem_offset));
            m_memValuesPtr[idx] =
                m_builder.CreateIntToPtr(v, tcgPtrType(temp.type)
#ifndef NDEBUG
                        , StringRef(temp.name) + "_ptr"
#endif
                        );
        }
    }

    return m_memValuesPtr[idx];
}

inline void TCGLLVMContextPrivate::delValue(int idx)
{
    /* XXX
    if(m_values[idx] && m_values[idx]->use_empty()) {
        if(!isa<Instruction>(m_values[idx]) ||
                !cast<Instruction>(m_values[idx])->getParent())
            delete m_values[idx];
    }
    */
    m_values[idx] = NULL;
}

inline void TCGLLVMContextPrivate::delPtrForValue(int idx)
{
    /* XXX
    if(m_memValuesPtr[idx] && m_memValuesPtr[idx]->use_empty()) {
        if(!isa<Instruction>(m_memValuesPtr[idx]) ||
                !cast<Instruction>(m_memValuesPtr[idx])->getParent())
            delete m_memValuesPtr[idx];
    }
    */
    m_memValuesPtr[idx] = NULL;
}

unsigned TCGLLVMContextPrivate::getValueBits(int idx)
{
    switch (m_tcgContext->temps[idx].type) {
        case TCG_TYPE_I32: return 32;
        case TCG_TYPE_I64: return 64;
        default: assert(false && "Unknown size");
    }
    return 0;
}

Value* TCGLLVMContextPrivate::getValue(int idx)
{
	assert(idx < TCG_MAX_TEMPS && "Invalid value index");
    if(m_values[idx] == NULL) {
        if(idx < m_tcgContext->nb_globals) {
            m_values[idx] = m_builder.CreateLoad(getPtrForValue(idx)
#ifndef NDEBUG
                    , StringRef(m_tcgContext->temps[idx].name) + "_v"
#endif
                    );
        } else if(m_tcgContext->temps[idx].temp_local) {
            m_values[idx] = m_builder.CreateLoad(getPtrForValue(idx));
#ifndef NDEBUG
            std::ostringstream name;
            name << "loc" << (idx - m_tcgContext->nb_globals) << "_v";
            m_values[idx]->setName(name.str());
#endif
        } else {
            // Temp value was not previousely assigned
            assert(false); // XXX: or return zero constant ?
        }
    }

    return m_values[idx];
}

void TCGLLVMContextPrivate::setValue(int idx, Value *v)
{
    delValue(idx);
    m_values[idx] = v;

    if(!v->hasName() && !isa<Constant>(v)) {
#ifndef NDEBUG
        if(idx < m_tcgContext->nb_globals)
            v->setName(StringRef(m_tcgContext->temps[idx].name) + "_v");
        if(m_tcgContext->temps[idx].temp_local) {
            std::ostringstream name;
            name << "loc" << (idx - m_tcgContext->nb_globals) << "_v";
            v->setName(name.str());
        } else {
            std::ostringstream name;
            name << "tmp" << (idx - m_tcgContext->nb_globals) << "_v";
            v->setName(name.str());
        }
#endif
    }

    if(idx < m_tcgContext->nb_globals) {
        // We need to save a global copy of a value
        m_builder.CreateStore(v, getPtrForValue(idx));

        if(m_tcgContext->temps[idx].fixed_reg) {
            /* Invalidate all dependent global vals and pointers */
            for(int i=0; i<m_tcgContext->nb_globals; ++i) {
                if(i != idx && !m_tcgContext->temps[idx].fixed_reg &&
                                    m_globalsIdx[i] == idx) {
                    delValue(i);
                    delPtrForValue(i);
                }
            }
        }
    } else if(m_tcgContext->temps[idx].temp_local) {
        // We need to save an in-memory copy of a value
        m_builder.CreateStore(v, getPtrForValue(idx));
    }
}

void TCGLLVMContextPrivate::initGlobalsAndLocalTemps()
{
    TCGContext *s = m_tcgContext;

    int reg_to_idx[TCG_TARGET_NB_REGS];
    for(int i=0; i<TCG_TARGET_NB_REGS; ++i)
        reg_to_idx[i] = -1;

    int argNumber = 0;
    for(int i=0; i<s->nb_globals; ++i) {
        if(s->temps[i].fixed_reg) {
            // This global is in fixed host register. We are
            // mapping such registers to function arguments
            m_globalsIdx[i] = argNumber++;
            reg_to_idx[s->temps[i].reg] = i;

        } else {
            // This global is in memory at (mem_reg + mem_offset).
            // Base value is not known yet, so just store mem_reg
            m_globalsIdx[i] = s->temps[i].mem_reg;
        }
    }

    // Map mem_reg to index for memory-based globals
    for(int i=0; i<s->nb_globals; ++i) {
        if(!s->temps[i].fixed_reg) {
            assert(reg_to_idx[m_globalsIdx[i]] >= 0);
            m_globalsIdx[i] = reg_to_idx[m_globalsIdx[i]];
        }
    }

    // Allocate local temps
    for(int i=s->nb_globals; i<TCG_MAX_TEMPS; ++i) {
        if(s->temps[i].temp_local) {
            std::ostringstream pName;
            pName << "loc_" << (i - s->nb_globals) << "ptr";
            m_memValuesPtr[i] = m_builder.CreateAlloca(
                tcgType(s->temps[i].type), 0, pName.str());
        }
    }
}

inline BasicBlock* TCGLLVMContextPrivate::getLabel(TCGLabel *lbl)
{
    if (m_labels.find(lbl) == m_labels.end()) 
    {
        std::ostringstream bbName;
        bbName << "label_" << lbl->id;
        m_labels.insert(std::make_pair(lbl, BasicBlock::Create(m_context, bbName.str())));
    }
    return m_labels[lbl];
}

inline void TCGLLVMContextPrivate::delLabel(TCGLabel *lbl)
{
    m_labels.erase(lbl);
}

void TCGLLVMContextPrivate::startNewBasicBlock(BasicBlock *bb)
{
    if(!bb)
        bb = BasicBlock::Create(m_context);
    else
        assert(bb->getParent() == 0);

    if(!m_builder.GetInsertBlock()->getTerminator())
        m_builder.CreateBr(bb);

    m_tbFunction->getBasicBlockList().push_back(bb);
    m_builder.SetInsertPoint(bb);

    /* Invalidate all temps */
    for(int i=0; i<TCG_MAX_TEMPS; ++i)
        delValue(i);

    /* Invalidate all pointers to globals */
    for(int i=0; i<m_tcgContext->nb_globals; ++i)
        delPtrForValue(i);
}

inline Value* TCGLLVMContextPrivate::generateQemuMemOp(bool ld,
        Value *value, Value *addr, int mem_index, int bits)
{
    assert(addr->getType() == intType(TARGET_LONG_BITS));
    assert(ld || value->getType() == intType(bits));
    assert(TCG_TARGET_REG_BITS == 64); //XXX

#ifdef CONFIG_SOFTMMU
    Value* tbArg = cast<Value>(m_builder.GetInsertBlock()->getParent()->arg_begin());
    Value* env = m_builder.CreateLoad(m_builder.CreateConstGEP1_32(tbArg, 0));
    Function* func;

    if (ld) {
    	switch (bits) {
    		case 8: func = m_qemu_ld_helpers[0]; break;
    		case 16: func = m_qemu_ld_helpers[1]; break;
    		case 32: func = m_qemu_ld_helpers[2]; break;
    		case 64: func = m_qemu_ld_helpers[3]; break;
    		default: abort();
    	}
    }
    else {
		switch (bits) {
			case 8: func = m_qemu_st_helpers[0]; break;
			case 16: func = m_qemu_st_helpers[1]; break;
			case 32: func = m_qemu_st_helpers[2]; break;
			case 64: func = m_qemu_st_helpers[3]; break;
			default: abort();
		}
    }

    SmallVector<Value*, 5> args;
    Function::arg_iterator ldstArg = func->arg_begin();
    args.push_back(m_builder.CreateIntToPtr(env, ldstArg->getType()));
    ++ldstArg;
    assert(addr->getType() == ldstArg->getType() && "Mismatch between type in generated code and helper arg");
    args.push_back(addr);

    if (!ld) {
       assert(value->getType() == ldstArg->getType() && "Mismatch between value type in generated type and helper arg");
       ++ldstArg;
       args.push_back(value);
    }

    ++ldstArg;
    args.push_back(ConstantInt::get(ldstArg->getType(), mem_index));
    ++ldstArg;
    args.push_back(ConstantInt::get(ldstArg->getType(), 0));

    Value* result = m_builder.CreateCall(func, args);

    if (ld) {
    	return result;
    }
    else {
    	return NULL;
    }
#else // CONFIG_SOFTMMU
#error Adapt code for new Qemu
    assert(false && "Not adapted to new Qemu");
    addr = m_builder.CreateZExt(addr, wordType());
    addr = m_builder.CreateAdd(addr,
        ConstantInt::get(wordType(), GUEST_BASE));
    addr = m_builder.CreateIntToPtr(addr, intPtrType(bits));
    if(ld) {
        return m_builder.CreateLoad(addr);
    } else {
        m_builder.CreateStore(value, addr);
        return NULL;
    }
#endif // CONFIG_SOFTMMU
}

void TCGLLVMContextPrivate::generateTraceCall(uintptr_t pc)
{
#ifdef CONFIG_S2E
#if 0
    if (pc == 0x000f56e7 || pc == 0x000f52ff || pc == 0xf530c) {
         m_builder.CreateCall(m_helperTraceInstruction);
    }
#endif
#endif
}

Value *TCGLLVMContextPrivate::generateCondition(TCGCond cond, Value *arg1, Value *arg2)
{
    switch (cond)
    {
    case TCG_COND_NEVER:  return ConstantInt::getFalse(m_builder.getContext());
    case TCG_COND_ALWAYS: return ConstantInt::getTrue(m_builder.getContext());
    case TCG_COND_EQ:     return m_builder.CreateICmpEQ(arg1, arg2);
    case TCG_COND_NE:     return m_builder.CreateICmpNE(arg1, arg2);
            /* signed */
    case TCG_COND_LT:     return m_builder.CreateICmpSLT(arg1, arg2);
    case TCG_COND_GE:     return m_builder.CreateICmpSGE(arg1, arg2);
    case TCG_COND_LE:     return m_builder.CreateICmpSLE(arg1, arg2);
    case TCG_COND_GT:     return m_builder.CreateICmpSGT(arg1, arg2);
            /* unsigned */
    case TCG_COND_LTU:    return m_builder.CreateICmpULT(arg1, arg2);
    case TCG_COND_GEU:    return m_builder.CreateICmpUGE(arg1, arg2);
    case TCG_COND_LEU:    return m_builder.CreateICmpULE(arg1, arg2);
    case TCG_COND_GTU:    return m_builder.CreateICmpUGT(arg1, arg2);
    default:
        llvm::errs() << "ERROR: Unknown condition code " << cond << " in TCG code" << '\n';
        tcg_abort();
        return 0;
    }
}

void TCGLLVMContextPrivate::generateOperation(TCGOp* op, const TCGArg *args)
{
    Value *v;
    const TCGOpcode opc = op->opc;
    TCGOpDef &def = tcg_op_defs[opc];

    switch(opc) {
    case INDEX_op_insn_start:
        break;

    /* predefined ops */
    case INDEX_op_discard:
        delValue(args[0]);
        break;

    case INDEX_op_call:
        {
            int nb_oargs = op->callo;
            int nb_iargs = op->calli;

            //int flags = args[nb_oargs + nb_iargs + 1];
            //assert((flags & TCG_CALL_TYPE_MASK) == TCG_CALL_TYPE_STD);

            std::vector<Value*> argValues;
            std::vector<llvm::Type*> argTypes;
            argValues.reserve(nb_iargs);
            argTypes.reserve(nb_iargs);
            for(int i=0; i < nb_iargs; ++i) {
                TCGArg arg = args[nb_oargs + i];
                if(arg != TCG_CALL_DUMMY_ARG) {
                    Value *v = getValue(arg);
                    argValues.push_back(v);
                    argTypes.push_back(v->getType());
                }
            }

            assert(nb_oargs == 0 || nb_oargs == 1);
            llvm::Type* retType = nb_oargs == 0 ?
                llvm::Type::getVoidTy(m_context) : wordType(getValueBits(args[1]));

            Value* result;

            //Generate this in S2E mode
            tcg_target_ulong helperAddrC = args[nb_oargs + nb_iargs];
            assert(helperAddrC);

            const char *helperName = tcg_helper_get_name(m_tcgContext,
                                                            (void*) helperAddrC);
            assert(helperName);

            std::string funcName = std::string("helper_") + helperName;
            Function* helperFunc = m_module->getFunction(funcName);
            if(!helperFunc) {
            	llvm::errs() << "TCGLLVMContextPrivate::generateOperation: Unknown helper function " << funcName
            			<< ", adding to external functions" << '\n';
                helperFunc = Function::Create(
                        FunctionType::get(retType, argTypes, false),
                        Function::PrivateLinkage, funcName, m_module);
                m_executionEngine->addGlobalMapping(helperFunc,
                                                    (void*) helperAddrC);
                /* XXX: Why do we need this ? */
                DynamicLibrary::AddSymbol(funcName, (void*) helperAddrC);
            }

            //Helper function types are compiled with clang and may differ in types
            //tcg-llvm is not strongly types, e.g., all pointers are simply i64s.
            //Cast values to appropriate helper function types when possible.
            Function::arg_iterator helperFuncArg = helperFunc->arg_begin();
            for (unsigned i = 0; i < argValues.size(); ++i) {
            	assert(helperFuncArg != helperFunc->arg_end() &&
            		"Helper function declaration has a different number of arguments from what generated tcg code passes");

            	Value* arg = argValues[i];
            	if (arg->getType() != helperFuncArg->getType()) {
            		if (helperFuncArg->getType()->isPointerTy() && arg->getType()->isIntegerTy()) {
            			//TODO: Assertion should take into account bit width of LLVM pointers
            			assert(sizeof(void *) * 8 == arg->getType()->getIntegerBitWidth() &&
            				"Integer does not have appropriate width to represent a pointer");

            			argValues.at(i) = m_builder.CreateIntToPtr(arg, helperFuncArg->getType(), "arg_" + funcName);
            		}
            		else {
            			llvm::errs() << "Don't know how to convert type " << *arg->getType() << " to type "
            					<< *helperFuncArg->getType() << " for helper function " << funcName
								<< " call" << '\n';
            			assert(false && "Missing argument type conversion for helper function call");
            		}
            	}

            	++helperFuncArg;
            }

            result = m_builder.CreateCall(helperFunc,
                                            ArrayRef<Value*>(argValues));


            /* Invalidate in-memory values because
             * function might have changed them */
            for(int i=0; i<m_tcgContext->nb_globals; ++i)
                delValue(i);

            for(int i=m_tcgContext->nb_globals; i<TCG_MAX_TEMPS; ++i)
                if(m_tcgContext->temps[i].temp_local)
                    delValue(i);

            /* Invalidate all pointers to globals */
            for(int i=0; i<m_tcgContext->nb_globals; ++i)
                delPtrForValue(i);

            if(nb_oargs == 1)
                setValue(args[0], result);
        }
        break;

    case INDEX_op_br:
        m_builder.CreateBr(getLabel(arg_label(args[0])));
        startNewBasicBlock();
        break;

#define __OP_BRCOND_C(tcg_cond, cond)                               \
            case tcg_cond:                                          \
                v = m_builder.CreateICmp ## cond(                   \
                        getValue(args[0]), getValue(args[1]));      \
            break;

#define __OP_BRCOND(opc_name, bits)                                 \
    case opc_name: {                                                \
        assert(getValue(args[0])->getType() == intType(bits));      \
        assert(getValue(args[1])->getType() == intType(bits));      \
        switch(args[2]) {                                           \
            __OP_BRCOND_C(TCG_COND_EQ,   EQ)                        \
            __OP_BRCOND_C(TCG_COND_NE,   NE)                        \
            __OP_BRCOND_C(TCG_COND_LT,  SLT)                        \
            __OP_BRCOND_C(TCG_COND_GE,  SGE)                        \
            __OP_BRCOND_C(TCG_COND_LE,  SLE)                        \
            __OP_BRCOND_C(TCG_COND_GT,  SGT)                        \
            __OP_BRCOND_C(TCG_COND_LTU, ULT)                        \
            __OP_BRCOND_C(TCG_COND_GEU, UGE)                        \
            __OP_BRCOND_C(TCG_COND_LEU, ULE)                        \
            __OP_BRCOND_C(TCG_COND_GTU, UGT)                        \
            default:                                                \
                tcg_abort();                                        \
        }                                                           \
        BasicBlock* bb = BasicBlock::Create(m_context);             \
        m_builder.CreateCondBr(v, getLabel(arg_label(args[3])), bb);           \
        startNewBasicBlock(bb);                                     \
    } break;

    __OP_BRCOND(INDEX_op_brcond_i32, 32)

#if TCG_TARGET_REG_BITS == 64
    __OP_BRCOND(INDEX_op_brcond_i64, 64)
#endif

#undef __OP_BRCOND_C
#undef __OP_BRCOND

    case INDEX_op_set_label:
        assert(getLabel(arg_label(args[0]))->getParent() == 0);
        startNewBasicBlock(getLabel(arg_label(args[0])));
        break;

    case INDEX_op_movi_i32:
        setValue(args[0], ConstantInt::get(intType(32), args[1]));
        break;

    case INDEX_op_mov_i32:
        // Move operation may perform truncation of the value
        assert(getValue(args[1])->getType() == intType(32) ||
                getValue(args[1])->getType() == intType(64));
        setValue(args[0],
                m_builder.CreateTrunc(getValue(args[1]), intType(32)));
        break;

#if TCG_TARGET_REG_BITS == 64
    case INDEX_op_movi_i64:
        setValue(args[0], ConstantInt::get(intType(64), args[1]));
        break;

    case INDEX_op_mov_i64:
        assert(getValue(args[1])->getType() == intType(64));
        setValue(args[0], getValue(args[1]));
        break;
#endif

    /* size extensions */
#define __EXT_OP(opc_name, truncBits, opBits, signE )               \
    case opc_name:                                                  \
        /*                                                          \
        assert(getValue(args[1])->getType() == intType(opBits) ||   \
               getValue(args[1])->getType() == intType(truncBits)); \
        */                                                          \
        setValue(args[0], m_builder.Create ## signE ## Ext(         \
                m_builder.CreateTrunc(                              \
                    getValue(args[1]), intType(truncBits)),         \
                intType(opBits)));                                  \
        break;

    __EXT_OP(INDEX_op_ext8s_i32,   8, 32, S)
    __EXT_OP(INDEX_op_ext8u_i32,   8, 32, Z)
    __EXT_OP(INDEX_op_ext16s_i32, 16, 32, S)
    __EXT_OP(INDEX_op_ext16u_i32, 16, 32, Z)

#if TCG_TARGET_REG_BITS == 64
    __EXT_OP(INDEX_op_ext8s_i64,   8, 64, S)
    __EXT_OP(INDEX_op_ext8u_i64,   8, 64, Z)
    __EXT_OP(INDEX_op_ext16s_i64, 16, 64, S)
    __EXT_OP(INDEX_op_ext16u_i64, 16, 64, Z)
    __EXT_OP(INDEX_op_ext32s_i64, 32, 64, S)
    __EXT_OP(INDEX_op_ext32u_i64, 32, 64, Z)
#endif

#undef __EXT_OP

    /* load/store */
#define __LD_OP(opc_name, memBits, regBits, signE)                  \
    case opc_name:                                                  \
        assert(getValue(args[1])->getType() == wordType());         \
        v = m_builder.CreateAdd(getValue(args[1]),                  \
                    ConstantInt::get(wordType(), args[2]));         \
        v = m_builder.CreateIntToPtr(v, intPtrType(memBits));       \
        v = m_builder.CreateLoad(v);                                \
        setValue(args[0], m_builder.Create ## signE ## Ext(         \
                    v, intType(regBits)));                          \
        break;

#ifdef TARGET_ARM

#define __ST_OP(opc_name, memBits, regBits)                         \
    case opc_name:  {                                                 \
        assert(getValue(args[0])->getType() == intType(regBits));   \
        assert(getValue(args[1])->getType() == wordType());         \
        Value* valueToStore = getValue(args[0]);                    \
                                                                    \
        if (TARGET_LONG_BITS == memBits                             \
            && args[1] == 0                                         \
            && args[2] == offsetof(CPUARMState, regs[15])) {        \
            valueToStore = handleSymbolicPcAssignment(valueToStore);\
        }                                                           \
                                                                    \
        v = m_builder.CreateAdd(getValue(args[1]),                  \
                    ConstantInt::get(wordType(), args[2]));         \
        v = m_builder.CreateIntToPtr(v, intPtrType(memBits));       \
        m_builder.CreateStore(m_builder.CreateTrunc(                \
                valueToStore, intType(memBits)), v);           \
    } break;

#elif defined(TARGET_I386)

#define __ST_OP(opc_name, memBits, regBits)                         \
    case opc_name:  {                                                 \
        assert(getValue(args[0])->getType() == intType(regBits));   \
        assert(getValue(args[1])->getType() == wordType());         \
        Value* valueToStore = getValue(args[0]);                    \
                                                                    \
        if (TARGET_LONG_BITS == memBits                             \
            && args[1] == 0                                         \
            && args[2] == offsetof(CPUX86State, eip)) { \
            valueToStore = handleSymbolicPcAssignment(valueToStore);\
        }                                                           \
                                                                    \
        v = m_builder.CreateAdd(getValue(args[1]),                  \
                    ConstantInt::get(wordType(), args[2]));         \
        v = m_builder.CreateIntToPtr(v, intPtrType(memBits));       \
        m_builder.CreateStore(m_builder.CreateTrunc(                \
                valueToStore, intType(memBits)), v);           \
    } break;

#endif


    __LD_OP(INDEX_op_ld8u_i32,   8, 32, Z)
    __LD_OP(INDEX_op_ld8s_i32,   8, 32, S)
    __LD_OP(INDEX_op_ld16u_i32, 16, 32, Z)
    __LD_OP(INDEX_op_ld16s_i32, 16, 32, S)
    __LD_OP(INDEX_op_ld_i32,    32, 32, Z)

    __ST_OP(INDEX_op_st8_i32,   8, 32)
    __ST_OP(INDEX_op_st16_i32, 16, 32)
    __ST_OP(INDEX_op_st_i32,   32, 32)

#if TCG_TARGET_REG_BITS == 64
    __LD_OP(INDEX_op_ld8u_i64,   8, 64, Z)
    __LD_OP(INDEX_op_ld8s_i64,   8, 64, S)
    __LD_OP(INDEX_op_ld16u_i64, 16, 64, Z)
    __LD_OP(INDEX_op_ld16s_i64, 16, 64, S)
    __LD_OP(INDEX_op_ld32u_i64, 32, 64, Z)
    __LD_OP(INDEX_op_ld32s_i64, 32, 64, S)
    __LD_OP(INDEX_op_ld_i64,    64, 64, Z)

    __ST_OP(INDEX_op_st8_i64,   8, 64)
    __ST_OP(INDEX_op_st16_i64, 16, 64)
    __ST_OP(INDEX_op_st32_i64, 32, 64)
    __ST_OP(INDEX_op_st_i64,   64, 64)
#endif

#undef __LD_OP
#undef __ST_OP

    /* arith */
#define __ARITH_OP(opc_name, op, bits)                              \
    case opc_name: {                                                \
        Value *v1 = getValue(args[1]);                              \
        Value *v2 = getValue(args[2]);                              \
        adjustTypeSize(bits, &v1, &v2);                             \
        assert(v1->getType() == intType(bits));                     \
        assert(v2->getType() == intType(bits));                     \
        setValue(args[0], m_builder.Create ## op(v1, v2));          \
    } break;

#define __ARITH_OP_DIV2(opc_name, signE, bits)                      \
    case opc_name:                                                  \
        assert(getValue(args[2])->getType() == intType(bits));      \
        assert(getValue(args[3])->getType() == intType(bits));      \
        assert(getValue(args[4])->getType() == intType(bits));      \
        v = m_builder.CreateShl(                                    \
                m_builder.CreateZExt(                               \
                    getValue(args[3]), intType(bits*2)),            \
                m_builder.CreateZExt(                               \
                    ConstantInt::get(intType(bits), bits),          \
                    intType(bits*2)));                              \
        v = m_builder.CreateOr(v,                                   \
                m_builder.CreateZExt(                               \
                    getValue(args[2]), intType(bits*2)));           \
        setValue(args[0], m_builder.Create ## signE ## Div(         \
                v, getValue(args[4])));                             \
        setValue(args[1], m_builder.Create ## signE ## Rem(         \
                v, getValue(args[4])));                             \
        break;

#define __ARITH_OP_ROT(opc_name, op1, op2, bits)                    \
    case opc_name:                                                  \
        assert(getValue(args[1])->getType() == intType(bits));      \
        assert(getValue(args[2])->getType() == intType(bits));      \
        v = m_builder.CreateSub(                                    \
                ConstantInt::get(intType(bits), bits),              \
                getValue(args[2]));                                 \
        setValue(args[0], m_builder.CreateOr(                       \
                m_builder.Create ## op1 (                           \
                    getValue(args[1]), getValue(args[2])),          \
                m_builder.Create ## op2 (                           \
                    getValue(args[1]), v)));                        \
        break;

#define __ARITH_OP_I(opc_name, op, i, bits)                         \
    case opc_name:                                                  \
        assert(getValue(args[1])->getType() == intType(bits));      \
        setValue(args[0], m_builder.Create ## op(                   \
                    ConstantInt::get(intType(bits), i),             \
                    getValue(args[1])));                            \
        break;

#define __ARITH_OP_BSWAP(opc_name, sBits, bits)                     \
    case opc_name: {                                                \
        assert(getValue(args[1])->getType() == intType(bits));      \
        llvm::Type* Tys[] = { intType(sBits) };                     \
        Function *bswap = Intrinsic::getDeclaration(m_module,       \
                Intrinsic::bswap, ArrayRef<llvm::Type*>(Tys,1));    \
        v = m_builder.CreateTrunc(getValue(args[1]),intType(sBits));\
        setValue(args[0], m_builder.CreateZExt(                     \
                m_builder.CreateCall(bswap, v), intType(bits)));    \
        } break;


    __ARITH_OP(INDEX_op_add_i32, Add, 32)
    __ARITH_OP(INDEX_op_sub_i32, Sub, 32)
    __ARITH_OP(INDEX_op_mul_i32, Mul, 32)

#ifdef TCG_TARGET_HAS_div_i32
    __ARITH_OP(INDEX_op_div_i32,  SDiv, 32)
    __ARITH_OP(INDEX_op_divu_i32, UDiv, 32)
    __ARITH_OP(INDEX_op_rem_i32,  SRem, 32)
    __ARITH_OP(INDEX_op_remu_i32, URem, 32)
#else
    __ARITH_OP_DIV2(INDEX_op_div2_i32,  S, 32)
    __ARITH_OP_DIV2(INDEX_op_divu2_i32, U, 32)
#endif

    __ARITH_OP(INDEX_op_and_i32, And, 32)
    __ARITH_OP(INDEX_op_or_i32,   Or, 32)
    __ARITH_OP(INDEX_op_xor_i32, Xor, 32)

    __ARITH_OP(INDEX_op_shl_i32,  Shl, 32)
    __ARITH_OP(INDEX_op_shr_i32, LShr, 32)
    __ARITH_OP(INDEX_op_sar_i32, AShr, 32)

    __ARITH_OP_ROT(INDEX_op_rotl_i32, Shl, LShr, 32)
    __ARITH_OP_ROT(INDEX_op_rotr_i32, LShr, Shl, 32)

    __ARITH_OP_I(INDEX_op_not_i32, Xor, (uint64_t) -1, 32)
    __ARITH_OP_I(INDEX_op_neg_i32, Sub, 0, 32)

    __ARITH_OP_BSWAP(INDEX_op_bswap16_i32, 16, 32)
    __ARITH_OP_BSWAP(INDEX_op_bswap32_i32, 32, 32)

#if TCG_TARGET_REG_BITS == 64
    __ARITH_OP(INDEX_op_add_i64, Add, 64)
    __ARITH_OP(INDEX_op_sub_i64, Sub, 64)
    __ARITH_OP(INDEX_op_mul_i64, Mul, 64)

#ifdef TCG_TARGET_HAS_div_i64
    __ARITH_OP(INDEX_op_div_i64,  SDiv, 64)
    __ARITH_OP(INDEX_op_divu_i64, UDiv, 64)
    __ARITH_OP(INDEX_op_rem_i64,  SRem, 64)
    __ARITH_OP(INDEX_op_remu_i64, URem, 64)
#else
    __ARITH_OP_DIV2(INDEX_op_div2_i64,  S, 64)
    __ARITH_OP_DIV2(INDEX_op_divu2_i64, U, 64)
#endif

    __ARITH_OP(INDEX_op_and_i64, And, 64)
    __ARITH_OP(INDEX_op_or_i64,   Or, 64)
    __ARITH_OP(INDEX_op_xor_i64, Xor, 64)

    __ARITH_OP(INDEX_op_shl_i64,  Shl, 64)
    __ARITH_OP(INDEX_op_shr_i64, LShr, 64)
    __ARITH_OP(INDEX_op_sar_i64, AShr, 64)

    __ARITH_OP_ROT(INDEX_op_rotl_i64, Shl, LShr, 64)
    __ARITH_OP_ROT(INDEX_op_rotr_i64, LShr, Shl, 64)

    __ARITH_OP_I(INDEX_op_not_i64, Xor, (uint64_t) -1, 64)
    __ARITH_OP_I(INDEX_op_neg_i64, Sub, 0, 64)

    __ARITH_OP_BSWAP(INDEX_op_bswap16_i64, 16, 64)
    __ARITH_OP_BSWAP(INDEX_op_bswap32_i64, 32, 64)
    __ARITH_OP_BSWAP(INDEX_op_bswap64_i64, 64, 64)
#endif

#undef __ARITH_OP_BSWAP
#undef __ARITH_OP_I
#undef __ARITH_OP_ROT
#undef __ARITH_OP_DIV2
#undef __ARITH_OP

    /* QEMU specific */
#if TCG_TARGET_REG_BITS == 64

#define __OP_QEMU_ST(opc_name, bits)                                \
    case opc_name:                                                  \
        generateQemuMemOp(false,                                    \
            m_builder.CreateIntCast(                                \
                getValue(args[0]), intType(bits), false),           \
            getValue(args[1]), args[2], bits);                      \
        break;


#define __OP_QEMU_LD(opc_name, bits, signE)                         \
    case opc_name:                                                  \
        v = generateQemuMemOp(true, NULL,                           \
            getValue(args[1]), args[2], bits);                      \
        setValue(args[0], m_builder.Create ## signE ## Ext(         \
            v, intType(std::max(TARGET_LONG_BITS, bits))));         \
        break;

#define __OP_QEMU_LDD(opc_name, bits)                               \
    case opc_name:                                                  \
        v = generateQemuMemOp(true, NULL,                           \
            getValue(args[1]), args[2], bits);                      \
        setValue(args[0], v);         \
        break;

    __OP_QEMU_ST(INDEX_op_qemu_st_i32, 32)
    __OP_QEMU_ST(INDEX_op_qemu_st_i64, 64)

    __OP_QEMU_LDD(INDEX_op_qemu_ld_i32, 32)
    __OP_QEMU_LDD(INDEX_op_qemu_ld_i64, 64)


#undef __OP_QEMU_LD
#undef __OP_QEMU_ST
#undef __OP_QEMU_LDD

#endif

    case INDEX_op_exit_tb:
        m_builder.CreateRet(ConstantInt::get(wordType(), args[0]));
        break;

    case INDEX_op_goto_tb:
#ifdef CONFIG_S2E
        m_builder.CreateStore(ConstantInt::get(intType(8), args[0]),
                    m_builder.CreateIntToPtr(ConstantInt::get(wordType(),
                        (uint64_t) &tcg_llvm_runtime.goto_tb),
                    intPtrType(8)));
#endif
        /* XXX: tb linking is disabled */
        break;

    case INDEX_op_deposit_i32: {
        //llvm::errs() << *m_tbFunction << "\n";
        Value *arg1 = getValue(args[1]);
        //llvm::errs() << "arg1=" << *arg1 << "\n";
        //arg1 = m_builder.CreateTrunc(arg1, intType(32));


        Value *arg2 = getValue(args[2]);
        //llvm::errs() << "arg2=" << *arg2 << "\n";

        uint32_t ofs = args[3];
        uint32_t len = args[4];

        if (ofs == 0 && len == 32) {
            setValue(args[0], arg2);
            break;
        }

        uint32_t mask = (1u << len) - 1;
        Value *t1, *ret;
        if (ofs + len < 32) {
            t1 = m_builder.CreateAnd(arg2, APInt(32, mask));
            t1 = m_builder.CreateShl(t1, APInt(32, ofs));
        } else {
            t1 = m_builder.CreateShl(arg2, APInt(32, ofs));
        }

        ret = m_builder.CreateAnd(arg1, APInt(32, ~(mask << ofs)));
        ret = m_builder.CreateOr(ret, t1);
        setValue(args[0], ret);
    }
    break;
#if TCG_TARGET_REG_BITS == 64
    case INDEX_op_deposit_i64: {
        Value *arg1 = getValue(args[1]);
        Value *arg2 = getValue(args[2]);
        arg2 = m_builder.CreateTrunc(arg2, intType(64));

        uint32_t ofs = args[3];
        uint32_t len = args[4];

        if (0 == ofs && 64 == len) {
            setValue(args[0], arg2);
            break;
        }

        uint64_t mask = (1u << len) - 1;
        Value *t1, *ret;

        if (ofs + len < 64) {
            t1 = m_builder.CreateAnd(arg2, APInt(64, mask));
            t1 = m_builder.CreateShl(t1, APInt(64, ofs));
        } else {
            t1 = m_builder.CreateShl(arg2, APInt(64, ofs));
        }

        ret = m_builder.CreateAnd(arg1, APInt(64, ~(mask << ofs)));
        ret = m_builder.CreateOr(ret, t1);
        setValue(args[0], ret);
    }
    break;
#endif
    case INDEX_op_setcond_i32: {
		Value *result = generateCondition(static_cast<TCGCond>(args[3]), getValue(args[1]), getValue(args[2]));
		setValue(args[0], m_builder.CreateSExt(result, intType(32)));
		break;
	}
	case INDEX_op_add2_i32: {
		Value *al = getValue(args[2]);
		Value *ah = getValue(args[3]);
		Value *bl = getValue(args[4]);
		Value *bh = getValue(args[5]);

		Value *a = m_builder.CreateOr(
				m_builder.CreateShl(
						m_builder.CreateZExt(ah, intType(64)),
						ConstantInt::get(intType(64), 32)),
				m_builder.CreateZExt(al, intType(64)));
		Value *b = m_builder.CreateOr(
				m_builder.CreateShl(
					m_builder.CreateZExt(bh, intType(64)),
					ConstantInt::get(intType(64), 32)),
				m_builder.CreateZExt(bl, intType(64)));

		Value *result = m_builder.CreateAdd(a, b);
		Value *rl = m_builder.CreateTrunc(result, intType(32));
		Value *rh = m_builder.CreateTrunc(
				m_builder.CreateLShr(result, ConstantInt::get(intType(64), 32)),
				intType(32));
		setValue(args[0], rl);
		setValue(args[1], rh);
		break;
	}
	case INDEX_op_movcond_i32: {
		Value *t0 = m_builder.CreateSExt(
				generateCondition(static_cast<TCGCond>(args[5]), getValue(args[1]), getValue(args[2])),
				intType(32));
		Value *v1 = getValue(args[3]);
		Value *v2 = getValue(args[4]);

		Value *not_t0 = m_builder.CreateNeg(t0);
		Value *t1 = m_builder.CreateAnd(v1, not_t0);
		Value *ret = m_builder.CreateAnd(v2, t0);
		setValue(args[0], m_builder.CreateOr(ret, t1));
		break;
    }

    default:
        std::cerr << "ERROR: unknown TCG micro operation '"
                  << def.name << "'" << std::endl;
        tcg_abort();
        break;
    }
}

void TCGLLVMContextPrivate::generateCode(TCGContext *s, TranslationBlock *tb)
{
    /* Create new function for current translation block */
    /* TODO: compute the checksum of the tb to see if we can reuse some code */
    std::ostringstream fName;
    fName << "tcg-llvm-tb-" << (m_tbCount++) << "-" << std::hex << tb->pc;

    /*
    if(m_tbFunction)
        m_tbFunction->eraseFromParent();
    */

    FunctionType *tbFunctionType = FunctionType::get(
            wordType(),
            std::vector<llvm::Type*>(1, intPtrType(64)), false);
    m_tbFunction = Function::Create(tbFunctionType,
            Function::PrivateLinkage, fName.str(), m_module);
    BasicBlock *basicBlock = BasicBlock::Create(m_context,
            "entry", m_tbFunction);
    m_builder.SetInsertPoint(basicBlock);

    m_tcgContext = s;

    /* Prepare globals and temps information */
    initGlobalsAndLocalTemps();

    /* Generate code for each opc */
    TCGOp *op;
    for ( int opc_idx = s->gen_first_op_idx; opc_idx >= 0; opc_idx = op->next) {
        op = &s->gen_op_buf[opc_idx];
        const TCGArg* args = &s->gen_opparam_buf[op->args];
        const TCGOpcode opc = op->opc;
 
        if(opc == INDEX_op_insn_start) {
#ifndef CONFIG_S2E
            // volatile store of current OPC index
            m_builder.CreateStore(ConstantInt::get(wordType(), opc_index),
                m_builder.CreateIntToPtr(
                    ConstantInt::get(wordType(),
                        (uint64_t) &tcg_llvm_runtime.last_opc_index),
                    wordPtrType()),
                true);
            // volatile store of current PC
            m_builder.CreateStore(ConstantInt::get(wordType(), args[0]),
                m_builder.CreateIntToPtr(
                    ConstantInt::get(wordType(),
                        (uint64_t) &tcg_llvm_runtime.last_pc),
                    wordPtrType()),
                true);
#endif
        }

        generateTraceCall(tb->pc);
        generateOperation(op, args);
        //llvm::errs() << *m_tbFunction << "\n";
    }

    /* Finalize function */
    if(!isa<ReturnInst>(m_tbFunction->back().back()))
        m_builder.CreateRet(ConstantInt::get(wordType(), 0));

    /* Clean up unused m_values */
    for(int i=0; i<TCG_MAX_TEMPS; ++i)
        delValue(i);

    /* Delete pointers after deleting values */
    for(int i=0; i<TCG_MAX_TEMPS; ++i)
        delPtrForValue(i);

    m_labels.clear();

#ifndef NDEBUG
    verifyFunction(*m_tbFunction);
#endif

    //KLEE will optimize the function later
    //m_functionPassManager->run(*m_tbFunction);

    tb->llvm_function = m_tbFunction;
    tb->llvm_tc_ptr = 0;
    tb->llvm_tc_end = 0;

#ifdef DEBUG_DISAS
    if (unlikely(qemu_loglevel_mask(CPU_LOG_TB_OP))) {
        qemu_log("OP:\n");
        tcg_dump_ops(s);
        qemu_log("\n");
    }
#endif

    if(qemu_loglevel_mask(CPU_LOG_LLVM_IR)) {
        std::string fcnString;
        llvm::raw_string_ostream s(fcnString);
        s << *m_tbFunction;
        qemu_log("OUT (LLVM IR):\n%s\n", s.str().c_str());
    }
}

/***********************************/
/* External interface for C++ code */

TCGLLVMContext::TCGLLVMContext()
        : m_private(new TCGLLVMContextPrivate)
{
}

TCGLLVMContext::~TCGLLVMContext()
{
    delete m_private;
}

FunctionPassManager* TCGLLVMContext::getFunctionPassManager() const
{
    return m_private->getFunctionPassManager();
}

void TCGLLVMContext::deleteExecutionEngine()
{
    m_private->deleteExecutionEngine();
}

LLVMContext& TCGLLVMContext::getLLVMContext()
{
    return m_private->m_context;
}

Module* TCGLLVMContext::getModule()
{
    return m_private->m_module;
}

ExecutionEngine* TCGLLVMContext::getExecutionEngine()
{
    return m_private->m_executionEngine;
}

#ifdef CONFIG_S2E
void TCGLLVMContext::initializeHelpers()
{
    return m_private->initializeHelpers();
}
#endif

void TCGLLVMContext::generateCode(TCGContext *s, TranslationBlock *tb)
{
    assert(tb->tcg_llvm_context == NULL);
    assert(tb->llvm_function == NULL);

    tb->tcg_llvm_context = this;
    m_private->generateCode(s, tb);
}

TCGLLVMContext* TCGLLVMContext::getInstance(void)
{
    static TCGLLVMContext* instance = NULL;
    if (!instance) {
        instance = new TCGLLVMContext();
    }
    return instance;
}

/*****************************/
/* Functions for QEMU c code */

TCGLLVMContext* TCGLLVM_Initialize(void)
{
    // Initialize targets and assembly printers/parsers.
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllDisassemblers();

    return TCGLLVMContext::getInstance();
}

void TCGLLVMContext_Close(TCGLLVMContext *self)
{
    delete self;
}

void TCGLLVM_GenerateCode(TCGLLVMContext *l, TCGContext *s, TranslationBlock *tb)
{
    l->generateCode(s, tb);
}

void TCGLLVM_TbAlloc(TranslationBlock *tb)
{
    tb->tcg_llvm_context = NULL;
    tb->llvm_function = NULL;
}

void TCGLLVM_TbFree(TranslationBlock *tb)
{
    if(tb->llvm_function) {
        tb->llvm_function->eraseFromParent();
    }
}

#ifndef CONFIG_S2E
int tcg_llvm_search_last_pc(TranslationBlock *tb, uintptr_t searched_pc)
{
    assert(tb->llvm_function && tb == tcg_llvm_runtime.last_tb);
    return tcg_llvm_runtime.last_opc_index;
}
#endif

const char* tcg_llvm_get_func_name(TranslationBlock *tb)
{
    static char buf[64];
    if(tb->llvm_function) {
        strncpy(buf, tb->llvm_function->getName().str().c_str(), sizeof(buf));
    } else {
        buf[0] = 0;
    }
    return buf;
}

uintptr_t tcg_llvm_qemu_tb_exec(CPUState *cpu, TranslationBlock *tb)
{
    CPUArchState *env = (CPUArchState *) cpu->env_ptr;
#ifndef CONFIG_S2E
    tcg_llvm_runtime.last_tb = tb;
#endif
    uintptr_t next_tb;

    next_tb = ((uintptr_t (*)(void*)) tb->llvm_tc_ptr)(&env);

#if 0
//#ifndef CONFIG_S2E
    do {
        next_tb = ((uintptr_t (*)(void*)) tb->llvm_tc_ptr)(&env);
        tb = NULL;
        if (next_tb && ((next_tb & 3) < 2)) {
            tb = (TranslationBlock *)(next_tb & ~3);
            tb = (TranslationBlock *)((uintptr_t)tb->llvm_tb_next[next_tb & 3]);
            if (!tb || !tb->llvm_tc_ptr) {
                tb = NULL;
            }else {
                tcg_llvm_runtime.last_tb = tb;
            }
        }
    } while(tb);

    //



#endif

    return next_tb;
}


/** Generates LLVM code for already translated TB */
int cpu_gen_llvm(CPUArchState *env, TranslationBlock *tb)
{
    TCGContext *s = &tcg_ctx;
    TCGLLVMContext* tcg_llvm_ctx = TCGLLVMContext_GetInstance();
    assert(tb->llvm_function == NULL);

    tcg_func_start(s);
    gen_intermediate_code(env, tb);
    tcg_llvm_ctx->generateCode(s, tb);
    tb->s2e_tb->llvm_function = tb->llvm_function;

    return 0;
}

