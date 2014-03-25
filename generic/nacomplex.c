#include "nacomplex.h"
#include <stdlib.h>
#include <string.h>

/* TODO: These are just stubs to test the different types */

/*
 * Functions hndling the Tcl_ObjType Complex
 */

static void		dupComplexInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr);
static void		freeComplexInternalRep(Tcl_Obj *listPtr);
static int		setComplexFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr);
static void		updateStringOfComplex(Tcl_Obj *listPtr);

const Tcl_ObjType ComplexTclType = {
	"complex",			/* name */\
	freeComplexInternalRep,	/* freeIntRepProc */
	dupComplexInternalRep,	/* dupIntRepProc */
	updateStringOfComplex,	/* updateStringProc */
	setComplexFromAny		/* setFromAnyProc */
};


int Complex_Init(Tcl_Interp *interp) {
	Tcl_RegisterObjType(&ComplexTclType);
	return TCL_OK;
}

/* The complex number is stored in a malloc'ed buffer
 * pointed to by internalRep.otherValuePtr.
 * On a 64-bit platform, it could fit into 
 * the internalRep alltogether. However this requires either a 
 * core change or a cast from double to void* */

static void dupComplexInternalRep(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr) {
	NumArray_Complex * storage = ckalloc(sizeof(NumArray_Complex));
	*storage = *((NumArray_Complex *) srcPtr->internalRep.otherValuePtr);
	copyPtr -> internalRep.otherValuePtr = storage;
}

static void freeComplexInternalRep(Tcl_Obj *objPtr) {
	ckfree(objPtr -> internalRep.otherValuePtr);
}

/* Copy of this, because it is not exported (but uses only public functionality) */
/*
 *----------------------------------------------------------------
 * Macro used by the Tcl core to clean out an object's internal
 * representation. Does not actually reset the rep's bytes. The ANSI C
 * "prototype" for this macro is:
 *
 * MODULE_SCOPE void	TclFreeIntRep(Tcl_Obj *objPtr);
 *----------------------------------------------------------------
 */

#define TclFreeIntRep(objPtr) \
    if ((objPtr)->typePtr != NULL) { \
	if ((objPtr)->typePtr->freeIntRepProc != NULL) { \
	    (objPtr)->typePtr->freeIntRepProc(objPtr); \
	} \
	(objPtr)->typePtr = NULL; \
    }


static int  setComplexFromAny(Tcl_Interp *interp, Tcl_Obj *objPtr) {
	/* Parse complex number out of the string rep */
	NumArray_Complex c;

	/* Maybe this should go into a static const array */
	const Tcl_ObjType * tclDoubleType = Tcl_GetObjType("double");
	const Tcl_ObjType * tclIntType = Tcl_GetObjType("int");
	
	if (objPtr -> typePtr == tclIntType) {
		int value;
		if (Tcl_GetIntFromObj(interp, objPtr, &value) != TCL_OK) {
			return TCL_ERROR;
		}
		c.re = value;
		c.im = 0.0;
	} else if (objPtr -> typePtr == tclDoubleType) {
		double value;
		if (Tcl_GetDoubleFromObj(interp, objPtr, &value) != TCL_OK) {
			return TCL_ERROR;
		}
		c.re = value;
		c.im = 0.0;
	} else {
		/* Cast from string representation */
	
		if (NumArray_ParseComplex(interp, objPtr->bytes, &c) != TCL_OK) {
			return TCL_ERROR;
		}
	}

	TclFreeIntRep(objPtr);
	objPtr -> internalRep.otherValuePtr = ckalloc(sizeof(NumArray_Complex));
	*((NumArray_Complex *)objPtr -> internalRep.otherValuePtr) = c;
	objPtr -> typePtr = &ComplexTclType;

	return TCL_OK;
}

static void updateStringOfComplex(Tcl_Obj *objPtr) {
	char buffer[NUMARRAY_COMPLEX_SPACE];
	NumArray_Complex c = *((NumArray_Complex *) objPtr -> internalRep.otherValuePtr);

	NumArray_PrintComplex(c, buffer);
	objPtr -> length = strlen(buffer);
	objPtr -> bytes = ckalloc(objPtr->length+1);
	memcpy(objPtr -> bytes, buffer, objPtr -> length+1);
}


Tcl_Obj * NumArray_NewComplexObj(NumArray_Complex c) {
	Tcl_Obj *cobj = Tcl_NewObj();
	NumArray_Complex *storage = ckalloc(sizeof(NumArray_Complex));
	*storage = c;
	cobj->internalRep.otherValuePtr = storage;
	cobj->typePtr = &ComplexTclType;
	Tcl_InvalidateStringRep(cobj);
	return cobj;
}

int NumArray_GetComplexFromObj(Tcl_Interp *interp, Tcl_Obj *cplxObj, NumArray_Complex *result) {
	if (Tcl_ConvertToType(interp, cplxObj, &ComplexTclType) != TCL_OK) {
		return TCL_ERROR;
	} else {
		*result = *((NumArray_Complex *)cplxObj -> internalRep.otherValuePtr);
		return TCL_OK;
	}
}

/* .... */


void NumArray_PrintComplex(NumArray_Complex c, char *buf) {
	Tcl_PrintDouble(NULL, c.re, buf);
	/* Skip to end of the first number. Too bad that Tcl_PrintDouble
	 * does not report the number of bytes written */
	buf += strlen(buf);
	if (! (c.im < 0) ) {
		/* for positive and NaN imaginary part, 
		 * add a + sign */
		*buf++ = '+';
	}

	Tcl_PrintDouble(NULL, c.im, buf);
	buf += strlen(buf);
	*buf++ = 'i';
	*buf++ = '\0';
}

int NumArray_ParseComplex(Tcl_Interp *interp, const char *buf, NumArray_Complex *dest) {
	/* have to use strtod because Tcl_ParseNumber is not exported.
	 * Whitespace is not allowed neither leading, trailing, nor in between
	 * the real and imag part. */
	NumArray_Complex result;
	char *nextptr;
	double first, second;

	first=strtod(buf, &nextptr);
	if (nextptr == buf) {
		/* Nothing was parsed - this is'nt a number */
		goto error;
	}

	switch (*nextptr) {
		case '\0':
			/* end of string - promote from real to complex */
			result.re = first;
			result.im = 0.0;
			*dest = result;
			return TCL_OK;
			break;
		case '+':
		case '-': {
			/* imaginary part must come... */
			char *imstart = nextptr; 
			second=strtod(nextptr, &imstart);
			/* now check if this is followed by 'i', 
			 * and thenend of string */
			if (imstart != nextptr 
				&& (*imstart == 'i' || *imstart == 'I')
				&& (*(imstart+1) == '\0')) {
				result.re = first;
				result.im = second;
				*dest = result;
				return TCL_OK;
			}
			break;
		}
		case 'i':
		case 'I': {
			/* pure imaginary value? */
			if (*(nextptr+1) == '\0') {
				result.re = 0.0;
				result.im = first;
				*dest = result;
				return TCL_OK;
			}
		}
	}
	/* if we are here, something is wrong */
error:
	if (interp) Tcl_SetObjResult(interp, Tcl_ObjPrintf("expected complex number but got \"%s\"", buf));
	return TCL_ERROR;
}

#define SQR(x) ((x)*(x))
#define PI 3.14159265358979323846264
/* Implementation of various transcendental functions */
NumArray_Complex NumArray_ComplexSin(NumArray_Complex c) {
	NumArray_Complex result;
	result.re=sin(c.re)*cosh(c.im);
	result.im=cos(c.re)*sinh(c.im);
	return result;
}

NumArray_Complex NumArray_ComplexCos(NumArray_Complex c) {
	NumArray_Complex result;
	result.re=cos(c.re)*cosh(c.im);
	result.im=-sin(c.re)*sinh(c.im);
	return result;
}

;
NumArray_Complex NumArray_ComplexTan(NumArray_Complex c) {
	NumArray_Complex result;
	double denom = SQR(cos(c.re))+SQR(sinh(c.im));
	result.re = sin(c.re)*cos(c.re)/denom;
	result.im = sinh(c.im)*cosh(c.im)/denom;
	return result;
}

NumArray_Complex NumArray_ComplexExp(NumArray_Complex c) {
	NumArray_Complex result;
	double r = exp(c.re);
	result.re = r * cos(c.im);
	result.im = r * sin(c.im);
	return result;
}

NumArray_Complex NumArray_ComplexLog(NumArray_Complex c) {
	NumArray_Complex result;
	result.im = atan2(c.im, c.re);
	result.re = log(hypot(c.re, c.im));
	return result;
}

;
NumArray_Complex NumArray_ComplexSinh(NumArray_Complex c) {
	NumArray_Complex result;
	result.re=sinh(c.re)*cos(c.im);
	result.im=cosh(c.re)*sin(c.im);
	return result;
}

;
NumArray_Complex NumArray_ComplexCosh(NumArray_Complex c) {
	NumArray_Complex result;
	result.re=cosh(c.re)*cos(c.im);
	result.im=sinh(c.re)*sin(c.im);
	return result;
}

;
NumArray_Complex NumArray_ComplexTanh(NumArray_Complex c) {
	NumArray_Complex result;
	
	double denom = cosh(2*c.re)+cos(2*c.im);
	result.re = sinh(2*c.re)/denom;
	result.im = sin(2*c.im)/denom;
	return result;
}

NumArray_Complex NumArray_ComplexAsin(NumArray_Complex c) {
	NumArray_Complex result;
	NumArray_Complex temp;
	/* asin(z)=-i*log(sqrt(1-z²)+i*z) */
	temp = NumArray_ComplexSqrt(NumArray_mkComplex(1.0-c.re*c.re+c.im*c.im,-2*c.re*c.im));
	temp = NumArray_ComplexLog(NumArray_ComplexAdd(temp, NumArray_mkComplex(-c.im, c.re)));
	result.re=temp.im;
	result.im=-temp.re;

	return result;
}

;
NumArray_Complex NumArray_ComplexAcos(NumArray_Complex c) {
	NumArray_Complex result;
	NumArray_Complex asin=NumArray_ComplexAsin(c);
	result.re=PI/2 - asin.re;
	result.im=-asin.im;
	return result;
}


NumArray_Complex NumArray_ComplexAtan(NumArray_Complex c) {
	NumArray_Complex result;
	result.re=0.5*atan2(2*c.re,1-SQR(c.re)-SQR(c.im));
	result.im=0.25*log((SQR(c.re)+SQR(c.im+1))/(SQR(c.re)+SQR(c.im-1)));
	return result;
}


NumArray_Complex NumArray_ComplexAsinh(NumArray_Complex c) {
	NumArray_Complex result;
	NumArray_Complex asin=NumArray_ComplexAsin(NumArray_mkComplex(-c.im, c.re));
	result.re=asin.im;
	result.im=-asin.re;
	return result;
}


NumArray_Complex NumArray_ComplexAcosh(NumArray_Complex c) {
	NumArray_Complex result;
	NumArray_Complex acos=NumArray_ComplexAcos(c);
	result.re=-acos.im;
	result.im=acos.re;
	return result;
}


NumArray_Complex NumArray_ComplexAtanh(NumArray_Complex c) {
	NumArray_Complex result;
	NumArray_Complex atan=NumArray_ComplexAtan(NumArray_mkComplex(-c.im, c.re));
	result.re=atan.im;
	result.im=-atan.re;
	return result;
}



