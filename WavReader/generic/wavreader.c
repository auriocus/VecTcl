/* Read a WAVE soundfile into numarray
 */
#include <vectcl.h>
#include <tcl.h>
#include <stdint.h>
#include <stdio.h>
#define MAX(x, y) ((x)>(y)?x:y)
#define MIN(x, y) ((x)<(y)?x:y)

enum WavChunks {
    RiffHeader = 0x46464952,
    WavRiff = 0x54651475,
    Format = 0x020746d66,
    LabeledText = 0x478747C6,
    Instrumentation = 0x478747C6,
    Sample = 0x6C706D73,
    Fact = 0x47361666,
    Data = 0x61746164,
    Junk = 0x4b4e554a,
};

enum WavFormat {
    PulseCodeModulation = 0x01,
    IEEEFloatingPoint = 0x03,
    ALaw = 0x06,
    MuLaw = 0x07,
    IMAADPCM = 0x11,
    YamahaITUG723ADPCM = 0x16,
    GSM610 = 0x31,
    ITUG721ADPCM = 0x40,
    MPEG = 0x50,
    Extensible = 0xFFFE
};

#if 0
int32 chunkid = 0;
bool datachunk = false;
while ( !datachunk ) {
    chunkid = reader.ReadInt32( );
    switch ( (WavChunks)chunkid ) {
    case WavChunks::Format:
        formatsize = reader.ReadInt32( );
        format = (WavFormat)reader.ReadInt16( );
        channels = (Channels)reader.ReadInt16( );
        channelcount = (int)channels;
        samplerate = reader.ReadInt32( );
        bitspersecond = reader.ReadInt32( );
        formatblockalign = reader.ReadInt16( );
        bitdepth = reader.ReadInt16( );
        if ( formatsize == 18 ) {
            int32 extradata = reader.ReadInt16( );
            reader.Seek( extradata, SeekOrigin::Current );
        }
        break;
    case WavChunks::RiffHeader:
        headerid = chunkid;
        memsize = reader.ReadInt32( );
        riffstyle = reader.ReadInt32( );
        break;
    case WavChunks::Data:
        datachunk = true;
        datasize = reader.ReadInt32( );
        break;
    default:
        int32 skipsize = reader.ReadInt32( );
        reader.Seek( skipsize, SeekOrigin::Current );
        break;
    }
}
#endif

#define STHROW(X) { Tcl_SetResult(interp, X, TCL_VOLATILE); return TCL_ERROR; }
#define READSCALAR(X)  \
		if (fread(&X, sizeof(X),1,wfile) < 1) {\
			STHROW("Unexpected EOF");\
		}

/* for doing blocked read of a large file */
#define BLOCKSIZE 1048576 /* 1 MSample */

int ReadWavCmd(ClientData dummy, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv) {
	if (objc != 2) {
		Tcl_WrongNumArgs(interp, 1, objv, "<wavefile>");
		return TCL_ERROR;
	}
	
	/* open the file */
	FILE* wfile = fopen(Tcl_GetString(objv[1]), "rb");
	if (wfile == NULL) {
		STHROW("Couldn't open file");
	}
	
	int32_t chunkid = 0;
	int datachunk = 0;
	int32_t formatsize;
	int16_t format;
	int16_t channels;
	int32_t samplerate;
	int32_t bitspersecond;
	int16_t formatblockalign;
	int16_t bitdepth;
	int32_t memsize;
	int32_t riffstyle;
	int32_t datasize;

	while ( !datachunk ) {
		READSCALAR(chunkid);

		switch ( chunkid ) {
		case Format:
			READSCALAR(formatsize);
			READSCALAR(format);
			READSCALAR(channels);
			READSCALAR(samplerate);
			READSCALAR(bitspersecond);
			READSCALAR(formatblockalign);
			READSCALAR(bitdepth);
			
			if ( formatsize == 18 ) {
				int16_t extradata;
				READSCALAR(extradata);
				fseek(wfile, extradata, SEEK_CUR);
			}
			break;
		case RiffHeader:
			READSCALAR(memsize);
			READSCALAR(riffstyle);
			break;
		case Data:
			datachunk = 1;
			READSCALAR(datasize);
			break;
		default: {
			int32_t skipsize; READSCALAR(skipsize);
			fseek(wfile, skipsize, SEEK_CUR);
			break;
		}
		}
	}
	
	if (format != PulseCodeModulation) {
		fclose(wfile);
		STHROW("Can only handle PCM data");
	}

	size_t samplesize = bitdepth/8*channels;
	size_t buffersize = BLOCKSIZE*samplesize;
	int16_t *samples=ckalloc(buffersize);
	size_t nsamples = datasize/samplesize;
	double smaxval = 1<<(bitdepth-1);
	
	/* Alloc double matrix */
	Tcl_Obj *matrix;
	if (channels==1 ) {
		matrix = NumArrayNewVector(NumArray_Float64, nsamples);
	} else {
		matrix = NumArrayNewMatrix(NumArray_Float64, nsamples, channels);
	}
	double *mPtr=NumArrayGetPtrFromObj(interp, matrix);
	
	ssize_t samplesleft;
	for (samplesleft=nsamples; samplesleft > 0; ) {
		ssize_t expected_samples = BLOCKSIZE;
		if (expected_samples > samplesleft) {
			expected_samples = samplesleft;
		}

		ssize_t samplesread = fread(samples, samplesize, expected_samples, wfile);
		
		if (samplesread < expected_samples) {
			/* file was truncated */
			Tcl_SetObjResult(interp, Tcl_ObjPrintf("channels=%d, bitdepth=%d,datasize=%d,samplerate=%d,expected_samples=%d samplesread=%d",
		channels, bitdepth, datasize, samplerate, expected_samples, samplesread));
			

			/*STHROW("Unexpected EOF reading the data"); */
			return TCL_ERROR;
		}
		
		samplesleft -= samplesread;

		size_t i;
		for (i=0; i<samplesread*channels; i++) {
			*mPtr++ = ((double)(samples[i]))/smaxval;
		}

	}

	fclose(wfile);
	ckfree(samples);
	/* build up dictionary with metadata */
	Tcl_Obj *result = Tcl_NewObj();
	
#define RESULTDICT(X) \
	if (Tcl_DictObjPut(interp, result, Tcl_NewStringObj(#X, -1), Tcl_NewIntObj(X)) != TCL_OK) { \
		return TCL_ERROR; \
	}

	RESULTDICT(samplerate);
	RESULTDICT(channels);
	RESULTDICT(bitdepth);
	RESULTDICT(datasize);
	RESULTDICT(nsamples);

	Tcl_DictObjPut(interp, result,  Tcl_NewStringObj("data", -1), matrix);

	Tcl_SetObjResult(interp, result);
	/*Tcl_SetObjResult(interp, Tcl_ObjPrintf("channels=%d, bitdepth=%d,datasize=%d,samplerate=%d,bitspersecond=%d",
		channels, bitdepth, datasize, samplerate, bitspersecond)); */
	return TCL_OK;
	
}

int Wavreader_Init(Tcl_Interp *interp) {
	if (interp == 0) return TCL_ERROR;

	#ifdef USE_TCL_STUBS
	if (Tcl_InitStubs(interp, TCL_VERSION, 0) == NULL) {
		return TCL_ERROR;
	}
	#endif
	
	if (Tcl_PkgRequire(interp, "vectcl", "0.1", 0) == NULL) {
		return TCL_ERROR;
	}
	
	if (Vectcl_InitStubs(interp, "0.1", 0) == NULL) {
		return TCL_ERROR;
	}

	if (Tcl_Eval(interp, "namespace eval wavreader {}") != TCL_OK) {
		return TCL_ERROR;
	}

	Tcl_CreateObjCommand(interp, "wavreader::readwav", ReadWavCmd, NULL, NULL);

	Tcl_PkgProvide(interp, PACKAGE_NAME, PACKAGE_VERSION);

	return TCL_OK;
}
