#ifndef _S2E_CXX_ENVWRAPPER_H
#define _S2E_CXX_ENVWRAPPER_H

#if !defined(__cplusplus)
#error This file is not supposed to be included from C!
#endif /* !defined(__cplusplus) */

#if defined(TARGET_ARM)
#include "s2e/cxx/CpuArmStateInfo.h"
#elif defined(TARGET_I386)
#include "s2e/cxx/CpuI386StateInfo.h"
#else
#error Unknown CPU target!
#endif

#endif /* _S2E_CXX_ENVWRAPPER_H */

