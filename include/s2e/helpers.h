#ifndef _S2E_HELPERS_H
#define _S2E_HELPERS_H

/********************************************
 ********* Includes *************************
 ********************************************/

/********************************************
 ***** struct/class forward declaration *****
 ********************************************/

/********************************************
 ******** variable declarations *************
 ********************************************/

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

/********************************************
 ******** C function declarations ***********
 ********************************************/

/* Register target-specific helpers with LLVM */
void helper_register_symbols(void);
void helper_register_symbol(const char *name, void *address);


#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* _S2E_HELPERS_H */
