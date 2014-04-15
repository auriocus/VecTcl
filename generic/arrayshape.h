#ifndef ARRAYSHAPE_H
#define ARRAYSHAPE_H

#include "vectcl.h"
SUBCOMMAND(NumArrayConcatCmd);
SUBCOMMAND(NumArrayDiagCmd);

int NumArrayDiagMatrix(Tcl_Interp *interp, Tcl_Obj *din, int diag, Tcl_Obj **dout);

#endif
