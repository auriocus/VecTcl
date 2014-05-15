# simple example for using vexpr to do automatic differentation (AD)
# AD is a technique to compute an expression alongside with it's
# analytic derivative using the chainrule
package require vectcl 
namespace import vectcl::*

proc isAD {x} { expr {[lindex $x 0] eq "AD"} }

proc mkAD {x dx} { list AD $x $dx }

namespace eval ADeval {
	# the definition of an AD expression is
	# a list with AD, then x, then dx (only scalar for clarity)
	# also there is no forwarding to numarray
	proc dissectargs {args} {
		# result is pairwise, value and derivative
		set result {}
		foreach el $args {
			if {[isAD $el]} {
				lassign $el AD x dx
				lappend result $x $dx
			} else {
				lappend result $el 0
			}
		}
		return $result
	}

	proc + {e1 e2} {
		lassign [dissectargs $e1 $e2] x dx y dy
		mkAD [expr {$x+$y}] [expr {$dx+$dy}]
	}

	proc - {e1 e2} {
		lassign [dissectargs $e1 $e2] x dx y dy
		mkAD [expr {$x-$y}] [expr {$dx-$dy}]
	}

	proc / {e1 e2} {
		lassign [dissectargs $e1 $e2] x dx y dy
		mkAD [expr {$x/$y}] [expr {($dx*$y - $dy*$x)/($y*$y)}]
	}

	proc * {e1 e2} {
		lassign [dissectargs $e1 $e2] x dx y dy
		mkAD [expr {$x*$y}] [expr {$x*$dy+$dx*$y}]
	}
	 
	proc exp {e} {
		lassign [dissectargs $e] x dx
		mkAD [expr {exp($x)}] [expr {exp($x)*$dx}]
	}

	proc sin {e} {
		lassign [dissectargs $e] x dx
		mkAD [expr {sin($x)}] [expr {cos($x)*$dx}]
	}

	proc cos {e} {
		lassign [dissectargs $e] x dx
		mkAD [expr {cos($x)}] [expr {-sin($x)*$dx}]
	}

}

# now inject into VecTcl by renaming the numarray procedures
namespace eval savedops {}

foreach op {+ - * / exp sin cos} {
	# this example throws away the numarray commands
	rename numarray::$op savedops::$op
	interp alias {} numarray::$op {} ADeval::$op
}

# test 
# dependent variable x
set rx [expr {3.1415926535/6}] ;# 30°
set x [mkAD $rx 1.0]

puts [vexpr {sin(x)}]
puts "f=[expr {sin($rx)}], df/dx = [expr {cos($rx)}]"

# Arjens example
set a -0.5
foreach rx {0 1 2 3 4 5} {
	set x [mkAD $rx 1.0]
	puts "x=$rx"
	puts "Automatic: [vexpr {exp(a*x)}]"
    puts "Manual:    [mkAD [expr {exp($a*$rx)}] [expr {$a*exp($a*$rx)}]]"
	
	puts "Automatic: [vexpr {2*sin(x)/cos(x)}]"
    puts "Manual:    [mkAD [expr {2*sin($rx)/cos($rx)}] [expr {2.0/cos($rx)**2}]]"
}


