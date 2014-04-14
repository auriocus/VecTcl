#include "fft.h"
#include "hsfft.h"

static int NumArrayFFTSignCmd( Tcl_Interp *interp, int objc, Tcl_Obj *const *objv, int sgn);

int NumArrayFFTCmd(
		ClientData dummy,
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv)
{
	return NumArrayFFTSignCmd(interp, objc, objv, 1); 
	/* forward FFT */
}

int NumArrayIFFTCmd(
		ClientData dummy,
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv)
{
	return NumArrayFFTSignCmd(interp, objc, objv, -1); 
	/* backward FFT */
}

static int NumArrayFFTSignCmd(
		Tcl_Interp *interp,
		int objc,
		Tcl_Obj *const *objv,
		int sgn)
{
	Tcl_Obj *naObj;
	int allocbuf = 0;

	NumArraySharedBuffer *sharedbuf, *inpsharedbuf;
	NumArrayInfo *info, *inpinfo;
	
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "numarray");
		return TCL_ERROR;
	}
	
	naObj = objv[1];
	
	if (Tcl_ConvertToType(interp, naObj, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}
	
	sharedbuf = naObj -> internalRep.twoPtrValue.ptr1;
	info = naObj -> internalRep.twoPtrValue.ptr2;
	/* if input is not complex, copy to new buffer */

	if (info -> type != NumArray_Complex128 || !info->canonical) {
		inpinfo = CreateNumArrayInfo(info -> nDim, info -> dims, NumArray_Complex128);
		inpsharedbuf = NumArrayNewSharedBuffer(inpinfo -> bufsize);
		if (NumArrayCopy(info, sharedbuf, inpinfo, inpsharedbuf)!= TCL_OK) {
			Tcl_SetResult(interp, "Can't convert input to complex", NULL);
			return TCL_ERROR;
		}
		allocbuf = 1;
	} else {
		inpsharedbuf = sharedbuf;
		inpinfo = info;
		allocbuf = 0;
	}

	/* Currently, only unit-stride vectors are supported */
	if (inpinfo -> nDim !=1) {
		Tcl_SetResult(interp, "N-D-FFT not implemented", NULL);
		goto cleanbuf;
	}


	/* reserve memory for output 
	 * for vectors, it is the same as the input */
	NumArrayInfo *resultinfo = DupNumArrayInfo(inpinfo);
	NumArraySharedBuffer *resultsharedbuf = NumArrayNewSharedBuffer(inpinfo->bufsize);
	
	NumArray_Complex *ibuf = NumArrayGetPtrFromSharedBuffer(inpsharedbuf);
	NumArray_Complex *obuf = NumArrayGetPtrFromSharedBuffer(resultsharedbuf);
	/* perform FFT */
	if (inpinfo->dims[0] != 0) {
		/* Do nothing for an empty array */
		fft_object workspace=fft_init(inpinfo->dims[0], sgn);
		fft_exec(workspace, ibuf, obuf);
		
	}

	/* free working copy of input */
	if (allocbuf) {
		NumArraySharedBufferDecrRefcount(inpsharedbuf);
		DeleteNumArrayInfo(inpinfo);
	}

	/* put result into interpreter */

	Tcl_Obj *result = Tcl_NewObj();
	NumArraySetInternalRep(result, resultsharedbuf, resultinfo);
    Tcl_SetObjResult(interp, result);
    return TCL_OK;

cleanbuf:
	if (allocbuf) {
		NumArraySharedBufferDecrRefcount(inpsharedbuf);
		DeleteNumArrayInfo(inpinfo);
	}
	return TCL_ERROR;
}


