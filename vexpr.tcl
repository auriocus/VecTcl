package require TclOO

namespace eval vectcl {
	variable ns [namespace current]
	
	interp alias {} ::numarray::ones {} ::numarray::constfill 1.0
	interp alias {} ::numarray::zeros {} ::numarray::constfill 0.0

	proc compile {xprstring} {
		variable compiler
		$compiler compile $xprstring
	}

	proc Init {} {
		variable ns
		# create parser
		variable parser [VMParser]
		# create compiler
		variable compiler [CompileVMath new $parser]
		# clean cache
		variable proccache {}
		variable cachecount 0

		namespace export vexpr vproc
	}

	proc vexpr {e} {
		# compile and execute an expression e
		variable proccache
		variable cachecount
		variable compiler

		if {[dict exists $proccache $e]} {	
			return [uplevel 1 [dict get $proccache $e]]
		} else {
			set procbody [$compiler compile $e]
			set procname ::numarray::compiledexpression[incr cachecount]

			proc $procname {} $procbody

			dict set proccache $e $procname

			return [uplevel 1 $procname]
		}
	}
	
	proc vproc {name arglist body} {
	    # compile and define a VMath procedure
	    variable compiler
	    variable cachecount

	    set procbody [$compiler compile $body -novarrefs]
	    set procname ::numarray::compiledexpression[incr cachecount]

	    proc $procname $arglist $procbody
	    interp alias {} $name {} $procname
	}

	oo::class create ${ns}::CompileVMath {
		variable tokens script varrefs tempcount parser
		variable purefunctions
		variable errpos

		constructor {p} {
			set parser $p
			# functions that can be performed during
			# constant folding (optimization)
			variable purefunctions {
				numarray::%
				numarray::*
				numarray::+
				numarray::-
				numarray::.*
				numarray::.+
				numarray::.-
				numarray::./
				numarray::.\\
				numarray::.^
				numarray::\\
				numarray::neg
				numarray::adjoint
				numarray::slice
				acos acosh asin asinh atan atanh 
				axismax axismin binarymax binarymin complex 
				concat constfill cos cosh dimensions double 
				exp log mean qreco reshape sin sinh sqrt
				std std1 sum tan tanh transpose list shape rows cols
			}
		}

		method compile {script_ args} {
			# Instantiate the parser
			set script $script_
			set varrefs {}
			set tempcount 0
			
			set ast [$parser parset $script]
			set errpos [$parser errpos]
			return [my {*}$ast {*}$args]
		}

		# get name for temp var
		method alloctemp {{n 1}} {
			if {$n == 1} {
				return "__temp[incr tempcount]"
			}

			for {set i 0} {$i<$n} {incr i; incr tempcount} {
				lappend result "__temp$tempcount"
			}
			return $result
		}

		# reference variables
		method varref {var} {
			if {[dict exists $varrefs $var]} {
				return [dict get $varrefs $var]
			}

			# check for qualified names
			if {[regexp  {^[^:]+::} $var]} {
				# it contains ::, but doesn't start with ::
				# i.e. a relative namespace varref like a::b
				set name [my alloctemp]
			} else {
				# either global ::var, or local var
				# use it as is
				set name $var
			}
			dict set varrefs $var $name
			return $name
		}

		method getvarrefs {} {
			dict keys $varrefs
		}

		method mkupvars {} {
			set upvars {}
			dict for {var ref} $varrefs {
				# pull in via upvar all non-global names
				# globals need not/may not be upvarred
				if {![string match ::* $var]} {
					append upvars [list upvar 1 $var $ref ]\n
				}
			}
			return $upvars
		}


		
		method constantfold {v} {
			# constantfold transforms the invocation 
			# of a function with constant input parameters
			# into the identity operation
			#
			# Since functions are procs, they can have side-effects
			# or be non-deterministic (like rand())
			# Therefore, only a set of trusted operations is accepted
			
			# careful: not every expression is a proper
			# list; e.g.
			# set a [numarray create {1 2 3}]
			if {[catch {lindex $v 0} cmd]} {
				# the command is not a list
				# return unchanged
				return $v
			}

			if {$cmd in $purefunctions} {
				# check whether the inputs are literals
				# i.e., do not contain brackets
				foreach arg [lrange $v 1 end] {
					# the funny expression matches any brackets []
					if {[regexp {[][]} $arg]} {
						# contains substitution - isn't constant
						return $v
					}
				}
				# evaluate the expression and return
				# with an identity operator
				return [list I [namespace eval ::numarray $v]]
			}
			# return as is
			return $v
		}

		method bracket {v} {
			#puts "Bracketizing $v"
			# create brackets for command substitution
			
			# Perform constant folding. 
			set v [my constantfold $v]
			
			# the cmd may not be a valid list
			# check for identity operator
			if {[catch {lindex $v 0} cmd]==0 && $cmd eq "I"} {
				return [list [lindex $v 1]]
			} else {
				# return bracketized
				return "\[$v\]"
			}
		}

		method Empty args {}
		method Expression-Compound {from to args} {
			foreach {operator arg} [list Empty {*}$args] {
				set operator [my {*}$operator]; set arg [my {*}$arg]
				set value [expr {$operator ne "" ? "$operator [my bracket $value] [my bracket $arg]" : $arg}]
			}
			return $value
		}
		
		method Term {from to args} {
			# first argument might be unary Sign
			set first [lindex $args 0]
			if {[lindex $first 0] eq "Sign"} {
				set sign [my {*}$first]
				set args [lrange $args 1 end]
			} else {
				set sign +
			}

			set value [my Expression-Compound $from $to {*}$args]

			switch $sign {
			    - {
				return "numarray::neg [my bracket $value]"
			      } 
			    ! {
				return "numarray::not [my bracket $value]"
			    }
			    + {
				return $value
			    }
			    default {
				error "Unkown unary operator $sign"
			    }
			}			
		}
		
		variable resultvar

		method Program {from to sequence args} {
			# check arguments
			set opt {-novarrefs 0}; set nopt [dict size $opt]
			foreach arg $args {
			    dict set opt $arg 1
			}
			if {[dict size $opt] != $nopt} { return -code error "Unkown option(s) $args" }

			# first check if we parsed the full program
			# if not, there was an error...
			if {$to+1 < [string length $script]} {
				# Parser error. Most probably near to 
				# errpos. Convert that into line:char number
				set lines [split $script \n]
				set lpos 0
				set linenr 1
				set errchar $errpos
				foreach line $lines {
				    set len [expr {[string length $line]+1}]
				    # length including the terminating \n
				    if {$errchar < $len} {
					set errline $linenr
					break
				    }
				    set errchar [expr {$errchar-$len}]
				    incr linenr
				}
				set errmsg "Error: Parse error in line $linenr:$errchar\n"
				append errmsg $line\n
				append errmsg "[string repeat " " $errchar]^"
				return -code error $errmsg
			}
		    
			# the single arg represents a sequence. 
			# Compile, don't bracketize anymore	
			set body [my {*}$sequence]
			
			# now determine the return value for this  sequence
			# resultvar is set, if a statement stores 
			# an intermediate computation as the result (ListAssignment)
			# if it's not set, the last expression evaluates 
			# to the result
			if {$resultvar ne ""} {
				append body "return \$[list $resultvar]"
			}

			
			# create upvars and prepend to body
			if {![dict get $opt -novarrefs]} {
			    set upvars [my mkupvars]
			} else {
				set upvars {}
			}
			
			return $upvars$body

		}

		method Sequence {from to args} {
			# every arg represents a Statement. Compile everything in sequence

			set resultvar {}
			set body {}
			foreach stmt $args {
				set stmtcompile [my {*}$stmt]
				if {$stmtcompile ne ""} {
					append body "[my constantfold $stmtcompile]\n"
				}
			}

			return $body

		}

		method Statement {from to stmt} {
			# can be expression, assignment, opassignment
			lassign $stmt type
			if {$type eq "Expression"} {
				set resultvar {}
				return [my {*}$stmt]
			} else {	
				# Assignment and OpAssignment 
				return [my {*}$stmt]
			}

		}
		
		method OpAssignment {from to varslice assignop expression} {
			# VarSlice1 VarSlice2 ... Expression
			
			lassign [my analyseslice $varslice] var slice
			set var [my varref $var]
			
			set op [my {*}$assignop]
			set value [my bracket [my {*}$expression]]
			
			if {$slice=={}} {
				# simple assignment
				set resultvar {}
				return "[list {*}$op $var] $value"
			} else {
				# assignment to slice
				set resultvar {}
				return "$op $var [my bracket $slice] $value"
			}
			
		}


		method Assignment {from to args} {
			# VarSlice1 VarSlice2 ... Expression
			
			set expression [lindex $args end]
			set value "[my bracket [my {*}$expression]]"

			if {[llength $args] == 2} {
				# single assignment
				set varslice [lindex $args 0 ]
				lassign [my analyseslice $varslice] var slice
				set var [my varref $var]
				if {$slice=={}} {
					# simple assignment
					set resultvar {}
					return "[list set $var] $value"
				} else {
					# assignment to slice
					set resultvar {}
					return "[list numarray::= $var] [my bracket $slice] $value"
				}
			} else {
				# list assignment
				set assignvars {}
				set slicevars {}
				foreach varslice [lrange $args 0 end-1] {	
					lassign [my analyseslice $varslice] var slice
					set var [my varref $var]
					if {$slice=={}} {
						# simple assignment
						lappend assignvars $var
					} else {
						# assignment to slice
						set temp [my alloctemp]
						lappend assignvars $temp
						lappend slicevars "[list numarray::= $var] [my bracket $slice] \$$temp"
					}
				}
				
				set resultvar [my alloctemp]
				set result "set $resultvar $value"
				append result "\nlassign \$$resultvar $assignvars"
				append result "\n[join $slicevars \n]"

				return $result
				# single assignment
			}
				
		}

		method analyseslice {VarSlice} {
			# return referenced variable and slice expr
			set slices [lassign $VarSlice -> from to Var]
			set var [my VarRef {*}$Var]
			# 
			if {[llength $slices]==0} { return [list $var {}] }
			set lslices list
			foreach slice $slices {
			    append lslices " [my bracket [my {*}$slice]]"
			}
			#puts "Slices evaled $lslices"
			return [list $var $lslices]
		}

		method SliceExpr {from to args} {
			if {[llength $args] == 0} {
				# it is a single :, meaning all
				return "I {0 -1 1}"
			}
			
			# otherwise at max 3 Expressions
			set result {}
			foreach indexpr $args {
				lappend result [my bracket [my {*}$indexpr]]
			}
			    
			# if single number, select a single row/column 
			if {[llength $result]==1} {
				lappend result {*}$result 1
			}

			# if stepsize is omitted, append 1
			if {[llength $result]==2} {
				lappend result 1
			}

			return "list [join $result]"
		}
		
		method RangeExpr {from to args} {
			# 2 or 3 Expressions
			set result {}
			foreach indexpr $args {
				lappend result [my bracket [my {*}$indexpr]]
			}
			    
			# if stepsize is omitted, append 1
			if {[llength $result]==2} {
				lappend result 1
			}

			return $result
		}

	
		method ForLoop {from to Var rangeexpr Sequence} {
		    # parse components
		    set iter [my {*}$rangeexpr]
		    lassign $iter start stop incr
		    set var [my VarRef {*}$Var]
		    set sequence [my {*}$Sequence]
		    # evaluate stop condition only once
		    set stopv [my alloctemp]
		    set body ""
		    append body "set $stopv $stop\n"
		    append body "for {set $var $start} {\$$var <= \$$stopv}  {incr $var $incr} {\n"
		    append body $sequence
		    append body "\n}"
		    return $body
		}
		
		method ForEachLoop {from to Var Expr Sequence} {
		    # parse components
		    set itexpr [my bracket [my {*}$Expr]]
		    set var [my VarRef {*}$Var]
		    set sequence [my {*}$Sequence]
		    
		    # could run a foreach loop
		    # but this is wasteful, it decomposes the expression
		    # into a list
		    set cv [my alloctemp]
		    set length [my alloctemp]
		    set itvar [my alloctemp]
		    set body ""
		    append body "set $itvar $itexpr\n"
		    append body "set $length \[numarray::rows \$$itvar\]\n"
		    append body "for {set $cv 0} {\$$cv < \$$length}  {incr $cv} {\n"
		    append body "set $var \[numarray::getrow \$$itvar \$$cv\]\n"
		    append body $sequence
		    append body "\n}"
		    return $body
		}

		method IfClause {from to Condition Then {Else {Empty}}} {
		    # parse components
		    set cond [my bracket [my {*}$Condition]]
		    set sthen [my {*}$Then]
		    set selse [my {*}$Else]
		    
		    set body ""
		    append body "[list if $cond] {\n"
		    append body $sthen
		    append body "\n}"
		    if {$selse ne ""} {
			append body " else {\n"
			append body $selse
			append body "\n}"
		    }

		    return $body
		}
		
		method WhileLoop {from to Condition Body} {
		    # parse components
		    set cond [my bracket [my {*}$Condition]]
		    set sbody [my {*}$Body]
		    
		    set body ""
		    append body "[list while $cond] {\n"
		    append body $sbody
		    append body "\n}"

		    return $body
		}

		method Literal {from to args} {
			# A complex literal number is used in literal arrays
			return "I [string range $script $from $to]"
		}
		
		method Verbatim {from to args} {
			# A complex literal number is used in literal arrays
			return [list I [string range $script $from $to]]
		}
		
		forward Number    my Verbatim
		forward SignedNumber my Verbatim
		forward ComplexNumber my Verbatim

		forward IndexExpr	my SignedNumber

		forward Expression   my Expression-Compound
		forward AddExpr      my Expression-Compound
		forward RelExpr      my Expression-Compound
		forward BoolAndExpr  my Expression-Compound
		forward BoolOrExpr   my Expression-Compound
		forward Factor       my Expression-Compound
		forward Fragment     my Expression-Compound

		method Expression-Operator {from to args} {
			return [list numarray::[string range $script $from $to]]
		}

		forward OrOp         my Expression-Operator
		forward AndOp        my Expression-Operator
		forward RelOp        my Expression-Operator
		forward AddOp        my Expression-Operator
		forward MulOp        my Expression-Operator
		forward PowOp        my Expression-Operator
		forward AssignOp     my Expression-Operator

		method Var {from to args} {
			list set [string range $script $from $to]
		}

		method VarSlice {from to args} {
			 lassign \
				[my analyseslice [list VarSlice $from $to {*}$args]] \
				var slices

			set var [my varref $var]
			if {$slices eq {}} {
				return [list set $var]
				# alternative:
				# return [list I "\$[list $var]"]
			} else {
				return "numarray::slice \[set [list $var]\] [my bracket $slices]"
			}
		}

		method VarRef {Var from to args} {
			string range $script $from $to
		}

		method FunctionName {from to args} {
			string range $script $from $to
		}
		
		method Sign {from to args} {
			# unary plus or minus, unary bool ! 
			string range $script $from $to
		}
		
		method Function {from to args} {
			# first arg is function name
			# rest is expressions
			set fargs [lrange $args 1 end]
			# first token is function name
			set fname [my {*}[lindex $args 0]]
			
			set result $fname
			foreach arg $fargs {
				append result " [my bracket [my {*}$arg]]"
			}

			return $result
		}

		method Transpose {from to args} {
			lassign $args fragment
			set value [my {*}$fragment]

			if {[llength $args]==1} {
				# just forward to Fragment
				return $value
			} else {
				return "numarray::adjoint [my bracket $value]"
			}
		}

		
	}

	# some expressions to test
	set testscripts {
		{3+4i}
		{a= b}
		{a=b+c+d}
		{A[:, 4] = b*3.0}
		{Q, R = qr(A)}
		{A \ x}
		{b = -sin(A)}
		{c = hstack(A,b)}
		{A += b}
		{b = c[0:1:-2]}
		{2-3}
		{5*(-c)}
		{x, y = list(y,x)}
		{a*b*c}
		{a.^b.^c}
		{-a+b}
		{-a.^b}
		{A={1 2 3}}
		{{{1 2 3} {4 5 6}}}
		{A = ws*3}
	}
	
	proc untok {AST input} {
		set args [lassign $AST type from to]
		set result [list $type [string range $input $from $to]]
		foreach arg $args {
			lappend result [untok $arg $input]
		}
		return $result
	}
	
	proc testcompiler {args} {
		variable testscripts
		variable compiler
		variable parser

		if {[llength $args] != 0} { 
			set scripts $args
		} else {
			set scripts $testscripts
		}
		foreach script $scripts {
			puts " === $script"
			set parsed [$parser parset $script]
			puts [untok $parsed $script]

			if {[catch {$compiler compile $script} compiled]} {
				puts stderr "Error: $compiled"
			} else {
				puts stdout "======Compiled: \n$compiled\n====END==="
			}
		}
	}

	proc mformat {m} {
	    # pretty print a matrix 
	    set d [numarray dimensions $m]
	    switch $d {
		1 { return "{$m}" }
		2 { return "{{[join $m "}\n {"]}}" }
		default { return -code error "Can't format $d-dimensional array"}
	    }
	}

	Init
}

# Additional functions (not implemented in C)
proc numarray::linspace {start stop n} {
	for {set i 0} {$i<$n} {incr i} {
		lappend result [expr {$start+($stop-$start)*double($i)/($n-1)}]
	}
	return $result
}

proc numarray::I {x} { set x }

proc numarray::vstack {args} {
	if {[llength $args]<2} {
		return -code error "vstack arr1 arr2 ?arr3 ...?"
	}

	set args [lassign $args a1 a2]
	set result [numarray concat $a1 $a2 0]
	foreach a $args {
		set result [numarray concat $result $a 0]
	}
	return $result
}

proc numarray::hstack {args} {
	if {[llength $args]<2} {
		return -code error "hstack arr1 arr2 ?arr3 ...?"
	}

	set args [lassign $args a1 a2]
	set result [numarray concat $a1 $a2 1]
	foreach a $args {
		set result [numarray concat $result $a 1]
	}
	return $result
}

proc numarray::min {args} {
	switch -exact [llength $args] {
		0 { return -code error "min arr1 ?arr2 ...?" }
		1 { return [axismin [lindex $args 0] 0] }
		default {
			set args [lassign $args a1 a2]
			set result [numarray binarymin $a1 $a2]
			foreach a $args {
				set result [numarray binarymin $result $a]
			}
			return $result
		}
	}
}

proc numarray::max {args} {
	switch -exact [llength $args] {
		0 { return -code error "max arr1 ?arr2 ...?" }
		1 { return [axismax [lindex $args 0] 0] }
		default {
			set args [lassign $args a1 a2]
			set result [numarray binarymax $a1 $a2]
			foreach a $args {
				set result [numarray binarymax $result $a]
			}
			return $result
		}
	}
}

proc numarray::cols {narray} {
	set c [lindex [numarray::shape $narray] 1]
	# case of a columnvector: return 1 row
	if {$c eq {}} {
		return 1
	} else {
		return $c
	}
}

proc numarray::rows {narray} {
	lindex [numarray::shape $narray] 0
}

proc numarray::inv {matrix} {
    # compute the inverse for a square matrix
    set dim [numarray::dimensions $matrix]
    if {[numarray dimensions $matrix]!=2} {
	return -code error "inverse defined for rank-2 only"
    }
    lassign [numarray::shape $matrix] m n
    if {$m!=$n} {
	return -code error "inverse: matrix must be square"
    }
    vexpr {matrix \ eye(n)}
}

namespace eval numarray {
    # Tcl stub implementations for new features
    interp alias {} ::numarray::getrow {} lindex
}
