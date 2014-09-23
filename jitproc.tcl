lappend auto_path .
package require vectcl

namespace eval vectcl {
	namespace export jit

	proc jitproc {arg xprstring} {
		set ccode [jitcompile $arg $xprstring]
		# now call tcc
		# tcc4tcl::ccode $ccode
	}

	proc jitcompile {arg xprstring} {
		variable parser
		if {![info exists jitcompiler]} {
			set jitcompiler [CompileJIT new $parser]
		}
		set ccode [$jitcompiler compile $xprstring -args $arg]
		return $ccode
	}

	oo::class create CompileJIT {
		variable tokens script varrefs tempcount parser
		variable purefunctions
		variable errpos

		variable debugsymbols

		constructor {p} {
			set parser $p
		}
		
		method compile {script_ args} {
			# Instantiate the parser
			set script $script_
			set varrefs {}
			set debugsymbols {}
			set tempcount 0
			
			set vartable {}
			set vcount 0
			
			set literals {}
			set lcount 0

			set symboltable {}
		
			set ast [$parser parset $script]
			set errpos [$parser errpos]
			# first pass: generate three-address code (SSA)
			set TAC [my {*}$ast {*}$args]
			
			# optimization
			set TAC_opt [my optimize {*}$TAC]

			# type inference
			set TAC_annot [my infer_type {*}$TAC_opt]
			
			# code generation
			set ccode [my codegen {*}$TAC_annot]
			
			# print literals and symbols for debug purposes
			set dbgout {}
			if {$literals != {}} {
				append dbgout "Literals: \n"
				dict for {lit val} $literals {
					append dbgout "$lit $val\n"
				}
			}
			
			if {$debugsymbols != {}} {
				append dbgout "Symbols: \n"
				dict for {val sym} $debugsymbols {
					append dbgout "$sym $val\n"
				}
			}

			puts stderr $dbgout
	
			return $ccode

		}
		
		method optimize {rvar tac} {
			# optimize away assigned temporaries
			# which are not arguments to phi functions
			
			# first record all variables on the right side of 
			# an assignment, count the usages
			for {set iter 0} {$iter<10} {incr iter} {
				set usecount [dict create $rvar 1]
				set assignments {}
				foreach instr $tac {
					set srcops [lassign $instr opcode dest]
					foreach op $srcops { dict incr usecount $op }
					if {$opcode eq "="} {
						dict set assignments $op $dest
					}
				}

				# find all variables that are used only once, 
				# in an assignment to another variable.
				set singleuse [dict filter $assignments script {src dest} {
					expr {[dict get $usecount $src] == 1}
				}]
				
				set nosubst true

				# move the code that creates those
				# to replace the assignment
				set resultcode {}
				set assignmap {}
				foreach instr $tac {
					set srcops [lassign $instr opcode dest]
					if {$opcode eq "="} {
						# maybe replace this assignment 
						if {[dict exists $assignmap $dest]} {
							lappend resultcode [dict get $assignmap $dest]
							set nosubst false
							continue
						}	
					}

					if {[dict exists $singleuse $dest]} {
						# this is a write instruction,
						# which should be replaced
						set assigndest [dict get $singleuse $dest]
						dict set assignmap $assigndest [list $opcode $assigndest {*}$srcops]
						continue
					}

					lappend resultcode $instr
				}

				set tac $resultcode
				if {$nosubst} { break }
			}

			if {$iter>=10} {
				puts stderr [join $tac \n]
				puts stderr $singleuse
				puts stderr $assignmap
			}	
			list $rvar $resultcode
		}
		
		method infer_type {rvar tac} { list $rvar $tac }
		
		method codegen {rvar tac} {
			# generate C code - for now just print three address code
			set code [join $tac \n]
			append code "\nTcl_SetObjResult(interp, $rvar)\n"
		}

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
			# add formal arguments to symbol table
			set resultcode {}
			foreach arg [dict get $opt -args] {
				lassign [my assignvar $arg [list Argument $arg]] tvar tcode
				lappend resultcode $tcode
			}
			
			lassign [my {*}$sequence] rvar tac
			lappend resultcode {*}$tac
		
			return [list $rvar $resultcode]

		}

		method Sequence {from to args} {
			# every arg represents a Statement. Compile everything in sequence

			set resultvar {}
			set resultcode {}
			foreach stmt $args {
				lassign [my {*}$stmt] rvar code
				lappend resultcode {*}$code
				#puts "Code $code rvar $rvar"
				if {($rvar ne {}) || ($code ne {})} {
					# ignore empty result for empty statements
					set resultvar $rvar
				}
			}

			return [list $resultvar $resultcode]

		}

		method Statement {from to stmt} {
			# can be expression, assignment, opassignment
			lassign $stmt type
			return [my  {*}$stmt]
		}
		
		method Empty {from to args} {
			list {} {}
		}

		method OpAssignment {from to varslice assignop expression} {
			# VarSlice1 VarSlice2 ... Expression
			error "Combined assignment not supported"			
		}

		method assignsym {sym value} {
			my addsymbol $sym $value
			dict set debugsymbols $value $sym
		}

		method assignvar {sym value} {
			set tvar [my allocvar]
			set code [list = $tvar $value]
			my assignsym $sym $tvar
			return [list $tvar $code]
		}

		method Assignment {from to args} {
			# VarSlice1 VarSlice2 ... Expression
			set expression [lindex $args end]
			lassign [my {*}$expression] rvar resultcode

			if {[llength $args] == 2} {
				# single assignment
				set varslice [lindex $args 0 ]
				lassign [my analyseslice $varslice] var slice
				if {$slice=={}} {
					# simple assignment - insert result var of expression
					# into the symboltable
					lassign [my assignvar $var $rvar] tvar code
					lappend resultcode $code
					return [list $tvar $resultcode]
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
		
		# support for Phi function placement
		method PhiIntersect {stbl1 stbl2} {
			# compute phi functions as an intersection 
			# of the symbol tables at point1 and point2
			# TODO extend for intersection of n codepaths
			set phicode {}
			dict for {sym val1} $stbl1 {
				if {[dict exists $stbl2 $sym]} {
					set val2 [dict get $stbl2 $sym]
					if {$val1 ne $val2} {
						# create new temporary for this variable
						set phiv [my allocvar]
						lappend phicode [list Phi $phiv $val1 $val2]
						my assignsym $sym $phiv
					}
				}
			}
			return $phicode
		}


		method varmap {map code} {
			set result {}
			foreach instr $code {
				set tail [lassign $instr f rvar]
				set instrmap [list $f $rvar]
				foreach arg $tail {
					if {[dict exists $map $arg]} {
						lappend instrmap [dict get $map $arg]
					} else {
						lappend instrmap $arg
					}
				}
				lappend result $instrmap
			}
			return $result
		}

		method SetSymbols {symtable} {
			set symboltable $symtable
		}
	
		method GetSymbols {} {
			return $symboltable
		}
	
		method ForEachLoop {from to Var Expr Sequence} {
		    # parse components
			error "ForEach loop not supported"
		}

		method IfClause {from to Condition Then {Else {Empty}}} {
		    # parse components
			lassign [my {*}$Condition] cvar ccode
			set stbl1 [my GetSymbols]

			lassign [my {*}$Then] tvar tcode
			set stbl2 [my GetSymbols]
			
			set ecode {}
			if {$Else ne "Empty"} {
				# restore symbol table from block befor if
				my SetSymbols $stbl1
				lassign [my {*}$Else] evar ecode
				set stbl1 [my GetSymbols]
			}

			set resultcode $ccode
			lappend resultcode [list If {} $cvar]
			lappend resultcode {*}$tcode
			
			if {$ecode ne {}} {
				lappend resultcode [list Else {} {}]
				lappend resultcode {*}$ecode
			}

			lappend resultcode [list EndIf {} {}]
			
			set phicode [my PhiIntersect $stbl1 $stbl2]
			lappend resultcode {*}$phicode
			
			list {} $resultcode
		}
		
		method WhileLoop {from to Condition Body} {
		    # parse components
			lassign [my {*}$Condition] cvar ccode
			set stbl1 [my GetSymbols]

			lassign [my {*}$Body] bvar bcode
			set stbl2 [my GetSymbols]
			
			set phicode [my PhiIntersect $stbl1 $stbl2]
			
			# the phi nodes are placed in the header
			# of the loop; thus we must replace all occurences 
			# of the symbol in stbl1 int the body of the loop
			
			foreach phi $phicode {
				lassign $phi Phi newcurr oldcurr 
				# bad: This code knows that the 1st operand is from stbl1
				dict set phimap $oldcurr $newcurr
			}

			set ccode [my varmap $phimap $ccode]
			set bcode [my varmap $phimap $bcode]

			set resultcode {{While {} {}}}
			lappend resultcode {*}$phicode
			lappend resultcode {*}$ccode
			lappend resultcode [list Do {} $cvar]
			lappend resultcode {*}$bcode
			lappend resultcode {EndWhile {} {}}

			return [list {} $resultcode]
		}

		method ForLoop {from to Var rangeexpr Sequence} {
		    # parse components
			error "For loop not supported"
		    return $body
		}
		
		variable literals
		variable lcount
		variable symboltable

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

		method addsymbol {sym val} {
			dict set symboltable $sym $val
		}

		method getsymbol {sym} {
			if {[catch {dict get $symboltable $sym} val]} {
				return -code error "Undefined variable $sym"
			}
			return $val
		}

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

	proc testjit {} {
		set code [vectcl::jitproc N	{
			i=0
			while N != 1 {
				if (N%2 == 1) {
					N=3*N+1
				} else {
					N=N/2
				}
				i=i+1
			}
			i
		}]
		puts $code


		set code [vectcl::jitproc {xv yv}	{
			xm=mean(xv); ym=mean(yv)
			beta=sum((xv-xm).*(yv-ym))./sum((xv-xm).^2)
			alpha=ym-beta*xm
			list(alpha, beta)
		}]
		puts $code
	}

}
	