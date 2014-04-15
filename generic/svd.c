#include "svd.h"
#include "math.h"
#include "arrayshape.h"

#define min(X, Y) ((X)<(Y) ? X : Y)
#define max(X, Y) ((X)>(Y) ? X : Y)

 /* Compute SVD  of real matrix object
  * store singular values in sv and singular vectors in U,V 
  * LAPACK: DGESVD */
int SVDecomposition(Tcl_Interp * interp, Tcl_Obj *matrix, Tcl_Obj **sv, Tcl_Obj **U, Tcl_Obj **V) {
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

	/* Create a copy of the matrix */
	NumArrayInfo *info = CreateNumArrayInfo(minfo->nDim, minfo->dims, NumArray_Float64);
	NumArraySharedBuffer *svdsharedbuf = NumArrayNewSharedBuffer(info->bufsize);

	if (NumArrayCopy(minfo, msharedbuf, info, svdsharedbuf) != TCL_OK) {
		Tcl_SetResult(interp, "Converting real matrix to double failed", NULL);
		NumArraySharedBufferDecrRefcount(svdsharedbuf);
		DeleteNumArrayInfo(info);
		return TCL_ERROR;
	}

	void * bufptr = NumArrayGetPtrFromSharedBuffer(svdsharedbuf);
	/* in column major form we use this pointer */
	double *SVDbuf = (double *) bufptr;

	int nu = min(m,n);
	*sv = NumArrayNewVector(NumArray_Float64, min(m+1,n));
	double *s;
	NumArrayGetBufferFromObj(NULL, *sv, &s);
	
	int wantu = U!=NULL;
	int wantv = V!=NULL;

	/* Constant zero for zeroing U and V */
	NumArray_ValueType zero;
	zero.type=NumArray_Float64;
	zero.value.Float64=0.0;

	double *Ubuf;
	double *Vbuf;
	if (wantu) {
		*U = NumArrayNewMatrix(NumArray_Float64, m, nu);
		NumArrayGetBufferFromObj(NULL, *U, &Ubuf); /* can't fail */
		/* Filling U with constant 0. A bit arcane... */
		NumArraySetValue((*U)->internalRep.twoPtrValue.ptr2, (*U)->internalRep.twoPtrValue.ptr1, zero);
	}
	if (wantv) {
		*V = NumArrayNewMatrix(NumArray_Float64, n, n);
		NumArrayGetBufferFromObj(NULL, *V, &Vbuf); /* can't fail */
		/* Filling V with constant 0. A bit arcane... */
		NumArraySetValue((*V)->internalRep.twoPtrValue.ptr2, (*V)->internalRep.twoPtrValue.ptr1, zero);
	}

	/* workspace */
	double *e = ckalloc(sizeof(double)*n);
	double *work = ckalloc(sizeof(double)*m);

	/* simple 2D accessor for canonical buffer */
#define A(i, j) (SVDbuf[i*n+j]) /* SVD is in col major */
#define U(i, j) (Ubuf[i*n+j])  /* U & V in row major */
#define V(i, j) (Vbuf[i*n+j])
	/* Reduce A to bidiagonal form, storing the diagonal elements
	   in s and the super-diagonal elements in e. */

	int nct = min(m-1,n);
	int nrt = max(0,min(n-2,m));
	int k;
	for (k = 0; k < max(nct,nrt); k++) {
		if (k < nct) {

			// Compute the transformation for the k-th column and
			// place the k-th diagonal in s[k].
			// Compute 2-norm of k-th column without under/overflow.
			s[k] = 0;
			int i;
			for (i = k; i < m; i++) {
				s[k] = hypot(s[k],A(i, k));
			}

			if (s[k] != 0.0) {
				if (A(k, k) < 0.0) {
					s[k] = -s[k];
				}
				for (i = k; i < m; i++) {
					A(i, k) /= s[k];
				}
				A(k, k) += 1.0;
			}
			s[k] = -s[k];
		}
		int j;
		for (j = k+1; j < n; j++) {
			if ((k < nct) & (s[k] != 0.0))  {

				// Apply the transformation.

				double t = 0;
				int i;
				for (i = k; i < m; i++) {
					t += A(i, k)*A(i, j);
				}
				t = -t/A(k, k);
				for (i = k; i < m; i++) {
					A(i, j) += t*A(i, k);
				}
			}

			// Place the k-th row of A into e for the
			// subsequent calculation of the row transformation.

			e[j] = A(k, j);
		}
		if (wantu & (k < nct)) {

			// Place the transformation in U for subsequent back
			// multiplication.
			int i;
			for (i = k; i < m; i++) {
				U(i, k) = A(i, k);
			}
		}
		if (k < nrt) {

			// Compute the k-th row transformation and place the
			// k-th super-diagonal in e[k].
			// Compute 2-norm without under/overflow.
			e[k] = 0;
			int i;
			for (i = k+1; i < n; i++) {
				e[k] = hypot(e[k],e[i]);
			}

			if (e[k] != 0.0) {
				if (e[k+1] < 0.0) {
					e[k] = -e[k];
				}
				int i;
				for (i = k+1; i < n; i++) {
					e[i] /= e[k];
				}
				e[k+1] += 1.0;
			}
			e[k] = -e[k];
			if ((k+1 < m) & (e[k] != 0.0)) {

				// Apply the transformation.
				int i;
				for (i = k+1; i < m; i++) {
					work[i] = 0.0;
				}

				int j;
				for (j = k+1; j < n; j++) {
					int i;
					for (i = k+1; i < m; i++) {
						work[i] += e[j]*A(i, j);
					}
				}

				for (j = k+1; j < n; j++) {
					double t = -e[j]/e[k+1];
					int i;
					for (i = k+1; i < m; i++) {
						A(i, j) += t*work[i];
					}
				}
			}
			if (wantv) {

				// Place the transformation in V for subsequent
				// back multiplication.
				int i;
				for (i = k+1; i < n; i++) {
					V(i, k) = e[i];
				}
			}
		}
	}

	// Set up the final bidiagonal matrix or order p.

	int p = min(n,m+1);
	if (nct < n) {
		s[nct] = A(nct, nct);
	}
	if (m < p) {
		s[p-1] = 0.0;
	}
	if (nrt+1 < p) {
		e[nrt] = A(nrt, p-1);
	}
	e[p-1] = 0.0;

	// If required, generate U.

	if (wantu) {
		int j;
		for (j = nct; j < nu; j++) {
			int i;
			for (i = 0; i < m; i++) {
				U(i, j) = 0.0;
			}
			U(j, j) = 1.0;
		}
		int k;
		for (k = nct-1; k >= 0; k--) {
			if (s[k] != 0.0) {
				int j;
				for (j = k+1; j < nu; j++) {
					double t = 0;
					int i;
					for (i = k; i < m; i++) {
						t += U(i, k)*U(i, j);
					}
					t = -t/U(k, k);
					for (i = k; i < m; i++) {
						U(i, j) += t*U(i, k);
					}
				}
				int i;
				for (i = k; i < m; i++ ) {
					U(i, k) = -U(i, k);
				}
				U(k, k) = 1.0 + U(k, k);
				for (i = 0; i < k-1; i++) {
					U(i, k) = 0.0;
				}
			} else {
				int i;
				for (i = 0; i < m; i++) {
					U(i, k) = 0.0;
				}
				U(k, k) = 1.0;
			}
		}
	}

	// If required, generate V.

	if (wantv) {
		int k;
		for (k = n-1; k >= 0; k--) {
			if ((k < nrt) & (e[k] != 0.0)) {
				int j;
				for (j = k+1; j < nu; j++) {
					double t = 0;
					int i;
					for (i = k+1; i < n; i++) {
						t += V(i, k)*V(i, j);
					}
					t = -t/V(k+1, k);
					for (i = k+1; i < n; i++) {
						V(i, j) += t*V(i, k);
					}
				}
			}
			int i;
			for (i = 0; i < n; i++) {
				V(i, k) = 0.0;
			}
			V(k, k) = 1.0;
		}
	}

	// Main iteration loop for the singular values.

	int pp = p-1;
	int iter = 0;
	double eps = pow(2.0,-52.0);
	double tiny = pow(2.0,-966.0);
	while (p > 0) {
		int k,kase;

		// Here is where a test for too many iterations would go.

		// This section of the program inspects for
		// negligible elements in the s and e arrays.  On
		// completion the variables kase and k are set as follows.

		// kase = 1     if s(p) and e[k-1] are negligible and k<p
		// kase = 2     if s(k) is negligible and k<p
		// kase = 3     if e[k-1] is negligible, k<p, and
		//              s(k), ..., s(p) are not negligible (qr step).
		// kase = 4     if e(p-1) is negligible (convergence).

		for (k = p-2; k >= -1; k--) {
			if (k == -1) {
				break;
			}
			if (fabs(e[k]) <=
					tiny + eps*(fabs(s[k]) + fabs(s[k+1]))) {
				e[k] = 0.0;
				break;
			}
		}
		if (k == p-2) {
			kase = 4;
		} else {
			int ks;
			for (ks = p-1; ks >= k; ks--) {
				if (ks == k) {
					break;
				}
				double t = (ks != p ? fabs(e[ks]) : 0.) + 
					(ks != k+1 ? fabs(e[ks-1]) : 0.);
				if (fabs(s[ks]) <= tiny + eps*t)  {
					s[ks] = 0.0;
					break;
				}
			}
			if (ks == k) {
				kase = 3;
			} else if (ks == p-1) {
				kase = 1;
			} else {
				kase = 2;
				k = ks;
			}
		}
		k++;

		// Perform the task indicated by kase.

		switch (kase) {

			// Deflate negligible s(p).

			case 1: {
						double f = e[p-2];
						e[p-2] = 0.0;
						int j;
						for (j = p-2; j >= k; j--) {
							double t = hypot(s[j],f);
							double cs = s[j]/t;
							double sn = f/t;
							s[j] = t;
							if (j != k) {
								f = -sn*e[j-1];
								e[j-1] = cs*e[j-1];
							}
							if (wantv) {
								int i;
								for (i = 0; i < n; i++) {
									t = cs*V(i, j) + sn*V(i, p-1);
									V(i, p-1) = -sn*V(i, j) + cs*V(i, p-1);
									V(i, j) = t;
								}
							}
						}
					}
					break;

					// Split at negligible s(k).

			case 2: {
						double f = e[k-1];
						e[k-1] = 0.0;
						int j;
						for (j = k; j < p; j++) {
							double t = hypot(s[j],f);
							double cs = s[j]/t;
							double sn = f/t;
							s[j] = t;
							f = -sn*e[j];
							e[j] = cs*e[j];
							if (wantu) {
								int i;
								for (i = 0; i < m; i++) {
									t = cs*U(i, j) + sn*U(i, k-1);
									U(i, k-1) = -sn*U(i, j) + cs*U(i, k-1);
									U(i, j) = t;
								}
							}
						}
					}
					break;

					// Perform one qr step.

			case 3: {

						// Calculate the shift.

						double scale = max(max(max(max(
											fabs(s[p-1]),fabs(s[p-2])),fabs(e[p-2])), 
									fabs(s[k])),fabs(e[k]));
						double sp = s[p-1]/scale;
						double spm1 = s[p-2]/scale;
						double epm1 = e[p-2]/scale;
						double sk = s[k]/scale;
						double ek = e[k]/scale;
						double b = ((spm1 + sp)*(spm1 - sp) + epm1*epm1)/2.0;
						double c = (sp*epm1)*(sp*epm1);
						double shift = 0.0;
						if ((b != 0.0) | (c != 0.0)) {
							shift = sqrt(b*b + c);
							if (b < 0.0) {
								shift = -shift;
							}
							shift = c/(b + shift);
						}
						double f = (sk + sp)*(sk - sp) + shift;
						double g = sk*ek;

						// Chase zeros.
						int j;
						for (j = k; j < p-1; j++) {
							double t = hypot(f,g);
							double cs = f/t;
							double sn = g/t;
							if (j != k) {
								e[j-1] = t;
							}
							f = cs*s[j] + sn*e[j];
							e[j] = cs*e[j] - sn*s[j];
							g = sn*s[j+1];
							s[j+1] = cs*s[j+1];
							if (wantv) {
								int i;
								for (i = 0; i < n; i++) {
									t = cs*V(i, j) + sn*V(i, j+1);
									V(i, j+1) = -sn*V(i, j) + cs*V(i, j+1);
									V(i, j) = t;
								}
							}
							t = hypot(f,g);
							cs = f/t;
							sn = g/t;
							s[j] = t;
							f = cs*e[j] + sn*s[j+1];
							s[j+1] = -sn*e[j] + cs*s[j+1];
							g = sn*e[j+1];
							e[j+1] = cs*e[j+1];
							if (wantu && (j < m-1)) {
								int i;
								for (i = 0; i < m; i++) {
									t = cs*U(i, j) + sn*U(i, j+1);
									U(i, j+1) = -sn*U(i, j) + cs*U(i, j+1);
									U(i, j) = t;
								}
							}
						}
						e[p-2] = f;
						iter = iter + 1;
					}
					break;

					// Convergence.

			case 4: {

						// Make the singular values positive.

						if (s[k] <= 0.0) {
							s[k] = (s[k] < 0.0 ? -s[k] : 0.0);
							if (wantv) {
								int i;
								for (i = 0; i <= pp; i++) {
									V(i, k) = -V(i, k);
								}
							}
						}

						// Order the singular values.

						while (k < pp) {
							if (s[k] >= s[k+1]) {
								break;
							}
							double t = s[k];
							s[k] = s[k+1];
							s[k+1] = t;
							if (wantv && (k < n-1)) {
								int i;
								for (i = 0; i < n; i++) {
									t = V(i, k+1); V(i, k+1) = V(i, k); V(i, k) = t;
								}
							}
							if (wantu && (k < m-1)) {
								int i;
								for (i = 0; i < m; i++) {
									t = U(i, k+1); U(i, k+1) = U(i, k); U(i, k) = t;
								}
							}
							k++;
						}
						iter = 0;
						p--;
					}
					break;
		}
	}
	/* free workspace */
	ckfree(work); ckfree(e);
	NumArraySharedBufferDecrRefcount(svdsharedbuf);
	DeleteNumArrayInfo(info);
	return TCL_OK;
}

#undef U
#undef V
#undef A


#if 0
/* ------------------------
   Public Methods
 * ------------------------ */

/** Return the left singular vectors
  @return     U
  */

public Matrix getU () {
	return new Matrix(U,m,min(m+1,n));
}

/** Return the right singular vectors
  @return     V
  */

public Matrix getV () {
	return new Matrix(V,n,n);
}

/** Return the one-dimensional array of singular values
  @return     diagonal of S.
  */

public double[] getSingularValues () {
	return s;
}

/** Return the diagonal matrix of singular values
  @return     S
  */

public Matrix getS () {
	Matrix X = new Matrix(n,n);
	double[][] S = X.getArray();
	for (int i = 0; i < n; i++) {
		for (int j = 0; j < n; j++) {
			S[i][j] = 0.0;
		}
		S[i][i] = this.s[i];
	}
	return X;
}

/** Two norm
  @return     max(S)
  */

public double norm2 () {
	return s[0];
}

/** Two norm condition number
  @return     max(S)/min(S)
  */

public double cond () {
	return s[0]/s[min(m,n)-1];
}

/** Effective numerical matrix rank
  @return     Number of nonnegligible singular values.
  */

public int rank () {
	double eps = pow(2.0,-52.0);
	double tol = max(m,n)*s[0]*eps;
	int r = 0;
	for (int i = 0; i < s.length; i++) {
		if (s[i] > tol) {
			r++;
		}
	}
	return r;
}
#endif

int NumArraySVD1Cmd(ClientData dummy, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv) {
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "matrix");
		return TCL_ERROR;
	}

	/* compute only singular values */
	Tcl_Obj *s;
	if (SVDecomposition(interp, objv[1], &s, NULL, NULL) != TCL_OK) {
		return TCL_ERROR;
	}

	Tcl_SetObjResult(interp, s);
	return TCL_OK;
}

int NumArraySVDCmd(ClientData dummy, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv) {
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "matrix");
		return TCL_ERROR;
	}

	/* compute SVD */
	Tcl_Obj *U, *V, *s;
	if (SVDecomposition(interp, objv[1], &s, &U, &V) != TCL_OK) {
		return TCL_ERROR;
	}
	
	/* stuff singular values into dense matrix */
	Tcl_Obj *S;
	NumArrayDiagMatrix(NULL, s, 0, &S);
	
	/* free singular values vector */
	Tcl_DecrRefCount(s);


	/* return as list */
	Tcl_Obj *result=Tcl_NewObj();
	Tcl_ListObjAppendElement(interp, result, U);
	Tcl_ListObjAppendElement(interp, result, S);
	Tcl_ListObjAppendElement(interp, result, V);

	Tcl_SetObjResult(interp, result);
	return TCL_OK;
}
