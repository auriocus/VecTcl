/* function definitions for basic linear algebra
 * matrix decompositions / equation system solving */

#ifndef SVD_H
#define SVD_H
#include "vectcl.h"

/* Compute the singular values */
int NumArraySVD1Cmd(ClientData dummy, Tcl_Interp *interp,
    int objc, Tcl_Obj *const *objv);

/* Compute the SVD A = U*diag(s)*VT */
int NumArraySVDCmd(ClientData dummy, Tcl_Interp *interp,
    int objc, Tcl_Obj *const *objv);


#endif
