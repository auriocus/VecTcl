# benchmark numarray against the memory bandwidth limit
set dir [file dirname [file dirname [info script]]]

lappend auto_path $dir $dir/lib
package require vectcl
namespace import vectcl::vexpr

proc compute_bandwidth {code rep} {
	upvar 1 A A
	upvar 1 B B
	upvar 1 memsize memsize

	set t [time $code $rep]

	set micros [lindex $t 0]
	set bw [expr {$memsize/1e9 / ($micros*1e-6)}]

	puts "[format %.3g $bw] GBytes/s ($t in $rep repetitions)"
	return $bw

}

proc bench_vec {vlength} {
	puts "Benchmarking vectors of length $vlength"
	# create two double vectors of size 100000
	vexpr {A=ones(vlength)+0.0; B=ones(vlength)+0.0}
	
	set rep [expr {50000000/$vlength}]
	run_bench $A $B $rep
}

proc bench_mat {M N} {
	puts "Benchmarking matrices of size $M x $N"
	# create two double vectors of size 100000
	vexpr {A=ones(M,N)+0.0; B=ones(M,N)+0.0}
	
	set rep [expr {20000000/($M*$N)+1}]
	run_bench $A $B $rep
}

proc run_bench {A B rep} {
	set memsize [dict get [numarray info $A] bufsize]
	
	variable results
	set line $memsize

	puts "Memcopy: "
	lappend line [compute_bandwidth {numarray fastcopy A $B} $rep]

	puts "Addition in C loop"
	lappend line [compute_bandwidth {numarray fastadd A $B} $rep]

	# puts "Result should be 1001: [numarray get $A 23]"

	puts "Reduction"
	lappend line [compute_bandwidth {numarray sum $B} $rep]

	puts "Assignment via iterators"
	lappend line [compute_bandwidth {numarray = A $B} $rep]

	puts "Addition via iterators"
	lappend line [compute_bandwidth {numarray += A $B} $rep]

	puts "Unary operator"
	lappend line [compute_bandwidth {numarray neg $B} $rep]

	puts "Binary operator"
	lappend line [compute_bandwidth {numarray + $A $B} $rep]

	lappend results $line
	# puts "Result should be 1001: [numarray get $A 23]"
	puts ""
}

set benchdir [file dirname [file dirname [info script]]]
set fd [open [file join $benchdir benchmark benchmemcpy-[clock scan now].dat] w]
fconfigure $fd -buffering line
# vectors
set results {}
foreach s {10 20 50 100 200 500 1000 2000 5000 10000 20000 50000 100000 200000 500000 1000000} {
	bench_vec $s
}
puts $fd "# Vectors"
puts $fd [join $results \n]
puts $fd "\n"

set results {}
foreach s {2 5 10 20 50 100 200 500 1000} {
	# square matrices
	bench_mat $s $s
}

puts $fd "# Square matrices N x N"
puts $fd [join $results \n]
puts $fd "\n"

set results {}
foreach N {5 10 20 50 100 200 500 1000 2000 5000 10000} {
	# wide matrices 2 x N
	bench_mat 2 $N
}

puts $fd "# Wide matrices 2 x N"
puts $fd [join $results \n]
puts $fd "\n"

set results {}
foreach N {5 10 20 50 100 200 500 1000 2000 5000 10000} {
	# tall matrices N x 2
	bench_mat $N 2
}

puts $fd "# Tall matrices N x 2"
puts $fd [join $results \n]
puts $fd "\n"

close $fd
