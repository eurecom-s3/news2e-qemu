/*
 * LLVM backend for Tiny Code Generator for QEMU
 *
 * Copyright (c) 2010 Volodymyr Kuznetsov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef TCG_LLVM_H
#define TCG_LLVM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "tcg.h"

/*****************************/
/* Functions for QEMU c code */

struct TranslationBlock;
struct TCGLLVMContext;
struct TCGLLVMTranslationBlock;

typedef uintptr_t (*TCGLLVMTBFunctionPointer)(void* volatile*);

extern struct TranslationBlock *tcg_llvm_last_tb;
extern struct TCGLLVMTranslationBlock *tcg_llvm_last_llvm_tb;
extern uint64_t tcg_llvm_last_opc_index;
extern uint64_t tcg_llvm_last_pc;

struct TCGLLVMContext* tcg_llvm_context_new(struct TCGContext *s);
void tcg_llvm_context_free(struct TCGLLVMContext *l);

void tcg_llvm_tb_alloc(struct TranslationBlock *tb);
void tcg_llvm_tb_free(struct TranslationBlock *tb);

void tcg_llvm_gen_code(struct TCGLLVMContext *l, struct TranslationBlock *tb);
const char* tcg_llvm_get_func_name(struct TranslationBlock *tb);

int tcg_llvm_search_last_pc(struct TranslationBlock *tb, uintptr_t searched_pc);

uintptr_t tcg_llvm_qemu_tb_exec(struct TranslationBlock *tb,
                            void* volatile* saved_AREGs);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

/***********************************/
/* External interface for C++ code */

namespace llvm {
    class Function;
    class LLVMContext;
    class Module;
}

struct TCGLLVMTranslationBlock
{
    TCGLLVMContext *tcgLLVMContext;
    llvm::Function *m_tbFunction;

    TCGLLVMTBFunctionPointer m_tbFunctionPointer;
    ptrdiff_t m_tbFunctionSize;
};

class TCGLLVMContextPrivate;
class TCGLLVMContext
{
private:
    TCGLLVMContextPrivate* m_private;

public:
    TCGLLVMContext(TCGContext *s);
    ~TCGLLVMContext();

    llvm::LLVMContext* getLLVMContext();
    llvm::Module* getModule();

    void generateCode(struct TranslationBlock *tb);
};

#endif

#endif

