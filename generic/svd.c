#include "svd.h"
#include "clapack_cutdown.h"

#define MIN(X, Y) ((X)<(Y) ? X : Y)
#define MAX(X, Y) ((X)>(Y) ? X : Y)

static int doSVD(Tcl_Interp *interp, Tcl_Obj *matrix, Tcl_Obj **s,  Tcl_Obj **U, Tcl_Obj **VT) {
	/* Compute singular value decomposition of matrix.
	 * Return singular values in s. If U and VT are not zero, 
	 * also compute the singular vectors */

	/* Convert A to VecTcl object */
	NumArrayInfo *info = NumArrayGetInfoFromObj(interp, matrix);
	if (!info) { return TCL_ERROR; }

	/* Check that it is a matrix */
	if (info->nDim > 2) {
		Tcl_SetResult(interp, "SVD only defined for 2D matrix", NULL);
		return TCL_ERROR;
	}

	if (ISEMPTYINFO(info)) {
		Tcl_SetResult(interp, "SVD of empty matrix undefined", NULL);
	}

	/* get matrix dimensions. For a vector, 
	 * set n=1 */
	long int m = info->dims[0];
	long int n = (info->nDim == 1) ? 1 : info->dims[1];

	int wantvectors = (U!=NULL) && (VT!=NULL);

	char *request = wantvectors ? "A" : "N";
	if (info->type != NumArray_Complex128) {
		/* Real-valued matrix, prepare for dgesdd */
		/* create a column-major copy of matrix 
		 * This also converts an integer matrix to double */
		Tcl_Obj *A = NumArrayNewMatrixColMaj(NumArray_Float64, m, n);
		NumArrayObjCopy(interp, matrix, A);
		
		if (wantvectors) {
			/* create a real matrix for U and V */
			*U = NumArrayNewMatrixColMaj(NumArray_Float64, m, m);
			*VT = NumArrayNewMatrixColMaj(NumArray_Float64, n, n);
		}
		/* create a real vector for the singular values */
		*s = NumArrayNewVector(NumArray_Float64, MIN(m,n));

		/* Extract the raw pointers from the VecTcl objects */
		double *Aptr = NumArrayGetPtrFromObj(interp, A);
		double *Uptr=NULL, *VTptr=NULL;
		if (wantvectors) {
			Uptr = NumArrayGetPtrFromObj(interp, *U);
			VTptr = NumArrayGetPtrFromObj(interp, *VT);
		}
		double *sptr = NumArrayGetPtrFromObj(interp, *s);
		
		/* setup workspace arrays */
		long int lwork = 3*MIN(m,n)*MIN(m,n) +
                       MAX(MAX(m,n),4*MIN(m,n)*MIN(m,n)+4*MIN(m,n));
		double* work=ckalloc(sizeof(double)*lwork);
		long int iworksize=(8*MIN(m,n));
		integer *iwork=ckalloc(sizeof(integer)*iworksize);

		long int lda = m; 
		/* Leading dimensions. We made a fresh copy for A and
		 * new matrices U, V, therefore we have the full matrices */
		long int ldu = m;
		long int ldvt = n;
		long int info;


/* Subroutine  int dgesdd_(Tcl_Interp *interp, char *jobz, integer *m, integer *n, doublereal *
	a, integer *lda, doublereal *s, doublereal *u, integer *ldu,
	doublereal *vt, integer *ldvt, doublereal *work, integer *lwork,
	integer *iwork, integer *info) */

		/* call out to dgesdd */
		int result=dgesdd_(interp, request, &m, &n, 
			Aptr, &lda, sptr, Uptr, 
			&ldu, VTptr, &ldvt, work,
			&lwork, iwork, &info);
		
		/* free workspace */
		ckfree(work);
		ckfree(iwork);
		/* A is also overwritten with junk */
		Tcl_DecrRefCount(A);

		if (result != TCL_OK) {
			/* release temporary storage for result */
			Tcl_DecrRefCount(*s);
			if (wantvectors) {
				Tcl_DecrRefCount(*U);
				Tcl_DecrRefCount(*VT);
			}
			return TCL_ERROR;
		}

		return TCL_OK;


	} else {
		/* Complex matrix, prepare for ZGESDD */
		/* create a column-major copy of matrix */
		Tcl_Obj *A = NumArrayNewMatrixColMaj(NumArray_Complex128, m, n);
		NumArrayObjCopy(interp, matrix, A);
		
		if (wantvectors) {
			/* create a complex matrix for U and V */
			*U = NumArrayNewMatrixColMaj(NumArray_Complex128, m, m);
			*VT = NumArrayNewMatrixColMaj(NumArray_Complex128, n, n);
		}

		/* create a real vector for the singular values */
		*s = NumArrayNewVector(NumArray_Float64, MIN(m,n));

		/* Extract the raw pointers from the VecTcl objects */
		NumArray_Complex *Aptr = NumArrayGetPtrFromObj(interp, A);
		NumArray_Complex *Uptr = NULL, *VTptr = NULL;
		
		if (wantvectors) {
			Uptr = NumArrayGetPtrFromObj(interp, *U);
			VTptr = NumArrayGetPtrFromObj(interp, *VT);
		}

		double *sptr = NumArrayGetPtrFromObj(interp, *s);
		
		/* setup workspace arrays */
		long int lwork=MIN(m,n)*MIN(m,n)+2*MIN(m,n)+MAX(m,n);
		NumArray_Complex* work=ckalloc(sizeof(NumArray_Complex)*lwork);
		long int lrwork=5*MIN(m,n)*MIN(m,n) + 5*MIN(m,n);
		double *rwork = ckalloc(sizeof(double)*lrwork);
		long int iworksize=(8*MIN(m,n));
		integer *iwork=ckalloc(sizeof(integer)*iworksize);
		long int lda = m;
		long int ldu = m;
		long int ldvt = n;
		long int info;

		/* int zgesdd_(Tcl_Interp *interp, char *jobz, integer *m, integer *n,
	doublecomplex *a, integer *lda, doublereal *s, doublecomplex *u,
	integer *ldu, doublecomplex *vt, integer *ldvt, doublecomplex *work,
	integer *lwork, doublereal *rwork, integer *iwork, integer *info)
*/
		/* call out to zgesdd */
		int result=zgesdd_(interp, request,  &m, &n, 
			(doublecomplex *)Aptr, &lda, sptr, (doublecomplex *)Uptr, 
			&ldu, (doublecomplex *)VTptr, &ldvt, (doublecomplex *)work, 
			&lwork, rwork, iwork, &info);
		
		/* free workspace */
		ckfree(rwork);
		ckfree(iwork);
		ckfree(work);
		/* A is also overwritten with junk */
		Tcl_DecrRefCount(A);
		
		if (result != TCL_OK) {
			/* release temporary storage for result */
			Tcl_DecrRefCount(*s);
			if (wantvectors) {
				Tcl_DecrRefCount(*U);
				Tcl_DecrRefCount(*VT);
			}
			return TCL_ERROR;
		}

		return TCL_OK;
	}

}

int NumArraySVDCmd(ClientData dummy, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv) {
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "matrix");
		return TCL_ERROR;
	}
	
	Tcl_Obj *matrix = objv[1];
	
	Tcl_Obj *s, *U, *VT;

	if (doSVD(interp, matrix, &s, &U, &VT) != TCL_OK) {
		return TCL_ERROR;
	}

	/* return as list */
	Tcl_Obj *result=Tcl_NewObj();
	Tcl_ListObjAppendElement(interp, result, s);
	Tcl_ListObjAppendElement(interp, result, U);
	Tcl_ListObjAppendElement(interp, result, VT);

	Tcl_SetObjResult(interp, result);
	return TCL_OK;
}

int NumArraySVD1Cmd(ClientData dummy, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv) {
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "matrix");
		return TCL_ERROR;
	}
	
	Tcl_Obj *matrix = objv[1];
	
	Tcl_Obj *s;

	if (doSVD(interp, matrix, &s, NULL, NULL) != TCL_OK) {
		return TCL_ERROR;
	}
	
	Tcl_SetObjResult(interp, s);
	return TCL_OK;
}
