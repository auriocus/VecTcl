/* Sample file for wrapping a LAPACK function 
 * (dgesvd / zgesvd)
 */
#include <vectcl.h>
#include <tcl.h>
#include <f2c.h>
#include <clapack.h>
#define MAX(x, y) ((x)>(y)?x:y)
#define MIN(x, y) ((x)<(y)?x:y)

int LapackSVDCmd(ClientData dummy, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv) {
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "matrix");
		return TCL_ERROR;
	}
	
	Tcl_Obj *matrix = objv[1];

	/* Convert the 1st argument to VecTcl object */
	NumArrayInfo *info = NumArrayGetInfoFromObj(interp, matrix);
	if (!info) { return TCL_ERROR; }

	/* Check that it is a matrix */
	if (info->nDim != 2) {
		Tcl_SetResult(interp, "SVD only defined for 2D matrix", NULL);
		return TCL_ERROR;
	}

	long int m = info->dims[0];
	long int n = info->dims[1];
	if (info->type != NumArray_Complex128) {
		/* Real-valued matrix, prepare for dgesvd */
		/* create a column-major copy of matrix 
		 * This also converts an integer matrix to double */
		Tcl_Obj *A = NumArrayNewMatrixColMaj(NumArray_Float64, m, n);
		NumArrayObjCopy(interp, matrix, A);
		/* create a complex matrix for U and V */
		Tcl_Obj *U = NumArrayNewMatrixColMaj(NumArray_Float64, m, m);
		Tcl_Obj *V = NumArrayNewMatrixColMaj(NumArray_Float64, n, n);
		/* create a real vector for the singular values */
		Tcl_Obj *s = NumArrayNewVector(NumArray_Float64, MIN(m,n));

		/* Extract the raw pointers from the VecTcl objects */
		double *Aptr = NumArrayGetPtrFromObj(interp, A);
		double *Uptr = NumArrayGetPtrFromObj(interp, U);
		double *Vptr = NumArrayGetPtrFromObj(interp, V);
		double *sptr = NumArrayGetPtrFromObj(interp, s);
		
		/* setup workspace arrays */
		long int lwork=MAX(MAX(1,3*MIN(m,n)+MAX(m,n)),5*MIN(m,n));
		double* work=ckalloc(sizeof(double)*lwork);
		long int lda = m;
		long int ldu = m;
		long int ldvt = n;
		long int info;

		/*
		 int dgesvd_(char *jobu, char *jobvt, integer *m, integer *n, 
	doublereal *a, integer *lda, doublereal *s, doublereal *u, integer *
	ldu, doublereal *vt, integer *ldvt, doublereal *work, integer *lwork, 
	integer *info) */

		/* call out to zgesvd */
		dgesvd_("A", "A", &m, &n, 
			Aptr, &lda, sptr, Uptr, 
			&ldu, Vptr, &ldvt, work, 
			&lwork, &info);
		
		/* free workspace */
		ckfree(work);
		/* A is also overwritten with junk */
		Tcl_DecrRefCount(A);

		/* join U, s, V into a list*/
		Tcl_Obj *result = Tcl_NewObj();
		Tcl_ListObjAppendElement(NULL, result, U);
		Tcl_ListObjAppendElement(NULL, result, s);
		Tcl_ListObjAppendElement(NULL, result, V);
		Tcl_SetObjResult(interp, result);
		return TCL_OK;


	} else {
		/* For complex values, prepare for ZGESVD */
		/* create a column-major copy of matrix */
		Tcl_Obj *A = NumArrayNewMatrixColMaj(NumArray_Complex128, m, n);
		NumArrayObjCopy(interp, matrix, A);
		/* create a complex matrix for U and V */
		Tcl_Obj *U = NumArrayNewMatrixColMaj(NumArray_Complex128, m, m);
		Tcl_Obj *V = NumArrayNewMatrixColMaj(NumArray_Complex128, n, n);
		/* create a real vector for the singular values */
		Tcl_Obj *s = NumArrayNewVector(NumArray_Float64, MIN(m,n));

		/* Extract the raw pointers from the VecTcl objects */
		NumArray_Complex *Aptr = NumArrayGetPtrFromObj(interp, A);
		NumArray_Complex *Uptr = NumArrayGetPtrFromObj(interp, U);
		NumArray_Complex *Vptr = NumArrayGetPtrFromObj(interp, V);
		double *sptr = NumArrayGetPtrFromObj(interp, s);
		
		/* setup workspace arrays */
		long int lwork=MAX(1,2*MIN(m,n)+MAX(m,n));
		NumArray_Complex* work=ckalloc(sizeof(NumArray_Complex)*lwork);
		double *rwork = ckalloc(5*MIN(m,n));
		long int lda = m;
		long int ldu = m;
		long int ldvt = n;
		long int info;

		/*
		int zgesvd_(char *jobu, char *jobvt, integer *m, integer *n, 
	doublecomplex *a, integer *lda, doublereal *s, doublecomplex *u, 
	integer *ldu, doublecomplex *vt, integer *ldvt, doublecomplex *work, 
	integer *lwork, doublereal *rwork, integer *info) */

		/* call out to zgesvd */
		zgesvd_("A", "A", &m, &n, 
			Aptr, &lda, sptr, Uptr, 
			&ldu, Vptr, &ldvt, work, 
			&lwork, rwork, &info);
		
		/* free workspace */
		ckfree(rwork);
		ckfree(work);
		/* A is also overwritten with junk */
		Tcl_DecrRefCount(A);

		/* join U, s, V into a list*/
		Tcl_Obj *result = Tcl_NewObj();
		Tcl_ListObjAppendElement(NULL, result, U);
		Tcl_ListObjAppendElement(NULL, result, s);
		Tcl_ListObjAppendElement(NULL, result, V);
		Tcl_SetObjResult(interp, result);
		return TCL_OK;
	}
}

int Lsvd_Init(Tcl_Interp *interp) {
	if (interp == 0) return TCL_ERROR;

	if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL) {
		return TCL_ERROR;
	}
	
	if (Tcl_PkgRequire(interp, "vectcl", "0.1", 0) == NULL) {
		return TCL_ERROR;
	}
	
	if (Vectcl_InitStubs(interp, "0.1", 0) == NULL) {
		return TCL_ERROR;
	}

	Tcl_CreateObjCommand(interp, "lsvd", LapackSVDCmd, NULL, NULL);

	Tcl_PkgProvide(interp, PACKAGE_NAME, PACKAGE_VERSION);

	return TCL_OK;
}
