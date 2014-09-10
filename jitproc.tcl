lappend auto_path .
package require vectcl

namespace eval vectcl {
	namespace export jit

	proc jit {arg xprstring} {
		variable jitcompiler
		variable parser
		if {![info exists jitcompiler]} {
			set jitcompiler [CompileJIT new $parser]
		}
		$jitcompiler compile $xprstring -args $arg
	}

	oo::class create CompileJIT {
		variable tokens script varrefs tempcount parser
		variable purefunctions
		variable errpos
		variable compileexpression

		constructor {p} {
			set parser $p
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

			
		variable resultvar

		method Program {from to sequence args} {
			# check arguments
			set opt {-args {}}; set nopt [dict size $opt]
			foreach {arg val} $args {
			    dict set opt $arg $val
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
			set compileexpression [vectcl::CompileVectorExpr new]
			# add formal arguments to symbol table
			foreach arg [dict get $opt -args] {
				$compileexpression addsymbol $arg [list Argument $arg]
			}
			lassign [my {*}$sequence] rvar tac
			append body "[join $tac "\n"]\n"
			append body "Tcl_SetObjResult($rvar)\n"
			append body "return TCL_OK; \n"
			
			
			set lit [$compileexpression getliterals]
			if {$lit != {}} {
				append body "Literals: \n"
				dict for {lit val} $lit {
					append body "$lit $val\n"
				}
			}
			$compileexpression destroy
			return $body

		}

		method Sequence {from to args} {
			# every arg represents a Statement. Compile everything in sequence

			set resultvar {}
			set resultcode {}
			foreach stmt $args {
				lassign [my {*}$stmt] rvar code
				lappend resultcode {*}$code
				if {($rvar ne {}) && ($code ne {})} {
					# ignore empty result for empty statements
					set resultvar $rvar
				}
			}

			return [list $resultvar $resultcode]

		}

		method Statement {from to stmt} {
			# can be expression, assignment, opassignment
			lassign $stmt type
			if {$type eq "Expression"} {
				set resultvar {}
				return [$compileexpression compile $script $stmt]
			} else {	
				# Assignment and OpAssignment
				return [my {*}$stmt]
			}

		}
		
		method Empty {from to args} {
			list {} {}
		}

		method OpAssignment {from to varslice assignop expression} {
			# VarSlice1 VarSlice2 ... Expression
			error "Combined assignment not supported"			
		}


		method Assignment {from to args} {
			# VarSlice1 VarSlice2 ... Expression
			set expression [lindex $args end]
			lassign [$compileexpression compile $script $expression] value resultcode

			if {[llength $args] == 2} {
				# single assignment
				set varslice [lindex $args 0 ]
				lassign [$compileexpression analyseslice $varslice] var slice
				if {$slice=={}} {
					# simple assignment
					set vsym [list Variable $var]
					$compileexpression addsymbol $var $vsym
					lappend resultcode [list Phi $vsym $value]
					return [list $value $resultcode]
				} else {
					# assignment to slice
					error "Slice assignment not supported"
				}
			} else {
				# list assignment
				error "List assignment not supported"
				return $result
			}
				
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
			error "For loop not supported"
		    return $body
		}
		
		method ForEachLoop {from to Var Expr Sequence} {
		    # parse components
			error "ForEach loop not supported"
		}

		method IfClause {from to Condition Then {Else {Empty}}} {
		    # parse components
			error "If clause not supported"
		}
		
		method WhileLoop {from to Condition Body} {
		    # parse components
			error "while loop not supported"
		 }
	}

	oo::class create CompileVectorExpr {
		
		variable script
		
		method compile {xpr ast} {
			set script $xpr
			set TAC [my {*}$ast]
			return $TAC
		}
		
		variable literals
		variable lcount
		variable symboltable

		constructor {} {
			set vartable {}
			set vcount 0
			set literals {}
			set lcount 0
			set symboltable {}
		}

		variable vartable
		variable vcount
		method allocvar {{vname {}}} {
			if {[dict exists $vartable $vname]} {
				return [dict get $vartable $vname]
			} else {
				incr vcount
				set tname [list Tempvar $vcount]
				if {$vname != {}} {
					dict set vartable $vname $tname
				}
				return $tname
			}
		}

		method allocliteral {value} {
			incr lcount
			dict set literals $lcount $value
			list Literal $lcount
		}

		method getliterals {} {
			return $literals
		}

		method addsymbol {sym val} {
			dict set symboltable $sym $val
		}

		method getsymbol {sym} {
			if {[catch {dict get $symboltable $sym} val]} {
				return -code error "Undefined variable $sym"
			}
			return $val
		}

		method Empty args {}
		method Expression-Compound {from to args} {
			#set resultcode {}
			#set resultvar {}
			
			# args contains: op1 x op2 x op3 ...
			set args [lassign $args firstarg]
			lassign [my {*}$firstarg] resultvar resultcode

			foreach {operator arg} $args {
				set operator [my {*}$operator]
				lassign [my {*}$arg] rvar2 code

				lappend resultcode {*}$code

				set rvar [my allocvar]
				lappend resultcode [list $operator $rvar $resultvar $rvar2]
				set resultvar $rvar

			}
			
			return [list $resultvar $resultcode]
		}
		
		method Term {from to args} {
			# first argument might be unary Sign
			set first [lindex $args 0]
			if {[lindex $first 0] eq "Sign"} {
				error "Sign not implemented"
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
		

		forward Number    my Literal
		forward SignedNumber my Literal
		forward ComplexNumber my Literal

		forward IndexExpr	my SignedNumber

		forward Expression   my Expression-Compound
		forward AddExpr      my Expression-Compound
		forward RelExpr      my Expression-Compound
		forward BoolAndExpr  my Expression-Compound
		forward BoolOrExpr   my Expression-Compound
		forward Factor       my Expression-Compound
		forward Fragment     my Expression-Compound

		method Expression-Operator {from to args} {
			return [string range $script $from $to]
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
		
		method analyseslice {VarSlice} {
			# return referenced variable and slice expr
			set slices [lassign $VarSlice -> from to Var]
			set var [my VarRef {*}$Var]
			# 
			if {[llength $slices]==0} { return [list $var {}] }
			
			foreach slice $slices {
			    error "Do something with $slice"
			}
			#puts "Slices evaled $lslices"
			return [list $var $lslices]
		}


		method VarSlice {from to args} {
			 lassign \
				[my analyseslice [list VarSlice $from $to {*}$args]] \
				var slices

			# set var [my varref $var]
			#
			set var [my getsymbol $var]
			if {$slices eq {}} {
				return [list $var {}]
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

		method Literal {from to args} {
			# create a new literal in the literal table
			set lit [my allocliteral [string range $script $from $to]]
			return [list $lit {}]
		}
		
		method Function {from to args} {
			# first arg is function name
			# rest is expressions
			set fargs [lrange $args 1 end]
			# first token is function name
			set fname [my {*}[lindex $args 0]]
			
			# result will be the TAC 
			set resultcode {}
			set argvars {}
			foreach arg $fargs {
				lassign [my {*}$arg] rvar code
				lappend resultcode {*}$code
				lappend argvars $rvar
			}
			
			set cmd [list CALL $fname]
			set resultvar [my allocvar]
			lappend resultcode [list $cmd $resultvar {*}$argvars]
			return [list $resultvar $resultcode]
		}

		method Transpose {from to args} {
			lassign $args fragment
			set value [my {*}$fragment]

			if {[llength $args]==1} {
				# just forward to Fragment
				return $value
			} else {
				lassign $value rvar code
				set resultvar [my allocvar]
				lappend code [list adjoint $resultvar $rvar]
				return [list $resultvar $code]
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
}
	
