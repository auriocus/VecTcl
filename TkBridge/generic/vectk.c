/* Bridge between Tk photo and numarray
 */
#include <vectcl.h>
#include <tcl.h>
#include <tk.h>
#define MAX(x, y) ((x)>(y)?x:y)
#define MIN(x, y) ((x)<(y)?x:y)

typedef unsigned char sample;
const sample maxval=0xFF;

inline sample
real2sample(const double value) {
	if (value >=(1.0-0.5/maxval)) return maxval;
	if (value <=0.0) return (sample)(0);
    return (sample)(maxval*value+0.5);
}

inline double sample2real(const sample v) {
	return ((double)(v))/((double)maxval);
}

int Photo2NumArrayCmd(ClientData dummy, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv) {
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "photo");
		return TCL_ERROR;
	}

	Tk_PhotoHandle source = Tk_FindPhoto (interp, Tcl_GetString(objv[1]));
	Tk_PhotoImageBlock sourceblock;
	Tk_PhotoGetImage(source, &sourceblock);
	int width=sourceblock.width;
	int height=sourceblock.height;
	int pitch=sourceblock.pitch;
	int pixelSize=sourceblock.pixelSize;
	int depth = 4; /* strange sourceblock.depth; */
	

	if ((depth != 1) && (depth != 3) && (depth != 4)) {
		Tcl_SetResult(interp, "Grayscale, RGB or RGBA photo expected. WTF is this?", NULL);
		return TCL_ERROR;
	}

	Tcl_Obj *matrix;

	if (depth == 1) {
		/* Grayscale. Alloc 2D object */
		matrix=NumArrayNewMatrix(NumArray_Float64, height, width);
		double *mPtr=NumArrayGetPtrFromObj(interp, matrix);
		/* copy the data */
		index_t offs = sourceblock.offset[0];
		sample *sPtr = sourceblock.pixelPtr + offs;
		index_t i,j;
		for (i=0; i<height; i++) {
			for (j=0; j<width; j++) {
				*mPtr++ = sample2real(sPtr[i*pitch+j*pixelSize]);
			}
		}

	} else {
		/* Color. Alloc 3D object height x width x depth */
		index_t dims[3];
		dims[0]=height;
		dims[1]=width;
		dims[2]=depth;
		
		matrix = Tcl_NewObj();
		NumArrayInfo *info = CreateNumArrayInfo(3, dims, NumArray_Float64);
		NumArraySharedBuffer *sharedbuf = NumArrayNewSharedBuffer(info->bufsize);
		NumArraySetInternalRep(matrix, sharedbuf, info);

		double *mPtr = NumArrayGetPtrFromObj(interp, matrix);
		/* copy the data */
		sample *sPtr = sourceblock.pixelPtr;
		int i,j,d;
		for (i=0; i<height; i++) {
			for (j=0; j<width; j++) {
				for (d=0; d<depth; d++) {
					*mPtr++ = sample2real(sPtr[i*pitch+j*pixelSize+sourceblock.offset[d]]);
				}
			}
		}
	}

	
	Tcl_SetObjResult(interp, matrix);
	return TCL_OK;
	
}

int NumArray2PhotoCmd(ClientData dummy, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv) {
	if (objc != 3) {
		Tcl_WrongNumArgs(interp, 1, objv, "matrix photo");
		return TCL_ERROR;
	}
	
	Tcl_Obj *matrix = objv[1];

	/* Convert the 1st argument to VecTcl object */
	NumArrayInfo *info = NumArrayGetInfoFromObj(interp, matrix);
	if (!info) { return TCL_ERROR; }

	/* Check that it is a matrix */
	if (info->nDim != 3 && info->nDim != 2) {
		Tcl_SetResult(interp, "2D (grayscale) or 3D (color) matrix expected", NULL);
		return TCL_ERROR;
	}

	if (info->type != NumArray_Float64) {
		Tcl_SetResult(interp, "floating point data expected", NULL);
		return TCL_ERROR;
	}
		
	index_t height = info->dims[0];
	index_t width = info->dims[1];
	int depth = 1; /* grayscale */
	
	index_t hpitch=info->pitches[0] / sizeof(double);
	index_t wpitch=info->pitches[1] / sizeof(double);
	index_t dpitch=0;

	if (info->nDim == 3) {
		depth = info->dims[2];
		dpitch = info->pitches[2] / sizeof(double);
		if (depth != 3 && depth != 4) {
			Tcl_SetResult(interp, "3 (RBG) or 4 (RGBA) color planes expected", NULL);
			return TCL_ERROR;
		}
	}

	/* now get a handle to the target photo image */
	
	Tk_PhotoHandle target = Tk_FindPhoto (interp, Tcl_GetString(objv[2]));
	if (target==NULL) {
		Tcl_SetResult(interp, "Cannot find image", NULL);
		return TCL_ERROR;
	}
	
	Tk_PhotoImageBlock outputline;
	outputline.width=width;
	outputline.height=height;
	outputline.pitch=width*depth;
	outputline.pixelSize=depth;

	outputline.pixelPtr=ckalloc(width*height*depth);
	
	double *mPtr=NumArrayGetPtrFromObj(interp, matrix);

	switch (depth) {
		case 1: {
			/* grayscale */
			index_t i,j,d;
			for (d=0; d<4; d++) {
				outputline.offset[d]=0;
			}
			
			sample *outptr = outputline.pixelPtr;
			/* copy loop over all pixels */
			for (i=0; i<height; i++) {
				for (j=0; j<width; j++) {
					*outptr++ = real2sample(mPtr[i*hpitch+j]);
				}
			}
			break;
		}

		case 3:
		case 4: {
			/* color */
			index_t i,j,d;
			for (d=0; d<4; d++) {
				outputline.offset[d]=d;
			}

			if (depth == 3) {
				/* no alpha */
				outputline.offset[3]=0;
			}

			sample *outptr = outputline.pixelPtr;
			/* copy loop over all pixels */
			for (i=0; i<height; i++) {
				for (j=0; j<width; j++) {
					for (d=0; d<depth; d++) {
						*outptr++ = real2sample(mPtr[i*hpitch+j*wpitch+d]);
					}
				}
			}
			break;
		}
	}

	/* Set the output photo to the size of the matrix */
	Tk_PhotoSetSize(interp, target, width, height);
	Tk_PhotoPutBlock(interp, target, &outputline,0,0,width,height,TK_PHOTO_COMPOSITE_SET);
	
	ckfree(outputline.pixelPtr);
	return TCL_OK;
}

int Vectcltk_Init(Tcl_Interp *interp) {
	if (interp == 0) return TCL_ERROR;

	#ifdef USE_TCL_STUBS
	if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL) {
		return TCL_ERROR;
	}
	#endif
	
	#ifdef USE_TK_STUBS
	if (Tk_InitStubs(interp, TK_VERSION, 0) == NULL) {
		return TCL_ERROR;
	}
	#endif  

	if (Tcl_PkgRequire(interp, "vectcl", "0.1", 0) == NULL) {
		return TCL_ERROR;
	}
	
	if (Vectcl_InitStubs(interp, "0.1", 0) == NULL) {
		return TCL_ERROR;
	}

	Tcl_CreateObjCommand(interp, "numarray::toPhoto", NumArray2PhotoCmd, NULL, NULL);
	Tcl_CreateObjCommand(interp, "numarray::fromPhoto", Photo2NumArrayCmd, NULL, NULL);

	Tcl_PkgProvide(interp, "vectcl::tk", PACKAGE_VERSION);

	return TCL_OK;
}
