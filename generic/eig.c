#include "eig.h"
#include "clapack_cutdown.h"

#define MIN(X, Y) ((X)<(Y) ? X : Y)
#define MAX(X, Y) ((X)>(Y) ? X : Y)

static int doEig(Tcl_Interp *interp, Tcl_Obj *matrix, Tcl_Obj **ev,  Tcl_Obj **V) {
	/* Compute eigen decomposition of matrix.
	 * Return eigenvalues in ev. If V is not NULL, 
	 * also compute the eigenvectors */

	/* Convert matrix to VecTcl object */
	NumArrayInfo *info = NumArrayGetInfoFromObj(interp, matrix);
	if (!info) { return TCL_ERROR; }

	/* Check that it is a square matrix */
	if (info->nDim != 2) {
		/* Could be a scalar. In this case return the trivial
		 * decomposition */
		if (ISSCALARINFO(info)) {
			*ev = Tcl_DuplicateObj(matrix);
			*V = Tcl_NewDoubleObj(1.0);
			return TCL_OK;
		}

		Tcl_SetResult(interp, "Eigendecomposition is only defined for square matrix", NULL);
		return TCL_ERROR;
	}


	/* get matrix dimensions */
	long int m = info->dims[0];
	long int n = info->dims[1];
	
	if (m != n) {
		Tcl_SetResult(interp, "Eigendecomposition is only defined for square matrix", NULL);
		return TCL_ERROR;
	}

	int wantvectors = (V!=NULL);

	char *jobvr = wantvectors ? "V" : "N";
	char *jobvl = "N"; 
	/* Never compute left vectors */

	if (info->type != NumArray_Complex128) {
		/* Real-valued matrix, prepare for dgeev */
		/* create a column-major copy of matrix 
		 * This also converts an integer matrix to double */
		Tcl_Obj *A = NumArrayNewMatrixColMaj(NumArray_Float64, m, n);
		NumArrayObjCopy(interp, matrix, A);

		Tcl_Obj *Vr = NULL; /* the right eigenvectors */

		if (wantvectors) {
			/* create a real matrix for the eigenvectors Vr */
			Vr = NumArrayNewMatrixColMaj(NumArray_Float64, m, m);
		}
		
		
		/* Extract the raw pointers from the VecTcl objects */
		double *Aptr = NumArrayGetPtrFromObj(interp, A);
		double *Vrptr=NULL;
		if (wantvectors) {
			Vrptr = NumArrayGetPtrFromObj(interp, Vr);
		}

		/* Space to store the eigenvalues */
		doublereal *wr = ckalloc(sizeof(doublereal)*n);
		doublereal *wi = ckalloc(sizeof(doublereal)*n);

		/* setup workspace arrays */
		integer lwork = 4*n;
		doublereal* work=ckalloc(sizeof(doublereal)*lwork);

		/* Leading dimensions of A and Vr 
		 * Don't compute left vectors. */
		integer lda = n;
		integer ldvr = n;
		
		integer ldvl = n;
		
		integer info;


/* Subroutine  int dgeev_ (Tcl_Interp *interp, char *jobvl, char *jobvr, 
 * integer *n, doublereal *	a, integer *lda, doublereal *wr, doublereal *wi, 
 * doublereal *vl, 	integer *ldvl, doublereal *vr, integer *ldvr, 
 * doublereal *work, integer *lwork, integer *info); */
		
		/* call out to dgeev */
		int errcode=dgeev_(interp, jobvl, jobvr, 
				&n, Aptr, &lda, wr, wi, 
				NULL, &ldvl, Vrptr, &ldvr,
				work, &lwork, &info);

		/* free workspace */
		ckfree(work);
		/* A is overwritten with junk */
		Tcl_DecrRefCount(A);

		if (errcode != TCL_OK) {
			/* release temporary storage for result */
			if (wantvectors) {
				Tcl_DecrRefCount(Vr);
			}
			ckfree(wr); ckfree(wi);
			if (errcode > 0) {
				RESULTPRINTF(("DGEEV failed to converge at eigenvector %d ", info));
			}
			return TCL_ERROR;
		}
		
		/* Now check, if the result is complex or real */
		int real = 1; int i;
		for (i=0; i<n; i++) {
			if (wi[i]!=0.0) {
				real = 0;
				break;
			}
		}

		if (real) {
			/* create a real vector for the eigenvalues */
			*ev = NumArrayNewVector(NumArray_Float64, n);
			double *evptr = NumArrayGetPtrFromObj(interp, *ev);
			
			/* Copy eigenvalues into this vector */
			int i;
			for (i=0; i<n; i++) {
				evptr[i] = wr[i];
			}
			
			/* Eigenvectors are contained in Vr */
			if (wantvectors) {
				*V = Vr;
			}
		} else {
			/* create a complex vector for the eigenvalues */
			*ev = NumArrayNewVector(NumArray_Complex128, n);
			NumArray_Complex *evptr = NumArrayGetPtrFromObj(interp, *ev);
			/* Copy eigenvalues into this vector */
			int i, j;
			for (i=0; i<n; i++) {
				evptr[i] = NumArray_mkComplex(wr[i], wi[i]);
			}
			
			/* Create a complex matrix for the eigenvectors */
			*V = NumArrayNewMatrixColMaj(NumArray_Complex128, n, n);
			
			/* Now, for real eigenvectors the columns of V contain
			 * the vector. For complex conjugate pairs, the two columns 
			 * contain real and imaginary part of the conjugate pair (grumpf) */
			NumArray_Complex *Vptr = NumArrayGetPtrFromObj(NULL, *V);
			#define V(i,j) Vptr[(i)+(j)*n]
			#define Vr(i, j) Vrptr[(i)+(j)*n]
			for (j=0; j<n; j++) {
				if (wi[j]==0.0) {
					/* real eigenvalue */
					for (i=0; i<n; i++) {
						V(i,j) = NumArray_mkComplex(Vr(i,j), 0.0);
					}
				} else {
					/* complex conjugate pair */
					for (i=0; i<n; i++) {
						V(i,j) = NumArray_mkComplex(Vr(i,j), Vr(i,j+1));
						V(i,j+1) = NumArray_mkComplex(Vr(i,j), -Vr(i,j+1));
					}
					
					j++;
				}
			}
			#undef V
			#undef Vr
			Tcl_DecrRefCount(Vr);
		}


		ckfree(wr); ckfree(wi);

		return TCL_OK;


	} else {
		/* Complex matrix, prepare for zgeev */
		/* create a column-major copy of matrix */
		Tcl_Obj *A = NumArrayNewMatrixColMaj(NumArray_Complex128, m, n);
		NumArrayObjCopy(interp, matrix, A);

		if (wantvectors) {
			/* create a real matrix for the eigenvectors Vr */
			*V = NumArrayNewMatrixColMaj(NumArray_Complex128, m, m);
		}
		
		
		/* Extract the raw pointers from the VecTcl objects */
		doublecomplex *Aptr = NumArrayGetPtrFromObj(interp, A);
		doublecomplex *Vrptr=NULL;
		if (wantvectors) {
			Vrptr = NumArrayGetPtrFromObj(interp, *V);
		}

		/* Space to store the eigenvalues */
		*ev = NumArrayNewVector(NumArray_Complex128, n);
		doublecomplex *w = NumArrayGetPtrFromObj(NULL, *ev);

		/* setup workspace arrays */
		integer lwork = 2*n;
		doublecomplex *work=ckalloc(sizeof(doublecomplex)*lwork);
		doublereal *rwork=ckalloc(sizeof(doublereal)*lwork);

		/* Leading dimensions of A and Vr 
		 * Don't compute left vectors. */
		integer lda = n;
		integer ldvr = n;
		
		integer ldvl = n;
		
		integer info;


/* Subroutine  int zgeev_(Tcl_Interp *interp, char *jobvl, char *jobvr,
 * integer *n, doublecomplex *a, integer *lda, doublecomplex *w, 
 * doublecomplex *vl, integer *ldvl, doublecomplex *vr, integer *ldvr, 
 * doublecomplex *work, integer *lwork, doublereal *rwork, integer *info) */
		
		/* call out to zgeev */
		int errcode=zgeev_(interp, jobvl, jobvr, 
				&n, Aptr, &lda, w, 
				NULL, &ldvl, Vrptr, &ldvr,
				work, &lwork, rwork, &info);

		/* free workspace */
		ckfree(work);
		ckfree(rwork);

		/* A is overwritten with junk */
		Tcl_DecrRefCount(A);

		if (errcode != TCL_OK) {
			/* release temporary storage for result */
			if (wantvectors) {
				Tcl_DecrRefCount(*V);
			}
			Tcl_DecrRefCount(*ev);

			if (errcode > 0) {
				RESULTPRINTF(("ZGEEV failed to converge at eigenvector %d ", info));
			}
			return TCL_ERROR;
		}
		
		return TCL_OK;

	}

}

int NumArrayEigVCmd(ClientData dummy, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv) {
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "matrix");
		return TCL_ERROR;
	}

	Tcl_Obj *matrix = objv[1];

	Tcl_Obj *ev;

	if (doEig(interp, matrix, &ev, NULL) != TCL_OK) {
		return TCL_ERROR;
	}

	Tcl_SetObjResult(interp, ev);
	return TCL_OK;
}

int NumArrayEigCmd(ClientData dummy, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv) {
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "matrix");
		return TCL_ERROR;
	}

	Tcl_Obj *matrix = objv[1];

	Tcl_Obj *ev, *V;

	if (doEig(interp, matrix, &ev, &V) != TCL_OK) {
		return TCL_ERROR;
	}

	/* return as list */
	Tcl_Obj *result=Tcl_NewObj();
	Tcl_ListObjAppendElement(interp, result, ev);
	Tcl_ListObjAppendElement(interp, result, V);

	Tcl_SetObjResult(interp, result);
	return TCL_OK;
}

