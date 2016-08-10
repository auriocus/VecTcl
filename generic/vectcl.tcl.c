/*
 * vectcl.c --
 */

#include "vectclInt.h"
#include "arrayshape.h"
#include "linalg.h"
#include "fft.h"
#include "svd.h"
#include "eig.h"
#include "schur.h"
#include "bcexecute.h"
#include "vmparser.h"
#include "intconv.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>

/* Copy of this, because it is not exported (but uses only public functionality) */
/*
 *----------------------------------------------------------------
 * Macro used by the Tcl core to clean out an object's internal
 * representation. Does not actually reset the rep's bytes. The ANSI C
 * "prototype" for this macro is:
 *
 * MODULE_SCOPE void	TclFreeIntRep(Tcl_Obj *objPtr);
 *----------------------------------------------------------------
 */

#define TclFreeIntRep(objPtr) \
	if ((objPtr)->typePtr != NULL) { \
		if ((objPtr)->typePtr->freeIntRepProc != NULL) { \
			(objPtr)->typePtr->freeIntRepProc(objPtr); \
		} \
		(objPtr)->typePtr = NULL; \
    }


/*
 * Functions hndling the Tcl_ObjType 
 */

static void		DupNumArrayInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr);
static void		FreeNumArrayInternalRep(Tcl_Obj *listPtr);
static int		SetNumArrayFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr);
static void		UpdateStringOfNumArray(Tcl_Obj *listPtr);
#ifdef LIST_INJECT
static int		SetListFromNumArray(Tcl_Interp *interp, Tcl_Obj *objPtr);
#endif

const Tcl_ObjType NumArrayTclType = {
	"NumArray",			/* name */\
	FreeNumArrayInternalRep,	/* freeIntRepProc */
	DupNumArrayInternalRep,	/* dupIntRepProc */
	UpdateStringOfNumArray,	/* updateStringProc */
	SetNumArrayFromAny		/* setFromAnyProc */
};

Tcl_ObjType * tclListType;
const Tcl_ObjType * tclDoubleType;
const Tcl_ObjType * tclIntType;
#ifndef TCL_WIDE_INT_IS_LONG
const Tcl_ObjType * tclWideIntType;
#endif

Tcl_SetFromAnyProc *listSetFromAny;

const char * NumArray_typename[NumArray_SentinelType+1]=NUMARRAYTYPESTRINGS;
//const char * NumArray_typesuffixes[NumArray_SentinelType+1]=NUMARRAYTYPESUFFIXES;

static int CreateNumArrayInfoFromList(Tcl_Interp *interp, Tcl_Obj* dimlist, NumArrayType dtype, NumArrayInfo **infoptr) {
	/* Create information with dimensions as in dimlist
	 * TODO catch out of memory */
	int d = 0;
	int nDim;
	index_t *dims = NULL;
	
	if (Tcl_ListObjLength(interp, dimlist, &nDim) != TCL_OK) {
		return TCL_ERROR;
	}

	if (nDim == 0) {
		Tcl_SetResult(interp, "Empty dimension list", NULL);
		return TCL_ERROR;
	}
	
	dims = ckalloc(sizeof(index_t)*nDim);

	for (d=0; d<nDim; d++) {
		Tcl_WideInt dim;
		Tcl_Obj *dimObj;
		Tcl_ListObjIndex(NULL, dimlist, d, &dimObj); /* can't fail */
		if (Tcl_GetWideIntFromObj(interp, dimObj, &dim) != TCL_OK) {
			goto cleandims;
		}
		
		/* only the first dim can be zero (for empty vector) */
		if (dim==0 && d!=0) {
			Tcl_SetResult(interp, "Zero dimension", NULL);
			goto cleandims;
		}

		if (dim<0) {
			Tcl_SetResult(interp, "Negative dimension", NULL);
			goto cleandims;
		}	
		
		dims[d]=dim;
	}

	*infoptr = CreateNumArrayInfo(nDim, dims, dtype);
	ckfree(dims);
	return TCL_OK;
cleandims:
	if (dims) ckfree(dims);
	*infoptr = NULL;
	return TCL_ERROR;
}

int NumArrayCompatibleDimensions(NumArrayInfo *info1, NumArrayInfo *info2) {
	/* return 1 if the dimensions are equal apart from singleton dimensions,
	 * i.e. if we can copy from one into the other by iterating */
	int d1=0, d2=0;
	while (d1 < info1->nDim && d2 < info2 -> nDim) {
		if (info1 -> dims[d1] == 1) { d1++; continue; }
		if (info2 -> dims[d2] == 1) { d2++; continue; }
		if (info1 -> dims[d1] != info2 -> dims[d2]) { return 0; }
		d1++; d2++;
	}

	while (d1 < info1->nDim && info1 -> dims[d1] == 1) d1++;
	while (d2 < info2->nDim && info2 -> dims[d2] == 1) d2++;

	return (d1 >= info1->nDim && d2 >= info2->nDim);
}

/*
 * Table of numarray subcommand names and implementations.
 */
typedef struct {
	const char *name;		/* The name of the subcommand. */
	Tcl_ObjCmdProc *proc;	/* The implementation of the subcommand. */
	const char *altname;	/* Name of the command in the namespace */
} EnsembleMap;


static const EnsembleMap implementationMap[] = {
	/* create fresh array from nested lists */
	{"create",	NumArrayCreateCmd,	NULL },
	/* create fresh array initialized to constant */
	{"constfill", NumArrayConstFillCmd, NULL},
	/* create identity matrix */
	{"eye", NumArrayEyeCmd, NULL},
	/* commands that return metadata */
	{"info", NumArrayInfoCmd, "__builtin__info"},
	{"dimensions", NumArrayDimensionsCmd, NULL},
	{"shape", NumArrayShapeCmd, NULL},
	/* element accessors */
	{"get", NumArrayGetCmd, NULL},
	{"set", NumArraySetCmd, "__builtin__set"},
	/* copying and elementwise addition on canonical
	 * arrays for benchmark purposes */
	{"fastcopy", NumArrayFastCopyCmd, NULL},
	{"fastadd", NumArrayFastAddCmd, NULL},
	/* linear regression, for benchmarking */
	{"linreg", NumArrayLinRegCmd, NULL},
	/* commands that change the shape */
	{"reshape", NumArrayReshapeCmd, NULL},
	{"transpose", NumArrayTransposeCmd, NULL},
	{"adjoint", NumArrayAdjointCmd, NULL},
	{"slice", NumArraySliceCmd, NULL},
	{"concat", NumArrayConcatCmd, NULL},
	{"diag", NumArrayDiagCmd, NULL},
	/* data type conversion operators */
	{"int", NumArrayConvIntCmd, NULL},
	{"bool", NumArrayConvBoolCmd, NULL},
	{"int8", NumArrayConvInt8Cmd, NULL},
	{"uint8", NumArrayConvUint8Cmd, NULL},
	{"int16", NumArrayConvInt16Cmd, NULL},
	{"uint16", NumArrayConvUint16Cmd, NULL},
	{"int32", NumArrayConvInt32Cmd, NULL},
	{"uint32", NumArrayConvUint32Cmd, NULL},
	{"int64", NumArrayConvInt64Cmd, NULL},
	{"uint64", NumArrayConvUint64Cmd, NULL},
	{"float32", NumArrayConvFloat32Cmd, NULL},
	{"float64", NumArrayConvFloat64Cmd, NULL},
	{"complex64", NumArrayConvComplex64Cmd, NULL},
	{"complex128", NumArrayConvComplex128Cmd, NULL},

	{"double", NumArrayConvDoubleCmd, NULL},
	{"complex", NumArrayConvComplexCmd, NULL},
	/* elementary manipulations of complex values*/
	{"abs", NumArrayAbsCmd, NULL},
	{"sign", NumArraySignCmd, NULL},
	{"real", NumArrayRealCmd, NULL},
	{"imag", NumArrayImagCmd, NULL},
	{"arg", NumArrayArgCmd, NULL},
	{"conj", NumArrayConjCmd, NULL},
	/* elementwise binary operators */
	{"+", NumArrayPlusCmd, NULL},
	{".+", NumArrayPlusCmd, NULL},
	{"-", NumArrayMinusCmd, NULL},
	{".-", NumArrayMinusCmd, NULL},
	{".*", NumArrayTimesCmd, NULL},
	{"./", NumArrayRdivideCmd, NULL},
	{".\\", NumArrayLdivideCmd, NULL},
	{".^", NumArrayPowCmd, NULL},
	{"binarymin", NumArrayMinCmd, NULL},
	{"binarymax", NumArrayMaxCmd, NULL},
	{"%", NumArrayReminderCmd, NULL},
	/* relation operators */
	{">", NumArrayGreaterCmd, NULL},
	{"<", NumArrayLesserCmd, NULL},
	{">=", NumArrayGreaterEqualCmd, NULL},
	{"<=", NumArrayLesserEqualCmd, NULL},
	{"==", NumArrayEqualCmd, NULL},
	{"!=", NumArrayUnequalCmd, NULL},
	/* boolean operators */
	{"not", NumArrayNotCmd, NULL},
	{"&&", NumArrayAndCmd, NULL},
	{"||", NumArrayOrCmd, NULL},
	/* binary matrix product */
	{"*", NumArrayDotCmd, NULL},
	{"\\", NumArrayBackslashCmd, NULL},
	{"/", NumArraySlashCmd, NULL},
	{"^", NumArrayMatrixPowCmd, NULL},
	{"**", NumArrayMatrixPowCmd, NULL},

	/* elementwise binary assignment operators */
	{"=", NumArraySetAssignCmd, NULL},
	{"+=", NumArrayPlusAssignCmd, NULL},
	{".+=", NumArrayPlusAssignCmd, NULL},
	{"-=", NumArrayMinusAssignCmd, NULL},
	{".-=", NumArrayMinusAssignCmd, NULL},
	{".*=", NumArrayTimesAssignCmd, NULL},
	{"./=", NumArrayRdivideAssignCmd, NULL},
	{".\\=", NumArrayLdivideAssignCmd, NULL},
	{".^=", NumArrayPowAssignCmd, NULL},
	/* elementwise unary minus */
	{"neg", NumArrayNegCmd, NULL},
	/* elementwise elementary transcendental functions */
	{"sin", NumArraySinCmd, NULL},
	{"cos", NumArrayCosCmd, NULL},
	{"tan", NumArrayTanCmd, NULL},
	{"exp", NumArrayExpCmd, NULL},
	{"log", NumArrayLogCmd, NULL},
	{"log10", NumArrayLog10Cmd, NULL},
	{"sqrt", NumArraySqrtCmd, NULL},
	{"sinh", NumArraySinhCmd, NULL},
	{"cosh", NumArrayCoshCmd, NULL},
	{"tanh", NumArrayTanhCmd, NULL},
	/* Inverse circular and hyperbolic functions */
	{"asin", NumArrayAsinCmd, NULL},
	{"acos", NumArrayAcosCmd, NULL},
	{"atan", NumArrayAtanCmd, NULL},
	{"asinh", NumArrayAsinhCmd, NULL},
	{"acosh", NumArrayAcoshCmd, NULL},
	{"atanh", NumArrayAtanhCmd, NULL},
	/* Matrix decompositions */
	{"qreco", NumArrayQRecoCmd, NULL},
	{"eigv", NumArrayEigVCmd, NULL},
	{"eig", NumArrayEigCmd, NULL},
	{"svd1", NumArraySVD1Cmd, NULL},
	{"svd", NumArraySVDCmd, NULL},
	{"schur", NumArraySchurCmd, NULL},
	/* Reductions */
	{"sum", NumArraySumCmd, NULL},
	{"axismin", NumArrayAxisMinCmd, NULL},
	{"axismax", NumArrayAxisMaxCmd, NULL},
	{"mean", NumArrayMeanCmd, NULL},
	{"std", NumArrayStdCmd, NULL},
	{"std1", NumArrayStd1Cmd, NULL},
	{"all", NumArrayAllCmd, NULL},
	{"any", NumArrayAnyCmd, NULL},
	/* FFT */
	{"fft", NumArrayFFTCmd, NULL},
	{"ifft", NumArrayIFFTCmd, NULL},
	/* Execute bytecode */
	{"bcexecute", NumArrayBCExecuteCmd, NULL},
	{NULL, NULL, NULL}
};

/*
 *----------------------------------------------------------------------
 *
 * myTclMakeEnsemble --
 *
 *	Create an ensemble from a table of implementation commands.
 *
 *	Adapted and simplified from a private function in 8.6
 *	
 * Results:
 *	Handle for the new ensemble, or NULL on failure.
 *
 * Side effects:
 */

	static Tcl_Command
myTcl_MakeEnsemble(
		Tcl_Interp *interp,
		const char *cmdname,
		const char *nsname, /* The ensemble name (as explained above) */
		const EnsembleMap map[]) /* The subcommands to create */
{
	Tcl_Command ensemble;
	Tcl_Namespace *ns;
	Tcl_Obj *prefix;
	int i, ensembleFlags = 0;

	/*
	 * Construct the path for the ensemble namespace and create it.
	 */

	ns = Tcl_CreateNamespace(interp, nsname, NULL, NULL);
	if (!ns) {
		Tcl_Panic("unable to create %s namespace!",
				nsname);
	}

	ensemble = Tcl_CreateEnsemble(interp, cmdname, ns, ensembleFlags);

	/*
	 * Create the ensemble mapping dictionary and the ensemble command procs.
	 */

	prefix = Tcl_NewStringObj(nsname, -1);
	Tcl_IncrRefCount(prefix);
	Tcl_AppendToObj(prefix, "::", -1);

	if (ensemble != NULL) {
		Tcl_Obj *mapDict, *fromObj, *toObj;

		mapDict=Tcl_NewObj();
		for (i=0; map[i].name != NULL ; i++) {
			fromObj = Tcl_NewStringObj(map[i].name, -1);
			toObj = Tcl_DuplicateObj(prefix);
			if (map[i].altname != NULL) {
				Tcl_AppendToObj(toObj, map[i].altname, -1);
			} else {
				Tcl_AppendToObj(toObj, map[i].name, -1);
			}
			Tcl_DictObjPut(NULL, mapDict, fromObj, toObj);

			if (map[i].proc) {
				Tcl_CreateObjCommand(interp, Tcl_GetString(toObj), map[i].proc, NULL, NULL);
			}
		}

		Tcl_SetEnsembleMappingDict(interp, ensemble, mapDict);
	}

	Tcl_DecrRefCount(prefix);
	return ensemble;
}

/*
 *----------------------------------------------------------------------
 *
 * myTcl_GetDoubleFromObj --
 *
 *  Copied from the Tcl source, but allow NaN values in the result
 *	Attempt to return a double from the Tcl object "objPtr". If the object
 *	is not already a double, an attempt will be made to convert it to one.
 *
 * Results:
 *	The return value is a standard Tcl object result. If an error occurs
 *	during conversion, an error message is left in the interpreter's
 *	result unless "interp" is NULL.
 *
 * Side effects:
 *	If the object is not already a double, the conversion will free any
 *	old internal representation.
 *
 *----------------------------------------------------------------------
 */

static int
myTcl_GetDoubleFromObj(
    Tcl_Interp *interp,         /* Used for error reporting if not NULL. */
    register Tcl_Obj *objPtr,	/* The object from which to get a double. */
    register double *dblPtr)	/* Place to store resulting double. */
{
	if (objPtr->typePtr == tclDoubleType) {
		*dblPtr = (double) objPtr->internalRep.doubleValue;
		return TCL_OK;
	}
	
	if (Tcl_ConvertToType(interp, objPtr, tclDoubleType) != TCL_OK) {
		return TCL_ERROR;
	}

	/* This is really buggy & braindead. SetDoubleFromAny fails to convert
	 * a string into the double type, if it fits into an integer. */
	
	if (objPtr->typePtr == tclDoubleType) {
		*dblPtr = (double) objPtr->internalRep.doubleValue;
		return TCL_OK;
	} else if (objPtr->typePtr == tclIntType) {
		*dblPtr = objPtr->internalRep.longValue;
		return TCL_OK;
	}
#ifndef TCL_WIDE_INT_IS_LONG
	if (objPtr->typePtr == tclWideIntType) {
		*dblPtr = (double) objPtr->internalRep.wideValue;
		return TCL_OK;
	}
#endif
	/* Any other case is handled by the standard code */
	return Tcl_GetDoubleFromObj(interp, objPtr, dblPtr);
}

int
NumArrayCreateCmd(
		ClientData dummy,
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv)
{
	Tcl_Obj *naObj;
	
	if (objc != 2) {
		Tcl_SetResult(interp, "numarray create <valuelist>", NULL);
		return TCL_ERROR;
	}
	
	naObj = objv[1];

	if (Tcl_ConvertToType(interp, naObj, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}

	Tcl_SetObjResult(interp, naObj);
	return TCL_OK;
}

int
NumArrayConstFillCmd(
		ClientData dummy,
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv)
{
	Tcl_Obj *naObj;
	double value;
	NumArrayInfo *info; 
	NumArraySharedBuffer *sharedbuf;

	if (objc < 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "value dim1 ?dim2 ...?");
		return TCL_ERROR;
	}
	
	if (myTcl_GetDoubleFromObj(interp, objv[1], &value) != TCL_OK) {
		return TCL_ERROR;
	}
	
	int ndim = objc-2;
	index_t *dims = ckalloc(sizeof(index_t)*ndim);
	index_t nelem = 1;
	int d;
	for (d=0; d<ndim; d++) {
		Tcl_WideInt dim;
		if (Tcl_GetWideIntFromObj(interp, objv[2+d], &dim) != TCL_OK) {
			goto cleandims;
		}
		/* only the first dim can be zero (for empty vector) */
		if (dim==0 && d!=0) {
			Tcl_SetResult(interp, "Zero dimension", NULL);
			goto cleandims;
		}

		if (dim<0) {
			Tcl_SetResult(interp, "Negative dimension", NULL);
			goto cleandims;
		}
		dims[d] = dim;
		nelem *= dim;
	}

	info = CreateNumArrayInfo(ndim, dims, NumArray_Float64);
	sharedbuf = NumArrayNewSharedBuffer(info->bufsize);

	/* Fill the buffer */
	double *bufptr = (double*) NumArrayGetPtrFromSharedBuffer(sharedbuf);
	index_t i;
	for (i=0; i<nelem; i++) {
		bufptr[i] = value;
	}

	/* put into result */
	naObj = Tcl_NewObj();
	NumArraySetInternalRep(naObj, sharedbuf, info);
	Tcl_SetObjResult(interp, naObj);
	
	ckfree(dims);
	return TCL_OK;

cleandims:
	ckfree(dims);
	return TCL_ERROR;
}

int
NumArrayEyeCmd(
		ClientData dummy,
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv)
{
	Tcl_Obj *naObj;
	index_t m, n;
	NumArrayInfo *info; 
	NumArraySharedBuffer *sharedbuf;

	if (objc != 2 && objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "dim1 ?dim2?");
		return TCL_ERROR;
	}
	
	Tcl_WideInt temp;
	if (Tcl_GetWideIntFromObj(interp, objv[1], &temp) != TCL_OK) {
		return TCL_ERROR;
	}
	m=temp;

	if (objc == 2) {
		n=m; 
	} else {
		if (Tcl_GetWideIntFromObj(interp, objv[2], &temp) != TCL_OK) {
			return TCL_ERROR;
		}
		n=temp;
	}

	if (m<0 || n<0) {
		Tcl_SetResult(interp, "Dimensions must be positive", NULL);
		return TCL_ERROR;
	}

	index_t *dims = ckalloc(sizeof(index_t)*2);
	dims[0]=m; dims[1]=n;
	info = CreateNumArrayInfo((n==1)? 1:2, dims, NumArray_Float64);
	sharedbuf = NumArrayNewSharedBuffer(info->bufsize);

	/* Fill the buffer */
	double *bufptr = (double*) NumArrayGetPtrFromSharedBuffer(sharedbuf);
	index_t i;
	for (i=0; i<m; i++) {
		index_t j; 
		for (j=0; j<n; j++) {
			*bufptr++ = ((i==j)?1.0:0.0);
		}
	}

	/* put into result */
	naObj = Tcl_NewObj();
	NumArraySetInternalRep(naObj, sharedbuf, info);
	Tcl_SetObjResult(interp, naObj);
	
	ckfree(dims);
	return TCL_OK;
}

/* return the metadata from the info object as dictionary */
int
NumArrayInfoCmd(
		ClientData dummy,
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv)
{
	Tcl_Obj *naObj, *plist, *infodict;
	NumArrayInfo * info; NumArraySharedBuffer *sharedbuf;
	int i;

	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "<numarray>");
		return TCL_ERROR;
	}

	naObj = objv[1];
	if (Tcl_ConvertToType(interp, naObj, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}

	info = naObj->internalRep.twoPtrValue.ptr2;

	sharedbuf = naObj -> internalRep.twoPtrValue.ptr1;
	infodict = Tcl_NewDictObj();

	plist = Tcl_NewObj();
	for (i=0; i<info->nDim; i++) {
		Tcl_ListObjAppendElement(interp, plist,  Tcl_NewWideIntObj(info->dims[i]));
	}
	Tcl_DictObjPut(interp, infodict, Tcl_NewStringObj("dimensions", -1), plist);
	
	Tcl_DictObjPut(interp, infodict, Tcl_NewStringObj("offset", -1), Tcl_NewWideIntObj(info->offset));

	plist = Tcl_NewObj();
	for (i=0; i<info->nDim; i++) {
		Tcl_ListObjAppendElement(interp, plist,  Tcl_NewWideIntObj(info->pitches[i]));
	}
	Tcl_DictObjPut(interp, infodict, Tcl_NewStringObj("pitches", -1), plist);
	
	Tcl_DictObjPut(interp, infodict, Tcl_NewStringObj("canonical", -1), Tcl_NewBooleanObj(info->canonical));
	Tcl_DictObjPut(interp, infodict, Tcl_NewStringObj("bufsize", -1), Tcl_NewWideIntObj(info->bufsize));
	Tcl_DictObjPut(interp, infodict, Tcl_NewStringObj("refcount", -1), Tcl_NewIntObj(sharedbuf->refcount));
	Tcl_DictObjPut(interp, infodict, Tcl_NewStringObj("type", -1), Tcl_NewIntObj(info->type));

	Tcl_SetObjResult(interp, infodict);
	return TCL_OK;
}


/* TODO return  a single element */
int
NumArrayGetCmd(
		ClientData dummy,
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv)
{
	NumArrayInfo *info;
	char *bufptr; int d;

	if (objc < 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "numarray ind ?ind ...?");
		return TCL_ERROR;
	}
	
	Tcl_Obj *naObj=objv[1];

	if (!(bufptr = NumArrayGetPtrFromObj(interp, naObj))) {
		return TCL_ERROR;
	}

	info = naObj -> internalRep.twoPtrValue.ptr2;
	
	if (info -> nDim != objc - 2) {
		Tcl_SetResult(interp, "Dimension mismatch", NULL);
		return TCL_ERROR;
	}

	
	for (d=0; d < info->nDim; d++) {
		index_t ind;
		Tcl_WideInt temp;
		if (Tcl_GetWideIntFromObj(interp, objv[d+2], &temp) != TCL_OK) {
			return TCL_ERROR;
		}
		ind=temp;

		/* negative index counts backward from the end */
		if (ind < 0) {
			ind += info->dims[d];
		}

		if (ind >= info->dims[d] || ind < 0) {
			Tcl_SetResult(interp, "Index out of bounds", NULL);
			return TCL_ERROR;
		}

		bufptr += ind*info->pitches[d];
	}

	switch (info->type) {
		case NumArray_Float64: {
			double value = *((double *) bufptr);
			Tcl_SetObjResult(interp, Tcl_NewDoubleObj(value));
			break;
		}
		case NumArray_Int: {
			NaWideInt value = *((NaWideInt *) bufptr);
			Tcl_SetObjResult(interp, Tcl_NewLongObj(value));
			break;
		}
		case NumArray_Complex128: {
			NumArray_Complex value = *((NumArray_Complex *) bufptr);
			Tcl_SetObjResult(interp, NumArray_NewComplexObj(value));
			break;
		}
		default: {
			RESULTPRINTF(("Error: unknown data type %d", info->type));
			return TCL_ERROR;
		}

	}

	return TCL_OK;
}

/* set a single element */
int
NumArraySetCmd(
		ClientData dummy,
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv)
{
	Tcl_Obj *naObj, *resultPtr;
	NumArrayInfo *info;
	NumArraySharedBuffer *sharedbuf;

	char *bufptr;
	int allocobj=0;
	int d;

	if (objc < 4) {
		Tcl_WrongNumArgs(interp, 1, objv, "<numarrvariable> ?index index ...? value");
		return TCL_ERROR;
	}

	naObj = Tcl_ObjGetVar2(interp, objv[1], NULL, TCL_LEAVE_ERR_MSG);
	if (naObj == NULL) {
		return TCL_ERROR;
	}

	if (Tcl_IsShared(naObj)) {
		naObj = Tcl_DuplicateObj(naObj);
		allocobj = 1;
	}

	if (Tcl_ConvertToType(interp, naObj, &NumArrayTclType) != TCL_OK) {
		goto cleanobj;
	}

	/* copy on write */
	NumArrayEnsureWriteable(naObj);

	sharedbuf = naObj->internalRep.twoPtrValue.ptr1;
	info = naObj->internalRep.twoPtrValue.ptr2;
	bufptr = NumArrayGetPtrFromSharedBuffer(sharedbuf);
	bufptr += info->offset;

	if (objc-3 != info->nDim) {
		Tcl_SetResult(interp, "Dimension mismatch.", NULL);
		goto cleanobj;
	}

	/* compute index into buffer */
	for (d=0; d<info->nDim; d++) {
		index_t index;
		Tcl_WideInt temp;
		if (Tcl_GetWideIntFromObj(interp, objv[d+2], &temp) != TCL_OK) { 
			goto cleanobj;
		}
		index=temp;

		if (index<0 || index >= info->dims[d]) {
			Tcl_SetResult(interp, "Index out of range.", NULL);
			goto cleanobj;
		}

		bufptr += info->pitches[d]*index;
	}

	/* get value to set */
	switch (info->type) {
		case NumArray_Float64: {
			double value;
			if (myTcl_GetDoubleFromObj(interp, objv[objc-1], &value) != TCL_OK) {
				goto cleanobj;
			}

			/* set value */
			*((double*)bufptr) = value;
			break;
		}
		case NumArray_Int: {
			Tcl_WideInt temp;
			if (Tcl_GetWideIntFromObj(interp, objv[objc-1], &temp) != TCL_OK) {
				goto cleanobj;
			}

			/* set value */
			*((NaWideInt*)bufptr) = temp;
			break;
		}
		case NumArray_Complex128: {
			NumArray_Complex value;
			if (NumArray_GetComplexFromObj(interp, objv[objc-1], &value) != TCL_OK) {
				goto cleanobj;
			}

			/* set value */
			*((NumArray_Complex*)bufptr) = value;
			break;
		}
		default: {
			RESULTPRINTF(("Error: unknown data type %d", info->type));
			return TCL_ERROR;
		}

	}

	Tcl_InvalidateStringRep(naObj);

	/* put back into variable and interp result */
	resultPtr = Tcl_ObjSetVar2(interp, objv[1], NULL, naObj, TCL_LEAVE_ERR_MSG);
	if (resultPtr == NULL) {
		return TCL_ERROR;
	}
    Tcl_SetObjResult(interp, resultPtr);
    return TCL_OK;

cleanobj:
	if (allocobj) Tcl_DecrRefCount(naObj);
	return TCL_ERROR;
}

int NumArrayFastCopyCmd(
		ClientData dummy,
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv)
{
	Tcl_Obj *naObj, *value, *resultPtr;
	int allocobj = 0; int d=0;

	NumArraySharedBuffer * sharedbuf, *valuebuf;
	NumArrayInfo *info, *valueinfo;
	
	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "numarrayvar numarray");
		return TCL_ERROR;
	}
	
	value = objv[2];

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
	
	/* Check if the arrays are compatible and can be copied */
	if (info -> type != valueinfo -> type) {
		Tcl_SetResult(interp, "Incompatible datatypes", NULL);
		return TCL_ERROR;
	}

	if (info->nDim != valueinfo -> nDim) {
		Tcl_SetResult(interp, "Dimension mismatch", NULL);
		return TCL_ERROR;
	}	

	/* compare dimensions */
	for (d=0; d<info->nDim; d++) {
		if (info->dims[d] != valueinfo -> dims[d]) {
			Tcl_SetResult(interp, "Size mismatch", NULL);
			return TCL_ERROR;
		}
	}

	/* check for canonical source array */
	if (!valueinfo -> canonical) {
		Tcl_SetResult(interp, "Source must be a canonical array", NULL);
		return TCL_ERROR;
	}
		
	/* in-place - handle sharing */
	if (Tcl_IsShared(naObj)) {
		naObj = Tcl_DuplicateObj(naObj);
		allocobj = 1;
	}

	NumArrayEnsureWriteable(naObj);

	info = naObj -> internalRep.twoPtrValue.ptr2;
	sharedbuf = naObj -> internalRep.twoPtrValue.ptr1;
	
	valuebuf = value -> internalRep.twoPtrValue.ptr1;

	if (info->bufsize != valueinfo -> bufsize) {
		Tcl_SetResult(interp, "Internal error", NULL);
		goto cleanobj;
	}

	/* copy by memcopying */
	memcpy(NumArrayGetPtrFromSharedBuffer(sharedbuf), 
		NumArrayGetPtrFromSharedBuffer(valuebuf), 
		info -> bufsize);

	Tcl_InvalidateStringRep(naObj);
	resultPtr = Tcl_ObjSetVar2(interp, objv[1], NULL, naObj, TCL_LEAVE_ERR_MSG);
	if (resultPtr == NULL) {
		goto cleanobj;
	}

    Tcl_SetObjResult(interp, resultPtr);
    return TCL_OK;

cleanobj:
	if (allocobj) Tcl_DecrRefCount(naObj);
	return TCL_ERROR;
}

int NumArrayLinRegCmd(
		ClientData dummy,
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv)
{
	Tcl_Obj *xval, *yval, *result;

	NumArraySharedBuffer *xbuf, *ybuf;
	NumArrayInfo *xvalinfo, *yvalinfo;
	
	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "vectorx vectory");
		return TCL_ERROR;
	}
	
	xval = objv[1];
	yval = objv[2];

	if (Tcl_ConvertToType(interp, xval, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}
	
	if (Tcl_ConvertToType(interp, yval, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}
	
	xbuf = xval -> internalRep.twoPtrValue.ptr1;
	ybuf = yval -> internalRep.twoPtrValue.ptr1;

	xvalinfo = xval -> internalRep.twoPtrValue.ptr2;
	yvalinfo = yval -> internalRep.twoPtrValue.ptr2;

	/* Check that we have two double vectors */
	if (xvalinfo -> type != NumArray_Float64 || yvalinfo -> type != NumArray_Float64) {
		Tcl_SetResult(interp, "Datatype must be double", NULL);
		return TCL_ERROR;
	}

	if (xvalinfo->nDim != 1 || yvalinfo -> nDim != 1) {
		Tcl_SetResult(interp, "Input data must be vectors", NULL);
		return TCL_ERROR;
	}	
	
	if (xvalinfo->dims[0] != yvalinfo -> dims[0]) {
		Tcl_SetResult(interp, "Input data must have the same length", NULL);
		return TCL_ERROR;
	}	

	index_t length = xvalinfo->dims[0];
	index_t xpitch = xvalinfo->pitches[0]/sizeof(double);
	index_t ypitch = yvalinfo->pitches[0]/sizeof(double);

	/* add value to dest by simple loop */
	double *x= (double *)NumArrayGetPtrFromSharedBuffer(xbuf);
	double *y= (double *)NumArrayGetPtrFromSharedBuffer(ybuf);
	
	/* Now compute the mean values */
	index_t i;
	double xm=0.0; double ym=0.0;
	for (i=0; i<length; i++) {
		xm+=x[i*xpitch]; 
		ym+=y[i*ypitch];
	}
	xm /= length;
	ym /= length;

	double xsum = 0.0;
	double ysum = 0.0;
	for (i=0; i<length; i++) {
		double dx=x[i*xpitch]-xm; 
		double dy=y[i*ypitch]-ym;
		xsum += dx*dy;
		ysum += dx*dx;
	}
	
	double b = xsum / ysum;
	double a = ym - b*xm;

	result = Tcl_NewObj();
	Tcl_ListObjAppendElement(interp, result,  Tcl_NewDoubleObj(a));
	Tcl_ListObjAppendElement(interp, result,  Tcl_NewDoubleObj(b));
	
	
    Tcl_SetObjResult(interp, result);
    return TCL_OK;
}



int NumArrayFastAddCmd(
		ClientData dummy,
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv)
{
	Tcl_Obj *naObj, *value, *resultPtr;
	int allocobj = 0; int d=0;

	NumArraySharedBuffer * sharedbuf, *valuebuf;
	NumArrayInfo *info, *valueinfo;
	
	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "numarrayvar numarray");
		return TCL_ERROR;
	}
	
	value = objv[2];

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
	
	/* Check if the arrays are compatible and can be copied */
	if (info -> type != NumArray_Float64 || valueinfo -> type != NumArray_Float64) {
		Tcl_SetResult(interp, "Datatype must be double", NULL);
		return TCL_ERROR;
	}

	if (info->nDim != valueinfo -> nDim) {
		Tcl_SetResult(interp, "Dimension mismatch", NULL);
		return TCL_ERROR;
	}	

	/* compare dimensions */
	for (d=0; d<info->nDim; d++) {
		if (info->dims[d] != valueinfo -> dims[d]) {
			Tcl_SetResult(interp, "Size mismatch", NULL);
			return TCL_ERROR;
		}
	}

	/* check for canonical source array */
	if (!valueinfo -> canonical) {
		Tcl_SetResult(interp, "Source must be a canonical array", NULL);
		return TCL_ERROR;
	}
		
	/* in-place - handle sharing */
	if (Tcl_IsShared(naObj)) {
		naObj = Tcl_DuplicateObj(naObj);
		allocobj = 1;
	}

	NumArrayEnsureWriteable(naObj);

	info = naObj -> internalRep.twoPtrValue.ptr2;
	sharedbuf = naObj -> internalRep.twoPtrValue.ptr1;
	
	valuebuf = value -> internalRep.twoPtrValue.ptr1;

	if (info->bufsize != valueinfo -> bufsize) {
		Tcl_SetResult(interp, "Internal error", NULL);
		goto cleanobj;
	}

	/* Compute number of elements */
	index_t nelem=1;
	for (d=0; d<info->nDim; d++) {
		nelem *= info->dims[d];
	}

	/* add value to dest by simple loop */
	double *dest= (double *)NumArrayGetPtrFromSharedBuffer(sharedbuf);
	double *src= (double *)NumArrayGetPtrFromSharedBuffer(valuebuf);
	
	index_t i;
	for (i=0; i<nelem; i++) {
		(*dest++)+=*src++;
	}

	Tcl_InvalidateStringRep(naObj);
	resultPtr = Tcl_ObjSetVar2(interp, objv[1], NULL, naObj, TCL_LEAVE_ERR_MSG);
	if (resultPtr == NULL) {
		goto cleanobj;
	}

    Tcl_SetObjResult(interp, resultPtr);
    return TCL_OK;

cleanobj:
	if (allocobj) Tcl_DecrRefCount(naObj);
	return TCL_ERROR;
}


int
NumArrayReshapeCmd(
		ClientData dummy,
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv)
{
	Tcl_Obj *naObj;
	NumArrayInfo *info; NumArrayInfo *reshapeinfo;
	int i; index_t nelem=1; index_t reshapenelem=1; int reshapedim;
	int allocobj = 0;

	if (objc < 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "numarray dim1 ?dim2 ...?");
		return TCL_ERROR;
	}

    naObj = objv[1];

	if (Tcl_ConvertToType(interp, naObj, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}

	if (Tcl_IsShared(naObj)) {
		naObj = Tcl_DuplicateObj(naObj);
		allocobj = 1;
	}

	/* handle slices by unsharing/canonalizing */
	NumArrayEnsureContiguous(naObj);

	info = naObj->internalRep.twoPtrValue.ptr2;
	
	/* compute number of elements */
	for (i=0; i<info->nDim; i++) {
		nelem *= info->dims[i];
	}

	reshapedim = objc-2;
	reshapeinfo = CreateNumArrayInfo(reshapedim, NULL, info -> type);
	reshapeinfo -> nDim = reshapedim;
	reshapeinfo -> bufsize = info -> bufsize;

	for (i=0; i<reshapedim; i++) {
		int dim;
		if (Tcl_GetIntFromObj(interp, objv[2+i], &dim) != TCL_OK) {
			goto cleaninfo;
		}	
		reshapenelem *= dim;
		reshapeinfo -> dims[i] = dim;
	}

	if (nelem != reshapenelem) {
		Tcl_SetResult(interp, "Dimension mismatch", NULL);
		goto cleaninfo;
	}

	reshapeinfo -> pitches[reshapedim-1] = NumArrayType_SizeOf(info -> type);
	for (i=reshapedim-2; i>=0; i--) {
		reshapeinfo -> pitches[i] = 
			reshapeinfo -> pitches[i+1] * reshapeinfo ->dims[i+1];
	}
	
	/* no error so far - replace info in variable */
	DeleteNumArrayInfo(info);
	naObj -> internalRep.twoPtrValue.ptr2 = reshapeinfo;
	Tcl_InvalidateStringRep(naObj);

    Tcl_SetObjResult(interp, naObj);
    return TCL_OK;

cleaninfo:
	DeleteNumArrayInfo(reshapeinfo);
	if (allocobj) Tcl_DecrRefCount(naObj);
	return TCL_ERROR;
}

static void DupNumArrayInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr) {
	NumArrayInfo *info = srcPtr -> internalRep.twoPtrValue.ptr2;
	copyPtr -> internalRep.twoPtrValue.ptr2 = DupNumArrayInfo (info);
	
	NumArraySharedBuffer * sharedbuf = srcPtr -> internalRep.twoPtrValue.ptr1;
	copyPtr -> internalRep.twoPtrValue.ptr1 = sharedbuf;
	NumArraySharedBufferIncrRefcount(sharedbuf);
	copyPtr -> typePtr = &NumArrayTclType;
	DEBUGPRINTF(("DupNumArrayInternalRep, refocunt %d\n", sharedbuf -> refcount));
}

static void FreeNumArrayInternalRep(Tcl_Obj *naPtr) {
	NumArrayInfo *info = naPtr -> internalRep.twoPtrValue.ptr2;
	DeleteNumArrayInfo(info);
	NumArrayDecrRefcount(naPtr);
}

static int
SingletonDimension(Tcl_Obj* list) {
	/* test a list for singleton dimension */
	int length; int llength; int lengthFirstElement;
	Tcl_Obj* first;
	
	Tcl_GetStringFromObj(list, &length);
	
	if (Tcl_ListObjLength(NULL, list, &llength) != TCL_OK) {
		return 0;
	}

	if (llength == 0) return 0;

	if (Tcl_ListObjIndex(NULL, list, 0, &first) != TCL_OK) {
		return 0;
	}
	
	Tcl_GetStringFromObj(first, &lengthFirstElement);

	return (length != lengthFirstElement);
}

static int 
ScanNumArrayDimensionsFromValue(Tcl_Interp *interp, Tcl_Obj* valobj, Tcl_Obj **result, NumArrayType *dtype) {
	Tcl_Obj *itobj, *dimlist;
	int nDim = 0; int allocobj = 0;


	dimlist = Tcl_NewListObj(0, NULL);
	int firstdim;
	
	/* Try if this is already a NumArray */
	if (valobj->typePtr == &NumArrayTclType) {
		int d;
		NumArrayInfo *info = valobj -> internalRep.twoPtrValue.ptr2;
		for (d=0; d<info->nDim; d++) {
			Tcl_ListObjAppendElement(interp, dimlist, Tcl_NewWideIntObj(info->dims[d]));
		}
		*result=dimlist;
		*dtype = info->type;
		return TCL_OK;
	}

	/* Try if this is an empty object/list */
	if (Tcl_ListObjLength(interp, valobj, &firstdim) != TCL_OK) {
		return TCL_ERROR;
	}

	if (firstdim == 0) {
		Tcl_ListObjAppendElement(interp, dimlist, Tcl_NewWideIntObj(0));
		*result=dimlist;
		*dtype=NumArray_Int;
		return TCL_OK;
	}


	if (Tcl_IsShared(valobj)) {
		valobj = Tcl_DuplicateObj(valobj);
		Tcl_IncrRefCount(valobj);
		allocobj = 1;
	}

	itobj = valobj;
	nDim = 0;
	/* TODO: Remove dimension limit 
	 * if it works reliably */
	while (nDim<20) {
		if (itobj -> typePtr == tclDoubleType || itobj -> typePtr == tclIntType) {
			/* we have arrived at the leaf */
			/* Handle case of a single number,  */
			/* else just break out of the loop */
			if (nDim==0) {
				nDim=1;
				Tcl_ListObjAppendElement(interp, dimlist, Tcl_NewWideIntObj(1));
			}
			if (itobj -> typePtr == tclDoubleType) {
				*dtype = NumArray_Float64;
			}
			if (itobj -> typePtr == tclIntType) {
				*dtype = NumArray_Int;
			}
			/* complex doesn't have a Tcl type
			 */

			break;
		} else if (itobj -> typePtr == tclListType) {
			/* there is one more level */
			int length;
			Tcl_Obj* next;
			if (Tcl_ListObjLength(interp, itobj, &length) != TCL_OK) {
				goto cleanobj;
			}

			if (length < 1) {
				Tcl_SetResult(interp, "Zero length dimension", NULL);
				goto cleanobj;
			}
			
			/* can't fail */
			if (Tcl_ListObjAppendElement(interp, dimlist, Tcl_NewIntObj(length)) != TCL_OK) {
				goto cleanobj;
			}
			if (Tcl_ListObjIndex(interp, itobj, 0, &next) != TCL_OK) {
				goto cleanobj;
			}

			itobj = next;
			nDim++;
		} else {
			/* treat everything else as a string */
			double dummy_float64;
			long dummy_long;
			NumArray_Complex dummy_complex128;
			/* try to convert to int, then double, then complex */
			if (Tcl_GetLongFromObj(interp, itobj, &dummy_long) == TCL_OK) {
				/* 1st: Try to convert to int. If succeeds, we are at the leaf
				 * Handle case of a single number, 
				 * else just break out of the loop */
				if (nDim==0) {
					nDim=1;
					Tcl_ListObjAppendElement(interp, dimlist, Tcl_NewIntObj(1));
				}
				*dtype = NumArray_Int;
				break;
			}

			if (myTcl_GetDoubleFromObj(interp, itobj, &dummy_float64) == TCL_OK) {
				/* 2nd: Try to convert to double. If succeeds, we are at the leaf
				 * Handle case of a single number, 
				 * else just break out of the loop */
				if (nDim==0) {
					nDim=1;
					Tcl_ListObjAppendElement(interp, dimlist, Tcl_NewIntObj(1));
				}
				*dtype = NumArray_Float64;
				break;
			}

			if (NumArray_GetComplexFromObj(interp, itobj, &dummy_complex128) == TCL_OK) {
				/* 2nd: Try to convert to double. If succeeds, we are at the leaf
				 * Handle case of a single number, 
				 * else just break out of the loop */
				if (nDim==0) {
					nDim=1;
					Tcl_ListObjAppendElement(interp, dimlist, Tcl_NewIntObj(1));
				}
				*dtype = NumArray_Complex128;
				break;
			} else {
				/* now the tricky part. Converting to list only fails
				 * for a malformed list. But a single non-double element
				 * like "foo", can be converted to a single-element list
				 * if ([string length $foo] == [string length [lindex $foo 0]])
				 * then we are at the leaf. */
				int llength;
				if (Tcl_ListObjLength(interp, itobj, &llength) != TCL_OK) {
					goto cleanobj;
				}
				
				if (llength == 0) {
					Tcl_SetResult(interp, "Zero length dimension", NULL);
					goto cleanobj;
				}

				if (llength > 1 || SingletonDimension(itobj)) {
					/* it is a dimension */
					
					Tcl_Obj *next;
					if (Tcl_ListObjAppendElement(interp, dimlist, Tcl_NewIntObj(llength)) != TCL_OK) {
						goto cleanobj;
					}
					if (Tcl_ListObjIndex(interp, itobj, 0, &next) != TCL_OK) {
						goto cleanobj;
					}

					itobj = next;
					nDim++;
				} else {
					/* Can't be identified? */
					*dtype = -1;
					break;
				}
			}
				
		}
	}
	*result = dimlist;
	if (allocobj) Tcl_DecrRefCount(valobj);
	return TCL_OK;
cleanobj:
	if (allocobj) Tcl_DecrRefCount(valobj);
	Tcl_DecrRefCount(dimlist);
	return TCL_ERROR;
}

int
NumArrayShapeCmd(
		ClientData dummy,
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv)
{
	NumArrayInfo *info;
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "numarray");
		return TCL_ERROR;
	}

	if (Tcl_ConvertToType(interp, objv[1], &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}
	
	info = objv[1] -> internalRep.twoPtrValue.ptr2;
	
	Tcl_Obj *result = Tcl_NewObj();
	int d;
	for (d=0; d<info->nDim; d++) {
		Tcl_ListObjAppendElement(interp, result,  Tcl_NewWideIntObj(info->dims[d]));
	}
	Tcl_SetObjResult(interp, result);
	return TCL_OK;
}


int
NumArrayDimensionsCmd(
		ClientData dummy,
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv)
{
	NumArrayInfo *info;
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "numarray");
		return TCL_ERROR;
	}

	if (Tcl_ConvertToType(interp, objv[1], &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}
	
	info = objv[1] -> internalRep.twoPtrValue.ptr2;
	
	Tcl_SetObjResult(interp, Tcl_NewIntObj(info->nDim));
	return TCL_OK;
}

int
NumArrayTransposeAdjointCmd(ClientData dummy, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv, int adjoint);
	
int
NumArrayAdjointCmd(
		ClientData dummy,
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv)
{

	return NumArrayTransposeAdjointCmd(dummy, interp, objc, objv, 1);
}

int
NumArrayTransposeCmd(
		ClientData dummy,
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv)
{

	return NumArrayTransposeAdjointCmd(dummy, interp, objc, objv, 0);
}


int
NumArrayTransposeAdjointCmd(
		ClientData dummy,
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv, int adjoint)
{	
	Tcl_Obj *naObj;
	NumArrayInfo *info, *transposeinfo;
	int dim1, dim2;
	int allocobj = 0;

	if (objc != 2 && objc != 4) {
		Tcl_WrongNumArgs(interp, 1, objv, "numarray ?dim1 dim2?");
		return TCL_ERROR;
	}
	
    naObj = objv[1];
	
	if (objc == 2 ) {
		dim1 = 0;
		dim2 = 1;
	}

	if (objc == 4) {
		if (Tcl_GetIntFromObj(interp, objv[2], &dim1) != TCL_OK) {
			return TCL_ERROR;
		}
		
		if (Tcl_GetIntFromObj(interp, objv[3], &dim2) != TCL_OK) {
			return TCL_ERROR;
		}

		if (dim1 == dim2) {
			Tcl_SetObjResult(interp, naObj);
			return TCL_OK;
		}
	}

	if (Tcl_ConvertToType(interp, naObj, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}

	if (Tcl_IsShared(naObj)) {
		naObj = Tcl_DuplicateObj(naObj);
		allocobj = 1;
	}

	info = naObj->internalRep.twoPtrValue.ptr2;
	
	/* handle special cases: 
	 * 1D transposition creates 2D array 
	 * empty object remains empty */

	if (info->nDim == 1) {
		if (info -> dims[0] == 0) {
			/* empty object. Just cleanup and leave */
			Tcl_SetObjResult(interp, naObj);
			return TCL_OK;
		}
		
		/* Check for scalar */
		if (info -> dims[0] == 1) {
			transposeinfo = DupNumArrayInfo(info);
			/* that's it */
		} else if ((dim1 == 0 && dim2 ==1) || (dim1 == 1 && dim2 == 0)) {
			/* in case of a columnvector,
			 * create rowvector, i.e. 1xN 2D array */
			transposeinfo = CreateNumArrayInfo(2, NULL, info->type);
			transposeinfo -> bufsize = info -> bufsize;
			/* a rowvector from canonical column vector 
			 * is in canonical form, too! */
			transposeinfo -> canonical = info -> canonical;
			transposeinfo -> dims[0] = 1;
			transposeinfo -> dims[1] = info->dims[0];

			transposeinfo -> offset = info->offset;

			transposeinfo -> pitches[0] = info->pitches[0]*info->dims[0];
			transposeinfo -> pitches[1] = info->pitches[0];

		} else {
			Tcl_SetResult(interp, "Dimension index out of range for columnvector", NULL);
			goto cleanobj;
		}
	} else if (info->nDim == 2 && info -> dims[0] == 1) {
		/* rowvector, to be transposed back to columnvector */
		if ((dim1 == 0 && dim2 ==1) || (dim1 == 1 && dim2 == 0)) {
			transposeinfo = CreateNumArrayInfo(1, NULL, info->type);
			transposeinfo -> bufsize = info -> bufsize;
			
			transposeinfo -> canonical = info -> canonical;
			transposeinfo -> dims[0] = info->dims[1];
			transposeinfo -> offset = info->offset;
			transposeinfo -> pitches[0] = info->pitches[1];
		} else {
			Tcl_SetResult(interp, "Dimension index out of range for columnvector", NULL);
			goto cleanobj;
		}
	} else {
		/* General case. 
		 * Swap dims, offsets, and pitches for the given dimensions */
		
		if (dim1 < 0 || dim1 >= info->nDim || dim2 < 0 || dim2 >= info->nDim) {
			Tcl_SetResult(interp, "Dimension index out of range", NULL);
			goto cleanobj;
		}

		transposeinfo = DupNumArrayInfo(info);
		
		transposeinfo -> dims[dim1] = info -> dims[dim2];
		transposeinfo -> dims[dim2] = info -> dims[dim1];

		transposeinfo -> pitches[dim1] = info -> pitches[dim2];
		transposeinfo -> pitches[dim2] = info -> pitches[dim1];

		transposeinfo -> canonical = 0;
	}

	
	/* no error so far */
	if (info -> type == NumArray_Complex128 && adjoint) {
		/* for complex values and if adjoint is requested, 
		 * copy data and conjugate values.*/
		NumArrayInfo *conjinfo = CreateNumArrayInfo(transposeinfo->nDim, transposeinfo->dims, info->type);
		NumArraySharedBuffer *conjbuf = NumArrayNewSharedBuffer(conjinfo -> bufsize);
		NumArraySharedBuffer *srcbuf = naObj -> internalRep.twoPtrValue.ptr1;

		NumArrayIterator it;
		NumArray_Complex *bufptr = (NumArray_Complex *)NumArrayGetPtrFromSharedBuffer(conjbuf);
		NumArrayIteratorInit(transposeinfo, srcbuf, &it);
		for (; !NumArrayIteratorFinished(&it); NumArrayIteratorAdvance(&it)) {
			*bufptr++ = NumArray_ComplexConj(NumArrayIteratorDeRefComplex(&it));
		}
		NumArraySharedBufferDecrRefcount(srcbuf);
		NumArraySetInternalRep(naObj, conjbuf, conjinfo);
		DeleteNumArrayInfo(transposeinfo);
		DeleteNumArrayInfo(info);
	} else {
		/* replace info in variable */
		DeleteNumArrayInfo(info);
		naObj -> internalRep.twoPtrValue.ptr2 = transposeinfo;
		Tcl_InvalidateStringRep(naObj);
	}
    Tcl_SetObjResult(interp, naObj);
    return TCL_OK;

/*cleaninfo: 
	DeleteNumArrayInfo(transposeinfo); */

cleanobj:
	if (allocobj) Tcl_DecrRefCount(naObj);
	return TCL_ERROR;
}

int
NumArraySliceCmd(
		ClientData dummy,
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv)
{
	Tcl_Obj *naObj;
	
	NumArrayInfo *info, *sliceinfo;
	NumArraySharedBuffer* sharedbuffer;

	Tcl_Obj *result;

	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "numarrayvalue slicelist");
		return TCL_ERROR;
	}
	
	naObj = objv[1];

	result = Tcl_NewObj();
		
	if (Tcl_ConvertToType(interp, naObj, &NumArrayTclType) != TCL_OK) {
		goto cleanobj;
	}
	
	sharedbuffer = naObj ->internalRep.twoPtrValue.ptr1;
	info = naObj -> internalRep.twoPtrValue.ptr2;

	if (NumArrayInfoSlice(interp, info, objv[2], &sliceinfo) != TCL_OK) {
		goto cleanobj;
	}
	
	NumArraySetInternalRep(result, sharedbuffer, sliceinfo);

	Tcl_SetObjResult(interp, result);
	return TCL_OK;

cleanobj:
	Tcl_DecrRefCount(result);
	return TCL_ERROR;
}

#define CONVERTER(TYPE) \
int NumArrayConv ## TYPE ## Cmd(\
		ClientData dummy,\
		Tcl_Interp *interp,\
		int objc,\
		Tcl_Obj *const *objv)\
{\
	if (objc != 2) {\
		Tcl_WrongNumArgs(interp, 1, objv, "numarray");\
		return TCL_ERROR;\
	}\
\
	Tcl_Obj *result;\
	Tcl_Obj *naObj = objv[1];\
\
	if (NumArrayConvertToType(interp, naObj, NumArray_ ## TYPE, &result) != TCL_OK) {\
		return TCL_ERROR;\
	}\
	Tcl_SetObjResult(interp, result);\
	return TCL_OK;\
}

MAP(CONVERTER, Int8, Uint8, Int16, Uint16, Int32, Uint32, Int64, Uint64, Float32, Float64, Complex64, Complex128, Bool)

#undef CONVERTER
/* createNumArraySharedBufferFromTypedList
 * 
 * Expect a nested list representation compatible with info 
 * Creates a matching memory buffer and copies/converts the data from 
 * the nested list into the buffer. 
 */

static int createNumArraySharedBufferFromTypedList(Tcl_Interp *interp, Tcl_Obj *list, NumArrayInfo const *info, NumArraySharedBuffer **sharedbuf) {
	
	/* reserve memory for data */
	*sharedbuf = NumArrayNewSharedBuffer(info->bufsize);
	char *bufptr = NumArrayGetPtrFromSharedBuffer(*sharedbuf);
	NumArraySharedBufferIncrRefcount(*sharedbuf);
	
	int nDim = info->nDim;


	/* in a canonical array, the innermost pitch 
	 * is space between adjacent elements */
	index_t pitch = info -> pitches[nDim-1];

	/* loop over data. Create a counter */
	index_t *counter = ckalloc(sizeof(index_t)*nDim);
	int d;
	for (d=0; d<nDim; d++) { counter[d]=0; }
	
	/* matroska is a counting structure for the nested list representation
	 * Its first element points to the whole array, and successive elements are
	 * pointers to the current sublist of the parent list for this index */
	Tcl_Obj **matroska;

	/* set all list pointers to first branch of nested structs */
	matroska = ckalloc(sizeof(Tcl_Obj*)*(nDim+1));
	matroska[0] = list;
	for (d=1; d<=nDim; d++) {
		if (Tcl_ListObjIndex(interp, matroska[d-1], 0, &matroska[d]) != TCL_OK) {
			goto cleanbuffer;
		}
	}

	/* Loop over all elements. 
	 * skip copying if this is the empty element */
	while (info->dims[0] != 0) {
		/* store the current element to shared buffer */
		switch (info->type) {
			case NumArray_Int: {
				Tcl_WideInt temp;
				if (Tcl_GetWideIntFromObj(interp, matroska[nDim], &temp) != TCL_OK) {
					goto cleanbuffer;
				}
				*(NaWideInt *) bufptr = temp;
				bufptr += pitch;
				break;
			}
			case NumArray_Float64:
				if (myTcl_GetDoubleFromObj(interp, matroska[nDim], (double *)bufptr) != TCL_OK) {
					goto cleanbuffer;
				}
				bufptr += pitch;
			
				break;
			case NumArray_Complex128:
				if (NumArray_GetComplexFromObj(interp, matroska[nDim], (NumArray_Complex *)bufptr) != TCL_OK) {
					goto cleanbuffer;
				}
				bufptr += pitch;
				break;
			default:
				/* Error */
				printf("Unknown data type\n");
				goto cleanbuffer;
		} /* end of switch datatype */

		/* advance the count
		 * count indices one up, handle carry */
		for (d=nDim-1; d>=0; d--) {
			/* advance this by one */
			counter[d]++;
			/* check for carry */
			if (counter[d] == info->dims[d]) {
				counter[d] = 0;
			} else {
				break;
			}
		}

		/* when all counters are back to zero, 
		 * we are finished */
		if (d<0) break;

		/* recalculate matroska list for wrapped-over counters */
		for (d=d+1; d<=nDim; d++) {
			int dlength;
			if (Tcl_ListObjLength(interp, matroska[d-1], &dlength) != TCL_OK) {
				goto cleanbuffer;
			}
			
			if (dlength != info->dims[d-1]) {
				Tcl_SetResult(interp, "Non-matching dimensions in nested list", NULL);
				goto cleanbuffer;
			}
			
			if (Tcl_ListObjIndex(interp, matroska[d-1], counter[d-1], &matroska[d]) != TCL_OK) {
				goto cleanbuffer;
			}
		}
	}

	ckfree(matroska);
	ckfree(counter);
	return TCL_OK;

cleanbuffer:
	/* in case of error release the intermediate structures */
	ckfree(matroska);
	ckfree(counter);
	NumArraySharedBufferDecrRefcount(*sharedbuf);
	
	return TCL_ERROR;
}


static int  SetNumArrayFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr) {
	/* Parse nested list of numeric values */
	NumArrayInfo* info;
	NumArraySharedBuffer *sharedbuf;

	Tcl_Obj *dimlist;
	NumArrayType dtype=NumArray_NoType;
	
	/* parse dimensions and update info */
	if (ScanNumArrayDimensionsFromValue(interp, objPtr, &dimlist, &dtype) != TCL_OK) {
		return TCL_ERROR;
	}

	while (dtype != NumArray_NoType) {
		if (CreateNumArrayInfoFromList(interp, dimlist, dtype, &info) != TCL_OK) {
			goto cleanlist;
		}

		int result = createNumArraySharedBufferFromTypedList(interp, objPtr, info, &sharedbuf);
		if (result == TCL_OK) {
			/* This conversion round was successful */
			TclFreeIntRep(objPtr);
			objPtr -> internalRep.twoPtrValue.ptr1 = sharedbuf;
			objPtr -> internalRep.twoPtrValue.ptr2 = info;
			objPtr -> typePtr = &NumArrayTclType;
			
			Tcl_DecrRefCount(dimlist);
			return TCL_OK;
		}

		/* If this was unsuccessful, try a higher dtype */
		dtype = NumArray_UpcastType(dtype);
		DeleteNumArrayInfo(info);
	}

cleanlist:
	Tcl_DecrRefCount(dimlist);
	return TCL_ERROR;
}

static void UpdateStringOfNumArray(Tcl_Obj *naPtr) {
	Tcl_DString srep;
	NumArrayInfo* info = naPtr -> internalRep.twoPtrValue.ptr2;
	char *buffer=NULL;

	int nDim = info -> nDim;
	index_t *counter = ckalloc(sizeof(index_t)*nDim);
	char **baseptr = ckalloc(sizeof(char*)*nDim);

	int d=0; /* d is the dimension counter */
	
	/* handle case of empty array */
	if (info->dims[0]==0) {
		naPtr -> length = 0;
		naPtr -> bytes = ckalloc(1);
		*(naPtr -> bytes) = '\0';
		ckfree(counter);
		ckfree(baseptr);
		return;
	}

	buffer = NumArrayGetPtrFromObj(NULL, naPtr); /* can't fail */
	Tcl_DStringInit(&srep);
	/* set all counters to initial value */
	for (d=0; d<nDim; d++) {
		counter[d] = 0;
	}

	baseptr[0] = buffer;

	for (d=1; d<nDim; d++) {
		baseptr[d] = baseptr[d-1];
	}

	while (1) {
		/* count from backwards
		 * for every counter that is 0, start a list */
		for (d=nDim-1; d>0; d--) {
			if (counter[d] == 0) {
				Tcl_DStringStartSublist(&srep);
			} else {
				break;
			}
		}

		/* Print this element */
		switch (info -> type) {
		/* handle integers */
		${ 
			foreach type $NA_INTEGERS {
				if {$type in $NA_SIGNEDINTEGERS} {
					set ctype int64_t
					set fmtproc format_int64
				} else {
					set ctype uint64_t
					set fmtproc format_uint64
				}
				if {$type in $NA_FIXEDINTEGERS} {
					set suffix [dict get $NA_TYPESUFFIXES $type]
				} else {
					set suffix ""
				}

				if {$type == "NumArray_Bool"} {
					set suffix ""
					set fmtproc format_bool
				}

				C {
			case ${= type$}:
				{
					char intbuf[NA_INTSPACE+MAX_SUFFIX];
					${= ctype$} el = *(${dict get $NA_TO_CTYPE $type$} *) (baseptr[nDim-1]);
					int len = ${= fmtproc$}(el, intbuf);
					${ 
						if {$suffix ne ""} {
							C { strncpy(intbuf+len, ${cquote $suffix$}, MAX_SUFFIX); }
						}
					$}
					Tcl_DStringAppendElement(&srep, intbuf);
					break;
				}
				}
			}
		$}

			case NumArray_Float32:
				{
					char dblbuf[TCL_DOUBLE_SPACE+1];
					float elptr = *(float *) (baseptr[nDim-1]);
					Tcl_PrintDouble(NULL, elptr, dblbuf);
					Tcl_DStringAppendElement(&srep, dblbuf);
					Tcl_DStringAppend(&srep, "f", 1);
					break;
				}
			case NumArray_Float64:
				{
					char dblbuf[TCL_DOUBLE_SPACE];
					double *elptr = (double *) (baseptr[nDim-1]);
					Tcl_PrintDouble(NULL, *elptr, dblbuf);
					Tcl_DStringAppendElement(&srep, dblbuf);
					break;
				}
			case NumArray_Complex128:
				{
					char cplxbuf[NUMARRAY_COMPLEX_SPACE];
					NumArray_Complex *elptr = (NumArray_Complex *) (baseptr[nDim-1]);
					NumArray_PrintComplex(*elptr, cplxbuf);
					Tcl_DStringAppendElement(&srep, cplxbuf);
					break;
				}
			default:
				printf("Error: unknown data type %d", info -> type);
		}
		
		/* count indices one up, handle carry */
		for (d=nDim-1; d>=0; d--) {
			/* advance this by one */
			counter[d]++;
			baseptr[d]+=info->pitches[d];
			/* check for carry */
			if (counter[d] == info->dims[d]) {
				counter[d] = 0;
				if (d!=0) Tcl_DStringEndSublist(&srep);
			} else {
				break;
			}
		}

		/* when all counters are back to zero,  */
		/* we are finished */
		if (d<0) break;

		/* recalculate indices for wrapped-over counters */
		for (d=d+1; d<nDim; d++) {
			baseptr[d] = baseptr[d-1];
		}		
	}

	/* there should be a way to move */
	/* the pointer from DString to Tcl_Obj, here use memcpy */
	naPtr -> length = Tcl_DStringLength(&srep);
	naPtr -> bytes = Tcl_Alloc(naPtr->length+1);
	memcpy(naPtr -> bytes, Tcl_DStringValue(&srep), naPtr -> length);
	naPtr -> bytes[naPtr->length] = '\0';

	/* cleanup temp memory */
	Tcl_DStringFree(&srep);
	ckfree(baseptr);
	ckfree(counter);
}

#ifdef LIST_INJECT
static int SetListFromNumArray(Tcl_Interp *interp, Tcl_Obj *objPtr) {
	/* check if we got a NumArray as input
	 * if not, handle by original proc 
	 * likewise, if there is */
	if (objPtr -> typePtr != &NumArrayTclType || objPtr -> bytes) {
		return listSetFromAny(interp, objPtr);
	}

	DEBUGPRINTF(("Converting NumArray to list\n"));

	/* extract internal rep */
	NumArrayInfo *info = objPtr -> internalRep.twoPtrValue.ptr2;
	NumArraySharedBuffer *sharedbuf = objPtr -> internalRep.twoPtrValue.ptr1;
	
	/* First check for the empty info. This must be handled by the original 
	 * proc, because it is the only way to create an empty list
	 * without getting back a reference to the empty object */
	if (ISEMPTYINFO(info)) {
		return listSetFromAny(interp, objPtr);
	}
	
	/* Create a new object the internal rep of which
	 * is then duplicated to objPtr */
	Tcl_Obj * result = Tcl_NewListObj(0, NULL);
	if (info -> nDim == 1) {
		/* 1d-case: Construct list of values */
		NumArrayIterator it;
		NumArrayIteratorInit(info, sharedbuf, &it);
		switch (info->type) {
			case NumArray_Int:
				for (; !NumArrayIteratorFinished(&it); NumArrayIteratorAdvance(&it)) {
					long value = NumArrayIteratorDeRefInt(&it);
					Tcl_ListObjAppendElement(interp, result, Tcl_NewLongObj(value));
				}
				break;
			case NumArray_Float64:
				for (; !NumArrayIteratorFinished(&it); NumArrayIteratorAdvance(&it)) {
					double value = NumArrayIteratorDeRefDouble(&it);
					Tcl_ListObjAppendElement(interp, result, Tcl_NewDoubleObj(value));
				}
				break;
			case NumArray_Complex128:
				for (; !NumArrayIteratorFinished(&it); NumArrayIteratorAdvance(&it)) {
					NumArray_Complex value = NumArrayIteratorDeRefComplex(&it);
					Tcl_ListObjAppendElement(interp, result, NumArray_NewComplexObj(value));
				}
				break;
			default:
				Tcl_SetResult(interp, "Unknown datatype (shimmer to list)", NULL);
				return TCL_ERROR;
		}
	} else {
		/* multidimensional case: create slices for each 
		 * row and add to list */
		index_t i;
		index_t nelem = info -> dims[0];
		for (i=0; i< nelem; i++) {
			Tcl_Obj *slice=Tcl_NewObj();
			NumArrayInfo *sliceinfo=DupNumArrayInfo(info);
			NumArrayInfoSlice1Axis(NULL, sliceinfo, 0, i, i, 1);
			NumArrayStripSingletonDimensions(sliceinfo);
			NumArraySetInternalRep(slice, sharedbuf, sliceinfo);

			Tcl_ListObjAppendElement(interp, result, slice);
		}
	}
	
	FreeNumArrayInternalRep(objPtr);

	/* now transfer the internal rep from the list obj */
	/* objPtr -> internalRep = result -> internalRep; */
	if (result -> typePtr) {
		/* copy the internal rep from result into objPtr,
		 * unless it is a pure string (empty object) */
		result -> typePtr -> dupIntRepProc(result, objPtr);
	} else {
		objPtr -> typePtr = result -> typePtr;
		/* necessary in case of empty string.
		 * Lists set it in dupIntRepProc. */
	}
	
	Tcl_DecrRefCount(result);

	return TCL_OK;
}
#endif

void NumArrayIncrRefcount(Tcl_Obj *naObj) {
	if (naObj -> typePtr == &NumArrayTclType) {
		NumArraySharedBuffer *sharedbuf = naObj -> internalRep.twoPtrValue.ptr1;
		NumArraySharedBufferIncrRefcount(sharedbuf);
	}
}

void NumArrayDecrRefcount(Tcl_Obj *naObj) {
	if (naObj -> typePtr == &NumArrayTclType) {
		NumArraySharedBuffer *sharedbuf = naObj -> internalRep.twoPtrValue.ptr1;
		NumArraySharedBufferDecrRefcount(sharedbuf);
	}
}

/* sign function */
static inline int isign(int x) {
	if (x==0) return 0;
	if (x<0) return -1;
	return 1;
}

static inline double fsign(double x) {
	/* NaN is a sign in it's own right */
	if (x!=x) return x;

	if (x<0) return -1.0;
	if (x>0) return 1.0;
	return 0.0;
}


/* Implement elementwise binary operators */
#define CMD NumArrayPlus
#define OPINT *result = op1 + op2;
#define OPDBL *result = op1 + op2;
#define OPCPLX *result = NumArray_ComplexAdd(op1, op2);
#include "binop.h"

#define CMD NumArrayMinus
#define OPINT *result = op1 - op2;
#define OPDBL *result = op1 - op2;
#define OPCPLX *result = NumArray_ComplexSubtract(op1, op2);
#include "binop.h"

#define CMD NumArrayTimes
#define OPINT *result = op1 * op2;
#define OPDBL *result = op1 * op2;
#define OPCPLX *result = NumArray_ComplexMultiply(op1, op2);
#include "binop.h"

#define CMD NumArrayLdivide
#define OPINT if (op1!=0) { \
		*result = op2 / op1; \
	} else { \
		*resultObj=Tcl_NewStringObj("Integer division by zero", -1);\
		return TCL_ERROR;\
	}
#define OPDBL *result = op2 / op1;
#define OPCPLX *result = NumArray_ComplexDivide(op2, op1);
#include "binop.h"

#define CMD NumArrayRdivide
#define OPINT if (op2!=0) { \
		*result = op1 / op2; \
	} else { \
		*resultObj=Tcl_NewStringObj("Integer division by zero", -1);\
		return TCL_ERROR;\
	}

#define OPDBL *result = op1 / op2;
#define OPCPLX *result = NumArray_ComplexDivide(op1, op2);
#include "binop.h"

#define CMD NumArrayReminder
#define OPINT if (op2!=0) { \
		*result = op1 % op2; \
	} else { \
		*resultObj=Tcl_NewStringObj("Integer division by zero", -1);\
		return TCL_ERROR;\
	}
#include "binop.h"

#define CMD NumArrayGreater
#define OPINT *result = (op1>op2);
#define OPDBL *result = (op1>op2);
#define DBLRES NaWideInt
#include "binop.h"

#define CMD NumArrayLesser
#define OPINT *result = (op1<op2);
#define OPDBL *result = (op1<op2);
#define DBLRES NaWideInt
#include "binop.h"

#define CMD NumArrayGreaterEqual
#define OPINT *result = (op1>=op2);
#define OPDBL *result = (op1<=op2);
#define DBLRES NaWideInt
#include "binop.h"

#define CMD NumArrayLesserEqual
#define OPINT *result = (op1<=op2);
#define OPDBL *result = (op1<=op2);
#define DBLRES NaWideInt
#include "binop.h"

#define CMD NumArrayEqual
#define OPINT *result = (op1==op2);
#define OPDBL *result = (op1==op2);
#define DBLRES NaWideInt
#define OPCPLX *result = ((op1.re==op2.re) && (op1.im==op2.im))
#define CPLXRES NaWideInt
#include "binop.h"

#define CMD NumArrayUnequal
#define OPINT *result = (op1!=op2);
#define OPDBL *result = (op1!=op2);
#define DBLRES NaWideInt
#define OPCPLX *result = ((op1.re!=op2.re) || (op1.im!=op2.im))
#define CPLXRES NaWideInt
#include "binop.h"

/* boolean operators */
#define CMD NumArrayNot
#define INTOP *result = !op;
#define INTRES NaWideInt
#include "uniop.h"

#define CMD NumArrayAnd
#define OPINT *result = (op1 && op2);
#include "binop.h"

#define CMD NumArrayOr
#define OPINT *result = (op1 || op2);
#include "binop.h"

#define CMD NumArrayPow
#define OPINT *result = pow(op1,op2);
#define INTRES double
#define OPDBL *result = pow(op1,op2);
#define OPCPLX *result = NumArray_ComplexPow(op1,op2);
#include "binop.h"

#define CMD NumArrayMin
#define OPINT *result = op1 < op2 ? op1 : op2;
#define OPDBL *result = op1 < op2 ? op1 : op2;
#include "binop.h"

#define CMD NumArrayMax
#define OPINT *result = op1 > op2 ? op1 : op2;
#define OPDBL *result = op1 > op2 ? op1 : op2;
#include "binop.h"


/* Implement elementwise binary assignment operators */
#define CMD NumArraySetAssignCmd
#define OPINT *result = op;
#define OPDBL *result = op;
#define OPCPLX *result = op;
#include "assignop.h"

#define CMD NumArrayPlusAssignCmd
#define OPINT *result += op;
#define OPDBL *result += op;
#define OPCPLX *result = NumArray_ComplexAdd(*result, op);
#include "assignop.h"

#define CMD NumArrayMinusAssignCmd
#define OPINT *result -= op;
#define OPDBL *result -= op;
#define OPCPLX *result = NumArray_ComplexSubtract(*result, op);
#include "assignop.h"

#define CMD NumArrayTimesAssignCmd
#define OPINT *result *= op;
#define OPDBL *result *= op;
#define OPCPLX *result = NumArray_ComplexMultiply(*result, op);
#include "assignop.h"

#define CMD NumArrayLdivideAssignCmd
#define OPINT if (*result!=0) { \
		*result = op / *result; \
	} else { \
		Tcl_SetResult(interp, "Integer division by zero", NULL);\
		return TCL_ERROR;\
	}
#define OPDBL *result = op / (*result);
#define OPCPLX *result = NumArray_ComplexDivide(op, *result);
#include "assignop.h"

#define CMD NumArrayRdivideAssignCmd
#define OPINT if (op!=0) { \
		*result /= op; \
	} else { \
		Tcl_SetResult(interp, "Integer division by zero", NULL);\
		return TCL_ERROR;\
	}
#define OPDBL *result /= op;
#define OPCPLX *result = NumArray_ComplexDivide(op, *result);
#include "assignop.h"

#define CMD NumArrayPowAssignCmd
#define OPDBL *result = pow(*result,op);
#include "assignop.h"

/* Implement unary functions/operators */

/* Data type conversion */
#define CMD NumArrayConvInt
#define INTRES NaWideInt
#define INTOP *result = op;
#define DBLRES NaWideInt
#define DBLOP *result = (NaWideInt)op;
#include "uniop.h"

#define CMD NumArrayConvDouble
#define INTRES double
#define INTOP *result = op;
#define DBLRES double
#define DBLOP *result = op;
#include "uniop.h"

#define CMD NumArrayConvComplex
#define INTRES NumArray_Complex
#define INTOP *result = NumArray_mkComplex(op, 0.0);
#define DBLRES NumArray_Complex
#define DBLOP *result = NumArray_mkComplex(op, 0.0);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = op;
#include "uniop.h"

#define CMD NumArrayAbs
#define INTRES NaWideInt
#define INTOP *result = labs(op);
#define DBLRES double
#define DBLOP *result = fabs(op);
#define CPLXRES double
#define CPLXOP *result = NumArray_ComplexAbs(op);
#include "uniop.h"

#define CMD NumArraySign
#define INTRES NaWideInt
#define INTOP *result = isign(op);
#define DBLRES double
#define DBLOP *result = fsign(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexSign(op);
#include "uniop.h"

#define CMD NumArrayArg
#define INTRES NaWideInt
#define INTOP *result = 0;
#define DBLRES double
#define DBLOP *result = 0.0;
#define CPLXRES double
#define CPLXOP *result = NumArray_ComplexArg(op);
#include "uniop.h"

#define CMD NumArrayConj
#define INTRES NaWideInt
#define INTOP *result = op;
#define DBLRES double
#define DBLOP *result = op;
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexConj(op);
#include "uniop.h"

#define CMD NumArrayReal
#define INTRES NaWideInt
#define INTOP *result = op;
#define DBLRES double
#define DBLOP *result = op;
#define CPLXRES double
#define CPLXOP *result = op.re;
#include "uniop.h"

#define CMD NumArrayImag
#define INTRES NaWideInt
#define INTOP *result = 0;
#define DBLRES double
#define DBLOP *result = 0.0;
#define CPLXRES double
#define CPLXOP *result = op.im;
#include "uniop.h"


#define CMD NumArrayNeg
#define INTRES NaWideInt
#define INTOP *result = -op;
#define DBLRES double
#define DBLOP *result = -op;
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexNeg(op);
#include "uniop.h"

#define CMD NumArraySin
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = sin(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexSin(op);
#include "uniop.h"

#define CMD NumArrayCos
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = cos(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexCos(op);
#include "uniop.h"

#define CMD NumArrayTan
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = tan(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexTan(op);
#include "uniop.h"

#define CMD NumArrayExp
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = exp(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexExp(op);
#include "uniop.h"

#define CMD NumArrayLog
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = log(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexLog(op);
#include "uniop.h"

#define CMD NumArrayLog10
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = log10(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexLog(op);
#include "uniop.h"

#define CMD NumArraySqrt
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = sqrt(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexSqrt(op);
#include "uniop.h"

#define CMD NumArraySinh
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = sinh(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexSinh(op);
#include "uniop.h"

#define CMD NumArrayCosh
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = cosh(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexCosh(op);
#include "uniop.h"

#define CMD NumArrayTanh
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = tanh(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexTanh(op);
#include "uniop.h"

#define CMD NumArrayAsin
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = asin(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexAsin(op);
#include "uniop.h"

#define CMD NumArrayAcos
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = acos(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexAcos(op);
#include "uniop.h"

#define CMD NumArrayAtan
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = atan(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexAtan(op);
#include "uniop.h"

#define CMD NumArrayAsinh
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = asinh(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexAsinh(op);
#include "uniop.h"

#define CMD NumArrayAcosh
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = acosh(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexAcosh(op);
#include "uniop.h"

#define CMD NumArrayAtanh
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = atanh(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexAtanh(op);
#include "uniop.h"


/* Implement reductions with optional dimension */
#define CMD NumArraySum
#define INIT ;
#define FIRST accum=op;
#define INTOP accum+=op;
#define DBLOP accum+=op;
#define CPLXOP accum=NumArray_ComplexAdd(accum, op);
#define RETURN ;
#include "reduction.h"


#define CMD NumArrayMean
#define INIT ;
#define FIRST accum=op;
#define INTRES double
#define INTOP accum+=op;
#define DBLOP accum+=op;
#define CPLXOP accum=NumArray_ComplexAdd(accum, op);
#define RETURN accum /= nlength;
#define CPLXRETURN accum.re /= nlength; accum.im /= nlength; 
#include "reduction.h"

#define CMD NumArrayAxisMin
#define INIT ;
#define FIRST accum=op;
#define INTOP if (op < accum) accum=op;
#define DBLOP if (op < accum) accum=op;
#define RETURN ;
#include "reduction.h"

#define CMD NumArrayAxisMax
#define INIT ;
#define FIRST accum=op;
#define INTOP if (op > accum) accum=op;
#define DBLOP if (op > accum) accum=op;
#define RETURN ;
#include "reduction.h"

#define CMD NumArrayStd
#define INIT double first; double sum=0;
#define FIRST accum=0; first=op;
#define DBLOP  accum+=(op-first)*(op-first); sum+=(op-first);
#define INTOP DBLOP
#define INTRES double
#define RETURN accum = (nlength==1)?0:sqrt(accum/(nlength-1) - (sum*sum/nlength)/(nlength-1));
#include "reduction.h"

#define CMD NumArrayStd1
#define INIT double first; double sum=0;
#define FIRST accum=0; first=op;
#define DBLOP  accum+=(op-first)*(op-first); sum+=(op-first);
#define INTOP  DBLOP
#define INTRES double
#define RETURN accum = sqrt(accum/nlength - (sum/nlength)*(sum/nlength));
#include "reduction.h"

#define CMD NumArrayAll
#define INIT ;
#define FIRST if (op) { accum=1; } else { accum=0; }
#define INTOP if (!op) { accum=0; break; } 
#define RETURN ;
#include "reduction.h"

#define CMD NumArrayAny
#define INIT ;
#define FIRST if (!op) { accum=0; } else { accum=1; }
#define INTOP if (op) { accum=1; break; } 
#define RETURN ;
#include "reduction.h"


int Vectcl_Init(Tcl_Interp* interp) {
	if (interp == 0) return TCL_ERROR;

	if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL) {
		return TCL_ERROR;
	}

	Tcl_PkgProvide(interp, PACKAGE_NAME, PACKAGE_VERSION);

	VecTclNumArrayObjType = &NumArrayTclType;
	Tcl_RegisterObjType(&NumArrayTclType);
	myTcl_MakeEnsemble(interp, "::numarray", "::numarray", implementationMap);

	/* Initialize complex data type */
	if (Complex_Init(interp) != TCL_OK) {
		return TCL_ERROR;
	}
	
	/* Initialize expression parser */
	if (Vmparser_Init(interp) != TCL_OK) {
		return TCL_ERROR;
	}


	/* casting away const is intended for the dirty hack */
	tclListType =  (Tcl_ObjType *) Tcl_GetObjType("list");	
	tclDoubleType = Tcl_GetObjType("double");
	tclIntType = Tcl_GetObjType("int");
#ifndef TCL_WIDE_INT_IS_LONG
	tclWideIntType = Tcl_GetObjType("wideInt");
#endif
	
	#ifdef LIST_INJECT
	/* copy list object proc from list type */
	listSetFromAny = tclListType -> setFromAnyProc;

	/* inject list conversion code 
	 * WARNING may break Tcl 
	 */
	tclListType->setFromAnyProc = SetListFromNumArray;
	Tcl_RegisterObjType(tclListType); 
	#endif

	return TCL_OK;
}


