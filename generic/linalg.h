/* function definitions for basic linear algebra
 * matrix decompositions / equation system solving */

#ifndef LINALG_H
#define LINALG_H
#include "vectclInt.h"

int NumArrayDotCmd(ClientData dummy, Tcl_Interp *interp,
    int objc, Tcl_Obj *const *objv);

int QRDecomposition(Tcl_Interp *interp, Tcl_Obj *matrix, Tcl_Obj **qr, Tcl_Obj **rdiag);

#endif
