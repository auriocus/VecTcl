#include "schur.h"
#include "clapack_cutdown.h"

#define MIN(X, Y) ((X)<(Y) ? X : Y)
#define MAX(X, Y) ((X)>(Y) ? X : Y)

static int doSchur(Tcl_Interp *interp, Tcl_Obj *matrix, Tcl_Obj **Z,  Tcl_Obj **T) {
	/* Compute Schur decomposition of a matrix.
	 * Return Schur vectors in Z and Schur form in T, 
	 */

	/* Convert matrix to VecTcl object */
	NumArrayInfo *info = NumArrayGetInfoFromObj(interp, matrix);
	if (!info) { return TCL_ERROR; }

	/* Check that it is a square matrix */
	if (info->nDim != 2) {
		/* Could be a scalar. In this case return the trivial
		 * decomposition */
		if (ISSCALARINFO(info)) {
			*T = Tcl_DuplicateObj(matrix);
			*Z = Tcl_NewDoubleObj(1.0);
			return TCL_OK;
		}

		Tcl_SetResult(interp, "Schur decomposition is only defined for square matrix", NULL);
		return TCL_ERROR;
	}


	/* get matrix dimensions */
	long int m = info->dims[0];
	long int n = info->dims[1];
	
	if (m != n) {
		Tcl_SetResult(interp, "Schur decomposition is only defined for square matrix", NULL);
		return TCL_ERROR;
	}

	char *jobvs = "V";
	char *sort = "N";

	if (info->type != NumArray_Complex128) {
		/* Real-valued matrix, prepare for dgees */
		/* create a column-major copy of matrix 
		 * This also converts an integer matrix to double */
		*T = NumArrayNewMatrixColMaj(NumArray_Float64, m, n);
		NumArrayObjCopy(interp, matrix, *T);

		*Z = NumArrayNewMatrixColMaj(NumArray_Float64, m, m);
		
		/* Extract the raw pointers from the VecTcl objects */
		double *Tptr = NumArrayGetPtrFromObj(interp, *T);
		double *Zptr = NumArrayGetPtrFromObj(interp, *Z);

		/* Space to store the eigenvalues */
		doublereal *wr = ckalloc(sizeof(doublereal)*n);
		doublereal *wi = ckalloc(sizeof(doublereal)*n);

		/* setup workspace arrays */
		integer lwork = 3*n;
		doublereal* work=ckalloc(sizeof(doublereal)*lwork);
		logical *bwork = NULL;
		integer sdim=0;

		/* Leading dimensions of T and Vr 
		 * Don't compute left vectors. */
		integer ldt = n;
		integer ldz = n;
		integer info;

/* Subroutine  int dgees_(char *jobvs, char *sort, L_fp select, 
 * integer *n, doublereal *a, integer *lda, integer *sdim, 
 * doublereal *wr, doublereal *wi, doublereal *vs, integer *ldvs, 
 * doublereal *work, integer *lwork, logical *bwork, integer *info) */


		/* call out to dgees */
		int errcode=dgees_(interp, jobvs, sort, NULL, 
				&n, Tptr, &ldt, &sdim,
				wr, wi, Zptr, &ldz,
				work, &lwork, bwork, &info);

		/* free workspace */
		ckfree(work);
		ckfree(wr); ckfree(wi);
		
		if (errcode != TCL_OK) {
			/* release temporary storage for result */
			Tcl_DecrRefCount(*Z);
			Tcl_DecrRefCount(*T);
			if (errcode > 0) {
				RESULTPRINTF(("DGEES failed to converge at eigenvector %d ", info));
			}
			return TCL_ERROR;
		}

		return TCL_OK;

	} else {
		/* Complex matrix, prepare for zgees */
		/* create a column-major copy of matrix 
		 * This also converts an integer matrix to double */
		*T = NumArrayNewMatrixColMaj(NumArray_Complex128, m, n);
		NumArrayObjCopy(interp, matrix, *T);

		*Z = NumArrayNewMatrixColMaj(NumArray_Complex128, m, m);
		
		/* Extract the raw pointers from the VecTcl objects */
		doublecomplex *Tptr = NumArrayGetPtrFromObj(interp, *T);
		doublecomplex *Zptr = NumArrayGetPtrFromObj(interp, *Z);

		/* Space to store the eigenvalues */
		doublecomplex *w = ckalloc(sizeof(doublecomplex)*n);

		/* setup workspace arrays */
		integer lwork = 2*n;
		doublecomplex *work=ckalloc(sizeof(doublecomplex)*lwork);
		doublereal *rwork=ckalloc(sizeof(doublereal)*n);
		logical *bwork = NULL;
		integer sdim=0;

		/* Leading dimensions of T and Vr 
		 * Don't compute left vectors. */
		integer ldt = n;
		integer ldz = n;
		integer info;

	/* Subroutine int zgees_(char *jobvs, char *sort, L_fp select, 
	 * integer *n, doublecomplex *a, integer *lda, integer *sdim, 
	 * doublecomplex *w, doublecomplex *vs, integer *ldvs, 
	 * doublecomplex *work, integer *lwork, doublereal *rwork, logical *bwork, integer *info) */

		/* call out to dgees */
		int errcode=zgees_(interp, jobvs, sort, NULL, 
				&n, Tptr, &ldt, &sdim,
				w, Zptr, &ldz,
				work, &lwork, rwork, bwork, &info);

		/* free workspace */
		ckfree(work);
		ckfree(rwork);
		ckfree(w);

		if (errcode != TCL_OK) {
			/* release temporary storage for result */
			Tcl_DecrRefCount(*Z);
			Tcl_DecrRefCount(*T);
			if (errcode > 0) {
				RESULTPRINTF(("ZGEES failed to converge at eigenvector %d ", info));
			}
			return TCL_ERROR;
		}
		
		return TCL_OK;

	}
}

int NumArraySchurCmd(ClientData dummy, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv) {
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "matrix");
		return TCL_ERROR;
	}

	Tcl_Obj *matrix = objv[1];

	Tcl_Obj *Z, *T;

	if (doSchur(interp, matrix, &Z, &T) != TCL_OK) {
		return TCL_ERROR;
	}

	/* return as list */
	Tcl_Obj *result=Tcl_NewObj();
	Tcl_ListObjAppendElement(interp, result, Z);
	Tcl_ListObjAppendElement(interp, result, T);

	Tcl_SetObjResult(interp, result);
	return TCL_OK;
}

