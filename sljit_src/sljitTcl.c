/* Sample file for wrapping a LAPACK function 
 * (dgesvd / zgesvd)
 */
#include <tcl.h>

int Sljit_Init(Tcl_Interp *interp) {
	if (interp == 0) return TCL_ERROR;

	if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL) {
		return TCL_ERROR;
	}
	
	Tcl_PkgProvide(interp, "sljit", "0.1");

	return TCL_OK;
}
