#ifndef _S2E_LLVM_H
#define _S2E_LLVM_H

/********************************************
 ********* Includes *************************
 ********************************************/

/********************************************
 ***** struct/class forward declaration *****
 ********************************************/

#if defined(__cplusplus)

namespace llvm {
    class Function;
}

using llvm::Function;

#else /* defined(__cplusplus) */

typedef struct Function Function;

#endif /* defined(__cplusplus) */

/********************************************
 ******** variable declarations *************
 ********************************************/

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

/********************************************
 ******** C function declarations ***********
 ********************************************/

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* _S2E_LLVM_H */
