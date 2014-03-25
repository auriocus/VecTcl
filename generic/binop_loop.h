
/* for scalar values, take the first value as 
 * the constant and operate over the second field */
static int BINOP_LOOP_FUN(CMD, T1, T2) (Tcl_Interp *interp,void *bufptr, NumArrayInfo *info1, NumArrayInfo *info2, NumArrayIterator *it1, NumArrayIterator *it2) {
	#ifndef OP
	RESULTPRINTF(("Operator undefined for types %s, %s", NumArray_typename[info1->type], NumArray_typename[info2->type]));
	return TCL_ERROR;
	#else 
	const int pitch = sizeof(TRES);
	if (ISSCALARINFO(info1)) {
		TRES op1 = UPCAST(T1, TRES, GETOP1);
		T2 *op2ptr = NumArrayIteratorDeRefPtr(it2);
		const int op2pitch = NumArrayIteratorRowPitchTyped(it2);
		const int length = NumArrayIteratorRowLength(it2);
		while (op2ptr) {
			int i;
			for (i=0; i<length; i++) {
				TRES *result = bufptr;
				bufptr += pitch;
				TRES op2 = UPCAST(T2, TRES, *op2ptr);
				OP;
				op2ptr += op2pitch;
			}
			op2ptr = NumArrayIteratorAdvanceRow(it2);
		}
	} else if (ISSCALARINFO(info2)) {
		TRES op2 = UPCAST(T2, TRES, GETOP2);
		T1 *op1ptr = NumArrayIteratorDeRefPtr(it1);
		const int op1pitch = NumArrayIteratorRowPitchTyped(it1);
		const int length = NumArrayIteratorRowLength(it1);
		while (op1ptr) {
			int i;
			for (i=0; i<length; i++) {
				TRES *result = bufptr;
				bufptr += pitch;
				TRES op1 = UPCAST(T1, TRES, *op1ptr);
				OP;
				op1ptr += op1pitch;
			}
			op1ptr = NumArrayIteratorAdvanceRow(it1);
		}

	} else {

		T1 *op1ptr = NumArrayIteratorDeRefPtr(it1);
		const int op1pitch = NumArrayIteratorRowPitchTyped(it1);
		T2 *op2ptr = NumArrayIteratorDeRefPtr(it2);
		const int op2pitch = NumArrayIteratorRowPitchTyped(it2);
		const int length = NumArrayIteratorRowLength(it1);
		
		while (op1ptr) {
			int i;
			for (i=0; i<length; i++) {
				TRES *result = bufptr;
				bufptr += pitch;
				TRES op1 = UPCAST(T1, TRES, *op1ptr);
				TRES op2 = UPCAST(T2, TRES, *op2ptr);
				OP;
				op1ptr += op1pitch;
				op2ptr += op2pitch;
			}
			op1ptr = NumArrayIteratorAdvanceRow(it1);
			op2ptr = NumArrayIteratorAdvanceRow(it2);
		}
	}
	return TCL_OK;
	#endif
}

#undef T1
#undef T2
#undef TRES
#undef OP
