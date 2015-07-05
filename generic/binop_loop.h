#define TUP UPCAST_COMMON(T1, T2)
/* for scalar values, take the first value as 
 * the constant and operate over the second field */
static int BINOP_LOOP_FUN(CMD, T1, T2) (Tcl_Obj *naObj1, Tcl_Obj *naObj2, Tcl_Obj **resultObj) {
	NumArrayInfo *info1 = naObj1->internalRep.twoPtrValue.ptr2;
	NumArrayInfo *info2 = naObj2->internalRep.twoPtrValue.ptr2;
	
	#ifndef OP
	*resultObj = Tcl_ObjPrintf("Operator undefined for types %s, %s", NumArray_typename[info1->type], NumArray_typename[info2->type]);
	return TCL_ERROR;
	#else 
	
	/* check if the operands have compatible dimensions */
	if (!NumArrayCompatibleDimensions(info1, info2) && 
		!ISSCALARINFO(info1)  && !ISSCALARINFO(info2)) {

		*resultObj = Tcl_NewStringObj("incompatible operands", -1);
		return TCL_ERROR;
	}

	/* Create new object  */
	NumArrayType resulttype = NATYPE_FROM_C(TRES);
	NumArrayInfo *resultinfo;
	/* Keep more complex dimension */
	if (ISSCALARINFO(info1)) {
		resultinfo = CreateNumArrayInfo(info2 -> nDim, info2 -> dims, resulttype);
	} else if  (ISSCALARINFO(info2)) {
		resultinfo = CreateNumArrayInfo(info1 -> nDim, info1 -> dims, resulttype);
	} else if (info1->nDim > info2 -> nDim) {
		resultinfo = CreateNumArrayInfo(info1 -> nDim, info1 -> dims, resulttype);
	} else {
		resultinfo = CreateNumArrayInfo(info2 -> nDim, info2 -> dims, resulttype);
	}

	/* allocate buffer of this size */
	NumArraySharedBuffer *sharedbuf = NumArrayNewSharedBuffer(resultinfo -> bufsize);
	char *bufptr = NumArrayGetPtrFromSharedBuffer(sharedbuf);

	/* the new shared buffer is in canonical form, 
	 * thus we can simply iterate over it by pointer
	 * arithmetics. But the input arrays may be non-canonical
	 * TODO optimize for canonical case */

	NumArrayIterator it1, it2;
	NumArrayIteratorInitObj(NULL, naObj1, &it1);
	NumArrayIteratorInitObj(NULL, naObj2, &it2);
	
	const index_t pitch = sizeof(TRES);
	if (ISSCALARINFO(info1)) {
		T1 *op1ptr = NumArrayIteratorDeRefPtr(&it1);
		TUP op1 = UPCAST(T1, TUP, *op1ptr);
		T2 *op2ptr = NumArrayIteratorDeRefPtr(&it2);
		const index_t op2pitch = NumArrayIteratorRowPitchTyped(&it2);
		const index_t length = NumArrayIteratorRowLength(&it2);
		while (op2ptr) {
			index_t i;
			for (i=0; i<length; i++) {
				TRES *result = (TRES *)bufptr;
				bufptr += pitch;
				TUP op2 = UPCAST(T2, TUP, *op2ptr);
				OP;
				op2ptr += op2pitch;
			}
			op2ptr = NumArrayIteratorAdvanceRow(&it2);
		}
	} else if (ISSCALARINFO(info2)) {
		T1 *op1ptr = NumArrayIteratorDeRefPtr(&it1);
		T2 *op2ptr = NumArrayIteratorDeRefPtr(&it2);
		TUP op2 = UPCAST(T2, TUP, *op2ptr);
		const index_t op1pitch = NumArrayIteratorRowPitchTyped(&it1);
		const index_t length = NumArrayIteratorRowLength(&it1);
		while (op1ptr) {
			index_t i;
			for (i=0; i<length; i++) {
				TRES *result = (TRES *)bufptr;
				bufptr += pitch;
				TUP op1 = UPCAST(T1, TUP, *op1ptr);
				OP;
				op1ptr += op1pitch;
			}
			op1ptr = NumArrayIteratorAdvanceRow(&it1);
		}

	} else {

		T1 *op1ptr = NumArrayIteratorDeRefPtr(&it1);
		const index_t op1pitch = NumArrayIteratorRowPitchTyped(&it1);
		T2 *op2ptr = NumArrayIteratorDeRefPtr(&it2);
		const index_t op2pitch = NumArrayIteratorRowPitchTyped(&it2);
		const index_t length = NumArrayIteratorRowLength(&it1);
		
		while (op1ptr) {
			index_t i;
			for (i=0; i<length; i++) {
				TRES *result = (TRES *)bufptr;
				bufptr += pitch;
				TUP op1 = UPCAST(T1, TUP, *op1ptr);
				TUP op2 = UPCAST(T2, TUP, *op2ptr);
				OP;
				op1ptr += op1pitch;
				op2ptr += op2pitch;
			}
			op1ptr = NumArrayIteratorAdvanceRow(&it1);
			op2ptr = NumArrayIteratorAdvanceRow(&it2);
		}
	}	
	NumArrayIteratorFree(&it1);
	NumArrayIteratorFree(&it2);
	
	*resultObj=Tcl_NewObj();
	NumArraySetInternalRep(*resultObj, sharedbuf, resultinfo);
	
	return TCL_OK;
	#endif
}

#undef T1
#undef T2
#undef TRES
#undef TUP
#undef OP
