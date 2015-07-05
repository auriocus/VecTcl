#include "arrayshape.h"
#include <string.h>
#include <stdlib.h>

#define MIN(x,y) ((x<y)?(x):(y))
#define MAX(x,y) ((x>y)?(x):(y))

/* Concatenate two arrays along dimension axis
 * The arrays must have equal dimensions except for axis
 */

int
NumArrayConcatCmd(
		ClientData dummy,
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv)
{
	Tcl_Obj *naObj1, *naObj2, *result;
	NumArrayInfo *info1, *info2, *resultinfo;
	NumArraySharedBuffer *buf1, *buf2, *resultbuf;
	NumArrayType resulttype;

	int i;
	int allocobj1 = 0;
	int allocobj2 = 0;

	int axis;
	index_t *resultdims=NULL;

	if (objc < 4) {
		Tcl_WrongNumArgs(interp, 1, objv, "numarray1 numarray2 axis");
		return TCL_ERROR;
	}

    naObj1 = objv[1];
	naObj2 = objv[2];

	if (Tcl_ConvertToType(interp, naObj1, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}

	if (Tcl_IsShared(naObj1)) {
		naObj1 = Tcl_DuplicateObj(naObj1);
		allocobj1 = 1;
	}
	
	if (Tcl_ConvertToType(interp, naObj2, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}

	if (Tcl_IsShared(naObj2)) {
		naObj2 = Tcl_DuplicateObj(naObj2);
		allocobj2 = 1;
	}

	result = Tcl_NewObj();

	if (Tcl_GetIntFromObj(interp, objv[3], &axis) != TCL_OK) {
		goto cleanobj;
	}

	if (axis<0) {
		Tcl_SetResult(interp, "Dimension out of bounds", NULL);
		goto cleanobj;
	}

	buf1  = naObj1->internalRep.twoPtrValue.ptr1;
	info1 = naObj1->internalRep.twoPtrValue.ptr2;

	buf2  = naObj2->internalRep.twoPtrValue.ptr1;
	info2 = naObj2->internalRep.twoPtrValue.ptr2;
	
	/* compare dimensions 
	 * The dimensions must agree except for 
	 * the selected axis. 
	 * Special cases: one array is empty or scalar */
	
	/* First aray is empty
	 * return reference on second object */
	if (ISEMPTYINFO(info1)) {
		Tcl_SetObjResult(interp, naObj2);
		if (allocobj1) Tcl_DecrRefCount(naObj1);
		return TCL_OK;
	}
	
	/* vice versa */
	if (ISEMPTYINFO(info2)) {
		Tcl_SetObjResult(interp, naObj1);
		if (allocobj2) Tcl_DecrRefCount(naObj2);
		return TCL_OK;
	}


	if (abs(info1->nDim - info2->nDim) > 1 && !(ISSCALARINFO(info1) || ISSCALARINFO(info2))) {
		Tcl_SetResult(interp, "Dimension mismatch", NULL);
		goto cleanobj;
	}

	/* Compute common data type */
	resulttype = NumArray_UpcastCommonType(info1 -> type, info2 -> type);
	if (resulttype < 0) {
		Tcl_SetResult(interp, "Error in datatype matching", NULL);
		goto cleanobj;
	}

	/* Special case: First array is scalar. 
	 * Add a single line along dimension axis
	 * and set it to the scalar value */
	if (ISSCALARINFO(info1)) {
		int nDim = info2->nDim;
		if (axis > nDim) {
			Tcl_SetResult(interp, "Dimension out of bounds", NULL);
			goto cleanobj;
		}

		resultdims = ckalloc(sizeof(index_t)*(nDim+1));
		memcpy(resultdims, info2->dims, sizeof(index_t)*nDim);
		if (axis == nDim) {
			/* append scalar as extra dimension at the end */
			resultdims[axis]=2;
			nDim++;
		} else {
			resultdims[axis]++;
		}

		/* reserve memory for the buffer */
		resultinfo=CreateNumArrayInfo(nDim, resultdims, resulttype);
		resultbuf=NumArrayNewSharedBuffer(resultinfo -> bufsize);
		
		/* copy the scalar value */
		NumArrayInfo* cpyinfo = DupNumArrayInfo(resultinfo);
		NumArrayInfoSlice1Axis(NULL, cpyinfo, axis, 0,0,1);

		NumArray_ValueType value;
		NumArrayGetScalarValueFromObj(NULL, naObj1, &value);

		NumArraySetValue(cpyinfo, resultbuf, value);
		DeleteNumArrayInfo(cpyinfo);

		/* copy the content of the second array */
		cpyinfo = DupNumArrayInfo(resultinfo);
		NumArrayInfoSlice1Axis(NULL, cpyinfo, axis, 1,-1,1);

		NumArrayCopy(info2, buf2, cpyinfo, resultbuf);
		DeleteNumArrayInfo(cpyinfo);

	} else if (ISSCALARINFO(info2)) {
		/* second array is a scalar - vice versa, but append at the end */
		int nDim = info1->nDim;
		if (axis > nDim) {
			Tcl_SetResult(interp, "Dimension out of bounds", NULL);
			goto cleanobj;
		}

		resultdims = ckalloc(sizeof(index_t)*(nDim+1));
		memcpy(resultdims, info1->dims, sizeof(index_t)*nDim);
		if (axis == nDim) {
			/* append scalar as extra dimension at the end */
			resultdims[axis]=2;
			nDim++;
		} else {
			resultdims[axis]++;
		}

		/* reserve memory for the buffer */
		resultinfo=CreateNumArrayInfo(nDim, resultdims, resulttype);
		resultbuf=NumArrayNewSharedBuffer(resultinfo -> bufsize);

		/* copy the scalar value */
		NumArrayInfo* cpyinfo = DupNumArrayInfo(resultinfo);
		NumArrayInfoSlice1Axis(NULL, cpyinfo, axis, -1,-1,1);

		NumArray_ValueType value;
		NumArrayGetScalarValueFromObj(NULL, naObj2, &value);

		NumArraySetValue(cpyinfo, resultbuf, value);
		DeleteNumArrayInfo(cpyinfo);

		/* copy the content of the first array */
		cpyinfo = DupNumArrayInfo(resultinfo);
		NumArrayInfoSlice1Axis(NULL, cpyinfo, axis, 0,-2,1);

		NumArrayCopy(info1, buf1, cpyinfo, resultbuf);
		DeleteNumArrayInfo(cpyinfo);

	} else {
		/* General case, dimensions must agree. 
		 * If one operand has a dimension more than the other,
		 * and it is the concat dimension, we must widen 
		 * the smaller operand and accept it.*/
		int nDim1 = info1 ->nDim;
		int nDim2 = info2 ->nDim;

		/* If axis is one dimension more than both operands,
		 * concat them into the new dimension */
		int extradim = 0;
		int enlarge1 = 0;
		int enlarge2 = 0;

		if (axis == nDim1 && axis == nDim2) {
			extradim = 1;
		}

		if (axis > nDim1  || axis > nDim2) {
			Tcl_SetResult(interp, "Dimension out of bounds", NULL);
			goto cleanobj;
		}

		for (i=0; i < MIN(nDim1, nDim2); i++) {
			if (i!= axis && info1->dims[i] != info2->dims[i]) {
				Tcl_SetResult(interp, "Dimension mismatch", NULL);
				goto cleanobj;
			}
		}
		
		resultdims = ckalloc(sizeof(index_t)*(MAX(nDim1,nDim2)+extradim));
		int nDim;
		if (nDim1 > nDim2) {
			memcpy(resultdims, info1->dims, sizeof(index_t)*nDim1);
			nDim = nDim1;
		} else {
			memcpy(resultdims, info2->dims, sizeof(index_t)*nDim2);
			nDim = nDim2;
		}

		if (extradim) {
			resultdims[axis]=2;
			nDim++;
			enlarge1=1;
			enlarge2=1;
		} else if (nDim1 > nDim2) {
			resultdims[axis]=info1->dims[axis] + 1;
			enlarge2=1;
		} else if (nDim2 > nDim1) {
			resultdims[axis]=info2->dims[axis] + 1;
			enlarge1=1;
		} else {
			resultdims[axis]=info1->dims[axis] + info2->dims[axis];
		}
		
		/* reserve buffer for this combined object */
		resultinfo=CreateNumArrayInfo(nDim, resultdims, resulttype);
		resultbuf=NumArrayNewSharedBuffer(resultinfo -> bufsize);

		/* copy the content of the first array */
		NumArrayInfo* cpyinfo = DupNumArrayInfo(resultinfo);
		if (enlarge1) {
			NumArrayInfoSlice1Axis(NULL, cpyinfo, axis, 0,0,1);
		} else {
			NumArrayInfoSlice1Axis(NULL, cpyinfo, axis, 0,info1->dims[axis]-1,1);
		}

		NumArrayCopy(info1, buf1, cpyinfo, resultbuf);
		DeleteNumArrayInfo(cpyinfo);
		
		/* copy the content of the second array */
		cpyinfo = DupNumArrayInfo(resultinfo);
		if (enlarge2) {
			NumArrayInfoSlice1Axis(NULL, cpyinfo, axis, -1,-1,1);
		} else {
			NumArrayInfoSlice1Axis(NULL, cpyinfo, axis, -info2->dims[axis],-1,1);
		}

		NumArrayCopy(info2, buf2, cpyinfo, resultbuf);
		DeleteNumArrayInfo(cpyinfo);

	}

	ckfree(resultdims);

	/* no error so far - replace info in variable */
	NumArraySetInternalRep(result, resultbuf, resultinfo);
    Tcl_SetObjResult(interp, result);
	
	if (allocobj1) Tcl_DecrRefCount(naObj1);
	if (allocobj2) Tcl_DecrRefCount(naObj2);
    
	return TCL_OK;

cleanobj:
	if (allocobj1) Tcl_DecrRefCount(naObj1);
	if (allocobj2) Tcl_DecrRefCount(naObj2);
	Tcl_DecrRefCount(result);
	return TCL_ERROR;
}

/* extract diagonal / put back */

int NumArrayDiagMatrix(Tcl_Interp *interp, Tcl_Obj *din, index_t diag, Tcl_Obj **dout) {
	
	if (Tcl_ConvertToType(interp, din, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}
	
	NumArrayInfo *info=din -> internalRep.twoPtrValue.ptr2;

	/* Check for dimensionality */
	if (info->nDim > 2) {
		RESULTPRINTF(("Diag undefined for %d dimensions", info->nDim));
		*dout=NULL;
		return TCL_ERROR;
	}

	if (info->nDim == 1 || (info->nDim == 2 && info->dims[0]==1)) {
		/* Row- or columnvector */
		index_t nelem = info->dims[info->nDim-1];
		/* Handle trivial cases, empty and scalar */
		if (nelem == 0) {
			*dout = Tcl_NewObj();
			return TCL_OK;
		}

		if (nelem==1 && diag==0) {
			/* simply create copy */
			*dout = din;
			Tcl_IncrRefCount(din);
			return TCL_OK;
		}

		/* Here we have a row- or columnvector with nelem >= 1 
		 * Create a square matrix with the appropriate size */
		index_t nsize = nelem + abs(diag);
		*dout = NumArrayNewMatrix(info->type, nsize, nsize);
		
		NumArrayIterator it;
		NumArrayIteratorInitObj(NULL, din, &it);

		switch (info->type) {
			case NumArray_Int: {
				index_t i, j;
				NaWideInt *cpyPtr = NumArrayGetPtrFromObj(NULL, *dout); /* can't fail */

				for (i=0; i<nsize; i++) {
					for (j=0; j<nsize; j++) {
						if (i-j == diag) {
							*cpyPtr++ = NumArrayIteratorDeRefInt(&it);
							NumArrayIteratorAdvance(&it);
						} else {
							*cpyPtr++ = 0;
						}
					}
				}
				break;
			}

			case NumArray_Float64: {
				index_t i, j;
				double *cpyPtr = NumArrayGetPtrFromObj(NULL, *dout); /* can't fail */

				for (i=0; i<nsize; i++) {
					for (j=0; j<nsize; j++) {
						if (i-j == diag) {
							*cpyPtr++ = NumArrayIteratorDeRefDouble(&it);
							NumArrayIteratorAdvance(&it);
						} else {
							*cpyPtr++ = 0.0;
						}
					}
				}
				break;
			}

			case NumArray_Complex128: {
				index_t i, j;
				NumArray_Complex *cpyPtr = NumArrayGetPtrFromObj(NULL, *dout); /* can't fail */

				for (i=0; i<nsize; i++) {
					for (j=0; j<nsize; j++) {
						if (i-j == diag) {
							*cpyPtr++ = NumArrayIteratorDeRefComplex(&it);
							NumArrayIteratorAdvance(&it);
						} else {
							*cpyPtr++ = NumArray_mkComplex(0.0, 0.0);
						}
					}
				}
				break;
			}

			default: {
				/* can't happen */
				Tcl_SetResult(interp, "Unknown datatype", NULL);
				return TCL_ERROR;
			}
		}

		NumArrayIteratorFree(&it);
		return TCL_OK;

	} else {
		/* 2D-input - we must extract the diagonal into a vector */
		const index_t m=info->dims[0];
		const index_t n=info->dims[1];

		if (diag <= -m || diag >=n) {
			Tcl_SetResult(interp, "requested diagonal out of range", NULL);
			*dout = NULL;
			return TCL_ERROR;
		}
		
		NumArrayIndex ind;
		NumArrayIndexInitObj(interp, din, &ind);
		index_t dlength;
		index_t i, j;
		if (diag >= 0) {
			i=0; j=diag;
			dlength = MIN(n-diag, m);
		} else {
			i=-diag; j=0;
			dlength = MIN(m+diag, n);
		}
		
		*dout = NumArrayNewVector(info->type, dlength);
		char *destptr = NumArrayGetPtrFromObj(NULL, *dout);
		const index_t elsize = NumArrayType_SizeOf(info->type);

		index_t k;
		for (k=0; k<dlength; k++) {
			char *srcptr = NumArrayIndex2DGetPtr(&ind, i++, j++);
			memcpy(destptr, srcptr, elsize);
			destptr += elsize;
		}
		
		return TCL_OK;
	}
}

int NumArrayDiagCmd(
		ClientData dummy,
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv)
{
	if (objc!=2 && objc!=3) {
		Tcl_WrongNumArgs(interp, 1, objv, "numarray index");
		return TCL_ERROR;
	}

	Tcl_Obj *matrix=objv[1];
	
	index_t diag=0;
	if (objc==2) {
		diag = 0;
	} else {
		Tcl_WideInt temp;
		if (Tcl_GetWideIntFromObj(interp, objv[2], &temp) != TCL_OK) {
			return TCL_ERROR;
		}
		diag=temp;
	}

	Tcl_Obj *result;
	if (NumArrayDiagMatrix(interp, matrix, diag, &result)!=TCL_OK) {
		return TCL_ERROR;
	}

	Tcl_SetObjResult(interp, result);
	return TCL_OK;
}
