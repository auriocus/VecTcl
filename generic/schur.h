/* function definitions for basic linear algebra
 * matrix decompositions / equation system solving */

#ifndef SCHUR_H
#define SCHUR_H
#include "vectclInt.h"

/* Compute the Schur form */
int NumArraySchurCmd(ClientData dummy, Tcl_Interp *interp,
    int objc, Tcl_Obj *const *objv);

#endif
