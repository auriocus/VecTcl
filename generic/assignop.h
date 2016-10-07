/* to be included with CMD and OP defined
 * it defines an elementwise binary assignment operator
 * which works by iterating over all elements
 * for compatible operands */
#include "compathack.h"

#ifndef ASSIGNOP_LOOP
typedef int (assignop_loop_fun) (Tcl_Interp *interp, NumArraySharedBuffer *sharedbuf, NumArrayInfo *sliceinfo, Tcl_Obj *value);
#define ASSIGNOP_LOOP_FUN(C, T1, T2) ASSIGNOP_LOOP_FUN1(C, T1, T2)
#define ASSIGNOP_LOOP_FUN1(C, T1, T2) C##_loop_##T1##_##T2
#define DECLARE_ASSIGNOP(T1, T2) static assignop_loop_fun ASSIGNOP_LOOP_FUN(CMD, T1, T2)
#define ASSIGNOP_LOOP
#endif

#define LOOPTBL LOOPTABLE_IMP1(CMD)
#define LOOPTABLE_IMP1(C) LOOPTABLE_IMP2(C)
#define LOOPTABLE_IMP2(C) C##_table

DECLARE_ASSIGNOP(NaWideInt, NaWideInt);
DECLARE_ASSIGNOP(double, NaWideInt);
DECLARE_ASSIGNOP(double, double);
DECLARE_ASSIGNOP(NumArray_Complex, NaWideInt);
DECLARE_ASSIGNOP(NumArray_Complex, double);
DECLARE_ASSIGNOP(NumArray_Complex, NumArray_Complex);

static assignop_loop_fun * LOOPTBL[3][3] = {
	{ ASSIGNOP_LOOP_FUN(CMD, NaWideInt, NaWideInt), NULL, NULL},
	{ ASSIGNOP_LOOP_FUN(CMD, double, NaWideInt), ASSIGNOP_LOOP_FUN(CMD, double, double), NULL},
	{ ASSIGNOP_LOOP_FUN(CMD, NumArray_Complex, NaWideInt), ASSIGNOP_LOOP_FUN(CMD, NumArray_Complex, double), ASSIGNOP_LOOP_FUN(CMD, NumArray_Complex, NumArray_Complex)}
};


int CMD(
		ClientData dummy,
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv)
{
	Tcl_Obj *naObj, *value, *resultPtr;
	int allocobj = 0; int slicing = 0;

	NumArraySharedBuffer * sharedbuf;
	NumArrayInfo *info, *sliceinfo, *valueinfo;
	
	if (objc != 4 && objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "numarrayvar ?slicelist? numarray");
		return TCL_ERROR;
	}
	
	if (objc==4) {
		/* slicing requested */
		value = objv[3];
		slicing = 1;
	} else {
		value = objv[2];
		slicing = 0;
	}

	naObj = Tcl_ObjGetVar2(interp, objv[1], NULL, TCL_LEAVE_ERR_MSG);
	if (naObj == NULL) {
		return TCL_ERROR;
	}

	if (Tcl_ConvertToType(interp, naObj, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}
	
	if (Tcl_ConvertToType(interp, value, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}
	
	info = naObj -> internalRep.twoPtrValue.ptr2;
	valueinfo = value -> internalRep.twoPtrValue.ptr2;
	
	/* Check if upcasting is required. This may need
	 * an out-of-place operation */
	if (info -> type < valueinfo -> type) {
		Tcl_Obj *conv;
		if (NumArrayConvertToType(interp, naObj, valueinfo ->type, &conv) != TCL_OK) {
			return TCL_ERROR;
		}
		
		naObj = conv;

	} else {
		/* in-place - handle sharing */
		if (Tcl_IsShared(naObj)) {
			naObj = Tcl_DuplicateObj(naObj);
			allocobj = 1;
		}

		NumArrayEnsureWriteable(naObj);
	}

	info = naObj -> internalRep.twoPtrValue.ptr2;
	sharedbuf = naObj -> internalRep.twoPtrValue.ptr1;
	
	/* handle slicing: create temporary sliceinfo */
	if (slicing) {
		if (NumArrayInfoSlice(interp, info, objv[2], &sliceinfo) != TCL_OK) {
			goto cleanobj;
		}
		/* note - due to value (esp. literal) sharing it can happen that parsing the slice 
		 * shimmers value back to a different type. Therefore we must ensure that 
		 * value and naObj are numeric arrays. Perverse example: x[x] = x
		 * naObj is never shared at this point */

		if (Tcl_ConvertToType(interp, value, &NumArrayTclType) != TCL_OK) {
			/* if an error occurs here, there is a refcounting bug */
			goto cleanobj;
		}
		valueinfo = value -> internalRep.twoPtrValue.ptr2;
		
	} else {
		/* No slicing. Take the full array */
		sliceinfo = info;
	}
	
	/* Check if dimensions are compatible. Also holds if value is scalar */
	if (!NumArrayCompatibleDimensions(sliceinfo, valueinfo) && !ISSCALARINFO(valueinfo)) {
		Tcl_SetResult(interp, "Dimension mismatch", NULL);
		goto cleaninfo;
	}
	
	/* map to int,double,complex - workaround
	 * until we have the real implementation */
	int ind1=NumArrayCompatTypeMap[sliceinfo->type];
	int ind2=NumArrayCompatTypeMap[valueinfo->type];
	if (ind1 < 0 || ind2 < 0) {
		Tcl_SetObjResult(interp, Tcl_ObjPrintf("Operator undefined for types %s, %s", NumArray_typename[sliceinfo->type], NumArray_typename[valueinfo->type]));
		goto cleaninfo;
	}

	
	if (LOOPTBL[ind1][ind2](interp, sharedbuf, sliceinfo, value)!=TCL_OK) {
		goto cleaninfo;
	}

	/* no error so far - discard temp sliceinfo, string rep and set variable and result */
	if (slicing) DeleteNumArrayInfo(sliceinfo);
	Tcl_InvalidateStringRep(naObj);

	resultPtr = Tcl_ObjSetVar2(interp, objv[1], NULL, naObj, TCL_LEAVE_ERR_MSG);
	if (resultPtr == NULL) {
		return TCL_ERROR;
	}
    Tcl_SetObjResult(interp, resultPtr);
    return TCL_OK;

cleaninfo:	
	if (slicing) DeleteNumArrayInfo(sliceinfo);
cleanobj:
	if (allocobj) Tcl_DecrRefCount(naObj);
	return TCL_ERROR;
}


#define DEREFIT_NaWideInt NumArrayIteratorDeRefInt
#define DEREFIT_double NumArrayIteratorDeRefDouble
#define DEREFIT_NumArray_Complex NumArrayIteratorDeRefComplex

#define GETOP_IMP1(T) DEREFIT_##T
#define GETOP_IMP(T) GETOP_IMP1(T)

#define GETOP UPCAST(T, TRES, GETOP_IMP(T)(&srcit))


#define TRES NaWideInt
#define T NaWideInt
#ifdef OPINT
#define OP OPINT
#endif
#include "assignop_loop.h"


#define TRES double
#define T NaWideInt
#ifdef OPDBL
#define OP OPDBL
#endif
#include "assignop_loop.h"

#define TRES double
#define T double
#ifdef OPDBL
#define OP OPDBL
#endif
#include "assignop_loop.h"

#define TRES NumArray_Complex
#define T NaWideInt
#ifdef OPCPLX
#define OP OPCPLX
#endif
#include "assignop_loop.h"

#define TRES NumArray_Complex
#define T double
#ifdef OPCPLX
#define OP OPCPLX
#endif
#include "assignop_loop.h"

#define TRES NumArray_Complex
#define T NumArray_Complex
#ifdef OPCPLX
#define OP OPCPLX
#endif
#include "assignop_loop.h"

#undef GETOP_IMP1
#undef GETOP_IMP
#undef GETOP

#undef CMD
#undef OPINT
#undef OPDBL
#undef OPCPLX
