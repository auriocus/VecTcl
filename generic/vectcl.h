#ifndef VECTCL_H
#define VECTCL_H
#include <tcl.h>
#include "nacomplex.h"

#include <stdlib.h>
#include <stddef.h>

typedef ptrdiff_t index_t;

/* data type for VecTcl objects */

extern const Tcl_ObjType* VecTclNumArrayObjType;

/* Stub init function. 
 * NOTE: This is not actually a real stubs library, but the linking
 * etc. should be identical. TODO: Replace later with real stubs */
char* Vectcl_InitStubs(Tcl_Interp *interp, const char *version, int exact);

typedef long int NaWideInt;

/* Possible datatypes */
typedef enum {
	NumArray_NoType=-1,
	NumArray_Int=0,
	NumArray_Int8=1,
	NumArray_Uint8=2,
	NumArray_Int16=3,
	NumArray_Uint16=4,
	NumArray_Int32=5,
	NumArray_Uint32=6,
	NumArray_Int64=7,
	NumArray_Uint64=8,
	NumArray_Float32=9,
	NumArray_Float64=10,
	NumArray_Complex64=11,
	NumArray_Complex128=12,
	NumArray_TclObj=13,
	NumArray_SentinelType=14
	/* NumArray_BigInt=4 ?*/
} NumArrayType;

#define NUMARRAYTYPESTRINGS {"int", "int8", "uint8", "int16", "uint16", \
	"int32", "uint32", "int64", "uint64", "float", "double", "complex64", "complex128", "Tcl_Obj", "Sentinel" }
/* struct for storing a single polymorphic value */
typedef struct {
	NumArrayType type;
	union {
		NaWideInt Int;
		double Float64;
		NumArray_Complex Complex;
	} value;
} NumArray_ValueType;

/* Manipulating datatypes */
NumArrayType NumArray_UpcastType(NumArrayType base);
NumArrayType NumArray_UpcastCommonType(NumArrayType type1, NumArrayType type2);

int NumArrayConvertToType(Tcl_Interp *interp, Tcl_Obj *naObj, NumArrayType type, Tcl_Obj **dest);
size_t NumArrayType_SizeOf(NumArrayType type);

/* Metadata to describe the raw buffer */
typedef struct  {
	NumArrayType type;
	int nDim;
	size_t bufsize;
	index_t offset;
	int canonical;
	index_t *dims;
	index_t *pitches;
} NumArrayInfo;

/* Constructor, destructor, copy constructor for NumArrayInfo */
NumArrayInfo* CreateNumArrayInfo(int nDim, const index_t *dims, NumArrayType dtype);
NumArrayInfo* CreateNumArrayInfoColMaj(int nDim, const index_t *dims, NumArrayType dtype);
void DeleteNumArrayInfo(NumArrayInfo* info);
NumArrayInfo* DupNumArrayInfo(NumArrayInfo* src);

#define ISSCALARINFO(i) (i->nDim == 1 && i->dims[0] == 1)
#define ISEMPTYINFO(i) (i->nDim == 1 && i->dims[0] == 0)

/* Create an array slice */
int NumArrayInfoSlice(Tcl_Interp *interp, NumArrayInfo *info, Tcl_Obj *slicelist, NumArrayInfo **resultPtr);
int NumArrayInfoSlice1Axis(Tcl_Interp *interp, NumArrayInfo *info, int axis, index_t start, index_t stop, index_t incr);

/* A refcounted buffer */
typedef struct {
	int refcount;
	char *buffer;
} NumArraySharedBuffer;

NumArraySharedBuffer *NumArrayNewSharedBuffer (size_t size);
void *NumArrayGetPtrFromSharedBuffer(NumArraySharedBuffer *sharedbuf);
void NumArraySharedBufferDecrRefcount(NumArraySharedBuffer *sharedbuf);
void NumArraySharedBufferIncrRefcount(NumArraySharedBuffer *sharedbuf);

/* Convenience to create a vector and 2D matrix */
Tcl_Obj *NumArrayNewVector(NumArrayType type, index_t m);
Tcl_Obj *NumArrayNewMatrix(NumArrayType type, index_t m, index_t n);
Tcl_Obj *NumArrayNewMatrixColMaj(NumArrayType type, index_t m, index_t n);

/* Assemble / retrieve info and data storage into a Tcl_Obj */
void NumArraySetInternalRep(Tcl_Obj *naObj, NumArraySharedBuffer *sharedbuf, NumArrayInfo *info);
NumArrayInfo *NumArrayGetInfoFromObj(Tcl_Interp *interp, Tcl_Obj* naObj);
NumArraySharedBuffer *NumArrayGetSharedBufferFromObj(Tcl_Interp *interp, Tcl_Obj* naObj);
void *NumArrayGetPtrFromObj(Tcl_Interp *interp, Tcl_Obj* naObj);

/* Handle copy-on-write */
void NumArrayEnsureContiguous(Tcl_Obj *naObj);
void NumArrayEnsureWriteable(Tcl_Obj *naObj);
void NumArrayUnshareBuffer(Tcl_Obj *naObj);

/* Iterator to loop over all elements in an array */
typedef struct {
	index_t counter;
	index_t pitch;
	index_t dim;
} NumArrayIteratorDimension;

typedef struct {
	char *ptr;
	int finished;
	int nDim;
	NumArrayIteratorDimension *dinfo;
	NumArrayType type;
	char *baseptr;
} NumArrayIterator;

/* Constructor and destructor for iterator objects */
int NumArrayIteratorInitObj(Tcl_Interp* interp, Tcl_Obj* naObj, NumArrayIterator *it);
void NumArrayIteratorInit(NumArrayInfo *info, NumArraySharedBuffer *buffer, NumArrayIterator *it);
void NumArrayIteratorInitColMaj(NumArrayInfo *info, NumArraySharedBuffer *buffer, NumArrayIterator *it);
void NumArrayIteratorFree(NumArrayIterator *it);
void* NumArrayIteratorReset(NumArrayIterator *it);

/* Iterator advance and test for end condition */
void* NumArrayIteratorAdvance(NumArrayIterator *it);
int NumArrayIteratorFinished(NumArrayIterator *it);

/* For faster looping, advance row-wise. */
void* NumArrayIteratorAdvanceRow(NumArrayIterator *it);
/* Retrieve pitch and
 * number of elements in the innermost loop */
index_t NumArrayIteratorRowPitchTyped(NumArrayIterator *it);
index_t NumArrayIteratorRowPitch(NumArrayIterator *it);
index_t NumArrayIteratorRowLength(NumArrayIterator *it);

/* Retrieve value from iterator */
/* Pointer */
void *NumArrayIteratorDeRefPtr(NumArrayIterator *it);
/* single dataype */
NaWideInt NumArrayIteratorDeRefInt(NumArrayIterator *it);
double NumArrayIteratorDeRefDouble(NumArrayIterator *it);
NumArray_Complex NumArrayIteratorDeRefComplex(NumArrayIterator *it);
/* polymorph value */
NumArray_ValueType NumArrayIteratorDeRefValue(NumArrayIterator *it);


/* Analogs to memcpy and memset,
 * When the destination buffer has a type larger than the source buffer/value
 * the value is upcast to the destination. 
 * The return value indicates whether the copy was successfull. */
int NumArrayCopy(NumArrayInfo *srcinfo, NumArraySharedBuffer *srcbuf, 
			NumArrayInfo *destinfo, NumArraySharedBuffer *destbuf);

/* same with a Tcl_Obj */
int NumArrayObjCopy(Tcl_Interp *interp, Tcl_Obj *srcObj, Tcl_Obj *destObj);

/* Fill a buffer with a constant value */
int NumArraySetValue(NumArrayInfo *destinfo, NumArraySharedBuffer *destbuf, NumArray_ValueType value);

/* Retrieve value from scalar NumArray */

int NumArrayGetScalarValueFromObj(Tcl_Interp *interp, Tcl_Obj* naObj, NumArray_ValueType *value);
/* Index object for simple 1D, 2D and 3D indexing */
typedef struct {
	index_t pitches[3];
	char *baseptr;
} NumArrayIndex;

void NumArrayIndexInit(NumArrayInfo *info, NumArraySharedBuffer *sharedbuf, NumArrayIndex *ind);
int NumArrayIndexInitObj(Tcl_Interp *interp, Tcl_Obj *naObj, NumArrayIndex *ind);
void* NumArrayIndex1DGetPtr(NumArrayIndex *ind, index_t i);
void* NumArrayIndex2DGetPtr(NumArrayIndex *ind, index_t i, index_t j);
void *NumArrayIndex3DGetPtr(NumArrayIndex *ind, index_t i, index_t j, index_t k);

#endif
