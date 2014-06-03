#include <tcl.h>
#include <stdio.h>
#include "f2c.h"

/*
  From the original manpage:
  --------------------------
  XERBLA is an error handler for the LAPACK routines.
  It is called by an LAPACK routine if an input parameter has an invalid value.
  A message is printed and execution stops.

  Instead of printing a message and stopping the execution, a
  ValueError is raised with the message.

  Parameters:
  -----------
  srname: Subroutine name to use in error message, maximum six characters.
          Spaces at the end are skipped.
  info: Number of the invalid parameter.
*/

/* The Tcl_Interp in a thread-safe way in a call chain from all subroutines
 * and return codes are used. Thus we can really just report an error
 * as usual within Tcl */

int vectcl_xerbla(Tcl_Interp *interp, char *srname, integer *info)
{
        const char* format = "%s: parameter number %d is invalid";

        Tcl_SetObjResult(interp, Tcl_ObjPrintf(format, srname, (int)*info));
	return TCL_ERROR;
}
