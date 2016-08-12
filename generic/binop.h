/* to be included with CMD and OP defined
 * it defines an elementwise binary operator
 * which works by iterating over all elements
 * for compatible operands */
#include "compathack.h"
#ifndef BINOP_LOOP
typedef int (binop_loop_fun) (Tcl_Obj *naObj1, Tcl_Obj *naObj2, Tcl_Obj **resultObj);
#define BINOP_LOOP_FUN(C, T1, T2) BINOP_LOOP_FUN1(C, T1, T2)
#define BINOP_LOOP_FUN1(C, T1, T2) C##_loop_##T1##_##T2
#define DECLARE_BINOP(T1, T2) static binop_loop_fun BINOP_LOOP_FUN(CMD, T1, T2)
#define BINOP_LOOP
#endif

#define LOOPTBL LOOPTABLE_IMP1(CMD)
#define LOOPTABLE_IMP1(C) LOOPTABLE_IMP2(C)
#define LOOPTABLE_IMP2(C) C##_table

#define TCLCMDPROC(X) NUMARRAYTPASTER(X,Cmd)

DECLARE_BINOP(NaWideInt, NaWideInt);
DECLARE_BINOP(NaWideInt, double);
DECLARE_BINOP(NaWideInt, NumArray_Complex);
DECLARE_BINOP(double, NaWideInt);
DECLARE_BINOP(double, double);
DECLARE_BINOP(double, NumArray_Complex);
DECLARE_BINOP(NumArray_Complex, NaWideInt);
DECLARE_BINOP(NumArray_Complex, double);
DECLARE_BINOP(NumArray_Complex, NumArray_Complex);


static binop_loop_fun * LOOPTBL[3][3] = {
	{ BINOP_LOOP_FUN(CMD, NaWideInt, NaWideInt), 
	  BINOP_LOOP_FUN(CMD, NaWideInt, double), 
	  BINOP_LOOP_FUN(CMD, NaWideInt, NumArray_Complex)},
	{ BINOP_LOOP_FUN(CMD, double, NaWideInt), 
	  BINOP_LOOP_FUN(CMD, double, double), 
	  BINOP_LOOP_FUN(CMD, double, NumArray_Complex),},
	{ BINOP_LOOP_FUN(CMD, NumArray_Complex, NaWideInt), 
	  BINOP_LOOP_FUN(CMD, NumArray_Complex, double), 
	  BINOP_LOOP_FUN(CMD, NumArray_Complex, NumArray_Complex),
	}	
};

int CMD(Tcl_Obj* naObj1, Tcl_Obj* naObj2, Tcl_Obj **resultObj);

MODULE_SCOPE
int TCLCMDPROC(CMD) ( 
		ClientData dummy,
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv)
{	
	Tcl_Obj *naObj1, *naObj2, *resultObj;

	int resultcode;

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
	
	resultcode=CMD(naObj1, naObj2, &resultObj);
	
	Tcl_SetObjResult(interp, resultObj);
	
	return resultcode;
}

int CMD(Tcl_Obj* naObj1, Tcl_Obj* naObj2, Tcl_Obj **resultObj) {

	NumArrayInfo *info1, *info2;
	info1 = naObj1->internalRep.twoPtrValue.ptr2;
	info2 = naObj2->internalRep.twoPtrValue.ptr2;
	/* map to int,double,complex - workaround
	 * until we have the real implementation */
	int ind1=NumArrayCompatTypeMap[info1->type];
	int ind2=NumArrayCompatTypeMap[info2->type];
	if (ind1 < 0 || ind2 < 0) {
		*resultObj = Tcl_ObjPrintf("Operator undefined for types %s, %s", NumArray_typename[info1->type], NumArray_typename[info2->type]);
		return TCL_ERROR;
	}
	return LOOPTBL[ind1][ind2](naObj1, naObj2, resultObj);
}

/* Implement the inner loop for the binary operators 
 * for all datatypes */

/* (NaWideInt,NaWideInt) -> NaWideInt */
#ifdef INTRES
	#define TRES INTRES
#else
	#define TRES NaWideInt
#endif
#define T1 NaWideInt
#define T2 NaWideInt

#ifdef OPINT
#define OP OPINT
#endif

#include "binop_loop.h"


/* (NaWideInt,double) -> double */
#ifdef DBLRES
	#define TRES DBLRES
#else
	#define TRES double
#endif
#define T1 NaWideInt
#define T2 double
#ifdef OPDBL
	#define OP OPDBL
#endif
#include "binop_loop.h"

/* (double, NaWideInt) -> double */
#ifdef DBLRES
	#define TRES DBLRES
#else
	#define TRES double
#endif
#define T1 double
#define T2 NaWideInt
#ifdef OPDBL
#define OP OPDBL
#endif
#include "binop_loop.h"

/* (double, double) -> double */
#ifdef DBLRES
	#define TRES DBLRES
#else
	#define TRES double
#endif
#define T1 double
#define T2 double
#ifdef OPDBL
#define OP OPDBL
#endif
#include "binop_loop.h"

/* (NaWideInt, complex) -> complex */
#ifdef CPLXRES
	#define TRES CPLXRES
#else
	#define TRES NumArray_Complex
#endif
#define T1 NaWideInt
#define T2 NumArray_Complex
#ifdef OPCPLX
#define OP OPCPLX
#endif
#include "binop_loop.h"

/* (double, complex) -> complex */
#ifdef CPLXRES
	#define TRES CPLXRES
#else
	#define TRES NumArray_Complex
#endif
#define T1 double
#define T2 NumArray_Complex
#ifdef OPCPLX
#define OP OPCPLX
#endif
#include "binop_loop.h"

/* (NaWideInt, complex) -> complex */
#ifdef CPLXRES
	#define TRES CPLXRES
#else
	#define TRES NumArray_Complex
#endif
#define T1 NumArray_Complex
#define T2 NaWideInt
#ifdef OPCPLX
#define OP OPCPLX
#endif
#include "binop_loop.h"

/* (double, complex) -> complex */
#ifdef CPLXRES
	#define TRES CPLXRES
#else
	#define TRES NumArray_Complex
#endif
#define T1 NumArray_Complex
#define T2 double
#ifdef OPCPLX
#define OP OPCPLX
#endif
#include "binop_loop.h"

/* (complex, complex) -> complex */
#ifdef CPLXRES
	#define TRES CPLXRES
#else
	#define TRES NumArray_Complex
#endif
#define T1 NumArray_Complex
#define T2 NumArray_Complex
#ifdef OPCPLX
#define OP OPCPLX
#endif
#include "binop_loop.h"


#undef GETOP1
#undef GETOP2
#undef GETOP_IMP

#undef CMD
#undef TCLCMD
#undef OP
#undef OPINT
#undef OPDBL
#undef OPCPLX
#undef INTRES
#undef DBLRES
#undef CPLXRES
