# benchmark numarray against the memory bandwidth limit
set benchdir [file dirname [file dirname [info script]]]

lappend auto_path $benchdir $benchdir/lib
package require vectcl
namespace import vectcl::vexpr

package require rbc
namespace import rbc::vector

# bug in napcore: it unsets global variable dir
package require napcore


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

	
	set solutions {
		"Tcl" linear_regression_tcl 1e10
		"Rbc" linear_regression_rbc 1e10
		"NAP" linear_regression_nap_1f 50000
		"NAP_LS" linear_regression_nap 50000
		"vexpr" linear_regression_vexpr 1e10
		"vexprQR" linear_regression_vexprQR 1e10
		"vexprC" linear_regression_C 1e10
		"vexprBC" linear_regression_vexprBC 1e10
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
