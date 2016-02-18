#ifndef _S2E_MONITOR_H
#define _S2E_MONITOR_H

/********************************************
 ********* Includes *************************
 ********************************************/

/********************************************
 ***** struct/class forward declaration *****
 ********************************************/

#if defined(__cplusplus)
extern "C" {
#endif /* defined(__cplusplus) */

/********************************************
 ******** variable declarations *************
 ********************************************/

/********************************************
 ******** C function declarations ***********
 ********************************************/

void qmp_s2e_exec(QDict *qdict, QObject **ret, Error **err);

#if defined(__cplusplus)
}
#endif /* defined(__cplusplus) */

#endif /* _S2E_MONITOR_H */
