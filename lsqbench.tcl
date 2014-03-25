lappend auto_path [file dirname [info script]]
package require vectcl
package require math::linearalgebra
namespace import math::linearalgebra::*
namespace import vectcl::vexpr

# create data in lists x and y
set x {}; set y {}
for {set i 0} {$i<1000} {incr i} {
	set xel [expr {($i-200.0)/300}]
	lappend x $xel
	lappend xsq [expr {$xel**2}]
	lappend y [expr {1.25+$xel+0.23*$xel**2+rand()}]
}

# fit a+b*x+c*x^2 to the data
# using matrx decomposition in Tcl(lib) and VecTcl
proc solveTcl {x y} {
	upvar 1 xsq xsq
	set n [llength $x] 
	if {1} {
		# doesn't work, because mul is missing
		set A [mkMatrix $n 3]
		setcol A 0 [mkVector $n 1.0]
		setcol A 1 $x
		setcol A 2 $xsq
	} else {

		# create matrix
		foreach xel $x {
			lappend A [list 1.0 $xel [expr {$xel**2}]]
		}
	}
	# solve least squares problem
	leastSquaresSVD $A $y
}

proc solveVecTcl {x y} {
	vexpr { 
		A=hstack(1,x,x.^2)
		A \ y
	}
}

proc solveVecTclSlice {x y} {
	vexpr { 
		n=llength(x)
		A=ones(n,3)
		A[:,1]=x
		A[:,2]=x.^2
		A \ y
	}
}


