#ifndef _S2E_ASSEMBLERHELPERSX64_H
#define _S2E_ASSEMBLERHELPERSX64_H

#include <stdint.h>

static inline int bit_scan_forward_64(uint64_t *SetIndex, uint64_t Mask)
{
    uint64_t result;
    
    __asm__(
        "xor %%rax, %%rax             \n" 
        "bsf %[mask], %[mask]         \n"
        "movq %[mask], (%[set_index]) \n"
        "setnz %%al                   \n"
        "movq %%rax, %[ret]           \n"
        : [ret] "=r" (result)
        : [set_index] "r" (SetIndex),
          [mask] "r" (Mask)
        : "rax");
    return (int) result;
}

#endif /* _S2E_ASSEMBLERHELPERSX64_H */