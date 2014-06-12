/* function definitions for basic linear algebra
 * matrix decompositions / equation system solving */

#ifndef EIG_H
#define EIG_H
#include "vectcl.h"

/* Compute the singular values */
int NumArrayEigVCmd(ClientData dummy, Tcl_Interp *interp,
    int objc, Tcl_Obj *const *objv);

/* Compute the SVD A = U*diag(s)*VT */
int NumArrayEigCmd(ClientData dummy, Tcl_Interp *interp,
    int objc, Tcl_Obj *const *objv);


#endif
