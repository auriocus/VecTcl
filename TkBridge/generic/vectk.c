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
		int offs = sourceblock.offset[0];
		sample *sPtr = sourceblock.pixelPtr + offs;
		int i,j;
		for (i=0; i<height; i++) {
			for (j=0; j<width; j++) {
				*mPtr++ = sample2real(sPtr[i*pitch+j*pixelSize]);
			}
		}

	} else {
		/* Color. Alloc 3D object height x width x depth */
		int dims[3];
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
		
	long int height = info->dims[0];
	long int width = info->dims[1];
	int depth = 1; /* grayscale */
	
	int hpitch=info->pitches[0] / sizeof(double);
	int wpitch=info->pitches[1] / sizeof(double);
	int dpitch=0;

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
			int i,j,d;
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
			int i,j,d;
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

int Vectk_Init(Tcl_Interp *interp) {
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

	Tcl_PkgProvide(interp, PACKAGE_NAME, PACKAGE_VERSION);

	return TCL_OK;
}

#if 0
#include "photoresize.hpp"
#include <iostream>

using namespace std;

const double EPSILON = 1e-10;

const double radius_gauss=1.1;

static double 
filter_gauss(double x)
{
    return exp(-2.5*x*x); 
}


double sinc(double x) {
  if (abs(x)<EPSILON) {
   return 1;
  } else {
   return sin(M_PI*x)/(M_PI*x);
  }
}

const double radius_lanczos=3.0;

double filter_lanczos (double x) {
  if (abs(x)>radius_lanczos) { return 0; 
   cout<<"hit"<<endl;
   }
  return sinc(x)*sinc(x/radius_lanczos);
}

class resample1d {
  vector<float> weights;
  vector<int> windowleft;
  int sourcelength, targetlength;
  int leftborder, rightborder;
  float filterradius;
  int taps;
public:
  resample1d(int sourcelength_, int targetlength_);
  ~resample1d() { }
  void print_weights();
  template <typename P> void operator () (const P* const sourcearray, P* targetarray, int sskip, int tskip);
  template <typename P> void operator () (const P* const sourcearray, P* targetarray);
};

resample1d::resample1d(int sourcelength_, int targetlength_) :sourcelength(sourcelength_), targetlength(targetlength_) {
  double scale = double (targetlength-1)/(sourcelength-1);
  double fscale;
  if (scale <1.0) {
    filterradius=radius_gauss/scale;
    fscale=scale;
  } else {
    filterradius=radius_gauss;
    fscale=1.0;
  } 
  taps=int(floor(2*filterradius + EPSILON));

  windowleft.reserve(targetlength);
  weights.reserve(taps*targetlength);

  cout << "# Radius "<<filterradius<<" Taps "<<taps<<endl;

  leftborder=0;
  rightborder=targetlength;

  for (int pos=0; pos<targetlength; pos++) {
    // compute weights
    double windowcenter=pos/scale;
    int windowstart=int(ceil(windowcenter-filterradius-EPSILON));
    windowleft[pos]=windowstart;

    for (int wpos=0; wpos<taps; wpos++) {
      weights[pos*taps+wpos]=filter_gauss((windowstart+wpos-windowcenter)*fscale);
    }

    // normalize
    double norm =0;
    for (int wpos=0; wpos<taps; wpos++) {
      int sourcepos=wpos+windowstart;
      if (sourcepos>=0 && sourcepos<sourcelength) {
        norm+=weights[pos*taps+wpos];
      } else if (sourcepos<0) {
        weights[pos*taps+wpos]=0;
        leftborder=pos+1;
      } else if (sourcepos>=sourcelength) {
        weights[pos*taps+wpos]=0;
        if (rightborder==targetlength) rightborder=pos-1;
      }	
    }

    if (abs(norm)>EPSILON) {
      for (int wpos=0; wpos<taps; wpos++) {
        weights[pos*taps+wpos]/=abs(norm);
      }
    }
  }

  if (rightborder<leftborder) {
    leftborder=targetlength;
    rightborder=targetlength;
  }  

}

void resample1d::print_weights() {
  cout<<"# Leftborder "<<leftborder<<"Rightborder "<<rightborder<<endl;
  for (int pos=0; pos<targetlength; pos++) {
    cout<<"# Position "<<pos<<endl;
    for (int wpos=0; wpos<taps; wpos++) {
      cout<<weights[taps*pos+wpos]<<"\t";
    }
    cout<<endl;
    for (int wpos=0; wpos<taps; wpos++) {
      // cout<<wpos+windowleft[pos]<<"\t";
    }
    //cout<<endl;
  }  
}

// Accumulator for standard values
template <typename P>
class accum {
  P sum;
public:
  accum() { sum=0; }
  void add (float weight, P value) {
    sum+=weight*value;
  }
  void store (P &where) {
    // typecast operator
    where=sum;
  }  
};



template <typename P> 
void resample1d::operator () (const P* const sourcearray, P* targetarray, int sskip, int tskip) {
  
  for (int pos=0; pos<leftborder; pos++) {
    register accum<P> sum;
    int left=windowleft[pos];
    int right=min(taps, sourcelength-left);
    for (int wpos=-left; wpos<right; wpos++) {
      sum.add(weights[taps*pos+wpos],sourcearray[(wpos+left)*sskip]);
    }
    sum.store(*targetarray);
    targetarray+=tskip;
  }
  

  for (int pos=leftborder; pos<rightborder; pos++) {
    register accum<P> sum;
    int left=windowleft[pos];
    for (int wpos=0; wpos<taps; wpos++) {
      sum.add(weights[taps*pos+wpos],sourcearray[(wpos+left)*sskip]);
    }
    sum.store(*targetarray);
    targetarray+=tskip;
  }


  for (int pos=rightborder; pos<targetlength; pos++) {
    register accum<P> sum;
    int left=windowleft[pos];
    for (int wpos=0; wpos<sourcelength-left; wpos++) {
      sum.add(weights[taps*pos+wpos],sourcearray[(wpos+left)*sskip]);
    }
    sum.store(*targetarray);
    targetarray+=tskip;
  }

}

template <typename P> 
void resample1d::operator () (const P* const sourcearray, P* targetarray) {
  
  for (int pos=0; pos<leftborder; pos++) {
    register accum<P> sum;
    int left=windowleft[pos];
    int right=min(taps, sourcelength-left);
    for (int wpos=-left; wpos<right; wpos++) {
      sum.add(weights[taps*pos+wpos],sourcearray[wpos+left]);
    }
    sum.store(*targetarray++);
  }
  

  for (int pos=leftborder; pos<rightborder; pos++) {
    register accum<P> sum;
    int left=windowleft[pos];
    for (int wpos=0; wpos<taps; wpos++) {
      sum.add(weights[taps*pos+wpos],sourcearray[wpos+left]);
    }
    sum.store(*targetarray++);
  }


  for (int pos=rightborder; pos<targetlength; pos++) {
    register accum<P> sum;
    int left=windowleft[pos];
    for (int wpos=0; wpos<sourcelength-left; wpos++) {
      sum.add(weights[taps*pos+wpos],sourcearray[wpos+left]);
    }
    sum.store(*targetarray++);
  }

}






typedef unsigned char sample;
const sample maxval=0xFF;
const int depth=4;

struct  tuple {
  sample col[3];
  sample alpha;
};



// Accumulator for tuples
template <>
class accum<tuple> {
  float colsum[3];
  float alphasum;
public:
  accum() : alphasum(0) { for (int c=0; c<3; c++) colsum[c]=0; }

  void add (float weight, const tuple &pxl) {
    alphasum+=pxl.alpha*weight;
    for (int i=0; i<3; i++)
      colsum[i]+=pxl.col[i]*pxl.alpha*weight;
  }

  void store (tuple &where) {
    // typecast operator
    // compute the color value weighted by alpha

    if (alphasum < EPSILON) {
      for (int c=0; c<3; c++) {
       where.col[c]=floatToSample(colsum[c]);
      }
      where.alpha=floatToSample(alphasum);
    } else { 
      for (int c=0; c<3; c++) {
        // all colors except alpha
        where.col[c]=floatToSample(colsum[c]/alphasum);
      }  
      where.alpha=floatToSample(alphasum);
    }
  
  }  
};


// Tk interface function for resampling
string resizephoto(Tcl_Interp *interp, 
                 Tk_PhotoHandle sourceh, 
		 Tk_PhotoHandle targeth, 
		 int xsize, int ysize)  {
/* Copy photo source to target, resample to xsize*ysize
   with standard filter options */
  
   // Get adress of the binary data
   Tk_PhotoImageBlock sourceblock;
   Tk_PhotoImageBlock outputline;
   Tk_PhotoGetImage(sourceh, &sourceblock);
   int source_xsize=sourceblock.width;
   int source_ysize=sourceblock.height;
   int source_pitch=sourceblock.pitch;
   int source_pixelSize=sourceblock.pixelSize;

   outputline.width=xsize;
   outputline.height=ysize;
   outputline.pitch=xsize*depth;
   outputline.pixelSize=depth;
   for (int i=0; i<depth;i++)
     outputline.offset[i]=i;
   
   vector <tuple> pixelPtr(xsize*ysize);
   outputline.pixelPtr=reinterpret_cast<sample*> (&(pixelPtr[0]));
   
   // Set the output photo to the requested size
   Tk_PhotoSetSize(interp, targeth, xsize, ysize);
   
   cout<<"Resampling from ("<<source_xsize<<", "<<source_ysize<<") to ("<<xsize<<", "<<ysize<<") "<<endl;

   // Allocate space for the intermediate interpolated image
   // First resample along x-direction
   // Size = new_x*old_y
   vector <tuple> yresized(xsize*source_ysize);
   // Setup resampling filter
   resample1d xsampler(source_xsize, xsize);
   for (int y=0; y<source_ysize; y++) {
     // resample every line
     tuple *sourceline=reinterpret_cast<tuple*> (sourceblock.pixelPtr + y*source_pitch);
     tuple *targetline=&(yresized[xsize*y]);
     xsampler(sourceline, targetline);
   }

   // Now resample along y-direction
   // Setup resampling filter
   resample1d ysampler(source_ysize, ysize); 
   for (int x=0; x<xsize; x++) {
     //resample every row
     tuple *sourcerow=&(yresized[x]);
     tuple *targetrow=&(pixelPtr[x]);
     ysampler(sourcerow, targetrow, xsize, xsize);
   }

   Tk_PhotoPutBlock(interp, targeth, &outputline,0,0,xsize,ysize,TK_PHOTO_COMPOSITE_SET);

   return ("All OK");
}


/****************************/
/****************************/
/**** end of resampling *****/
/****************************/
/****************************/


#endif
