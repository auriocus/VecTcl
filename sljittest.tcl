lappend auto_path ~/Library/Tcl/ . 
package require tcc4tcl
package require vectcl
package require sljit

set h [tcc4tcl::new]
$h add_include_path sljit_src/
$h add_include_path generic/
$h ccode {
typedef unsigned short __uint16_t, uint16_t;
typedef unsigned int __uint32_t, uint32_t;
typedef unsigned long __uint64_t, uint64_t;
#include <sljitLir.h>
#include <vectcl.h>

union executable_code {
	void* code;
	sljit_sw (SLJIT_CALL *func0)(void);
	sljit_sw (SLJIT_CALL *func1)(sljit_sw a);
	sljit_sw (SLJIT_CALL *func2)(sljit_sw a, sljit_sw b);
	sljit_sw (SLJIT_CALL *func3)(sljit_sw a, sljit_sw b, sljit_sw c);
};
typedef union executable_code executable_code;

#define FAILED(cond, text) \
	if (SLJIT_UNLIKELY(cond)) { \
		printf(text); \
		return; \
	}

#define CHECK(compiler) \
	if (sljit_get_compiler_error(compiler) != SLJIT_ERR_COMPILED) { \
		printf("Compiler error: %d\n", sljit_get_compiler_error(compiler)); \
		sljit_free_compiler(compiler); \
		return; \
	}


int tcc_sljitCmd (ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv) {
	/* copy test case for sljit floating point */

	/* This is test14 from the sljit test suite */
	/* Test fpu diadic functions. */
	executable_code code;
	struct sljit_compiler* compiler = sljit_create_compiler();
	sljit_d buf[15];
	int verbose=1;

	if (verbose)
		printf("Run test14\n");

	if (!sljit_is_fpu_available()) {
		if (verbose)
			printf("no fpu available, test14 skipped\n");
		if (compiler)
			sljit_free_compiler(compiler);
		return;
	}
	buf[0] = 7.25;
	buf[1] = 3.5;
	buf[2] = 1.75;
	buf[3] = 0.0;
	buf[4] = 0.0;
	buf[5] = 0.0;
	buf[6] = 0.0;
	buf[7] = 0.0;
	buf[8] = 0.0;
	buf[9] = 0.0;
	buf[10] = 0.0;
	buf[11] = 0.0;
	buf[12] = 8.0;
	buf[13] = 4.0;
	buf[14] = 0.0;

	FAILED(!compiler, "cannot create compiler\n");
	sljit_emit_enter(compiler, 0, 1, 3, 1, 6, 0, 0);

	/* ADD */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_IMM, sizeof(sljit_d));
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d));
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 2);
	sljit_emit_fop2(compiler, SLJIT_DADD, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 3, SLJIT_MEM2(SLJIT_S0, SLJIT_R0), 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop2(compiler, SLJIT_DADD, SLJIT_FR0, 0, SLJIT_FR0, 0, SLJIT_FR1, 0);
	sljit_emit_fop2(compiler, SLJIT_DADD, SLJIT_FR1, 0, SLJIT_FR0, 0, SLJIT_FR1, 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 4, SLJIT_FR0, 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 5, SLJIT_FR1, 0);

	/* SUB */
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR3, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 2);
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 2);
	sljit_emit_fop2(compiler, SLJIT_DSUB, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 6, SLJIT_FR3, 0, SLJIT_MEM2(SLJIT_S0, SLJIT_R1), SLJIT_DOUBLE_SHIFT);
	sljit_emit_fop2(compiler, SLJIT_DSUB, SLJIT_FR2, 0, SLJIT_FR2, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 2);
	sljit_emit_fop2(compiler, SLJIT_DSUB, SLJIT_FR3, 0, SLJIT_FR2, 0, SLJIT_FR3, 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 7, SLJIT_FR2, 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 8, SLJIT_FR3, 0);

	/* MUL */
	sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R1, 0, SLJIT_IMM, 1);
	sljit_emit_fop2(compiler, SLJIT_DMUL, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 9, SLJIT_MEM2(SLJIT_S0, SLJIT_R1), SLJIT_DOUBLE_SHIFT, SLJIT_FR1, 0);
	sljit_emit_fop2(compiler, SLJIT_DMUL, SLJIT_FR1, 0, SLJIT_FR1, 0, SLJIT_FR2, 0);
	sljit_emit_fop2(compiler, SLJIT_DMUL, SLJIT_FR5, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 2, SLJIT_FR2, 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 10, SLJIT_FR1, 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 11, SLJIT_FR5, 0);

	/* DIV */
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR5, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 12);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 13);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR4, 0, SLJIT_FR5, 0);
	sljit_emit_fop2(compiler, SLJIT_DDIV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 12, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 12, SLJIT_FR1, 0);
	sljit_emit_fop2(compiler, SLJIT_DDIV, SLJIT_FR5, 0, SLJIT_FR5, 0, SLJIT_FR1, 0);
	sljit_emit_fop2(compiler, SLJIT_DDIV, SLJIT_FR4, 0, SLJIT_FR1, 0, SLJIT_FR4, 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 13, SLJIT_FR5, 0);
	sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM1(SLJIT_S0), sizeof(sljit_d) * 14, SLJIT_FR4, 0);

	sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

	code.code = sljit_generate_code(compiler);
	CHECK(compiler);
	sljit_free_compiler(compiler);

	code.func1((sljit_sw)&buf);
	FAILED(buf[3] != 10.75, "test14 case 1 failed\n");
	FAILED(buf[4] != 5.25, "test14 case 2 failed\n");
	FAILED(buf[5] != 7.0, "test14 case 3 failed\n");
	FAILED(buf[6] != 0.0, "test14 case 4 failed\n");
	FAILED(buf[7] != 5.5, "test14 case 5 failed\n");
	FAILED(buf[8] != 3.75, "test14 case 6 failed\n");
	FAILED(buf[9] != 24.5, "test14 case 7 failed\n");
	FAILED(buf[10] != 38.5, "test14 case 8 failed\n");
	FAILED(buf[11] != 9.625, "test14 case 9 failed\n");
	FAILED(buf[12] != 2.0, "test14 case 10 failed\n");
	FAILED(buf[13] != 2.0, "test14 case 11 failed\n");
	FAILED(buf[14] != 0.5, "test14 case 12 failed\n");

	sljit_free_code(code.code);
	Tcl_SetObjResult(interp, Tcl_NewDoubleObj(buf[3]));
	return TCL_OK;
}

}

$h linktclcommand tcc_sljit tcc_sljitCmd

$h go

