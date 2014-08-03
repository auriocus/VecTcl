# benchmark numarray against the memory bandwidth limit
set benchdir [file dirname [file dirname [info script]]]

lappend auto_path $benchdir $benchdir/lib
package require vectcl
namespace import vectcl::vexpr

#package require rbc
#namespace import rbc::vector

# bug in napcore: it unsets global variable dir
#package require napcore

package require tcc4tcl

# benchmark linear regression formulae
proc linear_regression_tcl {xv yv {rep 1}} {
	# no setup required
	set t1 "0 microseconds"
	set t2 [time {
		set xsum 0.0; set ysum 0.0
		foreach x $xv y $yv {
			set xsum [expr {$xsum + $x}]
			set ysum [expr {$ysum + $y}]
		}
		set xm [expr {$xsum/[llength $xv]}]
		set ym [expr {$ysum/[llength $xv]}]
		set xsum 0.0; set ysum 0.0
		foreach x $xv y $yv {
			set dx [expr {$x - $xm}]
			set dy [expr {$y - $ym}]
			set xsum [expr {$xsum + $dx * $dy}]
			set ysum [expr {$ysum + $dx * $dx}]
		}
		set b [expr {$xsum / $ysum}]
		set a [expr {$ym - $b * $xm}]
	} $rep]
	
	list $t1 $t2 $a $b
}

proc linear_regression_vexpr_1f {x y rep} {
	set t1 [time {numarray create $x; numarray create $y}]
	# setup can't be repeated, because it is cached 
	# in the Tcl_Objs of x and y
	set t2 [time {
		vexpr { 
			beta=(mean(x.*y)-mean(x)*mean(y)) ./ (mean(x.^2)-mean(x).^2)
			alpha=mean(y)-beta*mean(x)
		}
	} $rep]
	list $t1 $t2 $alpha $beta
}


proc linear_regression_vexpr {xv yv rep} {
	set t1 [time {numarray create $xv; numarray create $yv}]
	# setup can't be repeated, because it is cached 
	# in the Tcl_Objs of x and y
	set t2 [time {
		vexpr { 
			xm=mean(xv); ym=mean(yv)
			beta=sum((xv-xm).*(yv-ym))./sum((xv-xm).^2)
			alpha=ym-beta*xm
		}
	} $rep]
	list $t1 $t2 $alpha $beta
}

proc linear_regression_vexprQR {xv yv rep} {
	set t1 [time {numarray create $xv; numarray create $yv}]
	# setup can't be repeated, because it is cached 
	# in the Tcl_Objs of x and y
	set t2 [time {
		vexpr {
			A=hstack(1, xv)
			alpha, beta = A \ yv
		}
	} $rep]
	list $t1 $t2 $alpha $beta
}

tcc4tcl::cproc linregjit_old {Tcl_Interp* interp Tcl_Obj* xv Tcl_Obj* yv Tcl_Obj* c1} ok {
	int NumArrayPlus(Tcl_Interp *interp, Tcl_Obj* op1, Tcl_Obj* op2, Tcl_Obj** result);
	int NumArrayMinus(Tcl_Interp *interp, Tcl_Obj* op1, Tcl_Obj* op2, Tcl_Obj** result);
	int NumArrayPow(Tcl_Interp *interp, Tcl_Obj* op1, Tcl_Obj* op2, Tcl_Obj** result);
	int NumArrayTimes(Tcl_Interp *interp, Tcl_Obj* op1, Tcl_Obj* op2, Tcl_Obj** result);
	int NumArrayRdivide(Tcl_Interp *interp, Tcl_Obj* op1, Tcl_Obj* op2, Tcl_Obj** result);
	
	int NumArraySum(Tcl_Interp *interp, Tcl_Obj* op, int axis, Tcl_Obj** result);
	int NumArrayMean(Tcl_Interp *interp, Tcl_Obj* op, int axis, Tcl_Obj** result);
	
	/* Temporary and standard objects */
	Tcl_Obj *temp1, *temp2, *temp3, *temp4, *temp5, *temp6, *temp7, *temp8;
	Tcl_Obj *result;
	int code;
	
	/* Intermediate results */
	Tcl_Obj *xm, *ym, *alpha, *beta;

	/* xm=mean(xv) */
	
	code = NumArrayMean(interp, xv, 0, &xm);
	if (code != TCL_OK) { return TCL_ERROR; }
	
	/* ym=mean(yv) */
	code = NumArrayMean(interp, yv, 0, &ym);
	if (code != TCL_OK) { return TCL_ERROR; }
	
	/* beta=sum((xv-xm).*(yv-ym))./sum((xv-xm).^2) */
	
	code=NumArrayMinus(interp, xv, xm, &temp1);
	if (code != TCL_OK) { return TCL_ERROR; }

	code=NumArrayMinus(interp, yv, ym, &temp2);
	if (code != TCL_OK) { return TCL_ERROR; }

	code=NumArrayTimes(interp, temp1, temp2, &temp3);
	if (code != TCL_OK) { return TCL_ERROR; }
	Tcl_DecrRefCount(temp1);
	Tcl_DecrRefCount(temp2);


	code=NumArraySum(interp, temp3, 0, &temp4);
	if (code != TCL_OK) { return TCL_ERROR; }
	Tcl_DecrRefCount(temp3);
	
	code=NumArrayMinus(interp, xv, xm, &temp5);
	if (code != TCL_OK) { return TCL_ERROR; }

	code=NumArrayPow(interp, temp5, c1, &temp7);
	if (code != TCL_OK) { return TCL_ERROR; }
	Tcl_DecrRefCount(temp5);

	code=NumArraySum(interp, temp7, 0, &temp8);
	if (code != TCL_OK) { return TCL_ERROR; }
	Tcl_DecrRefCount(temp7);

	code=NumArrayRdivide(interp, temp4, temp8, &beta);
	if (code != TCL_OK) { return TCL_ERROR; }
	Tcl_DecrRefCount(temp4);
	Tcl_DecrRefCount(temp8);

	/* alpha=ym-beta*xm */
	code=NumArrayTimes(interp, beta, xm, &temp1);
	if (code != TCL_OK) { return TCL_ERROR; }

	code = NumArrayMinus(interp, ym, temp1, &alpha);
	Tcl_DecrRefCount(temp1);
	if (code != TCL_OK) { return TCL_ERROR; }
	
	/* here the garbage collection is missing in case of errors */
	result=Tcl_NewObj();
	code=Tcl_ListObjAppendElement(interp, result, alpha);
	if (code != TCL_OK) { return TCL_ERROR;}

	code=Tcl_ListObjAppendElement(interp, result, beta);
	if (code != TCL_OK) { return TCL_ERROR;}
	
	Tcl_SetObjResult(interp, result);
	
	Tcl_DecrRefCount(xm);
	Tcl_DecrRefCount(ym);
	

	return TCL_OK;
}

tcc4tcl::cproc linregjit {Tcl_Interp* interp Tcl_Obj* xv Tcl_Obj* yv Tcl_Obj* c1} ok {
	int NumArrayPlus(Tcl_Obj* op1, Tcl_Obj* op2, Tcl_Obj** result);
	int NumArrayMinus(Tcl_Obj* op1, Tcl_Obj* op2, Tcl_Obj** result);
	int NumArrayPow(Tcl_Obj* op1, Tcl_Obj* op2, Tcl_Obj** result);
	int NumArrayTimes(Tcl_Obj* op1, Tcl_Obj* op2, Tcl_Obj** result);
	int NumArrayRdivide(Tcl_Obj* op1, Tcl_Obj* op2, Tcl_Obj** result);
	
	int NumArraySum(Tcl_Obj* op, int axis, Tcl_Obj** result);
	int NumArrayMean(Tcl_Obj* op, int axis, Tcl_Obj** result);

	extern const Tcl_ObjType NumArrayTclType;

	if (Tcl_ConvertToType(interp, xv, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}	
	
	if (Tcl_ConvertToType(interp, yv, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}	
	
	if (Tcl_ConvertToType(interp, c1, &NumArrayTclType) != TCL_OK) {
		return TCL_ERROR;
	}	
	
	/* Temporary and standard objects */
	Tcl_Obj *temp1, *temp2, *temp3, *temp4, *temp5, *temp6, *temp7, *temp8;
	Tcl_Obj *result;
	int code;
	
	/* Intermediate results */
	Tcl_Obj *xm, *ym, *alpha, *beta;

	/* xm=mean(xv) */
	
	code = NumArrayMean(xv, 0, &xm);
	if (code != TCL_OK) { 
		Tcl_SetObjResult(interp, xm);
		return TCL_ERROR;
	}
	
	/* ym=mean(yv) */
	code = NumArrayMean(yv, 0, &ym);
	if (code != TCL_OK) { 
		Tcl_SetObjResult(interp, ym);
		return TCL_ERROR;
	}
	
	/* beta=sum((xv-xm).*(yv-ym))./sum((xv-xm).^2) */
	
	code=NumArrayMinus(xv, xm, &temp1);
	if (code != TCL_OK) { 
		Tcl_SetObjResult(interp, temp1);
		return TCL_ERROR;
	}

	code=NumArrayMinus(yv, ym, &temp2);
	if (code != TCL_OK) { 
		Tcl_SetObjResult(interp, temp2);
		return TCL_ERROR;
	}

	code=NumArrayTimes(temp1, temp2, &temp3);
	Tcl_DecrRefCount(temp1);
	Tcl_DecrRefCount(temp2);
	if (code != TCL_OK) { 
		Tcl_SetObjResult(interp, temp3);
		return TCL_ERROR;
	}


	code=NumArraySum(temp3, 0, &temp4);
	Tcl_DecrRefCount(temp3);
	if (code != TCL_OK) { 
		Tcl_SetObjResult(interp, temp4);
		return TCL_ERROR;
	}
	
	code=NumArrayMinus(xv, xm, &temp5);
	if (code != TCL_OK) { 
		Tcl_SetObjResult(interp, temp5);
		return TCL_ERROR;
	}

	code=NumArrayPow(temp5, c1, &temp7);
	Tcl_DecrRefCount(temp5);
	if (code != TCL_OK) { 
		Tcl_SetObjResult(interp, temp7);
		return TCL_ERROR;
	}

	code=NumArraySum(temp7, 0, &temp8);
	Tcl_DecrRefCount(temp7);
	if (code != TCL_OK) { 
		Tcl_SetObjResult(interp, temp8);
		return TCL_ERROR;
	}

	code=NumArrayRdivide(temp4, temp8, &beta);
	Tcl_DecrRefCount(temp4);
	Tcl_DecrRefCount(temp8);
	if (code != TCL_OK) { 
		Tcl_SetObjResult(interp, beta);
		return TCL_ERROR;
	}

	/* alpha=ym-beta*xm */
	code=NumArrayTimes(beta, xm, &temp1);
	if (code != TCL_OK) { 
		Tcl_SetObjResult(interp, temp1);
		return TCL_ERROR;
	}

	code = NumArrayMinus(ym, temp1, &alpha);
	Tcl_DecrRefCount(temp1);
	if (code != TCL_OK) { 
		Tcl_SetObjResult(interp, alpha);
		return TCL_ERROR;
	}
	
	/* here the garbage collection is missing in case of errors */
	result=Tcl_NewObj();
	code=Tcl_ListObjAppendElement(interp, result, alpha);
	if (code != TCL_OK) { return TCL_ERROR;}

	code=Tcl_ListObjAppendElement(interp, result, beta);
	if (code != TCL_OK) { return TCL_ERROR;}
	
	Tcl_SetObjResult(interp, result);
	
	Tcl_DecrRefCount(xm);
	Tcl_DecrRefCount(ym);
	

	return TCL_OK;
}

proc linear_regression_vexprJIT {xv yv rep} {
	set t1 [time {numarray create $xv; numarray create $yv}]
	# setup can't be repeated, because it is cached 
	# in the Tcl_Objs of x and y
	set t2 [time {set result [linregjit $xv $yv 2.0]} $rep]
	list $t1 $t2 {*}$result
}



proc linear_regression_C {xv yv rep} {
	set t1 [time {numarray create $xv; numarray create $yv}]
	# setup can't be repeated, because it is cached 
	# in the Tcl_Objs of x and y
	set t2 [time {
		set result [numarray::linreg $xv $yv]
	} $rep]
	list $t1 $t2 {*}$result
}

proc linear_regression_vexprBC {xv yv rep} {
	set t1 [time {numarray create $xv; numarray create $yv}]
	# setup can't be repeated, because it is cached 
	# in the Tcl_Objs of x and y
	set t2 [time {
		set xm [numarray::mean $xv]
		set ym [numarray::mean $yv]
		set beta [numarray::./ [numarray::sum \
			[numarray::bcexecute [binary format c* {
				2 5 1 2
				2 6 3 4
				3 0 5 6}] {} $xv $xm $yv $ym]] \
			[numarray::sum \
			[numarray::bcexecute [binary format c* {
				2 0 1 2
				6 0 0 3}] {} $xv $xm 2.0]]]

		set alpha [expr {$ym-$beta*$xm}]

	} $rep]
	list $t1 $t2 $alpha $beta
}


proc linear_regression_rbc {xv yv rep} {
	set t1 [time {
		vector create x
		vector create y
		vector create alpha
		vector create beta
		x set $xv
		y set $yv
	}]
	set t2 [time {
		beta expr {(mean(x*y)-mean(x)*mean(y))/(mean(x^2)-mean(x)^2)}
		alpha expr {mean(y)-beta*mean(x)}
	} $rep]
	list $t1 $t2 [alpha index 0] [beta index 0]
}

proc linear_regression_nap {xv yv rep} {
	# nap does not convert lists into NAO
	# unfair comparison (shimmering to string)
	set t1 [time {
		nap "x = [list $xv]"
		nap "y = [list $yv]"
	}]
	set t2 [time {
		# built-in linear regression
		nap {p = regression(x, y)}
	} $rep]
	list $t1 $t2 {*}[$p]
}

proc linear_regression_nap_1f {xv yv rep} {
	# nap does not convert lists into NAO
	# unfair comparison (shimmering to string)
	set t1 [time {
		nap "x = [list $xv]"
		nap "y = [list $yv]"
	}]
	# direct formula
	set t2 [time {
		nap {xm = sum(x)/count(x)}
		nap {ym = sum(y)/count(y)}
		nap {beta = sum((x-xm)*(y-ym))/sum((x-xm)**2)}
		nap {alpha = ym-beta*xm}
	} $rep]
	list $t1 $t2 [$alpha] [$beta]
}


proc benchlinreg {vlength} {
	puts "Number of datapoints: $vlength"
	puts "============================="
	set rep [expr {1000000/$vlength+1}] 
	for {set i 0} {$i<$vlength} {incr i} {
		lappend x [expr {double($i)}]
		lappend y [expr {$i*3+rand()}]
	}

	
	set solutions_all {
		"Tcl" linear_regression_tcl 1e10
		"Rbc" linear_regression_rbc 1e10
		"NAP" linear_regression_nap_1f 50000
		"NAP_LS" linear_regression_nap 50000
		"vexpr" linear_regression_vexpr 1e10
		"vexprQR" linear_regression_vexprQR 1e10
		"vexprC" linear_regression_C 1e10
		"vexprJIT" linear_regression_vexprJIT 1e10
	}
	
	set solutions {
		"vexprJIT" linear_regression_vexprJIT 1e10
		"vexprC" linear_regression_C 1e10
		"vexpr" linear_regression_vexpr 1e10
	}


#	puts "Correctness: "
#	foreach {d s max} $solutions {
#		if {$vlength>$max} { continue }
#		puts "$d: "
#		puts [eval [list $s $x $y 1]]
#	}
#
#	return
	
	variable timings
	dict lappend timings N $vlength
	puts "Timings: "
	foreach {d s max} $solutions {
		if {$vlength>$max} {
			dict lappend timings $d NaN
			dict lappend timings "$d setup" NaN
			continue
		}
		puts -nonewline "$d: "
		lassign [eval [list $s $x $y $rep]] t1 t2 alpha beta
		set setuptime [expr {[lindex $t1 0]/double($vlength)}]
		set comptime [expr {[lindex $t2 0]/double($vlength)}]
		puts "[format %.3g $comptime] µs / sample, setup [format %.3g $setuptime] µs / sample"
		dict lappend timings $d $comptime
		dict lappend timings "$d setup" $setuptime
	}
}

set timings {}
# cutoff any one-time cost
benchlinreg 5
# geometric progression of vector length
exit
foreach size {5 10 20 50 100 200 500 1000 2000 5000 10000 20000 50000 100000 200000 500000 1000000} {
	benchlinreg $size
}

# write benchmark result to file
# i.e. transpose table
set keys [dict keys $timings]
set times [dict values $timings]
set fd [open [file join $benchdir benchmark benchresult-[clock scan now].dat] w]
puts $fd "# [join $keys \t]"
puts $fd [join [vexpr {times'}] \n]
close $fd
