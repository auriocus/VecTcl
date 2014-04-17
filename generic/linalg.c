#include "linalg.h"
#include "math.h"

int NumArrayDotCmd(ClientData dummy, Tcl_Interp *interp,
    int objc, Tcl_Obj *const *objv) 
{
    Tcl_Obj *naObj1, *naObj2, *resultObj;
    NumArrayInfo *info1, *info2, *resultinfo;
    NumArraySharedBuffer *sharedbuf;
	
    if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 1, objv, "numarray1 numarray2");
	    return TCL_ERROR;
    }
    
    naObj1 = objv[1];
    naObj2 = objv[2];
    
    if (Tcl_ConvertToType(interp, naObj1, &NumArrayTclType) != TCL_OK) {
	    return TCL_ERROR;
    }

    if (Tcl_ConvertToType(interp, naObj2, &NumArrayTclType) != TCL_OK) {
	    return TCL_ERROR;
    }

    info1 = naObj1->internalRep.twoPtrValue.ptr2;
    info2 = naObj2->internalRep.twoPtrValue.ptr2;
	
	/* handle case of scalar multiplication */
	if ((info1 -> nDim == 1 && info1 -> dims[0] == 1)
		|| (info2 -> nDim == 1 && info2 -> dims[0] == 1)) {
		/* same as elementwise multiplication */
		return NumArrayTimesCmd(dummy, interp, objc, objv);
	}
/*
    if (info1 -> type != NumArray_Float64 || info2 -> type != NumArray_Float64) {
		Tcl_SetResult(interp, "Dot product implemented only for doubles", NULL);
		return TCL_ERROR;
	}
*/
	
	#define T1 int
	#define T2 int
	#define TRES int
	#include "dotproductloop.h"	
	
	#define T1 int
	#define T2 double
	#define TRES double
	#include "dotproductloop.h"	
	
	#define T1 double
	#define T2 int
	#define TRES double
	#include "dotproductloop.h"	
	
	#define T1 double
	#define T2 double
	#define TRES double
	#include "dotproductloop.h"	

	#define T1 int
	#define T2 NumArray_Complex
	#define TRES NumArray_Complex
	#include "dotproductloop.h"	
	
	#define T1 NumArray_Complex
	#define T2 int
	#define TRES NumArray_Complex
	#include "dotproductloop.h"	
	
	#define T1 double
	#define T2 NumArray_Complex
	#define TRES NumArray_Complex
	#include "dotproductloop.h"	
	
	#define T1 NumArray_Complex
	#define T2 double
	#define TRES NumArray_Complex
	#include "dotproductloop.h"	
	
	#define T1 NumArray_Complex
	#define T2 NumArray_Complex
	#define TRES NumArray_Complex
	#include "dotproductloop.h"	
	
	
	{
		Tcl_SetResult(interp, "Unknown datatypes", NULL);
		return TCL_ERROR;
	}

	resultObj=Tcl_NewObj();
	NumArraySetInternalRep(resultObj, sharedbuf, resultinfo);
	Tcl_SetObjResult(interp, resultObj);

    return TCL_OK;
}

/* Compute QR factorization of real matrix object
 * store result in qr and diagonal elements in rdiag 
 * qr and rdiag are new objects */
/* LAPACK: DGEQRF */
int QRDecompositionColMaj(Tcl_Interp * interp, Tcl_Obj *matrix, Tcl_Obj **qr, Tcl_Obj **rdiag) {
	if (Tcl_ConvertToType(interp, matrix, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}
    
	NumArrayInfo *minfo = matrix -> internalRep.twoPtrValue.ptr2;
	NumArraySharedBuffer *msharedbuf = matrix -> internalRep.twoPtrValue.ptr1;
		
	if (minfo -> nDim != 2) {
		Tcl_SetResult(interp, "not a two-dimensional matrix", NULL);
		return TCL_ERROR;
	}

	/* get dimensions */
	const int m = minfo -> dims[0];
	const int n = minfo -> dims[1];

	if (m < n) {
		Tcl_SetResult(interp, "matrix has more columns than rows in QR", NULL);
		return TCL_ERROR;
	}

	/* Create qr as a column major copy of matrix */
	*qr = Tcl_NewObj();
	NumArrayInfo *info = CreateNumArrayInfoColMaj(minfo->nDim, minfo->dims, NumArray_Float64);
	NumArraySharedBuffer *qrsharedbuf = NumArrayNewSharedBuffer(info->bufsize);
	NumArraySetInternalRep(*qr, qrsharedbuf, info);
	
	if (NumArrayCopy(minfo, msharedbuf, info, qrsharedbuf) != TCL_OK) {
		Tcl_SetResult(interp, "Converting real matrix to double failed", NULL);
		Tcl_DecrRefCount(*qr);
		return TCL_ERROR;
	}
		
	void * bufptr = NumArrayGetPtrFromSharedBuffer(qrsharedbuf);

	/* in column major form we use this pointer */
	double *QRbuf = (double *) bufptr;
	Tcl_InvalidateStringRep(*qr);
	

	/* simple 2D accessor for canonical buffer */
	#define QR(i, j) (QRbuf[i+j*m])

	/* alloc space for rdiag */
	*rdiag = Tcl_NewObj();
	NumArrayInfo *rdiaginfo = CreateNumArrayInfo(1, &n, info -> type);
	NumArraySharedBuffer *rsharedbuf = NumArrayNewSharedBuffer(rdiaginfo->bufsize);
	NumArraySetInternalRep(*rdiag, rsharedbuf, rdiaginfo);
	double *Rdiag = (double *)NumArrayGetPtrFromSharedBuffer(rsharedbuf);

	/* Main loop. */
	int k;
	for (k = 0; k < n; k++) {
		/* Compute 2-norm of k-th column without under/overflow. */
		double nrm = 0;
		int i;
		/* Use direct formula ? */
		for (i = k; i < m; i++) {
			nrm +=QR(i,k)*QR(i,k);
		}
		nrm = sqrt(nrm);

		if (nrm != 0.0) {
			/* Form k-th Householder vector. */
			if (QR(k,k) < 0) {	
				nrm = -nrm;
			}
			int i;
			for (i = k; i < m; i++) {
				QR(i,k) /= nrm;
			}
			QR(k,k) += 1.0;

			/* Apply transformation to remaining columns. */
			int j;
			for (j = k+1; j < n; j++) {
				double s = 0.0; 
				int i;
				for (i = k; i < m; i++) {
					s += QR(i,k)*QR(i,j);
				}
				s = -s/QR(k,k);
				for (i = k; i < m; i++) {
					QR(i,j) += s*QR(i, k);
				}
			}
		}
		Rdiag[k] = -nrm;
	}
	#undef QR	
	return TCL_OK;
}

/* Solve A*X = B for real right-hand side B, if QR factorization is
 * already known. Return least-squares solution if n<m */
int QRsolveColMaj(Tcl_Interp *interp, Tcl_Obj *qr, Tcl_Obj *rdiag, Tcl_Obj *B) {
	
	NumArrayInfo *QRinfo = qr -> internalRep.twoPtrValue.ptr2;
	const int m = QRinfo -> dims[0];
	const int n = QRinfo -> dims[1];

	if (Tcl_ConvertToType(interp, B, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}
	
	NumArrayInfo *Binfo = B->internalRep.twoPtrValue.ptr2;
	NumArraySharedBuffer *Bsharedbuf = B->internalRep.twoPtrValue.ptr1;
	
	if (Binfo -> nDim >2) {
		Tcl_SetResult(interp, "right-hand side must be a matrix or vector", NULL);
		return TCL_ERROR;
	}
	if (Binfo -> dims[0] != m) {
		Tcl_SetResult(interp, "matrix row dimensions must agree", NULL);
		return TCL_ERROR;
	}
		
	/* simply ignore for now singular matrices. octave only prints
	 * a warning.
	 *
	if (!this.isFullRank()) {
		throw new RuntimeException("Matrix is rank deficient.");
	} */

	
	/* Copy right hand side in column major format */
	const int nx = Binfo -> nDim == 1 ? 1 : Binfo -> dims[1];
	Tcl_Obj *Xobj = Tcl_NewObj();
	NumArrayInfo *Xinfo = CreateNumArrayInfoColMaj(Binfo->nDim, Binfo->dims, NumArray_Float64);
	NumArraySharedBuffer *Xsharedbuf = NumArrayNewSharedBuffer(Xinfo->bufsize);
	NumArraySetInternalRep(Xobj, Xsharedbuf, Xinfo);
	
	if (NumArrayCopy(Binfo, Bsharedbuf, Xinfo, Xsharedbuf)!=TCL_OK) {
		Tcl_SetResult(interp, "Converting real RHS to double failed", NULL);
		Tcl_DecrRefCount(Xobj);
		return TCL_ERROR;
	}

	double *Xbuf, *QRbuf, *Rdiag;

	QRbuf = (double*) NumArrayGetPtrFromObj(NULL, qr);

	Xbuf = (double*) NumArrayGetPtrFromSharedBuffer(Xsharedbuf);

	Rdiag = (double *) NumArrayGetPtrFromObj(NULL, rdiag);

	#define X(i, j)  (Xbuf[i+j*m])
	/* QR is in column major order */
	#define QR(i, j) (QRbuf[i+j*m])

	/* Compute Y = transpose(Q)*B */
	int k;
	for (k = 0; k < n; k++) {
		int j;
		for (j = 0; j < nx; j++) {
			double s = 0.0;
			int i;
			for (i = k; i < m; i++) {
				s += QR(i,k)*X(i,j);
			}
			s = -s/QR(k,k);
			for (i = k; i < m; i++) {
				X(i,j) += s*QR(i,k);
			}
		}
	}
	/* Solve R*X = Y */
	for (k = n-1; k >= 0; k--) {
		int j;
		for (j = 0; j < nx; j++) {
			X(k,j) /= Rdiag[k];
		}
		int i;
		for (i = 0; i < k; i++) {
			int j;
			for (j = 0; j < nx; j++) {
				X(i,j) -= X(k,j)*QR(i,k);
			}
		}
	}
/*	return (new Matrix(X,n,nx).getMatrix(0,n-1,0,nx-1)); */
	
	/* cut off only the first n rows of X
	 * Note: The memory is not released */
	Xinfo -> dims[0] = n;

	Tcl_SetObjResult(interp, Xobj);
	return TCL_OK;
}


/* Compute QR factorization of complex matrix object
 * store result in qr and diagonal elements in rdiag 
 * qr and rdiag are new objects */
/* LAPACK ZGEQRF */
int QRDecompositionColMajC(Tcl_Interp * interp, Tcl_Obj *matrix, Tcl_Obj **qr, Tcl_Obj **rdiag) {
	if (Tcl_ConvertToType(interp, matrix, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}
    
	NumArrayInfo *minfo = matrix -> internalRep.twoPtrValue.ptr2;
	NumArraySharedBuffer *msharedbuf = matrix -> internalRep.twoPtrValue.ptr1;
		
	if (minfo -> nDim != 2) {
		Tcl_SetResult(interp, "not a two-dimensional matrix", NULL);
		return TCL_ERROR;
	}

	/* get dimensions */
	const int m = minfo -> dims[0];
	const int n = minfo -> dims[1];

	if (m < n) {
		Tcl_SetResult(interp, "matrix has more columns than rows in QR", NULL);
		return TCL_ERROR;
	}

	/* Create qr as a column major copy of matrix */
	*qr = Tcl_NewObj();
	NumArrayInfo *info = CreateNumArrayInfoColMaj(minfo->nDim, minfo->dims, NumArray_Complex128);
	NumArraySharedBuffer *qrsharedbuf = NumArrayNewSharedBuffer(info->bufsize);
	NumArraySetInternalRep(*qr, qrsharedbuf, info);
	
	if (NumArrayCopy(minfo, msharedbuf, info, qrsharedbuf) != TCL_OK) {
		Tcl_SetResult(interp, "Converting real matrix to double failed", NULL);
		Tcl_DecrRefCount(*qr);
		return TCL_ERROR;
	}
		
	void * bufptr = NumArrayGetPtrFromSharedBuffer(qrsharedbuf);

	/* in column major form we use this pointer */
	NumArray_Complex *QRbuf = (NumArray_Complex *) bufptr;
	Tcl_InvalidateStringRep(*qr);
	

	/* simple 2D accessor for canonical buffer */
	#define QR(i, j) (QRbuf[i+j*m])

	/* alloc space for rdiag */
	*rdiag = Tcl_NewObj();
	NumArrayInfo *rdiaginfo = CreateNumArrayInfo(1, &n, info -> type);
	NumArraySharedBuffer *rsharedbuf = NumArrayNewSharedBuffer(rdiaginfo->bufsize);
	NumArraySetInternalRep(*rdiag, rsharedbuf, rdiaginfo);
	NumArray_Complex *Rdiag = (NumArray_Complex *)NumArrayGetPtrFromSharedBuffer(rsharedbuf);

	/* Main loop. */
	int k;
	for (k = 0; k < n; k++) {
		/* Compute 2-norm of k-th column without under/overflow. */
		double nrm = 0;
		int i;
		/* Use direct formula ? */
		for (i = k; i < m; i++) {
			NumArray_Complex v = QR(i, k);
			nrm +=v.re*v.re+v.im*v.im;
		}
		nrm = sqrt(nrm);

		if (nrm != 0.0) {
			/* Form k-th Householder vector. 
			 * alpha = ||x|| * exp(i*arg(x_k)) 
			 * The phase factor is equivalent to x_k / |x_k| */
			NumArray_Complex alpha;
			double QRkkv=NumArray_ComplexAbs(QR(k,k));
			if (QRkkv != 0.0) {
				alpha=NumArray_ComplexScale(QR(k,k), nrm/QRkkv);
			} else {
				alpha=NumArray_mkComplex(nrm, 0);
			}
			
			int i;
			for (i = k; i < m; i++) {
				QR(i,k) = NumArray_ComplexDivide(QR(i, k), alpha);
			}
			QR(k,k).re += 1.0;

			/* Apply transformation to remaining columns. */
			int j;
			for (j = k+1; j < n; j++) {
				NumArray_Complex s = NumArray_mkComplex(0,0); 
				int i;
				for (i = k; i < m; i++) {
					s = NumArray_ComplexAdd(s, NumArray_ComplexMultiply(NumArray_ComplexConj(QR(i,k)),QR(i,j)));
				}
				s = NumArray_ComplexNeg(NumArray_ComplexDivide(s,QR(k,k)));
				for (i = k; i < m; i++) {
					QR(i,j) = NumArray_ComplexAdd(QR(i, j), NumArray_ComplexMultiply(s,QR(i, k)));
				}
			}
			Rdiag[k] = NumArray_ComplexNeg(alpha);
		} else {
			Rdiag[k] = NumArray_mkComplex(0,0);
		}
	}
	#undef QR	
	return TCL_OK;
}

/* Solve A*X = B for complex right-hand side B, if QR factorization is
 * already known. Return least-squares solution if n<m */
int QRsolveColMajC(Tcl_Interp *interp, Tcl_Obj *qr, Tcl_Obj *rdiag, Tcl_Obj *B) {
	
	NumArrayInfo *QRinfo = qr -> internalRep.twoPtrValue.ptr2;
	const int m = QRinfo -> dims[0];
	const int n = QRinfo -> dims[1];

	if (Tcl_ConvertToType(interp, B, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}
	
	NumArrayInfo *Binfo = B->internalRep.twoPtrValue.ptr2;
	NumArraySharedBuffer *Bsharedbuf = B->internalRep.twoPtrValue.ptr1;
	
	if (Binfo -> nDim >2) {
		Tcl_SetResult(interp, "right-hand side must be a matrix or vector", NULL);
		return TCL_ERROR;
	}
	if (Binfo -> dims[0] != m) {
		Tcl_SetResult(interp, "matrix row dimensions must agree", NULL);
		return TCL_ERROR;
	}
		
	/* simply ignore for now singular matrices. octave only prints
	 * a warning.
	 *
	if (!this.isFullRank()) {
		throw new RuntimeException("Matrix is rank deficient.");
	} */

	
	/* Copy right hand side in column major format */
	const int nx = Binfo -> nDim == 1 ? 1 : Binfo -> dims[1];
	Tcl_Obj *Xobj = Tcl_NewObj();
	NumArrayInfo *Xinfo = CreateNumArrayInfoColMaj(Binfo->nDim, Binfo->dims, NumArray_Complex128);
	NumArraySharedBuffer *Xsharedbuf = NumArrayNewSharedBuffer(Xinfo->bufsize);
	NumArraySetInternalRep(Xobj, Xsharedbuf, Xinfo);
	
	if (NumArrayCopy(Binfo, Bsharedbuf, Xinfo, Xsharedbuf)!=TCL_OK) {
		Tcl_SetResult(interp, "Converting real RHS to double failed", NULL);
		Tcl_DecrRefCount(Xobj);
		return TCL_ERROR;
	}

	NumArray_Complex *Xbuf, *QRbuf, *Rdiag;
	
	QRbuf = NumArrayGetPtrFromObj(NULL, qr);

	Xbuf = NumArrayGetPtrFromSharedBuffer(Xsharedbuf);

	Rdiag =  NumArrayGetPtrFromObj(NULL, rdiag);

	#define X(i, j)  (Xbuf[i+j*m])
	/* QR is in column major order */
	#define QR(i, j) (QRbuf[i+j*m])

	/* Compute Y = transpose(Q)*B */
	int k;
	for (k = 0; k < n; k++) {
		int j;
		for (j = 0; j < nx; j++) {
			NumArray_Complex s = NumArray_mkComplex(0,0); 
			int i;
			for (i = k; i < m; i++) {
				s = NumArray_ComplexAdd(s, NumArray_ComplexMultiply(NumArray_ComplexConj(QR(i,k)),X(i,j)));
			}
			s = NumArray_ComplexNeg(NumArray_ComplexDivide(s,QR(k,k)));
			for (i = k; i < m; i++) {
				X(i,j) = NumArray_ComplexAdd(X(i, j), NumArray_ComplexMultiply(s,QR(i, k)));
			}
		}
	}
	/* Solve R*X = Y */
	for (k = n-1; k >= 0; k--) {
		int j;
		for (j = 0; j < nx; j++) {
			X(k,j) = NumArray_ComplexDivide(X(k, j), Rdiag[k]);
		}
		int i;
		for (i = 0; i < k; i++) {
			int j;
			for (j = 0; j < nx; j++) {
				X(i,j) = NumArray_ComplexSubtract(X(i, j), NumArray_ComplexMultiply(X(k,j),QR(i,k)));
			}
		}
	}
/*	return (new Matrix(X,n,nx).getMatrix(0,n-1,0,nx-1)); */
	
	/* cut off only the first n rows of X
	 * Note: The memory is not released */
	Xinfo -> dims[0] = n;

	Tcl_SetObjResult(interp, Xobj);
	return TCL_OK;
}

int NumArrayQRecoCmd(ClientData dummy, Tcl_Interp *interp,
    int objc, Tcl_Obj *const *objv) 
{
	Tcl_Obj *qr, *rdiag, *matrix;
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "matrix");
		return TCL_ERROR;
	}

	matrix = objv[1];

	if (Tcl_ConvertToType(interp, matrix, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}

	NumArrayInfo *info = matrix->internalRep.twoPtrValue.ptr2;

	if (info->type == NumArray_Complex128) {
		if (QRDecompositionColMajC(interp, objv[1], &qr, &rdiag) != TCL_OK) {
			return TCL_ERROR;
		}
	} else {
		if (QRDecompositionColMaj(interp, objv[1], &qr, &rdiag) != TCL_OK) {
			return TCL_ERROR;
		}
	}

	/* join the QR vector matrix and the diagonal elements
	 * int a list */
	Tcl_Obj* result = Tcl_NewListObj(0, NULL);
	Tcl_ListObjAppendElement(interp, result, qr);
	Tcl_ListObjAppendElement(interp, result, rdiag);

	Tcl_SetObjResult(interp, result);

	return TCL_OK;
}

int NumArrayBackslashCmd(ClientData dummy, Tcl_Interp *interp,
    int objc, Tcl_Obj *const *objv) 
{
	Tcl_Obj *qr, *rdiag, *A, *y;
	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "matrix rhs");
		return TCL_ERROR;
	}
	
	A=objv[1]; y=objv[2];

	/* test if we must solve a real or complex system. */
	if (Tcl_ConvertToType(interp, A, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}

	if (Tcl_ConvertToType(interp, y, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}

	NumArrayInfo *Ainfo = A->internalRep.twoPtrValue.ptr2;
	NumArrayInfo *yinfo = y->internalRep.twoPtrValue.ptr2;

	if (Ainfo->type == NumArray_Complex128 || yinfo->type == NumArray_Complex128) {
		/* perform complex decomposition */
		if (QRDecompositionColMajC(interp, A, &qr, &rdiag) != TCL_OK) {
			return TCL_ERROR;
		}

		/* Solve the system */
		if (QRsolveColMajC(interp, qr, rdiag, y) != TCL_OK) {
			return TCL_ERROR;
		}
	} else {
		/* perform real decomposition */
		if (QRDecompositionColMaj(interp, A, &qr, &rdiag) != TCL_OK) {
			return TCL_ERROR;
		}

		/* Solve the system */
		if (QRsolveColMaj(interp, qr, rdiag, y) != TCL_OK) {
			return TCL_ERROR;
		}
	}
	return TCL_OK;
}

int NumArraySlashCmd(ClientData dummy, Tcl_Interp *interp,
    int objc, Tcl_Obj *const *objv) 
{
	Tcl_Obj *op1, *op2;
	NumArrayInfo *info1, *info2;
	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "array1 array2");
		return TCL_ERROR;
	}
	
	op1=objv[1]; op2=objv[2];

	if (Tcl_ConvertToType(interp, op1, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}

	if (Tcl_ConvertToType(interp, op2, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}

	info1 = op1->internalRep.twoPtrValue.ptr2;
	info2 = op2->internalRep.twoPtrValue.ptr2;

	if (ISSCALARINFO(info1) || ISSCALARINFO(info2)) {
		return NumArrayRdivideCmd(dummy, interp, objc, objv);
	}

	/* Matrix right inversion not implemented */
	Tcl_SetResult(interp, "Matrix rdivide not implemented", NULL);
	return TCL_ERROR;
}

int NumArrayMatrixPowCmd(ClientData dummy, Tcl_Interp *interp,
    int objc, Tcl_Obj *const *objv) 
{
	Tcl_Obj *op1, *op2;
	NumArrayInfo *info1, *info2;
	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "array1 array2");
		return TCL_ERROR;
	}
	
	op1=objv[1]; op2=objv[2];

	if (Tcl_ConvertToType(interp, op1, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}

	if (Tcl_ConvertToType(interp, op2, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}

	info1 = op1->internalRep.twoPtrValue.ptr2;
	info2 = op2->internalRep.twoPtrValue.ptr2;

	if (ISSCALARINFO(info1) || ISSCALARINFO(info2)) {
		return NumArrayPowCmd(dummy, interp, objc, objv);
	}

	/* Matrix right inversion not implemented */
	Tcl_SetResult(interp, "Matrix power operator not implemented", NULL);
	return TCL_ERROR;
}


