#include <tcl.h>
#include <vectclInt.h>

int VECTCLJIT_manual_gcc (ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv) {
	/*	Tcl_Obj ** literals; int nLiterals;
		if (Tcl_ListObjGetElements(interp, (Tcl_Obj *)cdata, &nLiterals, &literals) != TCL_OK) {
		return TCL_ERROR; 
		} */
		Tcl_Obj *temp1 = NULL;
		Tcl_Obj *temp2 = NULL;
		Tcl_Obj *temp7 = NULL;
		temp1 = objv[1];
		Tcl_IncrRefCount(temp1);
		temp2 = objv[2];
		Tcl_IncrRefCount(temp2);
		{

			NumArrayInfo *maxinfo = NumArrayGetInfoFromObj(interp, temp1);
			if (maxinfo == NULL) { goto error; }
			NumArrayInfo *resultinfo;
			resultinfo = CreateNumArrayInfo(maxinfo -> nDim, maxinfo -> dims, NATYPE_FROM_C(double));
			
			/* allocate buffer of this size */
			NumArraySharedBuffer *sharedbuf = NumArrayNewSharedBuffer(resultinfo -> bufsize);
			double *temp7ptr = NumArrayGetPtrFromSharedBuffer(sharedbuf);
			NumArrayIterator temp1_it;
			NumArrayIteratorInitObj(interp, temp1, &temp1_it);
			double *temp1ptr = NumArrayIteratorDeRefPtr(&temp1_it);
			const int temp1pitch = NumArrayIteratorRowPitchTyped(&temp1_it);
			NumArrayIterator temp2_it;
			NumArrayIteratorInitObj(interp, temp2, &temp2_it);
			double *temp2ptr = NumArrayIteratorDeRefPtr(&temp2_it);
			const int temp2pitch = NumArrayIteratorRowPitchTyped(&temp2_it);
			const int length = NumArrayIteratorRowLength(&temp1_it);
			while (temp1ptr) {
				int i=length;
				for (; i; i--) {
					double x=(*temp1ptr);
					double y=(*temp2ptr);

					(*temp7ptr) = x*x+y*y; 
				 	++temp7ptr;

					temp1ptr += temp1pitch; 
					temp2ptr += temp2pitch; 
				}
				temp1ptr = NumArrayIteratorAdvanceRow(&temp1_it);
				temp2ptr = NumArrayIteratorAdvanceRow(&temp2_it);
			}
			NumArrayIteratorFree(&temp1_it);
			NumArrayIteratorFree(&temp2_it);
			temp7 = Tcl_NewObj();
			NumArraySetInternalRep(temp7, sharedbuf, resultinfo);
			Tcl_IncrRefCount(temp7);
		}

		Tcl_SetObjResult(interp, temp7);
		if (temp1) Tcl_DecrRefCount(temp1);
		temp1 = NULL;
		if (temp2) Tcl_DecrRefCount(temp2);
		temp2 = NULL;
		if (temp7) Tcl_DecrRefCount(temp7);
		temp7 = NULL;
		return TCL_OK;
		error:
		if (temp1) Tcl_DecrRefCount(temp1);
		temp1 = NULL;
		if (temp2) Tcl_DecrRefCount(temp2);
		temp2 = NULL;
		if (temp7) Tcl_DecrRefCount(temp7);
		temp7 = NULL;
		return TCL_ERROR;
	}
		
int Gccbench_Init(Tcl_Interp* interp) {
	if (interp == 0) return TCL_ERROR;

	if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL) {
		return TCL_ERROR;
	}

	if (Vectcl_InitStubs(interp, "0.1", 0) == NULL) {
		return TCL_ERROR;
	}

	Tcl_CreateObjCommand(interp, "manual_gcc", VECTCLJIT_manual_gcc, NULL, NULL);
	return TCL_OK;
}

	
