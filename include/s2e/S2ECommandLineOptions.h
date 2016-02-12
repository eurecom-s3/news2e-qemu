#ifndef _S2E_S2ECOMMANDLINEOPTIONS_H
#define _S2E_S2ECOMMANDLINEOPTIONS_H

#if !defined(__cplusplus)
#include <stdbool.h>
#endif /* !defined(__cplusplus) */

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

#if !defined(__cplusplus)
typedef struct S2ECommandLineOptions S2ECommandLineOptions;
#endif /* !defined(__cplusplus) */

struct S2ECommandLineOptions
{
	/** LUA configuration file to load **/
	const char *config_file; 

    /** Directory to write output files to **/
    const char *output_dir;
	
    /**
     *  Always generate LLVM code and execute it in Klee.
     * This mode is useful for debugging LLVM and Klee.
     */
    bool always_execute_klee;

	/** Be verbose. */
    bool verbose;

    /** 
     * Maximum number of processes that are forked 
     * during symbolic execution.
     */
    unsigned max_processes;
};

#define S2ECommandLineOptions_DefaultValues() \
	{	                                      \
		.config_file = NULL,                  \
        .output_dir = NULL,                   \
        .always_execute_klee = false,         \
        .verbose = false,                     \
        .max_processes = 1,                   \
    }

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /*  _S2E_S2ECOMMANDLINEOPTIONS_H */
