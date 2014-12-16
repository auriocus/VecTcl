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
#include <vectclInt.h>

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

	/* code and const addresses */

	executable_code sljit_code;
	sljit_uw const_addrs[10];

	int tcc_compilesljitCmd (ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv) {
		/* compile the inner loop */
			struct sljit_compiler* compiler = sljit_create_compiler();
			if (!sljit_is_fpu_available()) {
				Tcl_SetResult(interp, "No FPU available", TCL_STATIC);
				return TCL_ERROR;
			}
			
			FAILED(!compiler, "cannot create compiler\n");
			sljit_emit_enter(compiler, 0, 0, 6, 0, 6, 0, 0);

			/* compute x*x+y*y -> z */
			struct sljit_const * consts[10];
			/* length: 0, &temp7ptr: 1, temp1ptr: 2, temp2ptr: 3, temp1pitch: 4, temp2pitch: 5 */

			consts[0] = sljit_emit_const(compiler, SLJIT_R3, 0, 1);
			consts[1] = sljit_emit_const(compiler, SLJIT_R0, 0, 0);
			consts[2] = sljit_emit_const(compiler, SLJIT_R1, 0, 0);
			consts[3] = sljit_emit_const(compiler, SLJIT_R2, 0, 0);
			
			consts[4] = sljit_emit_const(compiler, SLJIT_R4, 0, 0);
			consts[5] = sljit_emit_const(compiler, SLJIT_R5, 0, 0);
			
			
			sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_R4, 0, SLJIT_R4, 0, SLJIT_IMM, 3);
			sljit_emit_op2(compiler, SLJIT_SHL, SLJIT_R5, 0, SLJIT_R5, 0, SLJIT_IMM, 3);
			
			sljit_emit_op1(compiler, SLJIT_MOV, SLJIT_R0, 0, SLJIT_MEM1(SLJIT_R0), 0);

			/* label */
			struct sljit_label* label = sljit_emit_label(compiler);

			sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR0, 0, SLJIT_MEM1(SLJIT_R1), 0);
			sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_FR1, 0, SLJIT_MEM1(SLJIT_R2), 0);
			sljit_emit_fop2(compiler, SLJIT_DMUL, SLJIT_FR0, 0, SLJIT_FR0, 0, SLJIT_FR0, 0);
			sljit_emit_fop2(compiler, SLJIT_DMUL, SLJIT_FR1, 0, SLJIT_FR1, 0, SLJIT_FR1, 0);

			sljit_emit_fop2(compiler, SLJIT_DADD, SLJIT_FR0, 0, SLJIT_FR0, 0, SLJIT_FR1, 0);
			sljit_emit_fop1(compiler, SLJIT_DMOV, SLJIT_MEM1(SLJIT_R0), 0, SLJIT_FR0, 0); 
			
			sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R0, 0, SLJIT_R0, 0, SLJIT_IMM, sizeof(double));
			
			sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R1, 0, SLJIT_R1, 0, SLJIT_R4, 0);
			sljit_emit_op2(compiler, SLJIT_ADD, SLJIT_R2, 0, SLJIT_R2, 0, SLJIT_R5, 0);

			/* decrement and jump */
			/*sljit_emit_op2(compiler, SLJIT_SUB|SLJIT_SET_E, SLJIT_R3, 0, SLJIT_R3, 0, SLJIT_IMM, 1); */
			sljit_emit_op_custom(compiler, "\x49\xff\xc8", 3); /* dec r8 */
			struct sljit_jump* jump;
			jump = sljit_emit_jump(compiler, SLJIT_NOT_EQUAL);
			sljit_set_label(jump, label);
			/* write back temp7 - missing, value is lost */
			
			sljit_emit_return(compiler, SLJIT_UNUSED, 0, 0);

			sljit_code.code = sljit_generate_code(compiler);
			CHECK(compiler);
			sljit_free_compiler(compiler);

			/* get addresses of consts */
			for (int i=0; i<=5; i++) {
				const_addrs[i]=sljit_get_const_addr(consts[i]);
			}

			printf("slJIT code at %p\n", sljit_code.code);

			return TCL_OK;
	}

	/* Try to reduce the overhead for setting the constants */
	#define SET_CONST(i, val) *(sljit_sw*)(const_addrs[i]) = val;
	
	int tcc_sljitCmd (ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv) {
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
			
			/* length: 0, &temp7ptr: 1, temp1ptr: 2, temp2ptr: 3, temp1pitch: 4, temp2pitch: 5 */
			/* Transfer parameters to compiled function */
			SET_CONST(0, length);
			SET_CONST(1, &temp7ptr);
			SET_CONST(2, temp1ptr);
			SET_CONST(3, temp2ptr);
			SET_CONST(4, temp1pitch);
			SET_CONST(5, temp2pitch);

			while (temp1ptr) {
				sljit_code.func0();
				temp1ptr = NumArrayIteratorAdvanceRow(&temp1_it);
				temp2ptr = NumArrayIteratorAdvanceRow(&temp2_it);
			};
			
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
	
}

$h linktclcommand tcc_sljit tcc_sljitCmd
$h linktclcommand tcc_compilesljit tcc_compilesljitCmd

$h go
tcc_compilesljit

set a {3.0 2.0 3.0 1.0}
set b {4.0 5.0 6.0 1.0}
puts [tcc_sljit $a $b]
