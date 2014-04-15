/* function definitions for basic linear algebra
 * matrix decompositions / equation system solving */

#ifndef SVD_H
#define SVD_H
#include "vectcl.h"

/* Compute SVD for real matrix */
int SVDecompositionColMaj(Tcl_Interp * interp, Tcl_Obj *matrix, Tcl_Obj **sv, Tcl_Obj **U, Tcl_Obj **V);

int NumArraySVD1Cmd(ClientData dummy, Tcl_Interp *interp,
    int objc, Tcl_Obj *const *objv);

int NumArraySVDCmd(ClientData dummy, Tcl_Interp *interp,
    int objc, Tcl_Obj *const *objv);


#endif
