#ifndef S2E_QEMU_H
#define S2E_QEMU_H

#include <inttypes.h>

#ifdef __cplusplus
namespace s2e {
    struct S2E;
    struct S2ETranslationBlock;
}
using s2e::S2E;
using s2e::S2ETranslationBlock;
#else
struct S2E;
struct S2ETranslationBlock;
#endif

struct TranslationBlock;
struct TCGLLVMContext;

// XXX
struct CPUX86State;

#ifdef __cplusplus
extern "C" {
#endif

extern struct S2E* g_s2e;

/**************************/
/* Functions from S2E.cpp */

/** Initialize S2E instance. Called by main() */
struct S2E* s2e_initialize(struct TCGLLVMContext *tcgLLVMContext,
                           const char *s2e_config_file,
                           const char *s2e_output_dir);

/** Relese S2E instance and all S2E-related objects. Called by main() */
void s2e_close(struct S2E* s2e);

/** Initialize symbolic execution machinery. Should be called after
    QEMU pc is completely constructed */
void s2e_initialize_symbolic_execution(struct S2E *s2e);

/*********************************/
/* Functions from CorePlugin.cpp */

/** Allocate S2E parts of the tanslation block. Called from tb_alloc() */
void s2e_tb_alloc(struct TranslationBlock *tb);

/** Free S2E parts of the translation block. Called from tb_flush() and tb_free() */
void s2e_tb_free(struct TranslationBlock *tb);

/** Called by cpu_gen_code() at the beginning of translation process */
void s2e_on_translate_block_start(
        struct S2E* s2e,
        struct TranslationBlock *tb, uint64_t pc);

/** Called by cpu_gen_code() before translation of each instruction */
void s2e_on_translate_instruction_start(
        struct S2E* s2e,
        struct TranslationBlock* tb, uint64_t pc);

/** Called by cpu_gen_code() after translation of each instruction */
void s2e_on_translate_instruction_end(
        struct S2E* s2e,
        struct TranslationBlock* tb, uint64_t pc);

void s2e_on_exception(struct S2E *s2e,  unsigned intNb);

/**********************************/
/* Functions from S2EExecutor.cpp */
uintptr_t s2e_qemu_tb_exec(struct S2E* s2e, struct TranslationBlock* tb,
                           void* volatile* saved_AREGs);

#ifdef __cplusplus
}
#endif

#endif // S2E_QEMU_H
