# benchmark numarray against the memory bandwidth limit
set dir [file dirname [file dirname [info script]]]

lappend auto_path $dir $dir/lib
package require vectcl
namespace import vectcl::vexpr

package require rbcvec
namespace import rbc::vector

package require napcore

# benchmark linear regression formulae
proc linear_regression_tcl {xv yv} {
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
    list $a $b
}

proc linear_regression_vexpr_1f {x y} {
	vexpr { 
		beta=(mean(x.*y)-mean(x)*mean(y)) ./ (mean(x.^2)-mean(x).^2)
		alpha=mean(y)-beta*mean(x)
	}
	list $alpha $beta
}


proc linear_regression_vexpr {xv yv} {
	vexpr { 
		xm=mean(xv); ym=mean(yv)
		beta=sum((xv-xm).*(yv-ym))./sum((xv-xm).^2)
		alpha=ym-beta*xm
	}
	list $alpha $beta
}

proc linear_regression_vexprQR {xv yv} {
	vexpr {
		A=hstack(1, xv)
		A \ yv
	}
}

proc linear_regression_rbc {xv yv} {
	vector create x
	vector create y
	vector create alpha
	vector create beta
	x set $xv
	y set $yv
	beta expr {(mean(x*y)-mean(x)*mean(y))/(mean(x^2)-mean(x)^2)}
	alpha expr {mean(y)-beta*mean(x)}
	list [alpha index 0] [beta index 0]
}

proc linear_regression_nap {xv yv} {
	# nap does not convert lists into NAO
	# unfair comparison (shimmering to string)
	nap "x = [list $xv]"
	nap "y = [list $yv]"
	# built-in linear regression
	nap {p = regression(x, y)}
	$p
	list 1 2
}

proc linear_regression_nap_1f {xv yv} {
	# nap does not convert lists into NAO
	# unfair comparison (shimmering to string)
	nap "x = [list $xv]"
	nap "y = [list $yv]"
	# direct formula
	nap {xm = sum(x)/count(x)}
	nap {ym = sum(y)/count(y)}
	nap {beta = sum((x-xm)*(y-ym))/sum((x-xm)**2)}
	nap {alpha = ym-beta*xm}
	list [$alpha] [$beta]
}


proc benchlinreg {vlength} {
	puts "Number of datapoints: $vlength"
	puts "============================="
	set rep [expr {1000000/$vlength}] 
	for {set i 0} {$i<$vlength} {incr i} {
		lappend x [expr {double($i)}]
		lappend y [expr {$i*3+rand()}]
	}

	
	set solutions {
		"pure Tcl" linear_regression_tcl
		"Rbc" linear_regression_rbc
		"NAP" linear_regression_nap_1f
		"vexpr matrix inversion" linear_regression_vexprQR
		"vexpr direct formula" linear_regression_vexpr_1f
	}
	
	puts "Correctness: "
	foreach {d s} $solutions {
		puts "$d: "
		puts [eval [list $s $x $y]]
	}

	puts "Timings: "
	foreach {d s} $solutions {
		puts "$d: "
		puts [time [list $s $x $y] $rep]
	}
}

benchlinreg 10
benchlinreg 10
benchlinreg 100
benchlinreg 1000
benchlinreg 10000
benchlinreg 100000

