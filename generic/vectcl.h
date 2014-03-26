#ifndef VECTCL_H
#define VECTCL_H
#include <tcl.h>
#include "nacomplex.h"

/* Token pasting macro */
#define NUMARRAYTPASTER(X, Y) X##Y
#define NUMARRAYTPASTER3(X, Y, Z) X##Y##Z
#define NUMARRAYTPASTER4(X, Y, Z, W) X##Y##Z##W

typedef enum {
	NumArray_NoType=-1,
	NumArray_Int64=0,
	NumArray_Float64=1,
	NumArray_Complex128=2,
	NumArray_SentinelType=3
} NumArrayType;

extern const char * NumArray_typename[NumArray_SentinelType];

/* Macros for preprocessor magic
 * Convert between C type and numeric array type */

#define C_FROM_NATYPE_NumArray_Int64 int
#define C_FROM_NATYPE_NumArray_Float64 double
#define C_FROM_NATYPE_NumArray_Complex128 NumArray_Complex

#define NATYPE_FROM_C_int	NumArray_Int64
#define NATYPE_FROM_C_double NumArray_Float64
#define NATYPE_FROM_C_NumArray_Complex NumArray_Complex128

#define C_FROM_NATYPE(X) NUMARRAYTPASTER(C_FROM_NATYPE_, X)
#define NATYPE_FROM_C(X) NUMARRAYTPASTER(NATYPE_FROM_C_, X)

/* Macro to handle upcasting */
#define UPCAST(TFROM, TTO, X) NUMARRAYTPASTER4(UPCAST_, TFROM, _, TTO)(X)

#define UPCAST_int_int(X) X
#define UPCAST_int_double(X) X
#define UPCAST_double_double(X) X
#define UPCAST_int_NumArray_Complex(X) NumArray_mkComplex(X, 0.0)
#define UPCAST_double_NumArray_Complex(X) NumArray_mkComplex(X, 0.0)
#define UPCAST_NumArray_Complex_NumArray_Complex(X) X




/* Useful to print formatted error messages */
#define RESULTPRINTF(X) Tcl_SetObjResult(interp, Tcl_ObjPrintf X)

/* struct for storing a single polymorphic value */
typedef struct {
	NumArrayType type;
	union {
		int Int;
		double Float64;
		NumArray_Complex Complex;
	} value;
} NumArray_ValueType;

NumArrayType NumArray_UpcastType(NumArrayType base);
NumArrayType NumArray_UpcastCommonType(NumArrayType type1, NumArrayType type2);

int NumArrayConvertToType(Tcl_Interp *interp, Tcl_Obj *naObj, NumArrayType type, Tcl_Obj **dest);

typedef struct  {
	NumArrayType type;
	int nDim;
	int bufsize;
	int canonical;
	int *dims;
	int *offsets;
	int *pitches;
} NumArrayInfo;

typedef struct {
	int refcount;
	char *buffer;
} NumArraySharedBuffer;

extern const Tcl_ObjType NumArrayTclType;

NumArrayInfo* CreateNumArrayInfo(int nDim, int*dims, NumArrayType dtype);
void DeleteNumArrayInfo(NumArrayInfo* info);
NumArrayInfo* DupNumArrayInfo(NumArrayInfo* src);

int NumArrayInfoSlice(Tcl_Interp *interp, NumArrayInfo *info, Tcl_Obj *slicelist, NumArrayInfo **resultPtr);
int NumArrayInfoSlice1Axis(Tcl_Interp *interp, NumArrayInfo *info, int axis, int start, int stop, int incr);

int NumArrayGetBufferFromObj(Tcl_Interp *interp, Tcl_Obj* naObj, char ** bufptr);
void NumArrayIncrRefcount(Tcl_Obj* naObj);
void NumArrayDecrRefcount(Tcl_Obj* naObj);
void NumArraySetInternalRep(Tcl_Obj *naObj, NumArraySharedBuffer *sharedbuf, NumArrayInfo *info);
void NumArrayEnsureContiguous(Tcl_Obj *naObj);
void NumArrayEnsureWriteable(Tcl_Obj *naObj);

#define ISSCALARINFO(i) (i->nDim == 1 && i->dims[0] == 1)
#define ISEMPTYINFO(i) (i->nDim == 1 && i->dims[0] == 0)

NumArraySharedBuffer *NumArrayNewSharedBuffer (int size);
char * NumArrayGetPtrFromSharedBuffer(NumArraySharedBuffer *sharedbuf);
void NumArraySharedBufferDecrRefcount(NumArraySharedBuffer *sharedbuf);
void NumArraySharedBufferIncrRefcount(NumArraySharedBuffer* sharedbuf);
void NumArrayUnshareBuffer(Tcl_Obj *naObj);

/* Iterator to loop over all elements in array */
typedef struct {
	int counter;
	int pitch;
	int dim;
} NumArrayIteratorDimension;

typedef struct {
	void *baseptr;
	int finished;
	int nDim;
	NumArrayIteratorDimension *dinfo;
	NumArrayType type;
} NumArrayIterator;


/* Init & Free iterator from NumArray */
int NumArrayIteratorInitObj(Tcl_Interp* interp, Tcl_Obj* naObj, NumArrayIterator *it);
void NumArrayIteratorInit(NumArrayInfo *info, NumArraySharedBuffer *buffer, NumArrayIterator *it);
void NumArrayIteratorFree(NumArrayIterator *it);

/* Iterator advance and test for end condition */
/*int * NumArrayIteratorGetIndices(NumArrayIterator *it);
int NumArrayIteratorGetIndex(NumArrayIterator* it, int dim);*/
void* NumArrayIteratorAdvance(NumArrayIterator *it);

/* void *NumArrayIteratorAdvanceRow(NumArrayIterator *it); */

int NumArrayIteratorFinished(NumArrayIterator *it);

/* Retrieve value from iterator */
NumArray_ValueType NumArrayIteratorDeRefValue(NumArrayIterator *it);
double NumArrayIteratorDeRefDouble(NumArrayIterator *it);
double *NumArrayIteratorDeRefDoublePtr(NumArrayIterator *it);
int NumArrayIteratorDeRefInt(NumArrayIterator *it);
NumArray_Complex NumArrayIteratorDeRefComplex(NumArrayIterator *it);
char *NumArrayIteratorDeRefCharPtr(NumArrayIterator *it);
void *NumArrayIteratorDeRefPtr(NumArrayIterator *it);


/* Analogs to memcpy and memset,
 * When the destination buffer has a type larger than the source buffer/value
 * the value is upcast to the destination. 
 * The return value indicates whether the copy was successfull. */
int NumArrayCopy(NumArrayInfo *srcinfo, NumArraySharedBuffer *srcbuf, 
			NumArrayInfo *destinfo, NumArraySharedBuffer *destbuf);

int NumArraySetValue(NumArrayInfo *destinfo, NumArraySharedBuffer *destbuf, NumArray_ValueType value);

/* Retrieve value from scalar NumArray */

int NumArrayGetScalarValueFromObj(Tcl_Interp *interp, Tcl_Obj* naObj, NumArray_ValueType *value);
/* Index object for simple 1D, 2D and 3D indexing */
typedef struct {
	int pitches[3];
	char *baseptr;
} NumArrayIndex;

void NumArrayIndexInit(NumArrayInfo *info, NumArraySharedBuffer *sharedbuf, NumArrayIndex *ind);
int NumArrayIndexInitObj(Tcl_Interp *interp, Tcl_Obj *naObj, NumArrayIndex *ind);
double NumArrayIndex1DGetDouble(NumArrayIndex *ind, int i);
double NumArrayIndex2DGetDouble(NumArrayIndex *ind, int i, int j);

#define SUBCOMMAND(X) \
	int	X(ClientData dummy, Tcl_Interp *interp,\
		int objc, Tcl_Obj *const *objv)

SUBCOMMAND(NumArrayCreateCmd);
SUBCOMMAND(NumArrayConstFillCmd);
SUBCOMMAND(NumArrayInfoCmd);
SUBCOMMAND(NumArrayDimensionsCmd);
SUBCOMMAND(NumArrayReshapeCmd);
SUBCOMMAND(NumArrayTransposeCmd);
SUBCOMMAND(NumArrayAdjointCmd);
SUBCOMMAND(NumArraySliceCmd);
SUBCOMMAND(NumArraySetCmd);
SUBCOMMAND(NumArrayGetCmd);
SUBCOMMAND(NumArrayFastCopyCmd);
SUBCOMMAND(NumArrayFastAddCmd);
SUBCOMMAND(NumArrayConvDoubleCmd);
SUBCOMMAND(NumArrayConvComplexCmd);
SUBCOMMAND(NumArrayPlusCmd);
SUBCOMMAND(NumArrayMinusCmd);
SUBCOMMAND(NumArrayTimesCmd);
SUBCOMMAND(NumArrayLdivideCmd);
SUBCOMMAND(NumArrayRdivideCmd);
SUBCOMMAND(NumArrayBackslashCmd);
SUBCOMMAND(NumArrayReminderCmd);
SUBCOMMAND(NumArrayPowCmd);
SUBCOMMAND(NumArrayMinCmd);
SUBCOMMAND(NumArrayMaxCmd);
SUBCOMMAND(NumArraySetAssignCmd);
SUBCOMMAND(NumArrayPlusAssignCmd);
SUBCOMMAND(NumArrayMinusAssignCmd);
SUBCOMMAND(NumArrayTimesAssignCmd);
SUBCOMMAND(NumArrayLdivideAssignCmd);
SUBCOMMAND(NumArrayRdivideAssignCmd);
SUBCOMMAND(NumArrayPowAssignCmd);
SUBCOMMAND(NumArrayNegCmd);
SUBCOMMAND(NumArrayNegCmd);
SUBCOMMAND(NumArraySinCmd);
SUBCOMMAND(NumArrayCosCmd);
SUBCOMMAND(NumArrayTanCmd);
SUBCOMMAND(NumArrayExpCmd);
SUBCOMMAND(NumArrayLogCmd);
SUBCOMMAND(NumArraySqrtCmd);
SUBCOMMAND(NumArraySinhCmd);
SUBCOMMAND(NumArrayCoshCmd);
SUBCOMMAND(NumArrayTanhCmd);
SUBCOMMAND(NumArrayAsinCmd);
SUBCOMMAND(NumArrayAcosCmd);
SUBCOMMAND(NumArrayAtanCmd);
SUBCOMMAND(NumArrayAsinhCmd);
SUBCOMMAND(NumArrayAcoshCmd);
SUBCOMMAND(NumArrayAtanhCmd);
SUBCOMMAND(NumArrayQRecoCmd);
SUBCOMMAND(NumArraySumCmd);
SUBCOMMAND(NumArrayAxisMinCmd);
SUBCOMMAND(NumArrayAxisMaxCmd);
SUBCOMMAND(NumArrayMeanCmd);
SUBCOMMAND(NumArrayStdCmd);
SUBCOMMAND(NumArrayStd1Cmd);

#endif
