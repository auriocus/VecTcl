#include "vectclInt.h"
#include "map.h"

/* This actually isn't a stub file. 
  * It contains the actual implementations out of laziness */

/* The pointer to the object type. Must be populated in either
 * Vectcl_Init or Vectcl_InitStubs */
const Tcl_ObjType* VecTclNumArrayObjType;

char* Vectcl_InitStubs(Tcl_Interp *interp, const char *version, int exact) {
	/* Ignore version for now */
	VecTclNumArrayObjType = Tcl_GetObjType("NumArray");
	if (!VecTclNumArrayObjType) {
		return NULL;
	} else {
		return PACKAGE_VERSION;
	}
}


/* Manipulating datatypes */

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

size_t NumArrayType_SizeOf(NumArrayType type) {
#define TYPESIZE(X) case X: return sizeof(C_FROM_NATYPE(X));

	switch (type) {
		MAP(TYPESIZE, NA_NUMERICTYPES) 
		default: 
			printf("Error: unknown data type %d", type);
			return 1;	
	}
}

/* Function to upcast an array to the type requested */
int NumArrayConvertToType(Tcl_Interp *interp, Tcl_Obj *naObj, NumArrayType type, Tcl_Obj **dest) {
	NumArrayInfo *info, *convinfo; 
	NumArraySharedBuffer *sharedbuf, *convbuf;
	if (Tcl_ConvertToType(interp, naObj, VecTclNumArrayObjType) != TCL_OK) {
		return TCL_ERROR;
	}
	
	info = naObj -> internalRep.twoPtrValue.ptr2;
	sharedbuf = naObj -> internalRep.twoPtrValue.ptr1;

	/* Check for no-op */
	if (type == info -> type) {
		*dest = naObj;
		Tcl_IncrRefCount(*dest);
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

	/* all conversions which can be done by the C compiler */
	#define BUILTINCONV(X, Y) \
		if (type == X && info -> type == Y) { \
			C_FROM_NATYPE(X) *bufptr = NumArrayIteratorDeRefPtr(&convit); \
			for (; ! NumArrayIteratorFinished(&it); NumArrayIteratorAdvance(&it)) { \
				* bufptr++ = (C_FROM_NATYPE(X)) (*((C_FROM_NATYPE(Y)*)NumArrayIteratorDeRefPtr(&it))); \
			} \
			goto ready; \
		}
		
	#define INNER(X) MAPARG(BUILTINCONV, X, NA_REALTYPES)
	#define OUTER MAP(INNER, NA_REALTYPES)
	OUTER
	#undef OUTER
	#undef INNER
	#undef BUILTINCONV

	/* conversions to complex from real: set imag part to 0 */
	#define REAL2COMPLEXCONV(CX, RY) \
		if (type == CX && info -> type == RY) { \
			C_FROM_NATYPE(CX) *bufptr = NumArrayIteratorDeRefPtr(&convit); \
			for (; ! NumArrayIteratorFinished(&it); NumArrayIteratorAdvance(&it)) { \
				C_FROM_NATYPE(CX) value; \
				value.re = *((C_FROM_NATYPE(RY)*)NumArrayIteratorDeRefPtr(&it)); \
				value.im = 0.0; \
				* bufptr++ = value; \
			} \
			goto ready; \
		}
		
	#define INNER(X) MAPARG(REAL2COMPLEXCONV, X, NA_REALTYPES)
	#define OUTER MAP(INNER, NumArray_Complex64, NumArray_Complex128)
	OUTER
	#undef OUTER
	#undef INNER
	#undef REAL2COMPLEXCONV

	/* If we are here, no conversion has triggered */
	Tcl_SetResult(interp, "Unknown data type conversion", NULL);
	DeleteNumArrayInfo(convinfo);
	NumArraySharedBufferDecrRefcount(convbuf);
	return TCL_ERROR;

ready:
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

static inline NumArrayInfo * AllocNumArrayInfo(int nDim) {
	/* Allocate uninitialized space for NumArrayInfo */
	char *allocptr = malloc(sizeof(NumArrayInfo)+2*sizeof(index_t)*nDim);
	NumArrayInfo* result = (NumArrayInfo *)allocptr;
	result -> nDim = nDim;
	result -> dims = (index_t*) (allocptr+sizeof(NumArrayInfo));
	result -> pitches = (index_t*) (allocptr+sizeof(NumArrayInfo)+sizeof(index_t)*nDim);
	return result;
}

/* Constructor, destructor, copy constructor for NumArrayInfo */
NumArrayInfo* CreateNumArrayInfo(int nDim, const index_t *dims, NumArrayType dtype) {
	/* Create empty information with nDim number of dimensions
	 * initialised to dimensions in dims.
	 * initialized to zero size, if dims == NULL
	 * TODO catch out of memory */
	int d = 0;
	size_t elemsize=NumArrayType_SizeOf(dtype);
	
	NumArrayInfo* result = AllocNumArrayInfo(nDim);
	result -> canonical = 1;
	result -> bufsize = elemsize;
	result -> type = dtype;
	result -> offset = 0;
	
	for (d=0; d<nDim; d++) {
		index_t dim=0;
		if (dims) {
			dim=dims[d];
			if (dim<0) dim=0;
		}
		result -> bufsize *= dim;
		result -> dims[d] = dim;
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
NumArrayInfo* CreateNumArrayInfoColMaj(int nDim, const index_t *dims, NumArrayType dtype) {
	/* Create empty information with nDim number of dimensions
	 * initialised to dimensions in dims.
	 * initialized to zero size, if dims == NULL
	 * TODO catch out of memory */
	int d = 0;
	size_t elemsize=NumArrayType_SizeOf(dtype);
	
	NumArrayInfo* result = AllocNumArrayInfo(nDim);
	result -> canonical = 0; /* column major */
	result -> bufsize = elemsize;
	result -> type = dtype;
	result -> offset = 0;

	for (d=0; d<nDim; d++) {
		index_t dim=0;
		if (dims) {
			dim=dims[d];
			if (dim<0) dim=0;
		}
		result -> bufsize *= dim;
		result -> dims[d] = dim;
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


void DeleteNumArrayInfo(NumArrayInfo* info) {
	free(info);
}

NumArrayInfo* DupNumArrayInfo(NumArrayInfo* src) {
	/* Copy NumArrayInfo into new struct */
	int i = 0;
	int nDim = src -> nDim;

	NumArrayInfo* result = AllocNumArrayInfo(nDim);
	result -> canonical = src -> canonical;
	result -> bufsize = src -> bufsize;
	result -> type = src -> type;
	result -> offset = src -> offset;

	for (i=0; i<nDim; i++) {
		result -> dims[i] = src -> dims[i];
		result -> pitches[i] = src -> pitches[i];
	}
	DEBUGPRINTF(("DupNumArrayInfo %p->%p\n", src, result));
	return result;
}


/* Create an array slice */

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

		Tcl_WideInt start, stop, incr;

		if (Tcl_GetWideIntFromObj(interp, elems[0], &start) != TCL_OK) {
			goto cleaninfo;
		}

		if (Tcl_GetWideIntFromObj(interp, elems[1], &stop) != TCL_OK) {
			goto cleaninfo;
		}

		if (Tcl_GetWideIntFromObj(interp, elems[2], &incr) != TCL_OK) {
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
			Tcl_WideInt swap = start;
			start = stop; 
			stop = swap;
		}

		/* Count number of elements in this dimension */
		index_t nelem = (stop - start) / incr + 1;
		sliceinfo -> dims[d] = nelem;
		sliceinfo -> offset += start * info->pitches[d];
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

int NumArrayInfoSlice1Axis(Tcl_Interp *interp, NumArrayInfo *info, int axis, index_t start, index_t stop, index_t incr) {
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
		index_t swap = start;
		start = stop; 
		stop = swap;
	}

	/* Count number of elements in this dimension */
	index_t nelem = (stop - start) / incr + 1;
	info -> dims[axis] = nelem;
	info -> offset += start * info->pitches[axis];
	info -> pitches[axis] = info -> pitches[axis]*incr;

	return TCL_OK;
}

/* Remove singleton dimensions */
void NumArrayStripSingletonDimensions(NumArrayInfo *info) {
	 /* Note this also transforms rowvectors into columnvectors
	 * but these are interchangeable (and with lists) anyway */
	
	/* count number of singleton dimensions */
	
	int d=0, dest=0;
	for (d=0; d<info -> nDim; d++) {
		info -> dims[dest] = info -> dims[d];
		info -> pitches[dest] = info -> pitches[d];
		if (info -> dims[d] != 1) {
			dest++;
		}
	}

	
	info -> nDim = dest;
	if (info -> nDim == 0) {
		/* it reduced to a single scalar value */
		info -> nDim = 1;
	}
}


/* A refcounted buffer */
NumArraySharedBuffer *NumArrayNewSharedBuffer (size_t size) {
	/* Alloc this buffer in one block */
	char *baseptr = malloc(sizeof(NumArraySharedBuffer)+size);
	NumArraySharedBuffer* sharedbuf= (NumArraySharedBuffer*) baseptr;
	sharedbuf -> refcount = 0;
	sharedbuf -> buffer	  = baseptr + sizeof(NumArraySharedBuffer);
	return sharedbuf;
}

void * NumArrayGetPtrFromSharedBuffer(NumArraySharedBuffer *sharedbuf) {
	return (char*)sharedbuf + sizeof(NumArraySharedBuffer);
}

void NumArraySharedBufferIncrRefcount(NumArraySharedBuffer* sharedbuf) {
	sharedbuf -> refcount++;
	DEBUGPRINTF(("===%p NumArraySharedBufferIncrRefcount = %d\n", sharedbuf, sharedbuf->refcount));
}

void NumArraySharedBufferDecrRefcount(NumArraySharedBuffer *sharedbuf) {
	sharedbuf -> refcount--;
	DEBUGPRINTF(("===%p NumArraySharedBufferDecrRefcount = %d\n", sharedbuf, sharedbuf->refcount));
	if (sharedbuf->refcount <= 0) {
		/* destroy this buffer */
		free(sharedbuf);
		DEBUGPRINTF(("===%p  deleted\n", sharedbuf));
	}
}

int NumArrayIsShared(NumArraySharedBuffer *sharedbuf) {
	return sharedbuf -> refcount > 1;
}

/* Convenience to create a vector and 2D matrix */
/* Functions to create a vector or matrix */
Tcl_Obj *NumArrayNewVector(NumArrayType type, index_t m) {
	Tcl_Obj* result = Tcl_NewObj();
	NumArrayInfo *info = CreateNumArrayInfo(1, &m, type);
	NumArraySharedBuffer *sharedbuf = NumArrayNewSharedBuffer(info->bufsize);
	NumArraySetInternalRep(result, sharedbuf, info);
	return result;
}

Tcl_Obj *NumArrayNewMatrix(NumArrayType type, index_t m, index_t n) {
	Tcl_Obj* result = Tcl_NewObj();
	index_t dims[2];
	dims[0]=m; dims[1]=n;
	NumArrayInfo *info = CreateNumArrayInfo(2, dims, type);
	NumArraySharedBuffer *sharedbuf = NumArrayNewSharedBuffer(info->bufsize);
	NumArraySetInternalRep(result, sharedbuf, info);
	return result;
}

Tcl_Obj *NumArrayNewMatrixColMaj(NumArrayType type, index_t m, index_t n) {
	Tcl_Obj* result = Tcl_NewObj();
	index_t dims[2];
	dims[0]=m; dims[1]=n;
	NumArrayInfo *info = CreateNumArrayInfoColMaj(2, dims, type);
	NumArraySharedBuffer *sharedbuf = NumArrayNewSharedBuffer(info->bufsize);
	NumArraySetInternalRep(result, sharedbuf, info);
	return result;
}


/* Assemble / retrieve info and data storage into a Tcl_Obj */
void NumArraySetInternalRep(Tcl_Obj *naObj, NumArraySharedBuffer *sharedbuf, NumArrayInfo *info) {
	Tcl_InvalidateStringRep(naObj);
	naObj -> internalRep.twoPtrValue.ptr1 = sharedbuf;
	NumArraySharedBufferIncrRefcount(sharedbuf);
	naObj -> internalRep.twoPtrValue.ptr2 = info;
	naObj -> typePtr = VecTclNumArrayObjType;
}

/* Extract the NumArrayInfo* from a Tcl_Obj
 * If naObj can't be converted to NumArray, 
 * NULL is returned */
NumArrayInfo *NumArrayGetInfoFromObj(Tcl_Interp *interp, Tcl_Obj* naObj) {
	if (Tcl_ConvertToType(interp, naObj, VecTclNumArrayObjType) != TCL_OK) {
		return NULL;
	}
	
	return naObj -> internalRep.twoPtrValue.ptr2;
}

/* Extract the NumArraySharedBuffer* from a Tcl_Obj
 * If naObj can't be converted to NumArray, 
 * NULL is returned */
NumArraySharedBuffer *NumArrayGetSharedBufferFromObj(Tcl_Interp *interp, Tcl_Obj* naObj) {
	if (Tcl_ConvertToType(interp, naObj, VecTclNumArrayObjType) != TCL_OK) {
		return NULL;
	}
	
	return naObj -> internalRep.twoPtrValue.ptr1;
}

/* Extract a pointer to the first element from a Tcl_Obj
 * If naObj can't be converted to NumArray, 
 * NULL is returned */
void *NumArrayGetPtrFromObj(Tcl_Interp *interp, Tcl_Obj* naObj) {
	if (Tcl_ConvertToType(interp, naObj, VecTclNumArrayObjType) != TCL_OK) {
		return NULL;
	}
	NumArraySharedBuffer *sharedbuf = naObj -> internalRep.twoPtrValue.ptr1;
	NumArrayInfo *info = naObj -> internalRep.twoPtrValue.ptr2;
	return sharedbuf->buffer + info->offset;
}

/* Handle copy-on-write */
void NumArrayEnsureContiguous(Tcl_Obj *naObj) {
	/* copy buffer/unshare */
	if (naObj -> typePtr == VecTclNumArrayObjType) {
		NumArrayInfo * info = naObj -> internalRep.twoPtrValue.ptr2;

		if (!info->canonical) { 
			NumArrayUnshareBuffer(naObj);
			/* copying the data always creates the canonical form */
		}
	}
}

void NumArrayEnsureWriteable(Tcl_Obj *naObj) {
	if (naObj -> typePtr == VecTclNumArrayObjType) {
		NumArraySharedBuffer *sharedbuf = naObj -> internalRep.twoPtrValue.ptr1;
		if (NumArrayIsShared(sharedbuf))  {
			NumArrayUnshareBuffer(naObj);
		}
	}
}

void NumArrayUnshareBuffer(Tcl_Obj *naObj) {
	if (naObj -> typePtr == VecTclNumArrayObjType) {
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
		DeleteNumArrayInfo(info);
	}
}


/* Iterator to loop over all elements in an array */
/* Constructor and destructor for iterator objects */
int NumArrayIteratorInitObj(Tcl_Interp* interp, Tcl_Obj* naObj, NumArrayIterator *it) {
	NumArraySharedBuffer *sharedbuf; NumArrayInfo *info;

	if (Tcl_ConvertToType(interp, naObj, VecTclNumArrayObjType) != TCL_OK) {
		return TCL_ERROR;
	}
	
	sharedbuf = naObj -> internalRep.twoPtrValue.ptr1;
	info = naObj -> internalRep.twoPtrValue.ptr2;
	
	NumArrayIteratorInit(info, sharedbuf, it);

	return TCL_OK;
}


void NumArrayIteratorInit(NumArrayInfo *info, NumArraySharedBuffer *sharedbuf, NumArrayIterator *it) {
	int d;
	int nDim = info->nDim;

	it -> finished = 0;
	it -> nDim = nDim;
	it -> type = info -> type;
	it -> dinfo = malloc(sizeof(NumArrayIteratorDimension)*(info->nDim+1));

	it->baseptr = (char *)NumArrayGetPtrFromSharedBuffer(sharedbuf)+info->offset;
	it->ptr = it->baseptr;
	
	/* Check for an empty array */
	if (nDim == 1 && info -> dims[0] == 0) {
		it -> finished = 1;
		it -> dinfo[0].counter = 0;
		it -> dinfo[0].dim = 0;
		it -> dinfo[0].pitch = 0;
	} else {
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
	}
	/* append a singleton dimension for AdvanceRow 
	 * in the vector case */
	it -> dinfo[it->nDim].counter = 1;
	it -> dinfo[it->nDim].dim = 1;
	it -> dinfo[it->nDim].pitch = 0;
	
}

void NumArrayIteratorInitColMaj(NumArrayInfo *info, NumArraySharedBuffer *sharedbuf, NumArrayIterator *it) {
	int d;
	int nDim = info->nDim;

	it -> finished = 0;
	it -> nDim = nDim;
	it -> type = info -> type;
	it -> dinfo = malloc(sizeof(NumArrayIteratorDimension)*(info->nDim+1));

	it->baseptr = (char *)NumArrayGetPtrFromSharedBuffer(sharedbuf)+info->offset;

	/* copy dimensional information into the 
	 * iterators counter in reverse order, 
	 * while stripping singleton dimensions */
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

void NumArrayIteratorFree(NumArrayIterator *it) {
	free(it->dinfo);
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

/* Iterator advance and test for end condition */
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

int NumArrayIteratorFinished(NumArrayIterator *it) {
	return it->finished;
}

/* For faster looping, advance row-wise. */
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

/* Retrieve pitch and
 * number of elements in the innermost loop */
index_t NumArrayIteratorRowLength(NumArrayIterator *it) {
	return it->dinfo[0].dim;
}

index_t NumArrayIteratorRowPitch(NumArrayIterator *it) {
	return it->dinfo[0].pitch;
}

index_t NumArrayIteratorRowPitchTyped(NumArrayIterator *it) {
	return it->dinfo[0].pitch / NumArrayType_SizeOf(it->type);
}


/* Retrieve value from iterator */
/* Pointer */
void *NumArrayIteratorDeRefPtr(NumArrayIterator *it) {
	return it->ptr;
}

/* single dataype */
NaWideInt NumArrayIteratorDeRefInt(NumArrayIterator *it) {
	return *((NaWideInt*) (it->ptr));
}

double NumArrayIteratorDeRefDouble(NumArrayIterator *it) {
	return *((double*) (it->ptr));
}

NumArray_Complex NumArrayIteratorDeRefComplex(NumArrayIterator *it) {
	return *((NumArray_Complex*) (it->ptr));
}

/* polymorph value */
NumArray_ValueType NumArrayIteratorDeRefValue(NumArrayIterator *it) {
	NumArray_ValueType value;
	value.type = it->type;
	switch (value.type) {
		case NumArray_Int:
			value.value.Int = *((NaWideInt*) (it->ptr));
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


/* Analogs to memcpy and memset,
 * When the destination buffer has a type larger than the source buffer/value
 * the value is upcast to the destination. 
 * The return value indicates whether the copy was successfull. */

int NumArrayCopy(NumArrayInfo *srcinfo, NumArraySharedBuffer *srcbuf, 
			NumArrayInfo *destinfo, NumArraySharedBuffer *destbuf) {
	
	NumArrayIterator srcit, destit;
	NumArrayIteratorInit(srcinfo, srcbuf, &srcit);
	NumArrayIteratorInit(destinfo, destbuf, &destit);
	const index_t srcpitch=NumArrayIteratorRowPitchTyped(&srcit);
	const index_t destpitch=NumArrayIteratorRowPitchTyped(&destit);
	const index_t length = NumArrayIteratorRowLength(&srcit);

	#define COPYLOOP(TRES, T) \
		if (destinfo->type == NATYPE_FROM_C(TRES) && srcinfo->type == NATYPE_FROM_C(T)) { \
			TRES *result = NumArrayIteratorDeRefPtr(&destit); \
			T* opptr = NumArrayIteratorDeRefPtr(&srcit); \
			while (result) { \
				index_t i; \
				for (i=0; i<length; i++) { \
					*result = UPCAST(T, TRES, *opptr); \
					opptr+=srcpitch; \
					result+=destpitch; \
				} \
				result = NumArrayIteratorAdvanceRow(&destit); \
				opptr = NumArrayIteratorAdvanceRow(&srcit); \
			} \
		} else 
		
	COPYLOOP(NaWideInt, NaWideInt)
	COPYLOOP(double, NaWideInt)
	COPYLOOP(double, double)
	COPYLOOP(NumArray_Complex, NaWideInt)
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

/* same with a Tcl_Obj */
int NumArrayObjCopy(Tcl_Interp *interp, Tcl_Obj *srcObj, Tcl_Obj *destObj) {
	if (Tcl_ConvertToType(interp, srcObj, VecTclNumArrayObjType) != TCL_OK) {
		return TCL_ERROR;
	}
	if (Tcl_ConvertToType(interp, destObj, VecTclNumArrayObjType) != TCL_OK) {
		return TCL_ERROR;
	}

	NumArraySharedBuffer *srcbuf = srcObj -> internalRep.twoPtrValue.ptr1;
	NumArrayInfo *srcinfo = srcObj -> internalRep.twoPtrValue.ptr2;
	
	NumArraySharedBuffer *destbuf = destObj -> internalRep.twoPtrValue.ptr1;
	NumArrayInfo *destinfo = destObj -> internalRep.twoPtrValue.ptr2;

	if (NumArrayCopy(srcinfo, srcbuf, destinfo, destbuf) != TCL_OK) {
		Tcl_SetResult(interp, "Can't copy data", NULL);
		return TCL_ERROR;
	}

	return TCL_OK;
}

/* Fill a buffer with a constant value */
int NumArraySetValue(NumArrayInfo *destinfo, NumArraySharedBuffer *destbuf, NumArray_ValueType value) {
	if (value.type > destinfo->type) {
		/* don't downcast - signal error */
		return TCL_ERROR;
	}

	NumArrayIterator destit;
	NumArrayIteratorInit(destinfo, destbuf, &destit);
	
	/* copy/conversion code for upcasting */
	
	if (destinfo -> type == NumArray_Int) {
		
		NaWideInt intvalue;
		if (value.type == NumArray_Int) {
			intvalue = value.value.Int;
		} else {
			goto cleanit;
		}

		for (; ! NumArrayIteratorFinished(&destit); 
			NumArrayIteratorAdvance(&destit)) {
				*(NaWideInt *) NumArrayIteratorDeRefPtr(&destit) = intvalue;
		}
	
	} else if (destinfo -> type == NumArray_Float64) {
		double dblvalue;
		if (value.type == NumArray_Int) {
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
		if (value.type == NumArray_Int) {
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

/* Retrieve value from scalar NumArray */
int NumArrayGetScalarValueFromObj(Tcl_Interp *interp, Tcl_Obj* naObj, NumArray_ValueType *value) {
	if (Tcl_ConvertToType(interp, naObj, VecTclNumArrayObjType) != TCL_OK) {
		return TCL_ERROR;
	}

	NumArraySharedBuffer *sharedbuf = naObj->internalRep.twoPtrValue.ptr1;
	NumArrayInfo * info = naObj -> internalRep.twoPtrValue.ptr2;
	
	if (info->nDim == 1 && info->dims[0] == 1) {
		char *bufptr = NumArrayGetPtrFromSharedBuffer(sharedbuf);
		bufptr += info->offset;

		value -> type = info -> type;
		switch (value -> type) {
			case NumArray_Int:
				value->value.Int = *((NaWideInt*) bufptr);
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

/* Index object for simple 1D, 2D and 3D indexing */
void NumArrayIndexInit(NumArrayInfo *info, NumArraySharedBuffer *sharedbuf, NumArrayIndex *ind) {
	int d;

	ind -> pitches[0] = 0;
	ind -> pitches[1] = 0;
	ind -> pitches[2] = 0;

	ind -> baseptr = NumArrayGetPtrFromSharedBuffer(sharedbuf);
	for (d=0; d<info->nDim; d++) {
		ind -> pitches[d] = info -> pitches[d];
	}

	ind -> baseptr += info->offset;
}

int NumArrayIndexInitObj(Tcl_Interp *interp, Tcl_Obj *naObj, NumArrayIndex *ind) {
	NumArraySharedBuffer *sharedbuf; NumArrayInfo *info;

	if (Tcl_ConvertToType(interp, naObj, VecTclNumArrayObjType) != TCL_OK) {
		return TCL_ERROR;
	}
	
	sharedbuf = naObj -> internalRep.twoPtrValue.ptr1;
	info = naObj -> internalRep.twoPtrValue.ptr2;
	
	NumArrayIndexInit(info, sharedbuf, ind);

	return TCL_OK;
}

void *NumArrayIndex1DGetPtr(NumArrayIndex *ind, index_t i) {
	return ind->baseptr + i*ind->pitches[0];
}

void *NumArrayIndex2DGetPtr(NumArrayIndex *ind, index_t i, index_t j) {
	return ind->baseptr + i*ind->pitches[0] + j*ind->pitches[1];
}

void *NumArrayIndex3DGetPtr(NumArrayIndex *ind, index_t i, index_t j, index_t k) {
	return ind->baseptr + i*ind->pitches[0] + j*ind->pitches[1]+ k*ind->pitches[2];
}

