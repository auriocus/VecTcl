/*
 * vectcl.c --
 */

#include "vectcl.h"
#include "arrayshape.h"
#include "linalg.h"
#include "fft.h"
#include <string.h>
#include <stdlib.h>
//#define DEBUG_REFCOUNT
#ifdef DEBUG_REFCOUNT
#include <stdio.h>
#define dprintf(X) printf X
#else
#define dprintf(X)
#endif
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

/*
 * Functions handling numerical data types
 */

NumArrayType NumArray_UpcastType(NumArrayType base) {
	NumArrayType result = base+1;
	if (result >= NumArray_SentinelType) {
		/* signal that we don't have a type left */
		result = NumArray_NoType;
	}
	return result;
}

NumArrayType NumArray_UpcastCommonType(NumArrayType type1, NumArrayType type2) {
	NumArrayType result = type1 > type2 ? type1 : type2;
	if (result >= NumArray_SentinelType || type1 < 0 || type2 < 0) {
		/* signal that we don't have a type left */
		result = -1;
	}

	return result;
}

int NumArrayType_SizeOf(NumArrayType type) {
	switch (type) {
		case NumArray_Int64:
			return sizeof(int);
		case NumArray_Float64:
			return sizeof(double);
		case NumArray_Complex128:
			return sizeof(NumArray_Complex);
		default: 
			printf("Error: unknown data type %d", type);
			return 1;	
	}
}

const char * NumArray_typename[NumArray_SentinelType]={
	"int", "double",  "complex" };

/* Function to upcast an array to the type requested */
int NumArrayConvertToType(Tcl_Interp *interp, Tcl_Obj *naObj, NumArrayType type, Tcl_Obj **dest) {
	NumArrayInfo *info, *convinfo; 
	NumArraySharedBuffer *sharedbuf, *convbuf;
	if (Tcl_ConvertToType(interp, naObj, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}
	
	info = naObj -> internalRep.twoPtrValue.ptr2;
	sharedbuf = naObj -> internalRep.twoPtrValue.ptr1;

	if (type < info -> type) {
		/* Downcasting is an error */
		Tcl_SetResult(interp, "Cannot downcast numeric array to requested type", NULL);
		return TCL_ERROR;
	}
	
	/* Check for no-op */
	if (type == info -> type) {
		return TCL_OK;
	}

	/* Create a new internal rep */
	convinfo = CreateNumArrayInfo(info -> nDim, info -> dims, type);
	convbuf = NumArrayNewSharedBuffer(convinfo -> bufsize);

	NumArrayIterator it, convit;
	NumArrayIteratorInit(info, sharedbuf, &it);
	NumArrayIteratorInit(convinfo, convbuf, &convit);
	/* The new buffer is in canonical form, 
	 * therefore simply advance the pointer at every element */
	if (type == NumArray_Float64 && info -> type == NumArray_Int64) {
		double *bufptr = NumArrayIteratorDeRefPtr(&convit);
		for (; ! NumArrayIteratorFinished(&it); NumArrayIteratorAdvance(&it)) {
				* bufptr++ = NumArrayIteratorDeRefInt(&it);
		}
	} else if (type == NumArray_Complex128 && info -> type == NumArray_Float64) {
		NumArray_Complex *bufptr = NumArrayIteratorDeRefPtr(&convit);
		for (; ! NumArrayIteratorFinished(&it); NumArrayIteratorAdvance(&it)) {
				NumArray_Complex value; 
				value.re = NumArrayIteratorDeRefDouble(&it);
				value.im = 0.0;
				* bufptr++ = value;
		}
	} else if (type == NumArray_Complex128 && info -> type == NumArray_Int64) {
		NumArray_Complex *bufptr = NumArrayIteratorDeRefPtr(&convit);
		for (; ! NumArrayIteratorFinished(&it); NumArrayIteratorAdvance(&it)) {
				NumArray_Complex value; 
				value.re = NumArrayIteratorDeRefInt(&it);
				value.im = 0.0;
				* bufptr++ = value;
		}
	} else {
		Tcl_SetResult(interp, "Unknown data type conversion", NULL);
		DeleteNumArrayInfo(convinfo);
		NumArraySharedBufferDecrRefcount(convbuf);
		return TCL_ERROR;
	}

	NumArrayIteratorFree(&it);
	NumArrayIteratorFree(&convit);

	/* now set the internal rep, possibly unsharing the object */
	*dest=Tcl_NewObj();
	
	NumArraySetInternalRep(*dest, convbuf, convinfo);
	return TCL_OK;
}

/*
 * Functions handling NumArrayInfo objects 
 * (metadata describing the memory buffer 
 */

NumArrayInfo* CreateNumArrayInfo(int nDim, const int *dims, NumArrayType dtype) {
	/* Create empty information with nDim number of dimensions
	 * initialised to dimensions in dims.
	 * initialized to zero size, if dims == NULL
	 * TODO catch out of memory */
	int d = 0;
	int elemsize=NumArrayType_SizeOf(dtype);
	
	NumArrayInfo* result = ckalloc(sizeof(NumArrayInfo));
	result -> nDim = nDim;
	result -> canonical = 1;
	result -> bufsize = elemsize;
	result -> type = dtype;
	result -> dims = ckalloc(sizeof(int)*nDim);
	result -> offsets = ckalloc(sizeof(int)*nDim);
	result -> pitches = ckalloc(sizeof(int)*nDim);
	
	for (d=0; d<nDim; d++) {
		int dim=0;
		if (dims) {
			dim=dims[d];
			if (dim<0) dim=0;
		}
		result -> bufsize *= dim;
		result -> dims[d] = dim;
		result -> offsets[d] = 0;
	}

	if (result -> bufsize == 0) {
		result -> bufsize = 1;
		/* mallocing a zero size buffer can 
		 * lead to errors. */
	}

	/* compute pitches */
	result -> pitches[nDim-1]=elemsize;
	for (d=nDim-2; d>=0; d--) {
		result -> pitches[d] = 
			result -> pitches[d+1] * result ->dims[d+1];
	}

	return result;
}

/* The same as CreateNumArrayInfo, but creates
 * column major storage (=Fortran format)
 * to optimize cache access pattern in QR, LU etc. */
NumArrayInfo* CreateNumArrayInfoColMaj(int nDim, const int *dims, NumArrayType dtype) {
	/* Create empty information with nDim number of dimensions
	 * initialised to dimensions in dims.
	 * initialized to zero size, if dims == NULL
	 * TODO catch out of memory */
	int d = 0;
	int elemsize=NumArrayType_SizeOf(dtype);
	
	NumArrayInfo* result = ckalloc(sizeof(NumArrayInfo));
	result -> nDim = nDim;
	result -> canonical = 0; /* column major */
	result -> bufsize = elemsize;
	result -> type = dtype;
	result -> dims = ckalloc(sizeof(int)*nDim);
	result -> offsets = ckalloc(sizeof(int)*nDim);
	result -> pitches = ckalloc(sizeof(int)*nDim);
	
	for (d=0; d<nDim; d++) {
		int dim=0;
		if (dims) {
			dim=dims[d];
			if (dim<0) dim=0;
		}
		result -> bufsize *= dim;
		result -> dims[d] = dim;
		result -> offsets[d] = 0;
	}

	if (result -> bufsize == 0) {
		result -> bufsize = 1;
		/* mallocing a zero size buffer can 
		 * lead to errors. */
	}

	/* compute pitches */
	result -> pitches[0]=elemsize;
	for (d=1; d<nDim; d++) {
		result -> pitches[d] = 
			result -> pitches[d-1] * result ->dims[d-1];
	}

	return result;
}


int CreateNumArrayInfoFromList(Tcl_Interp *interp, Tcl_Obj* dimlist, NumArrayType dtype, NumArrayInfo **infoptr) {
	/* Create information with dimensions as in dimlist
	 * TODO catch out of memory */
	int d = 0;
	int nDim;
	int *dims = NULL;
	
	if (Tcl_ListObjLength(interp, dimlist, &nDim) != TCL_OK) {
		return TCL_ERROR;
	}

	if (nDim == 0) {
		Tcl_SetResult(interp, "Empty dimension list", NULL);
		return TCL_ERROR;
	}
	
	dims = ckalloc(sizeof(int)*nDim);

	for (d=0; d<nDim; d++) {
		int dim;
		Tcl_Obj *dimObj;
		Tcl_ListObjIndex(NULL, dimlist, d, &dimObj); /* can't fail */
		if (Tcl_GetIntFromObj(interp, dimObj, &dim) != TCL_OK) {
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
	return TCL_OK;
cleandims:
	if (dims) ckfree(dims);
	*infoptr = NULL;
	return TCL_ERROR;
}

void DeleteNumArrayInfo(NumArrayInfo* info) {
	ckfree(info -> pitches);
	ckfree(info -> offsets);
	ckfree(info -> dims);
	ckfree(info);
}

NumArrayInfo* DupNumArrayInfo(NumArrayInfo* src) {
	/* Copy NumArrayInfo into new struct */
	int i = 0, nDim = 0;
	NumArrayInfo* result = ckalloc(sizeof(NumArrayInfo));
	nDim = src -> nDim;
	result -> nDim = nDim;
	result -> canonical = src -> canonical;
	result -> bufsize = src -> bufsize;
	result -> type = src -> type;

	result -> dims = ckalloc(sizeof(int)*nDim);
	result -> offsets = ckalloc(sizeof(int)*nDim);
	result -> pitches = ckalloc(sizeof(int)*nDim);
	for (i=0; i<nDim; i++) {
		result -> dims[i] = src -> dims[i];
		result -> offsets[i] = src -> offsets[i];
		result -> pitches[i] = src -> pitches[i];
	}
	dprintf(("DupNumArrayInfo %p->%p\n", src, result));
	return result;
}

void NumArrayStripSingletonDimensions(NumArrayInfo *info) {
	/* Remove singleton dimensions.
	 * Note this also transforms rowvectors into columnvectors
	 * but these are interchangeable (and with lists) anyway */
	
	/* count number of singleton dimensions */
	
	int d=0, dest=0, skipoffset=0;
	for (d=0; d<info -> nDim; d++) {
		info -> dims[dest] = info -> dims[d];
		info -> offsets[dest] = info -> offsets[d];
		info -> pitches[dest] = info -> pitches[d];
		if (info -> dims[d] == 1) {
			skipoffset += info -> offsets[d];
		} else {
			dest++;
		}
	}

	
	info -> nDim = dest;
	if (info -> nDim == 0) {
		/* it reduced to a single scalar value */
		info -> nDim = 1;
	}
	
	if (info->dims[info -> nDim - 1] == 1) {
		/* if the last dimension was a singleton
		 * we added its offset to skipoffset already, so
		 * skipoffset contains the total offset */
		info -> offsets[info->nDim - 1] = skipoffset;
	} else {
		/* if the last dimension was not a singleton,
		 * add every offset from the singletons here */
		info -> offsets[info->nDim - 1] += skipoffset;
	}
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

int NumArrayInfoSlice1Axis(Tcl_Interp *interp, NumArrayInfo *info, int axis, int start, int stop, int incr) {
	if (axis < 0 || axis >= info->nDim) {
		Tcl_SetResult(interp, "Dimension mismatch.", NULL);
		return TCL_ERROR;
	}

	/* convert negative indices 
	 * start and stop count from the end */
	if (start<0) {
		start = info->dims[axis]+start;
	}

	if (start >=  info->dims[axis] || start < 0) {
		Tcl_SetResult(interp, "Start index out of bounds", NULL);
		return TCL_ERROR;
	}
	
	if (stop<0) {
		stop = info->dims[axis]+stop;
	}

	if (stop >=  info->dims[axis] || stop < 0) {
		Tcl_SetResult(interp, "Stop index out of bounds", NULL);
		return TCL_ERROR;
	}

	if (start > stop) {
		/* Empty slice. Return empty info */
		info->nDim = 1;
		info -> dims[1]=0;
		return TCL_OK;
	}

	if (incr == 0) {
		/* zero increment is undefined */
		Tcl_SetResult(interp, "Zero increment in slice", NULL);
		return TCL_ERROR;
	}

	if (incr < 0) {
		/* swap start and stop for negative increment */
		int swap = start;
		start = stop; 
		stop = swap;
	}

	/* Count number of elements in this dimension */
	int nelem = (stop - start) / incr + 1;
	info -> dims[axis] = nelem;
	info -> offsets[axis] = info -> offsets[axis] + start * info->pitches[axis];
	info -> pitches[axis] = info -> pitches[axis]*incr;

	return TCL_OK;
}


/* Slicelist is a list of lists
 * {{start stop incr} {start stop incr} ...}
 * for every dimension */
int NumArrayInfoSlice(Tcl_Interp *interp, NumArrayInfo *info, Tcl_Obj *slicelist, NumArrayInfo **resultPtr) {
	int slicec; Tcl_Obj **slicev;
	int d; NumArrayInfo *sliceinfo;
	
	if (Tcl_ListObjGetElements(interp, slicelist, &slicec, &slicev)!=TCL_OK) {
		return TCL_ERROR;
	}

	/* there must be slice information for every dimension */
	if (slicec != info->nDim) {
		Tcl_SetResult(interp, "Dimension mismatch", NULL);
		return TCL_ERROR;
	}
	
	sliceinfo=DupNumArrayInfo(info);
	sliceinfo -> canonical = 0;

	for (d=0; d<slicec; d++) {
		int llength;
		Tcl_Obj **elems;
		if (Tcl_ListObjGetElements(interp, slicev[d], &llength, &elems) != TCL_OK) {
			goto cleaninfo;
		}

		if (llength != 3) {
			Tcl_SetResult(interp, "Slice list must contain 3 indices (start, stop, increment)", NULL);
			goto cleaninfo;
		}

		int start, stop, incr;

		if (Tcl_GetIntFromObj(interp, elems[0], &start) != TCL_OK) {
			goto cleaninfo;
		}

		if (Tcl_GetIntFromObj(interp, elems[1], &stop) != TCL_OK) {
			goto cleaninfo;
		}

		if (Tcl_GetIntFromObj(interp, elems[2], &incr) != TCL_OK) {
			goto cleaninfo;
		}

		/* convert negative indices 
		 * start and stop count from the end */
		if (start<0) {
			start = info->dims[d]+start;
		}

		if (start >=  info->dims[d] || start < 0) {
			Tcl_SetResult(interp, "Start index out of bounds", NULL);
			goto cleaninfo;
		}
		
		if (stop<0) {
			stop = info->dims[d]+stop;
		}

		if (stop >=  info->dims[d] || stop < 0) {
			Tcl_SetResult(interp, "Stop index out of bounds", NULL);
			goto cleaninfo;
		}

		if (start > stop) {
			/* Empty slice. Return empty info */
			DeleteNumArrayInfo(sliceinfo);
			*resultPtr = CreateNumArrayInfo(1, NULL, info->type);
			return TCL_OK;
		}

		if (incr == 0) {
			/* zero increment is undefined */
			Tcl_SetResult(interp, "Zero increment in slice", NULL);
			goto cleaninfo;
		}

		if (incr < 0) {
			/* swap start and stop for negative increment */
			int swap = start;
			start = stop; 
			stop = swap;
		}

		/* Count number of elements in this dimension */
		int nelem = (stop - start) / incr + 1;
		sliceinfo -> dims[d] = nelem;
		sliceinfo -> offsets[d] = info -> offsets[d] + start * info->pitches[d];
		sliceinfo -> pitches[d] = info -> pitches[d]*incr;
	}

	/* now remove any singleton dimension */
	NumArrayStripSingletonDimensions(sliceinfo); 

	*resultPtr = sliceinfo;
	return TCL_OK;
cleaninfo:
	DeleteNumArrayInfo(sliceinfo);
	return TCL_ERROR;
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
	/* commands that change the shape */
	{"reshape", NumArrayReshapeCmd, NULL},
	{"transpose", NumArrayTransposeCmd, NULL},
	{"adjoint", NumArrayAdjointCmd, NULL},
	{"slice", NumArraySliceCmd, NULL},
	{"concat", NumArrayConcatCmd, NULL},
	/* data type conversion operators */
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
	int *dims = ckalloc(sizeof(int)*ndim);
	int nelem = 1;
	int d;
	for (d=0; d<ndim; d++) {
		int dim;
		if (Tcl_GetIntFromObj(interp, objv[2+d], &dim) != TCL_OK) {
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
	int i;
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
	int m, n;
	NumArrayInfo *info; 
	NumArraySharedBuffer *sharedbuf;

	if (objc != 2 && objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "dim1 ?dim2?");
		return TCL_ERROR;
	}
	
	if (Tcl_GetIntFromObj(interp, objv[1], &m) != TCL_OK) {
		return TCL_ERROR;
	}

	if (objc == 2) {
		n=m; 
	} else {
		if (Tcl_GetIntFromObj(interp, objv[2], &n) != TCL_OK) {
			return TCL_ERROR;
		}
	}

	if (m<0 || n<0) {
		Tcl_SetResult(interp, "Dimensions must be positive", NULL);
		return TCL_ERROR;
	}

	int *dims = ckalloc(sizeof(int)*2);
	dims[0]=m; dims[1]=n;
	info = CreateNumArrayInfo((n==1)? 1:2, dims, NumArray_Float64);
	sharedbuf = NumArrayNewSharedBuffer(info->bufsize);

	/* Fill the buffer */
	double *bufptr = (double*) NumArrayGetPtrFromSharedBuffer(sharedbuf);
	int i;
	for (i=0; i<m; i++) {
		int j; 
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
		Tcl_ListObjAppendElement(interp, plist,  Tcl_NewIntObj(info->dims[i]));
	}
	Tcl_DictObjPut(interp, infodict, Tcl_NewStringObj("dimensions", -1), plist);
	
	plist = Tcl_NewObj();
	for (i=0; i<info->nDim; i++) {
		Tcl_ListObjAppendElement(interp, plist,  Tcl_NewIntObj(info->offsets[i]));
	}
	Tcl_DictObjPut(interp, infodict, Tcl_NewStringObj("offset", -1), plist);

	plist = Tcl_NewObj();
	for (i=0; i<info->nDim; i++) {
		Tcl_ListObjAppendElement(interp, plist,  Tcl_NewIntObj(info->pitches[i]));
	}
	Tcl_DictObjPut(interp, infodict, Tcl_NewStringObj("pitches", -1), plist);
	
	Tcl_DictObjPut(interp, infodict, Tcl_NewStringObj("canonical", -1), Tcl_NewBooleanObj(info->canonical));
	Tcl_DictObjPut(interp, infodict, Tcl_NewStringObj("bufsize", -1), Tcl_NewIntObj(info->bufsize));
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

	if (NumArrayGetBufferFromObj(interp, naObj, &bufptr) != TCL_OK) {
		return TCL_ERROR;
	}

	info = naObj -> internalRep.twoPtrValue.ptr2;
	
	if (info -> nDim != objc - 2) {
		Tcl_SetResult(interp, "Dimension mismatch", NULL);
		return TCL_ERROR;
	}

	for (d=0; d < info->nDim; d++) {
		int ind;
		if (Tcl_GetIntFromObj(interp, objv[d+2], &ind) != TCL_OK) {
			return TCL_ERROR;
		}
		/* negative index counts backward from the end */
		if (ind < 0) {
			ind += info->dims[d];
		}

		if (ind >= info->dims[d] || ind < 0) {
			Tcl_SetResult(interp, "Index out of bounds", NULL);
			return TCL_ERROR;
		}

		bufptr += ind*info->pitches[d] + info->offsets[d];
	}

	switch (info->type) {
		case NumArray_Float64: {
			double value = *((double *) bufptr);
			Tcl_SetObjResult(interp, Tcl_NewDoubleObj(value));
			break;
		}
		case NumArray_Int64: {
			int value = *((int *) bufptr);
			Tcl_SetObjResult(interp, Tcl_NewIntObj(value));
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

	if (objc-3 != info->nDim) {
		Tcl_SetResult(interp, "Dimension mismatch.", NULL);
		goto cleanobj;
	}

	/* compute index into buffer */
	for (d=0; d<info->nDim; d++) {
		int index;
		if (Tcl_GetIntFromObj(interp, objv[d+2], &index) != TCL_OK) { 
			goto cleanobj;
		}

		if (index<0 || index >= info->dims[d]) {
			Tcl_SetResult(interp, "Index out of range.", NULL);
			goto cleanobj;
		}

		bufptr += info->pitches[d]*index + info->offsets[d];
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
		case NumArray_Int64: {
			int value;
			if (Tcl_GetIntFromObj(interp, objv[objc-1], &value) != TCL_OK) {
				goto cleanobj;
			}

			/* set value */
			*((int*)bufptr) = value;
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
	int nelem=1;
	for (d=0; d<info->nDim; d++) {
		nelem *= info->dims[d];
	}

	/* add value to dest by simple loop */
	double *dest= (double *)NumArrayGetPtrFromSharedBuffer(sharedbuf);
	double *src= (double *)NumArrayGetPtrFromSharedBuffer(valuebuf);
	
	int i;
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
	int i; int nelem=1; int reshapenelem=1; int reshapedim;
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
		reshapeinfo -> offsets[i] = 0;
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
	dprintf(("DupNumArrayInternalRep, refocunt %d\n", sharedbuf -> refcount));
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
			Tcl_ListObjAppendElement(interp, dimlist, Tcl_NewIntObj(info->dims[d]));
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
		Tcl_ListObjAppendElement(interp, dimlist, Tcl_NewIntObj(0));
		*result=dimlist;
		*dtype=NumArray_Int64;
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
				Tcl_ListObjAppendElement(interp, dimlist, Tcl_NewIntObj(1));
			}
			if (itobj -> typePtr == tclDoubleType) {
				*dtype = NumArray_Float64;
			}
			if (itobj -> typePtr == tclIntType) {
				*dtype = NumArray_Int64;
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
			int dummy_int64;
			NumArray_Complex dummy_complex128;
			/* try to convert to int, then double, then complex */
			if (Tcl_GetIntFromObj(interp, itobj, &dummy_int64) == TCL_OK) {
				/* 1st: Try to convert to int. If succeeds, we are at the leaf
				 * Handle case of a single number, 
				 * else just break out of the loop */
				if (nDim==0) {
					nDim=1;
					Tcl_ListObjAppendElement(interp, dimlist, Tcl_NewIntObj(1));
				}
				*dtype = NumArray_Int64;
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
		Tcl_ListObjAppendElement(interp, result,  Tcl_NewIntObj(info->dims[d]));
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

			transposeinfo -> offsets[0] = 0;
			transposeinfo -> offsets[1] = info->offsets[0];

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
			transposeinfo -> offsets[0] = info->offsets[1];
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

		transposeinfo -> offsets[dim1] = info -> offsets[dim2];
		transposeinfo -> offsets[dim2] = info -> offsets[dim1];

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
	int pitch = info -> pitches[nDim-1];

	/* loop over data. Create a counter */
	int *counter = ckalloc(sizeof(int)*nDim);
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
			case NumArray_Int64:
				if (Tcl_GetIntFromObj(interp, matroska[nDim], (int *)bufptr) != TCL_OK) {
					goto cleanbuffer;
				}
				bufptr += pitch;
				break;
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
				exit(-1);
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
	int *counter = ckalloc(sizeof(int)*nDim);
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

	NumArrayGetBufferFromObj(NULL, naPtr, &buffer); /* can't fail */
	Tcl_DStringInit(&srep);
	/* set all counters to initial value */
	for (d=0; d<nDim; d++) {
		counter[d] = 0;
	}

	baseptr[0] = buffer + info -> offsets[0];
	for (d=1; d<nDim; d++) {
		baseptr[d] = baseptr[d-1]+info->offsets[d];
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
			case NumArray_Int64:
				{
					char intbuf[TCL_INTEGER_SPACE];
					int *elptr = (int *) (baseptr[nDim-1]);
					snprintf(intbuf, TCL_INTEGER_SPACE, "%d", *elptr);
					Tcl_DStringAppendElement(&srep, intbuf);
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
			baseptr[d] = baseptr[d-1]+info->offsets[d];
		}		
	}

	/* there should be a way to move */
	/* the pointer from DString to Tcl_Obj, here use memcpy */
	naPtr -> length = Tcl_DStringLength(&srep);
	naPtr -> bytes = ckalloc(naPtr->length+1);
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
	 * if not, handle by original proc */
	if (objPtr -> typePtr != &NumArrayTclType) {
		return listSetFromAny(interp, objPtr);
	}

	dprintf(("Converting NumArray to list\n"));

	/* extract internal rep */
	NumArrayInfo *info = objPtr -> internalRep.twoPtrValue.ptr2;
	NumArraySharedBuffer *sharedbuf = objPtr -> internalRep.twoPtrValue.ptr1;

	/* DIRTY HACK: Create new object the internal rep of which
	 * is then transferred to objPtr without properly releasing 
	 * it. */
	Tcl_Obj * result = Tcl_NewListObj(0,NULL);
	if (info -> nDim == 1) {
		/* 1d-case: Construct list of doubles */
		NumArrayIterator it;
		NumArrayIteratorInit(info, sharedbuf, &it);
		switch (info->type) {
			case NumArray_Int64:
				for (; !NumArrayIteratorFinished(&it); NumArrayIteratorAdvance(&it)) {
					int value = NumArrayIteratorDeRefInt(&it);
					Tcl_ListObjAppendElement(interp, result, Tcl_NewIntObj(value));
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
		int i;
		int nelem = info -> dims[0];
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
	objPtr -> internalRep = result -> internalRep;
	objPtr -> typePtr = tclListType;
	return TCL_OK;
}
#endif

NumArraySharedBuffer *NumArrayNewSharedBuffer (int size) {
	NumArraySharedBuffer* sharedbuf=ckalloc(sizeof(NumArraySharedBuffer));
	sharedbuf -> refcount = 0;
	sharedbuf -> buffer	  = ckalloc(size);
	return sharedbuf;
}

int NumArrayIsShared(NumArraySharedBuffer *sharedbuf) {
	return sharedbuf -> refcount > 1;
}


void NumArraySharedBufferIncrRefcount(NumArraySharedBuffer* sharedbuf) {
	sharedbuf -> refcount++;
	dprintf(("===%p NumArraySharedBufferIncrRefcount = %d\n", sharedbuf, sharedbuf->refcount));
}

void NumArraySharedBufferDecrRefcount(NumArraySharedBuffer *sharedbuf) {
	sharedbuf -> refcount--;
	dprintf(("===%p NumArraySharedBufferDecrRefcount = %d\n", sharedbuf, sharedbuf->refcount));
	if (sharedbuf->refcount <= 0) {
		/* destroy this buffer */
		ckfree(sharedbuf->buffer);
		ckfree(sharedbuf);
		dprintf(("===%p  deleted\n", sharedbuf));
	}
}

char * NumArrayGetPtrFromSharedBuffer(NumArraySharedBuffer *sharedbuf) {
	return sharedbuf -> buffer;
}

void NumArrayUnshareBuffer(Tcl_Obj *naObj) {
	if (naObj -> typePtr == &NumArrayTclType) {
		NumArraySharedBuffer *sharedbuf = naObj -> internalRep.twoPtrValue.ptr1;
		NumArrayInfo * info = naObj -> internalRep.twoPtrValue.ptr2;
		
		/* create info for contiguous buffer */
		NumArrayInfo *copyinfo = CreateNumArrayInfo(info->nDim, info->dims, info->type);

		/* alloc fresh shared buffer */
		NumArraySharedBuffer *copysharedbuf = NumArrayNewSharedBuffer(copyinfo -> bufsize);

		/* copy data */
		NumArrayCopy(info, sharedbuf, copyinfo, copysharedbuf);

		/* set into object and release reference to old buffer */
		naObj -> internalRep.twoPtrValue.ptr1 = copysharedbuf;
		naObj -> internalRep.twoPtrValue.ptr2 = copyinfo;
		NumArraySharedBufferIncrRefcount(copysharedbuf);
		NumArraySharedBufferDecrRefcount(sharedbuf);
	}
}

void NumArrayEnsureContiguous(Tcl_Obj *naObj) {
	/* copy buffer/unshare */
	if (naObj -> typePtr == &NumArrayTclType) {
		NumArrayInfo * info = naObj -> internalRep.twoPtrValue.ptr2;

		if (!info->canonical) { 
			NumArrayUnshareBuffer(naObj);
			/* copying the data always creates the canonical form */
		}
	}
}

void NumArrayEnsureWriteable(Tcl_Obj *naObj) {
	if (naObj -> typePtr == &NumArrayTclType) {
		NumArraySharedBuffer *sharedbuf = naObj -> internalRep.twoPtrValue.ptr1;
		if (NumArrayIsShared(sharedbuf))  {
			NumArrayUnshareBuffer(naObj);
		}
	}
}

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

int NumArrayGetBufferFromObj(Tcl_Interp *interp, Tcl_Obj* naObj, char ** bufptr) {
	if (Tcl_ConvertToType(interp, naObj, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}
	NumArraySharedBuffer *sharedbuf = naObj -> internalRep.twoPtrValue.ptr1;
	*bufptr = sharedbuf->buffer;
	return TCL_OK;
}

void NumArraySetInternalRep(Tcl_Obj *naObj, NumArraySharedBuffer *sharedbuf, NumArrayInfo *info) {
	Tcl_InvalidateStringRep(naObj);
	naObj -> internalRep.twoPtrValue.ptr1 = sharedbuf;
	NumArraySharedBufferIncrRefcount(sharedbuf);
	naObj -> internalRep.twoPtrValue.ptr2 = info;
	naObj -> typePtr = &NumArrayTclType;
}

void NumArrayIteratorInitColMaj(NumArrayInfo *info, NumArraySharedBuffer *sharedbuf, NumArrayIterator *it) {
	int d;
	int nDim = info->nDim;

	it -> finished = 0;
	it -> nDim = nDim;
	it -> type = info -> type;
	it -> dinfo = ckalloc(sizeof(NumArrayIteratorDimension)*(info->nDim+1));


	/* compute total offset */
	int offset=0;
	for (d=0; d<info->nDim; d++) {
		offset += info->offsets[d];
	}

	it->baseptr = NumArrayGetPtrFromSharedBuffer(sharedbuf)+offset;

	/* copy dimensional information into the 
	 * iterators counter in reverse order, 
	 * while stripping sngleton dimensions */
	int dfull; 
	for (d=0, dfull=0; dfull < nDim; dfull++) {
		/* strip singleton dimensions */
		if (info -> dims[dfull] == 1) {
			continue;
		}
		it -> dinfo[d].counter = info -> dims[dfull];
		it -> dinfo[d].dim = info -> dims[dfull];
		it -> dinfo[d].pitch = info -> pitches[dfull];
		d++;
	}
	/* check if it was a scalar */
	if (d==0) {
		it -> nDim=1;
		it -> dinfo[0].counter = 1;
		it -> dinfo[0].dim = 1;
		it -> dinfo[0].pitch = 0;
	} else {
		it -> nDim = d;
	}

	/* append a singleton dimension for AdvanceRow 
	 * in the vector case */
	it -> dinfo[it->nDim].counter = 1;
	it -> dinfo[it->nDim].dim = 1;
	it -> dinfo[it->nDim].pitch = 0;
	
}


void NumArrayIteratorInit(NumArrayInfo *info, NumArraySharedBuffer *sharedbuf, NumArrayIterator *it) {
	int d;
	int nDim = info->nDim;

	it -> finished = 0;
	it -> nDim = nDim;
	it -> type = info -> type;
	it -> dinfo = ckalloc(sizeof(NumArrayIteratorDimension)*(info->nDim+1));


	/* compute total offset */
	int offset=0;
	for (d=0; d<info->nDim; d++) {
		offset += info->offsets[d];
	}

	it->baseptr = NumArrayGetPtrFromSharedBuffer(sharedbuf)+offset;
	it->ptr = it->baseptr;

	/* copy dimensional information into the 
	 * iterators counter in reverse order, 
	 * while stripping sngleton dimensions */
	int dfull; 
	for (d=0, dfull=nDim-1; dfull >= 0; dfull--) {
		/* strip singleton dimensions */
		if (info -> dims[dfull] == 1) {
			continue;
		}
		it -> dinfo[d].counter = info -> dims[dfull];
		it -> dinfo[d].dim = info -> dims[dfull];
		it -> dinfo[d].pitch = info -> pitches[dfull];
		d++;
	}
	/* check if it was a scalar */
	if (d==0) {
		it -> nDim=1;
		it -> dinfo[0].counter = 1;
		it -> dinfo[0].dim = 1;
		it -> dinfo[0].pitch = 0;
	} else {
		it -> nDim = d;
	}

	/* append a singleton dimension for AdvanceRow 
	 * in the vector case */
	it -> dinfo[it->nDim].counter = 1;
	it -> dinfo[it->nDim].dim = 1;
	it -> dinfo[it->nDim].pitch = 0;
	
}


int NumArrayIteratorInitObj(Tcl_Interp* interp, Tcl_Obj* naObj, NumArrayIterator *it) {
	NumArraySharedBuffer *sharedbuf; NumArrayInfo *info;

	if (Tcl_ConvertToType(interp, naObj, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}
	
	sharedbuf = naObj -> internalRep.twoPtrValue.ptr1;
	info = naObj -> internalRep.twoPtrValue.ptr2;
	
	NumArrayIteratorInit(info, sharedbuf, it);

	return TCL_OK;
}

void NumArrayIteratorFree(NumArrayIterator *it) {
	ckfree(it->dinfo);
}

void* NumArrayIteratorReset(NumArrayIterator *it) {
	int d;
	for (d=0; d<it->nDim; d++) {
		it->dinfo[d].counter = it->dinfo[d].dim;
	}
	it -> ptr = it->baseptr;
	it -> finished = 0;
	return it -> ptr;
}

int NumArrayIteratorRowLength(NumArrayIterator *it) {
	return it->dinfo[0].dim;
}

int NumArrayIteratorRowPitch(NumArrayIterator *it) {
	return it->dinfo[0].pitch;
}

int NumArrayIteratorRowPitchTyped(NumArrayIterator *it) {
	return it->dinfo[0].pitch / NumArrayType_SizeOf(it->type);
}

void* NumArrayIteratorAdvance(NumArrayIterator *it) {
	/* count indices one down, handle carry */
	NumArrayIteratorDimension *info=it->dinfo;
	if (--(info[0].counter)) {
		return it->ptr += info[0].pitch;
	} else {
		/* the fastest counter has wrapped over to zero. 
		 * Now carry over */
		int d;
		for (d=1; d < it->nDim; d++) {
			/* reset the d-1 dimension */
			info[d-1].counter=info[d-1].dim;
			it->ptr -= info[d-1].pitch*(info[d-1].dim-1);
			
			/* advance the d dimension, check for carry */
			
			if (--(info[d].counter)) {
				return it->ptr += info[d].pitch;
			}
		}
	}

	it->finished = 1;
	return NULL;
}

void* NumArrayIteratorAdvanceRow(NumArrayIterator *it) {
	/* count indices one down, handle carry */
	NumArrayIteratorDimension *info=it->dinfo;
	if (--(info[1].counter)) {
		return it->ptr += info[1].pitch;
	} else {
		/* the fastest counter has wrapped over to zero. 
		 * Now carry over */
		int d;
		for (d=2; d < it->nDim; d++) {
			/* reset the d-1 dimension */
			info[d-1].counter=info[d-1].dim;
			it->ptr-=info[d-1].pitch*(info[d-1].dim-1);
			
			/* advance the d dimension, check for carry */
			
			if (--(info[d].counter)) {
				return it->ptr += info[d].pitch;
			}
		}
	}

	it->finished = 1;
	return NULL;
}


int NumArrayIteratorFinished(NumArrayIterator *it) {
	return it->finished;
}



NumArray_ValueType NumArrayIteratorDeRefValue(NumArrayIterator *it) {
	NumArray_ValueType value;
	value.type = it->type;
	switch (value.type) {
		case NumArray_Int64:
			value.value.Int = *((int*) (it->ptr));
			return value;
		case NumArray_Float64:
			value.value.Float64 = *((double*) (it->ptr));
			return value;
		case NumArray_Complex128:
			value.value.Complex = *((NumArray_Complex*) (it->ptr));
			return value;
		default:
			printf("Unknown data type in array %d", value.type);
			value.type = -1;
			return value;
	}
}

double NumArrayIteratorDeRefDouble(NumArrayIterator *it) {
	return *((double*) (it->ptr));
}

double *NumArrayIteratorDeRefDoublePtr(NumArrayIterator *it) {
	return (double*) (it->ptr);
}

char *NumArrayIteratorDeRefCharPtr(NumArrayIterator *it) {
	return it->ptr;
}

void *NumArrayIteratorDeRefPtr(NumArrayIterator *it) {
	return it->ptr;
}

int NumArrayIteratorDeRefInt(NumArrayIterator *it) {
	return *((int*) (it->ptr));
}

NumArray_Complex NumArrayIteratorDeRefComplex(NumArrayIterator *it) {
	return *((NumArray_Complex*) (it->ptr));
}

/*
int * NumArrayIteratorGetIndices(NumArrayIterator *it) {
	return it->counter;
}

int NumArrayIteratorGetIndex(NumArrayIterator* it, int dim) {
	return it->counter[dim];
} */

/* Implement copying ("memcpy") and fillig with constant ("memset") */

int NumArrayCopy(NumArrayInfo *srcinfo, NumArraySharedBuffer *srcbuf, 
			NumArrayInfo *destinfo, NumArraySharedBuffer *destbuf) {
	
	NumArrayIterator srcit, destit;
	NumArrayIteratorInit(srcinfo, srcbuf, &srcit);
	NumArrayIteratorInit(destinfo, destbuf, &destit);
	const int srcpitch=NumArrayIteratorRowPitchTyped(&srcit);
	const int destpitch=NumArrayIteratorRowPitchTyped(&destit);
	const int length = NumArrayIteratorRowLength(&srcit);

	#define COPYLOOP(TRES, T) \
		if (destinfo->type == NATYPE_FROM_C(TRES) && srcinfo->type == NATYPE_FROM_C(T)) { \
			TRES *result = NumArrayIteratorDeRefPtr(&destit); \
			T* opptr = NumArrayIteratorDeRefPtr(&srcit); \
			while (result) { \
				int i; \
				for (i=0; i<length; i++) { \
					*result = UPCAST(T, TRES, *opptr); \
					opptr+=srcpitch; \
					result+=destpitch; \
				} \
				result = NumArrayIteratorAdvanceRow(&destit); \
				opptr = NumArrayIteratorAdvanceRow(&srcit); \
			} \
		} else 
		
	COPYLOOP(int, int)
	COPYLOOP(double, int)
	COPYLOOP(double, double)
	COPYLOOP(NumArray_Complex, int)
	COPYLOOP(NumArray_Complex, double)
	COPYLOOP(NumArray_Complex, NumArray_Complex) {
		goto cleanit;
	}

	NumArrayIteratorFree(&srcit);
	NumArrayIteratorFree(&destit);
	
	return TCL_OK;

cleanit:
	NumArrayIteratorFree(&srcit);
	NumArrayIteratorFree(&destit);
	return TCL_ERROR;
}

int NumArraySetValue(NumArrayInfo *destinfo, NumArraySharedBuffer *destbuf, NumArray_ValueType value) {
	if (value.type > destinfo->type) {
		/* don't downcast - signal error */
		return TCL_ERROR;
	}

	NumArrayIterator destit;
	NumArrayIteratorInit(destinfo, destbuf, &destit);
	
	/* copy/conversion code for upcasting */
	
	if (destinfo -> type == NumArray_Int64) {
		
		int intvalue;
		if (value.type == NumArray_Int64) {
			intvalue = value.value.Int;
		} else {
			goto cleanit;
		}

		for (; ! NumArrayIteratorFinished(&destit); 
			NumArrayIteratorAdvance(&destit)) {
				*(int *) NumArrayIteratorDeRefPtr(&destit) = intvalue;
		}
	
	} else if (destinfo -> type == NumArray_Float64) {
		double dblvalue;
		if (value.type == NumArray_Int64) {
			dblvalue = value.value.Int;
		} else if (value.type == NumArray_Float64) {
			dblvalue = value.value.Float64;
		} else {
			goto cleanit;
		}

		
		for (; ! NumArrayIteratorFinished(&destit); 
			NumArrayIteratorAdvance(&destit)) {
				*(double *) NumArrayIteratorDeRefPtr(&destit) = dblvalue;
		}

	} else if (destinfo -> type == NumArray_Complex128) {
		NumArray_Complex cplxvalue;
		if (value.type == NumArray_Int64) {
			cplxvalue.re = value.value.Int;
			cplxvalue.im = 0.0;
		} else if (value.type == NumArray_Float64) {
			cplxvalue.re = value.value.Float64;
			cplxvalue.im = 0.0;
		} else if (value.type == NumArray_Complex128) {
			cplxvalue = value.value.Complex;
		} else {
			goto cleanit;
		}
			
		for (; ! NumArrayIteratorFinished(&destit); 
			NumArrayIteratorAdvance(&destit)) {
				*(NumArray_Complex *) NumArrayIteratorDeRefPtr(&destit) = cplxvalue;
		}
	} else {
		goto cleanit;
	}


	NumArrayIteratorFree(&destit);
	
	return TCL_OK;

cleanit:
	NumArrayIteratorFree(&destit);
	return TCL_ERROR;
}

int NumArrayGetScalarValueFromObj(Tcl_Interp *interp, Tcl_Obj* naObj, NumArray_ValueType *value) {
	if (Tcl_ConvertToType(interp, naObj, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}

	NumArraySharedBuffer *sharedbuf = naObj->internalRep.twoPtrValue.ptr1;
	NumArrayInfo * info = naObj -> internalRep.twoPtrValue.ptr2;
	
	if (info->nDim == 1 && info->dims[0] == 1) {
		void *bufptr = NumArrayGetPtrFromSharedBuffer(sharedbuf);
		bufptr += info->offsets[0];

		value -> type = info -> type;
		switch (value -> type) {
			case NumArray_Int64:
				value->value.Int = *((int*) bufptr);
				break;
			case NumArray_Float64:
				value->value.Float64 = *((double*) bufptr);
				break;
			case NumArray_Complex128:
				value->value.Complex = *((NumArray_Complex*) bufptr);
				break;
			default:
				Tcl_SetResult(interp, "Unknown data type in array", NULL);
				return TCL_ERROR;
		}
		return TCL_OK;
	} else {
		Tcl_SetResult(interp, "Expected scalar value", NULL);
		return TCL_ERROR;
	}
	
}


/* implement simple 1D, 2D and 3D indices */

void NumArrayIndexInit(NumArrayInfo *info, NumArraySharedBuffer *sharedbuf, NumArrayIndex *ind) {
	int d;

	ind -> pitches[0] = 0;
	ind -> pitches[1] = 0;
	ind -> pitches[2] = 0;

	ind -> baseptr = NumArrayGetPtrFromSharedBuffer(sharedbuf);
	int offset  = 0;
	for (d=0; d<info->nDim; d++) {
		offset += info -> offsets[d];
		ind -> pitches[d] = info -> pitches[d];
	}

	ind -> baseptr += offset;
}

int NumArrayIndexInitObj(Tcl_Interp *interp, Tcl_Obj *naObj, NumArrayIndex *ind) {
	NumArraySharedBuffer *sharedbuf; NumArrayInfo *info;

	if (Tcl_ConvertToType(interp, naObj, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}
	
	sharedbuf = naObj -> internalRep.twoPtrValue.ptr1;
	info = naObj -> internalRep.twoPtrValue.ptr2;
	
	NumArrayIndexInit(info, sharedbuf, ind);

	return TCL_OK;
}

double NumArrayIndex1DGetDouble(NumArrayIndex *ind, int i) {
	return *((double *) (ind->baseptr + i*ind->pitches[0]));
}

double NumArrayIndex2DGetDouble(NumArrayIndex *ind, int i, int j) {
	return *((double *) (ind->baseptr + i*ind->pitches[0] + j*ind->pitches[1]));
}

double NumArrayIndex3DGetDouble(NumArrayIndex *ind, int i, int j, int k) {
	return *((double *) (ind->baseptr + i*ind->pitches[0] + j*ind->pitches[1]+ k*ind->pitches[2]));
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
#define CMD NumArrayPlusCmd
#define OPINT *result = op1 + op2;
#define OPDBL *result = op1 + op2;
#define OPCPLX *result = NumArray_ComplexAdd(op1, op2);
#include "binop.h"

#define CMD NumArrayMinusCmd
#define OPINT *result = op1 - op2;
#define OPDBL *result = op1 - op2;
#define OPCPLX *result = NumArray_ComplexSubtract(op1, op2);
#include "binop.h"

#define CMD NumArrayTimesCmd
#define OPINT *result = op1 * op2;
#define OPDBL *result = op1 * op2;
#define OPCPLX *result = NumArray_ComplexMultiply(op1, op2);
#include "binop.h"

#define CMD NumArrayLdivideCmd
#define OPINT if (op1!=0) { \
		*result = op2 / op1; \
	} else { \
		Tcl_SetResult(interp, "Integer division by zero", NULL);\
		return TCL_ERROR;\
	}
#define OPDBL *result = op2 / op1;
#define OPCPLX *result = NumArray_ComplexDivide(op2, op1);
#include "binop.h"

#define CMD NumArrayRdivideCmd
#define OPINT if (op2!=0) { \
		*result = op1 / op2; \
	} else { \
		Tcl_SetResult(interp, "Integer division by zero", NULL);\
		return TCL_ERROR;\
	}

#define OPDBL *result = op1 / op2;
#define OPCPLX *result = NumArray_ComplexDivide(op1, op2);
#include "binop.h"

#define CMD NumArrayReminderCmd
#define OPINT if (op2!=0) { \
		*result = op1 % op2; \
	} else { \
		Tcl_SetResult(interp, "Integer division by zero", NULL);\
		return TCL_ERROR;\
	}
#include "binop.h"

#define CMD NumArrayGreaterCmd
#define OPINT *result = (op1>op2);
#define OPDBL *result = (op1>op2);
#define DBLRES int
#include "binop.h"

#define CMD NumArrayLesserCmd
#define OPINT *result = (op1<op2);
#define OPDBL *result = (op1<op2);
#define DBLRES int
#include "binop.h"

#define CMD NumArrayGreaterEqualCmd
#define OPINT *result = (op1>=op2);
#define OPDBL *result = (op1<=op2);
#define DBLRES int
#include "binop.h"

#define CMD NumArrayLesserEqualCmd
#define OPINT *result = (op1<=op2);
#define OPDBL *result = (op1<=op2);
#define DBLRES int
#include "binop.h"

#define CMD NumArrayEqualCmd
#define OPINT *result = (op1==op2);
#define OPDBL *result = (op1==op2);
#define DBLRES int
#define OPCPLX *result = ((op1.re==op2.re) && (op1.im==op2.im))
#define CPLXRES int
#include "binop.h"

#define CMD NumArrayUnequalCmd
#define OPINT *result = (op1!=op2);
#define OPDBL *result = (op1!=op2);
#define DBLRES int
#define OPCPLX *result = ((op1.re!=op2.re) || (op1.im!=op2.im))
#define CPLXRES int
#include "binop.h"

/* boolean operators */
#define CMD NumArrayNotCmd
#define INTOP *result = !op;
#define INTRES int
#include "uniop.h"

#define CMD NumArrayAndCmd
#define OPINT *result = (op1 && op2);
#include "binop.h"

#define CMD NumArrayOrCmd
#define OPINT *result = (op1 || op2);
#include "binop.h"

#define CMD NumArrayPowCmd
/*#define OPINT *result = pow(op1,op2); */
#define OPDBL *result = pow(op1,op2);
#define OPCPLX *result = NumArray_ComplexPow(op1,op2);
#include "binop.h"

#define CMD NumArrayMinCmd
#define OPINT *result = op1 < op2 ? op1 : op2;
#define OPDBL *result = op1 < op2 ? op1 : op2;
#include "binop.h"

#define CMD NumArrayMaxCmd
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
#define CMD NumArrayConvDoubleCmd
#define INTRES double
#define INTOP *result = op;
#define DBLRES double
#define DBLOP *result = op;
#include "uniop.h"

#define CMD NumArrayConvComplexCmd
#define INTRES NumArray_Complex
#define INTOP *result = NumArray_mkComplex(op, 0.0);
#define DBLRES NumArray_Complex
#define DBLOP *result = NumArray_mkComplex(op, 0.0);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = op;
#include "uniop.h"

#define CMD NumArrayAbsCmd
#define INTRES int
#define INTOP *result = abs(op);
#define DBLRES double
#define DBLOP *result = fabs(op);
#define CPLXRES double
#define CPLXOP *result = NumArray_ComplexAbs(op);
#include "uniop.h"

#define CMD NumArraySignCmd
#define INTRES int
#define INTOP *result = isign(op);
#define DBLRES double
#define DBLOP *result = fsign(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexSign(op);
#include "uniop.h"

#define CMD NumArrayArgCmd
#define INTRES int
#define INTOP *result = 0;
#define DBLRES double
#define DBLOP *result = 0.0;
#define CPLXRES double
#define CPLXOP *result = NumArray_ComplexArg(op);
#include "uniop.h"

#define CMD NumArrayConjCmd
#define INTRES int
#define INTOP *result = op;
#define DBLRES double
#define DBLOP *result = op;
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexConj(op);
#include "uniop.h"

#define CMD NumArrayRealCmd
#define INTRES int
#define INTOP *result = op;
#define DBLRES double
#define DBLOP *result = op;
#define CPLXRES double
#define CPLXOP *result = op.re;
#include "uniop.h"

#define CMD NumArrayImagCmd
#define INTRES int
#define INTOP *result = 0;
#define DBLRES double
#define DBLOP *result = 0.0;
#define CPLXRES double
#define CPLXOP *result = op.im;
#include "uniop.h"


#define CMD NumArrayNegCmd
#define INTRES int
#define INTOP *result = -op;
#define DBLRES double
#define DBLOP *result = -op;
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexNeg(op);
#include "uniop.h"

#define CMD NumArraySinCmd
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = sin(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexSin(op);
#include "uniop.h"

#define CMD NumArrayCosCmd
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = cos(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexCos(op);
#include "uniop.h"

#define CMD NumArrayTanCmd
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = tan(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexTan(op);
#include "uniop.h"

#define CMD NumArrayExpCmd
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = exp(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexExp(op);
#include "uniop.h"

#define CMD NumArrayLogCmd
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = log(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexLog(op);
#include "uniop.h"

#define CMD NumArraySqrtCmd
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = sqrt(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexSqrt(op);
#include "uniop.h"

#define CMD NumArraySinhCmd
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = sinh(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexSinh(op);
#include "uniop.h"

#define CMD NumArrayCoshCmd
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = cosh(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexCosh(op);
#include "uniop.h"

#define CMD NumArrayTanhCmd
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = tanh(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexTanh(op);
#include "uniop.h"

#define CMD NumArrayAsinCmd
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = asin(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexAsin(op);
#include "uniop.h"

#define CMD NumArrayAcosCmd
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = acos(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexAcos(op);
#include "uniop.h"

#define CMD NumArrayAtanCmd
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = atan(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexAtan(op);
#include "uniop.h"

#define CMD NumArrayAsinhCmd
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = asinh(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexAsinh(op);
#include "uniop.h"

#define CMD NumArrayAcoshCmd
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = acosh(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexAcosh(op);
#include "uniop.h"

#define CMD NumArrayAtanhCmd
#define INTRES double
#define DBLRES double
#define INTOP DBLOP
#define DBLOP *result = atanh(op);
#define CPLXRES NumArray_Complex
#define CPLXOP *result = NumArray_ComplexAtanh(op);
#include "uniop.h"


/* Implement reductions with optional dimension */
#define CMD NumArraySumCmd
#define INIT ;
#define FIRST accum=op;
#define INTOP accum+=op;
#define DBLOP accum+=op;
#define CPLXOP accum=NumArray_ComplexAdd(accum, op);
#define RETURN ;
#include "reduction.h"


#define CMD NumArrayMeanCmd
#define INIT ;
#define FIRST accum=op;
#define INTRES double
#define INTOP accum+=op;
#define DBLOP accum+=op;
#define CPLXOP accum=NumArray_ComplexAdd(accum, op);
#define RETURN accum /= nlength;
#define CPLXRETURN accum.re /= nlength; accum.im /= nlength; 
#include "reduction.h"

#define CMD NumArrayAxisMinCmd
#define INIT ;
#define FIRST accum=op;
#define INTOP if (op < accum) accum=op;
#define DBLOP if (op < accum) accum=op;
#define RETURN ;
#include "reduction.h"

#define CMD NumArrayAxisMaxCmd
#define INIT ;
#define FIRST accum=op;
#define INTOP if (op > accum) accum=op;
#define DBLOP if (op > accum) accum=op;
#define RETURN ;
#include "reduction.h"

#define CMD NumArrayStdCmd
#define INIT double first; double sum=0;
#define FIRST accum=0; first=op;
#define DBLOP  accum+=(op-first)*(op-first); sum+=(op-first);
#define INTOP DBLOP
#define INTRES double
#define RETURN accum = (nlength==1)?0:sqrt(accum/(nlength-1) - (sum*sum/nlength)/(nlength-1));
#include "reduction.h"

#define CMD NumArrayStd1Cmd
#define INIT double first; double sum=0;
#define FIRST accum=0; first=op;
#define DBLOP  accum+=(op-first)*(op-first); sum+=(op-first);
#define INTOP  DBLOP
#define INTRES double
#define RETURN accum = sqrt(accum/nlength - (sum/nlength)*(sum/nlength));
#include "reduction.h"

#define CMD NumArrayAllCmd
#define INIT ;
#define FIRST if (op) { accum=1; } else { accum=0; }
#define INTOP if (!op) { accum=0; break; } 
#define RETURN ;
#include "reduction.h"

#define CMD NumArrayAnyCmd
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

	Tcl_RegisterObjType(&NumArrayTclType);
	myTcl_MakeEnsemble(interp, "::numarray", "::numarray", implementationMap);

	Complex_Init(interp);

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


