static int ASSIGNOP_LOOP_FUN(CMD, TRES, T) (Tcl_Interp *interp, NumArraySharedBuffer *sharedbuf, NumArrayInfo *sliceinfo, Tcl_Obj* value) {

	NumArrayInfo *valueinfo = value -> internalRep.twoPtrValue.ptr2;
	
	#ifndef OP
	RESULTPRINTF(("Assignment operator undefined for types %s, %s", NumArray_typename[sliceinfo->type], NumArray_typename[valueinfo->type]));
	return TCL_ERROR;
	#else 
	
	if (ISSCALARINFO(valueinfo)) {
		/* special case of scalar value. This is assigned
		 * to all elements in the slice.  */
		NumArrayIterator cpyit, srcit;
		NumArrayIteratorInitObj(NULL, value, &srcit); /* can't fail anymore */
		TRES op = GETOP;

		NumArrayIteratorInit(sliceinfo, sharedbuf, &cpyit);
		TRES *result = NumArrayIteratorDeRefPtr(&cpyit);
		const index_t cpypitch=NumArrayIteratorRowPitchTyped(&cpyit);
		const index_t length = NumArrayIteratorRowLength(&cpyit);
		while (result) {
			index_t i;
			for (i=0; i<length; i++) {
				OP
				result+=cpypitch;
			}
			result = NumArrayIteratorAdvanceRow(&cpyit);
		}
		
		NumArrayIteratorFree(&cpyit);
		NumArrayIteratorFree(&srcit);

	} else {

		/* start copying / in-place computing by iterating over both slices */
		NumArrayIterator srcit, cpyit;
		NumArrayIteratorInit(sliceinfo, sharedbuf, &cpyit);
		NumArrayIteratorInitObj(NULL, value, &srcit); /* can't fail anymore */

		const index_t srcpitch=NumArrayIteratorRowPitchTyped(&srcit);
		const index_t cpypitch=NumArrayIteratorRowPitchTyped(&cpyit);
		const index_t length = NumArrayIteratorRowLength(&srcit);
		TRES *result = NumArrayIteratorDeRefPtr(&cpyit);
		T* opptr = NumArrayIteratorDeRefPtr(&srcit);
		while (result) {
			index_t i;
			for (i=0; i<length; i++) {
				TRES op = UPCAST(T, TRES, *opptr);
				OP
				opptr+=srcpitch;
				result+=cpypitch;
			}
			result = NumArrayIteratorAdvanceRow(&cpyit);
			opptr = NumArrayIteratorAdvanceRow(&srcit);
		}
		
		NumArrayIteratorFree(&cpyit);
		NumArrayIteratorFree(&srcit);
	}
	return TCL_OK;
	#endif
}
#undef TRES
#undef T
#undef OP
