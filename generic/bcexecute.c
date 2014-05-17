#include "bcexecute.h"
/*  
 * Execution engine for bytecode */
SUBCOMMAND(NumArrayBCExecuteCmd) {
	if (objc < 4) {
		Tcl_WrongNumArgs(interp, 1, objv, "bytecode reduction numarray ?numarray ...?");
		return TCL_ERROR;
	}

	/* number of operands */
	int noperands = objc-3;
	Tcl_Obj *const *operands = objv+3;

	int bclength; 
	unsigned char *bc;
	bc=Tcl_GetByteArrayFromObj(objv[1], &bclength);
	
	if (bclength <= 0) {
		Tcl_SetResult(interp, "NOP bytecode", NULL);
		return TCL_ERROR;
	}

	/* Reduction must be a list: operator axis */
	int llength;
	if (Tcl_ListObjLength(interp, objv[2], &llength) != TCL_OK) {
		return TCL_ERROR;
	}

	if (llength != 0) {
		Tcl_SetResult(interp, "Reduction not implemented", NULL);
		return TCL_ERROR;
	}

	/* Check that all operands have compatible dimensions */
	NumArrayInfo *baseinfo=NULL;
	NumArrayType resulttype=NumArray_Int;
	int i;
	for (i=0; i<noperands; i++) {
		NumArrayInfo *info=NumArrayGetInfoFromObj(interp, operands[i]);
		if (!info) {
			return TCL_ERROR;
		}

		/* save datatype */
		resulttype = NumArray_UpcastCommonType(resulttype, info->type);

		if (ISSCALARINFO(info)) {
			continue;
			/* Scalars are always compatible */
		}
		
		if (baseinfo == NULL) {
			/* save the first non-scalar info
			 * and use it for subsequent comparisons */
			baseinfo = info;
			continue;
		}
		
		if (!NumArrayCompatibleDimensions(baseinfo, info)) {
			Tcl_SetResult(interp, "Incompatible operands", NULL);
			return TCL_ERROR;
		}

		/* Save the most complex operand shape 
		 * for the result */
		if (info->nDim > baseinfo->nDim) {
			baseinfo = info;
		}
	}

	/* For now, do only doubles */
	if (resulttype != NumArray_Float64) {
		Tcl_SetResult(interp, "Only doubles are implemented", NULL);
		return TCL_ERROR;
	}

	if (bclength%4 != 0) {
		/* A bytecode is CMD OP1 OP2 OP3 
		    to perform, e.g., op1=op2+op3 */
		Tcl_SetResult(interp, "Error in bytecode, length % 4 != 0", NULL);
		return TCL_ERROR;
	}

	/* create result */
	NumArrayInfo *resultinfo=CreateNumArrayInfo(baseinfo->nDim, baseinfo->dims, resulttype);
	NumArraySharedBuffer *resultbuf=NumArrayNewSharedBuffer(resultinfo->bufsize);
	Tcl_Obj *result = Tcl_NewObj();
	NumArraySetInternalRep(result, resultbuf, resultinfo);

	/* Run through the bytecode and find the highest register number */
	int nregs=0;
	int b;
	for (b=0; b<bclength; b+=4) {
		/* leave aside the opcode itself */
		if (bc[b+1]>nregs) nregs=bc[b+1];
		if (bc[b+2]>nregs) nregs=bc[b+2];
		if (bc[b+3]>nregs) nregs=bc[b+3];
	}
	nregs+=1;
	
	/* Maybe the number of operands exceeds the 
	 * addressed registers */

	if (noperands+1>nregs) nregs=noperands+1;

	/* reserve memory for the registers */
	/* For mixed types, this must be an array of NumArray_Value
	 * with corresponding translated bytecode TODO */
	#define REGTYPE double
	REGTYPE *registers = ckalloc(sizeof(REGTYPE)*nregs);

	/* create iterators and pitches */
	NumArrayIterator *opit = ckalloc(sizeof(NumArrayIterator)*noperands);
	int *rpitches = ckalloc(sizeof(int)*noperands);
	char **rowptrs = ckalloc(sizeof(char *)*noperands);
	for (i=0; i<noperands; i++) {
		NumArrayIteratorInitObj(NULL, operands[i], &opit[i]);
		rpitches[i]=NumArrayIteratorRowPitch(&opit[i]);
		rowptrs[i]=NumArrayIteratorDeRefPtr(&opit[i]);
	}
	
	/* Compute total number of elements and inner length */
	const int nelem = (resultinfo->bufsize) / NumArrayType_SizeOf(resulttype);
	int rowlength = 1;
	for (i=0; i<noperands; i++) {
		if (rpitches[i] != 0) {
			rowlength=NumArrayIteratorRowLength(&opit[i]);
			break;
		}		
	}
	

	double *resultptr = NumArrayGetPtrFromSharedBuffer(resultbuf);
	int el=0;
	while (el<nelem) {
		int j;
		for (j=0; j<rowlength; j++) {
			/* load operands */
			for (i=0; i<noperands; i++) {
				registers[i+1]=*((double *) (rowptrs[i]+j*rpitches[i]));
			}

			/* execute bytecode */
			for (b=0; b<bclength; b+=4) {
				int code = bc[b];
				int r1 = bc[b+1];
				int r2 = bc[b+2];
				int r3 = bc[b+3];
				switch (code) {
					case BC_PLUS: 
						registers[r1] = registers[r2]+registers[r3];
						break;
					case BC_MINUS: 
						registers[r1] = registers[r2]-registers[r3];
						break;
					case BC_TIMES: 
						registers[r1] = registers[r2]*registers[r3];
						break;
					case BC_DIVIDE: 
						registers[r1] = registers[r2]/registers[r3];
						break;
					case BC_NEGATE: 
						registers[r1] = -registers[r2];
						break;
					case BC_POW: 
						registers[r1] = pow(registers[r2], registers[r3]);
						break;
					default:
						/* Error */
						Tcl_SetResult(interp, "Error in bytecode, unknown instruction", NULL);
						goto clean;
				}
			}
			/* store result */
			*resultptr++=registers[0];
			el++;
		}
		/* Advance non-scalar iterators */
		for (i=0; i<noperands; i++) {
			if (rpitches[i]!=0) {
				rowptrs[i] = NumArrayIteratorAdvanceRow(&opit[i]);
			}
		}

	}


	ckfree(rowptrs);
	ckfree(rpitches);
	ckfree(opit);
	ckfree(registers);

	Tcl_SetObjResult(interp, result);
	return TCL_OK;
clean:
	ckfree(rowptrs);
	ckfree(rpitches);
	ckfree(opit);
	ckfree(registers);
	Tcl_DecrRefCount(result);
	return TCL_ERROR;
}

