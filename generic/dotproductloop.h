/* dot product for input types T1, T2 and output TRES */
/* helper macros to deal with different types */

#define INIT_NaWideInt NaWideInt sum=0;
#define INIT_double double sum=0;
#define INIT_NumArray_Complex NumArray_Complex sum=NumArray_mkComplex(0.0, 0.0);

#define PRODUCT_NaWideInt(X, Y) X*Y
#define PRODUCT_double(X, Y) X*Y
#define PRODUCT_NumArray_Complex(X, Y) NumArray_ComplexMultiply(X, Y)
#define ADDTO_NaWideInt(var, expr) var += expr
#define ADDTO_double(var, expr) var += expr
#define ADDTO_NumArray_Complex(var, expr) var = NumArray_ComplexAdd(var, expr)

#define INIT INIT_help(TRES)
#define INIT_help(TRES) NUMARRAYTPASTER(INIT_, TRES)
#define PRODUCT(X, Y) PRODUCT_help(TRES, X, Y) 
#define PRODUCT_help(TRES, X, Y) NUMARRAYTPASTER(PRODUCT_, TRES)(X, Y)
#define ADDTO(X, Y) ADDTO_help(TRES, X, Y)
#define ADDTO_help(TRES, X, Y) NUMARRAYTPASTER(ADDTO_, TRES)(X, Y)

/* switch on datatypes */

if (info1->type == NATYPE_FROM_C(T1) && info2->type == NATYPE_FROM_C(T2)) {
/* check if the operands have compatible dimensions */
/* could be Kronecker product of two vectors */
if (info1->nDim == 1 && info2->nDim ==2 && info2->dims[0] == 1) {
	index_t resultdims[2];
	resultdims[0] = info1->dims[0];
	resultdims[1] = info2->dims[1];
	resultinfo = CreateNumArrayInfo(2, resultdims, info1->type);
	sharedbuf  = NumArrayNewSharedBuffer(resultinfo->bufsize);

	TRES *bufptr = (TRES *)NumArrayGetPtrFromSharedBuffer(sharedbuf);

	NumArrayIterator it1;
	NumArrayIterator it2;
	NumArrayIteratorInitObj(NULL, naObj1, &it1);
	NumArrayIteratorInitObj(NULL, naObj2, &it2);

	/* outer loop */
	while (!NumArrayIteratorFinished(&it1)) {
		TRES v1 = UPCAST(T1, TRES, *(T1 *) NumArrayIteratorDeRefPtr(&it1));
		void *op2ptr = NumArrayIteratorReset(&it2);
		/* inner loop */
		while (op2ptr) { 
			TRES v2 = UPCAST(T2, TRES, *(T2 *) op2ptr);
			*bufptr++ = PRODUCT(v1, v2);
			op2ptr = NumArrayIteratorAdvance(&it2);
		}
		
		NumArrayIteratorAdvance(&it1);
	}
	
	NumArrayIteratorFree(&it1);
	NumArrayIteratorFree(&it2);
} else if (info1->nDim>1 && info1->dims[info1->nDim-1] == info2->dims[0]) {
	/* standard matrix multiplikation */

	info2 = DupNumArrayInfo(info2);
	const index_t op2pitch = info2->pitches[0];

	int resultndim = info1->nDim + info2->nDim - 2;
	index_t *dims=ckalloc(sizeof(index_t)*resultndim);
	int d;

	for (d=0; d<info1->nDim-1; d++) {	
		dims[d]=info1->dims[d];
	}
	for (d=1; d<info2->nDim; d++) {	
		dims[d+info1->nDim-2]=info2->dims[d];
	}

	resultinfo = CreateNumArrayInfo(resultndim, dims, 
		NumArray_UpcastCommonType(info1->type, info2->type));
	ckfree(dims);

	sharedbuf = NumArrayNewSharedBuffer(resultinfo -> bufsize);
	

	NumArraySharedBuffer *buf1 = naObj1 -> internalRep.twoPtrValue.ptr1;
	NumArraySharedBuffer *buf2 = naObj2 -> internalRep.twoPtrValue.ptr1;

	NumArrayInfoSlice1Axis(NULL, info2, 0, 0, 0, 1);

	NumArrayIterator it1;
	NumArrayIterator it2;
	NumArrayIteratorInit(info1, buf1, &it1);
	NumArrayIteratorInit(info2, buf2, &it2);

	/* Now run nested loop, outer = op1, inner = op2 */
	const index_t op1pitch = NumArrayIteratorRowPitch(&it1);
	TRES *result = (TRES *) NumArrayGetPtrFromSharedBuffer(sharedbuf);
	char *op1ptr = NumArrayIteratorDeRefPtr(&it1);
	
	const index_t length = NumArrayIteratorRowLength(&it1);

	while (op1ptr) {
		char *op2ptr = NumArrayIteratorReset(&it2);
		while (op2ptr) {
			INIT;
			index_t i;
			for (i=0; i<length; i++) {
				TRES v1 = UPCAST(T1, TRES, *(T1 *) op1ptr);
				TRES v2 = UPCAST(T2, TRES, *(T2 *) op2ptr);
				ADDTO(sum, PRODUCT(v1, v2));
				op1ptr += op1pitch;
				op2ptr += op2pitch;
			}
			*result++ = sum;
			op2ptr = NumArrayIteratorAdvance(&it2);
			op1ptr = NumArrayIteratorDeRefPtr(&it1);
		}	
		op1ptr = NumArrayIteratorAdvanceRow(&it1);
	}

	NumArrayIteratorFree(&it1);
	NumArrayIteratorFree(&it2);
	DeleteNumArrayInfo(info2);
} else {
	Tcl_SetResult(interp, "incompatible operands", NULL);
	return TCL_ERROR;
}

} else 
#undef TRES
#undef T1
#undef T2
