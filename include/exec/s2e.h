#ifndef _EXEC_S2E_H
#define _EXEC_S2E_H

#if defined(CONFIG_LLVM)
#if defined(__cplusplus)
namespace llvm {
    class Function;
}
using llvm::Function;
#else
typedef struct Function Function;
typedef struct TranslationBlock TranslationBlock;
#endif /* !defined(__cplusplus) */
#endif /* defined(CONFIG_LLVM) */

#ifdef CONFIG_S2E


uintptr_t s2e_notdirty_mem_write(CPUState *cpu, hwaddr ram_addr);
int s2e_ismemfunc(struct MemoryRegion *mr, int isWrite);
uint8_t s2e_io_readb(CPUState *cpu,
                 CPUIOTLBEntry *iotlbentry,
                 target_ulong addr,
                 uintptr_t retaddr);
uint16_t s2e_io_readw(CPUState *cpu,
                 CPUIOTLBEntry *iotlbentry,
                 target_ulong addr,
                 uintptr_t retaddr);
uint32_t s2e_io_readl(CPUState *cpu,
                 CPUIOTLBEntry *iotlbentry,
                 target_ulong addr,
                 uintptr_t retaddr);
uint64_t s2e_io_readq(CPUState *cpu,
                 CPUIOTLBEntry *iotlbentry,
                 target_ulong addr,
                 uintptr_t retaddr);
void s2e_io_writeb(CPUState *cpu,
               CPUIOTLBEntry *iotlbentry,
               uint8_t val,
               target_ulong addr,
               uintptr_t retaddr);
void s2e_io_writew(CPUState *cpu,
               CPUIOTLBEntry *iotlbentry,
               uint16_t val,
               target_ulong addr,
               uintptr_t retaddr);
void s2e_io_writel(CPUState *cpu,
               CPUIOTLBEntry *iotlbentry,
               uint32_t val,
               target_ulong addr,
               uintptr_t retaddr);
void s2e_io_writeq(CPUState *cpu,
               CPUIOTLBEntry *iotlbentry,
               uint64_t val,
               target_ulong addr,
               uintptr_t retaddr);

void *s2e_qemu_get_first_se(void);
void *s2e_qemu_get_next_se(void *se);
const char *s2e_qemu_get_se_idstr(void *se);
void s2e_qemu_save_state(QEMUFile *f, void *se);
void s2e_qemu_load_state(QEMUFile *f, void *se);
int cpu_gen_llvm(CPUArchState *env, struct TranslationBlock *tb); /* defined in translate-all.c */

extern bool g_s2e_fork_on_symbolic_address;


#endif

#endif /* _EXEC_S2E_H */
