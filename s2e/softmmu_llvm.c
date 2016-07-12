#include "tcg/tcg.h"
//The undef of CONFIG_S2E here is necessary to get the "vanilla" version of the code,
//without the check if memory is concrete
#undef CONFIG_S2E

#define MMUSUFFIX _mmu 
 
#define SHIFT 0 
#include "softmmu_template.h" 
  
#define SHIFT 1 
#include "softmmu_template.h" 
   
#define SHIFT 2 
#include "softmmu_template.h" 
    
#define SHIFT 3 
#include "softmmu_template.h" 
#undef MMUSUFFIX 

