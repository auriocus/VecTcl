/* to be included with CMD and OP defined
 * it defines an elementwise binary operator
 * which works by iterating over all elements
 * for compatible operands */
int CMD( 
		ClientData dummy,
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv)
{	
	Tcl_Obj *naObj, *resultObj;
	NumArrayInfo *info, *resultinfo;
	NumArraySharedBuffer *resultbuf;

	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "numarray");
		return TCL_ERROR;
	}
	
    naObj = objv[1];
	
	if (Tcl_ConvertToType(interp, naObj, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}

	info = naObj->internalRep.twoPtrValue.ptr2;
	
	NumArrayIterator it;
	NumArrayIteratorInitObj(NULL, naObj, &it);
	/* the new shared buffer is in canonical form, 
	 * thus we can simply iterate over it by pointer
	 * arithmetics. But the input array may be non-canonical
	 * TODO optimize for canonical case */
	switch (info -> type) {
		#ifdef DBLOP
		case NumArray_Float64: {
			resultinfo = CreateNumArrayInfo(info -> nDim, info -> dims, NATYPE_FROM_C(DBLRES));

			/* allocate buffer for result */
			resultbuf = NumArrayNewSharedBuffer(resultinfo -> bufsize);
			DBLRES *bufptr = (DBLRES*) NumArrayGetPtrFromSharedBuffer(resultbuf);

			for (; ! NumArrayIteratorFinished(&it); 
				NumArrayIteratorAdvance(&it)) {

				DBLRES *result = bufptr++;
				double op = NumArrayIteratorDeRefDouble(&it);
				DBLOP;
			}
			NumArrayIteratorFree(&it);
			break;
		}
		#endif
		
		#ifdef INTOP
		case NumArray_Int64: {
			resultinfo = CreateNumArrayInfo(info -> nDim, info -> dims, NATYPE_FROM_C(INTRES));

			/* allocate buffer for result */
			resultbuf = NumArrayNewSharedBuffer(resultinfo -> bufsize);
			INTRES *bufptr = (INTRES*) NumArrayGetPtrFromSharedBuffer(resultbuf);

			for (; ! NumArrayIteratorFinished(&it); 
				NumArrayIteratorAdvance(&it)) {

				INTRES *result = bufptr++;
				int op = NumArrayIteratorDeRefInt(&it);
				INTOP;
			}
			NumArrayIteratorFree(&it);
			break;
		}
		#endif

		#ifdef CPLXOP
		case NumArray_Complex128: {
			resultinfo = CreateNumArrayInfo(info -> nDim, info -> dims, NATYPE_FROM_C(CPLXRES));

			/* allocate buffer for result */
			resultbuf = NumArrayNewSharedBuffer(resultinfo -> bufsize);
			CPLXRES *bufptr = (CPLXRES*) NumArrayGetPtrFromSharedBuffer(resultbuf);

			for (; ! NumArrayIteratorFinished(&it); 
				NumArrayIteratorAdvance(&it)) {

				CPLXRES *result = bufptr++;
				NumArray_Complex op = NumArrayIteratorDeRefComplex(&it);
				CPLXOP;
			}
			NumArrayIteratorFree(&it);
			break;
		}
		#endif

		default:
			
			RESULTPRINTF(("Undefined function for datatype %s", NumArray_typename[info->type]));
            return TCL_ERROR;
	}

	resultObj=Tcl_NewObj();
	NumArraySetInternalRep(resultObj, resultbuf, resultinfo);
	Tcl_SetObjResult(interp, resultObj);

	return TCL_OK;
}

#undef CMD

#undef INTOP
#undef INTRES
#undef DBLOP
#undef DBLRES
#undef CPLXOP
#undef CPLXRES
