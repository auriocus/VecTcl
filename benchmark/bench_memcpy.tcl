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
	
	puts "Memcopy: "
	compute_bandwidth {numarray fastcopy A $B} $rep

	puts "Addition in C loop"
	compute_bandwidth {numarray fastadd A $B} $rep

	# puts "Result should be 1001: [numarray get $A 23]"

	puts "Reduction"
	compute_bandwidth {numarray sum $B} $rep

	puts "Assignment via iterators"
	compute_bandwidth {numarray = A $B} $rep

	puts "Addition via iterators"
	compute_bandwidth {numarray += A $B} $rep

	puts "Unary operator"
	compute_bandwidth {numarray neg $B} $rep

	puts "Binary operator"
	compute_bandwidth {numarray + $A $B} $rep

	# puts "Result should be 1001: [numarray get $A 23]"
	puts ""
}

# vectors
bench_vec 100
bench_vec 1000
bench_vec 10000
bench_vec 100000
bench_vec 1000000

# square matrices
bench_mat 10 10
bench_mat 100 100
bench_mat 1000 1000

# rectangular matrices
bench_mat 2 1000
bench_mat 1000 2

bench_mat 2 10000
bench_mat 10000 2

