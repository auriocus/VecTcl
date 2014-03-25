# benchmark numarray against the memory bandwidth limit
set dir [file dirname [file dirname [info script]]]

lappend auto_path $dir $dir/lib
package require vectcl
namespace import vectcl::vexpr

# benchmark linear regression formulae
proc benchsolvesys {vlength} {
	puts "Number of datapoints: $vlength"
	puts "============================="
	set A {}
	set y {}
	for {set i 0} {$i<$vlength} {incr i} {
		lappend y [expr {$i*3+rand()}]
		set line {}
		for {set j 0} {$j<$vlength} {incr j} {
			lappend line [expr {$i+rand()}]
		}
		lappend A $line
	}

	puts [numarray info $A]
	puts [numarray info $y]

	puts [time {vexpr {p=A \ y}}]
	puts "Difference: [vexpr {sum(((A*p-y)./y).^2)}]"
}

benchsolvesys 10
benchsolvesys 10
benchsolvesys 50
benchsolvesys 100
benchsolvesys 200
benchsolvesys 500
benchsolvesys 1000
