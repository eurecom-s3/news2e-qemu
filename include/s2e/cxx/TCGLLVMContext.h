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

#ifndef _S2E_CXX_TCG_LLVM_H
#define _S2E_CXX_TCG_LLVM_H

#if !defined(__cplusplus)
#error This file is not supposed to be included from C!
#endif /* !defined(__cplusplus) */

#include <inttypes.h>

#include "s2e/target/TCGLLVMContext.h"

/*****************************/
/* Functions for QEMU c code */


/***********************************/
/* External interface for C++ code */

namespace llvm {
    class Function;
    class LLVMContext;
    class Module;
    class ModuleProvider;
    class ExecutionEngine;
    namespace legacy {
        class FunctionPassManager;
    }
}

class TCGLLVMContextPrivate;
class TCGLLVMContext
{
private:
	TCGLLVMContext();

    TCGLLVMContextPrivate* m_private;


public:
    ~TCGLLVMContext();

    llvm::LLVMContext& getLLVMContext();

    llvm::Module* getModule();
    llvm::ModuleProvider* getModuleProvider();

    llvm::ExecutionEngine* getExecutionEngine();

    void deleteExecutionEngine();
    llvm::legacy::FunctionPassManager* getFunctionPassManager() const;

    /** Called after linking all helper libraries */
    void initializeHelpers();

    void generateCode(struct TCGContext *s,
                      struct TranslationBlock *tb);

    static TCGLLVMContext* getInstance(void);


    /**
     * Return an LLVM module containing a copy of the
     * LLVM IR code generated at S2E compile time (helpers etc).
     * This code is included as a binary blob, and loaded into a Module
     * by this function.
     * @param context The LLVM context in which the module is created
     * @return A copy of an LLVM module containing the compiled code
     */
    static llvm::Module* getCompiledBitcode(llvm::LLVMContext& context);

};


#endif /* _S2E_CXX_TCG_LLVM_H */

