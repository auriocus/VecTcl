/* to be included with CMD and OP defined
 * it defines an elementwise binary operator
 * which works by iterating over all elements
 * for compatible operands */

#define TCLCMDPROC(X) NUMARRAYTPASTER(X,Cmd)

int CMD(Tcl_Obj* naObj, Tcl_Obj **resultObj);

MODULE_SCOPE
int TCLCMDPROC(CMD) ( 
		ClientData dummy,
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv)
{	
	Tcl_Obj *naObj, *resultObj;
	int resultcode;

	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "numarray");
		return TCL_ERROR;
	}

    naObj = objv[1];

	if (Tcl_ConvertToType(interp, naObj, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}

	resultcode=CMD(naObj, &resultObj);
	
	Tcl_SetObjResult(interp, resultObj);

	return resultcode;
}


int CMD(Tcl_Obj* naObj, Tcl_Obj **resultObj) {

	NumArrayInfo *info, *resultinfo;
	NumArraySharedBuffer *resultbuf;
	
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
			DBLRES *result = (DBLRES*) NumArrayGetPtrFromSharedBuffer(resultbuf);
		
			const index_t srcpitch=NumArrayIteratorRowPitchTyped(&it);
			const index_t length = NumArrayIteratorRowLength(&it);
			double* opptr = NumArrayIteratorDeRefPtr(&it);
			while (opptr) {
				index_t i;
				for (i=0; i<length; i++) {
					double op = *opptr;
					DBLOP;
					opptr+=srcpitch;
					result++;
				}
				opptr = NumArrayIteratorAdvanceRow(&it);
			}
		
			NumArrayIteratorFree(&it);
			break;
		}
		#endif
		
		#ifdef INTOP
		case NumArray_Int: {
			resultinfo = CreateNumArrayInfo(info -> nDim, info -> dims, NATYPE_FROM_C(INTRES));

			/* allocate buffer for result */
			resultbuf = NumArrayNewSharedBuffer(resultinfo -> bufsize);
			INTRES *result = (INTRES*) NumArrayGetPtrFromSharedBuffer(resultbuf);

			const index_t srcpitch=NumArrayIteratorRowPitchTyped(&it);
			const index_t length = NumArrayIteratorRowLength(&it);
			NaWideInt* opptr = NumArrayIteratorDeRefPtr(&it);
			while (opptr) {
				index_t i;
				for (i=0; i<length; i++) {
					NaWideInt op = *opptr;
					INTOP;
					opptr+=srcpitch;
					result++;
				}
				opptr = NumArrayIteratorAdvanceRow(&it);
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
			CPLXRES *result = (CPLXRES*) NumArrayGetPtrFromSharedBuffer(resultbuf);
		
			const index_t srcpitch=NumArrayIteratorRowPitchTyped(&it);
			const index_t length = NumArrayIteratorRowLength(&it);
			NumArray_Complex* opptr = NumArrayIteratorDeRefPtr(&it);
			while (opptr) {
				index_t i;
				for (i=0; i<length; i++) {
					NumArray_Complex op = *opptr;
					CPLXOP;
					opptr+=srcpitch;
					result++;
				}
				opptr = NumArrayIteratorAdvanceRow(&it);
			}
	
			NumArrayIteratorFree(&it);
			break;
		}
		#endif

		default:
			
			*resultObj = Tcl_ObjPrintf("Undefined function for datatype %s", NumArray_typename[info->type]);
            return TCL_ERROR;
	}

	*resultObj=Tcl_NewObj();
	NumArraySetInternalRep(*resultObj, resultbuf, resultinfo);

	return TCL_OK;
}

#undef CMD
#undef TCLCMDPROC

#undef INTOP
#undef INTRES
#undef DBLOP
#undef DBLRES
#undef CPLXOP
#undef CPLXRES
