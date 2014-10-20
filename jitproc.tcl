lappend auto_path .
package require vectcl

namespace eval vectcl {
	namespace export jit

	proc jitproc {arg xprstring} {
		set ccode [jitcompile $arg $xprstring]
		# now call tcc in some distant future
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
		
		method dict_get_default {dict arg default} {
			if {[dict exists $dict $arg]} {
				return [dict get $dict $arg]
			} else {
				return $default
			}
		}

		variable typetable
		variable signatures

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

			set typetable {}

			# store common signatures as a lookup table
			# these functions are also pure and can be constant-folded
			# or optimized away in dead-code elimination
			#
			# TODO % and .^ are a lie - % only works on integer 
			# and .^ upcasts to float
			set signatures {
				+ binary
				- binary
				.* binary
				./ binary
				.^ binary
				% binary
				> relational
				< relational
				== relational
				!= relational
				>= relational
				<= relational
				{CALL sum} reduction
				{CALL mean} reduction
				{CALL std} reduction
				{CALL sin} unary-float
				{CALL cos} unary-float
				{CALL tan} unary-float
				{CALL exp} unary-float
				{CALL log} unary-float
			}
		
			set ast [$parser parset $script]
			set errpos [$parser errpos]
			# first pass: generate three-address code (SSA)
			set TAC [my {*}$ast {*}$args]
			
			# optimization
			set TAC_opt [my optimize {*}$TAC]

			# type inference
			set TAC_annot [my infer_type {*}$TAC_opt]
			
			set TAC_bloop [my infer_basic_loop {*}$TAC_annot]
			# code generation
			set ccode [my codegen {*}$TAC_bloop]
			
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
			
			if {$typetable != {}} {
				append dbgout "Types: \n"
				dict for {sym type} $typetable {
					append dbgout "$sym $type\n"
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
		
		method infer_type {rvar tac} {
			# first deduce type of all literals
			dict for {sym value} $literals {
				set info [numarray info $value]
				set type [lindex {int double complex} [dict get $info type]]
				set shape [dict get $info dimensions]
				dict set typetable $sym [list $type $shape]
			}

			for {set link 0} {$link<10} {incr link} {
				puts stderr "Link stage $link"
				set finish true
				foreach instr $tac {
					set args [lassign $instr opcode dest]
					
					if {[catch {my type_on_instr $opcode $args} resulttype]} {
						puts stderr "Warning: Aliasing incompatible types $instr"
						dict set typetable $dest $resulttype
					}

					if {![dict exists $typetable $dest] || \
						([dict get $typetable $dest] ne $resulttype)} {
						dict set typetable $dest $resulttype
						set finish false
					}
				}

				if {$finish} { break }
			}
			list $rvar $tac 
		}
		
		method type_on_instr {opcode arguments} {
			set argtypes [lmap arg $arguments {my dict_get_default $typetable $arg undefined}]
			set opcode [my dict_get_default $signatures $opcode $opcode]
			# compute a common type, if possible
			lassign $argtypes t1 t2
			lassign $t1 type1 shape1
			lassign $t2 type2 shape2

			switch -exact -- $opcode {
				Phi {
					puts "$t1 - $t2"

					if {$t1 eq {undefined}} {
						return $t2
					}

					if {$t2 eq {undefined}} {
						return $t1
					}

					# here both t1 and t2 are defined types
					if {$t1 ne $t2} {
						# might be a contradiction. They can still
						# be compatible, if the number of dimensions agree
						# (e.g. {n} == {m}
						if {($type1 eq $type2 ) && ([llength $shape1] == [llength $shape2])} {
							puts stderr "Compatible types $t1 - $t2"
							return $t1
						}
						puts stderr "Warning: incompatible types $t1 - $t2"
						return -code error Any
					} else {
						return $t1
					}
				}
				= {
					return $t1
				}
				
				* { 
					set type [my upcast $type1 $type2]
					if {$type eq "Any"} { return Any }
					set shape [list {*}$shape1 {*}[lrange $shape2 1 end]]
					return [list $type $shape]
				}
					
				/ { 
					set type [my upcast $type1 $type2]
					if {$type eq "Any"} { return Any }
					if {$shape1 eq {1} && $shape2 eq {1}} { 
						# scalar division
						set shape 1
					} else {
						error "Non-scalar right division not implemented"
					}
					return [list $type $shape]
				}
				
				\\ { 
					set type [my upcast $type1 $type2]
					set type [my upcast $type double] ;# int is always promoted to float
					if {$type eq "Any"} { return Any }
					set shape [list {*}[lrange $shape1 1 end] {*}[lrange $shape2 1 end]] 
					if {$shape eq {}} { set shape 1 } ;# scalar reverse division
					return [list $type $shape]
				}

				binary {
					set type [my upcast $type1 $type2]
					if {$type eq "Any"} { return Any }

					# perform shape and type promotion
					if {$shape1 eq 1} { 
						set shape $shape2
					} elseif {$shape2 eq 1} { 
						set shape $shape1 
					} else {
						# dimensions must match
						if {[llength $shape1] != [llength $shape2]} {
							return Any
							# don't signal the mismatch here, but we could
							error "Shape mismatch"
						} else {
							set shape $shape1\
						}
					}

					return [list $type $shape]
				}

				relational {
					if {$type1 eq "Any" || $type2 eq "Any"} {
						return Any
					}

					# perform shape promotion
					if {$shape1 eq 1} { 
						set shape $shape2
					} elseif {$shape2 eq 1} { 
						set shape $shape1 
					} else {
						# dimensions must match
						if {[llength $shape1] != [llength $shape2]} {
							return Any
							# don't signal the mismatch here, but we could
							error "Shape mismatch"
						} else {
							set shape $shape1\
						}
					}

					return [list int $shape]
				}
				
				unary-float {
					lassign $t1 type shape
					if {$type eq "complex" || $type eq "double"} { return $t1 }
					if {$type eq "int"} { return [list double $shape] }
					return Any

				}
				reduction {
					# second argument is dimension
					# doesn't work, ignore for now
					lassign $t1 type1 shape1
					if {[llength $argtypes] == 1} {
						set dim 0
					} else {
						set dim [lindex $arguments 1]
					}
					if {$type1 eq "Any"} { return Any }
					switch [llength $shape1] {
						0 { error "Undefined shape" }
						
						1 { 
							set n [lindex $shape1 0]
							if {$n == 0} { return Any }; # empty
							if {$n == 1} { return $t1 }; # scalar
							return [list $type1 {1}]; # vector
						}
						2 {
							return [list $type1 {n}]
						}
						default {
							# for more than two dimensions cut one off
							return [list $type1 [lrange $shape1 0 end-1]]
						}
					}
				}

				default {
					return Any
				}
			}
		}

		method upcast {t1 t2} {
			foreach type {Any complex double int} {
				if {$t1 == $type || $t2 == $type} {
					return $type
				}
			}

			error "Don't know how to cast $t1, $t2"
			
		}
		
		# a basic loop is represented as a dictionary
		# input: dict/set of input variables
		# output: output variable
		# code: list of SSA instructions
		method fuse_basic_loops {b1 b2} {
			# take two basic loops and
			# create one out of them
			
			# unpack structures
			set b1in [dict get $b1 input]
			set b2in [dict get $b2 input]
			set b1out [dict get $b1 output]
			set b2out [dict get $b2 output]
			set b1code [dict get $b1 code]
			set b2code [dict get $b2 code]

			# input = both inputs minus output from the first block
			set bin [dict merge $b1in $b2in]
			dict unset bin $b1out

			set b [dict create \
				input $bin \
				output $b2out \
				code [list {*}$b1code {*}$b2code] \
				]

			return $b
			
		}

		method b_from_SSA {instr} {
			# convert a single instruction into the equivalent basic loop
			foreach input [lassign $instr opcode dest] {
				dict set inp $input 1
			}
			dict set b input $inp
			dict set b output $dest
			dict set b code $instr
		}

		method trivial_basic_loop {tac} {
			# turn any instruction into a basic loop
			# except for control constructs
			set result {}
			foreach opcode {If Else Endif While Do EndWhile Phi} {
				dict set instr_class $opcode control
			}
			
			foreach opcode {= + - .+ .- .* ./ .^ % == < > <= >= !=} {
				dict set instr_class $opcode bloop
			}
			
			foreach opcode {sin cos tan exp log} {
				dict set instr_class [list CALL $opcode] bloop
			}
			
			foreach opcode {sum mean std} {
				dict set instr_class [list CALL $opcode] reduction
			}
			
			set ip 0
			foreach instr $tac {
				lassign $instr opcode dest
				set loop [my b_from_SSA $instr]

				set type [my dict_get_default $instr_class $opcode call]
				dict set result $ip type $type
				dict set result $ip loop $loop
				incr ip
			}
			return $result
		}


		method infer_basic_loop {rvar tac} {
			# try to move around instructions such that 
			# we can infer the basic loops
			set tac_bloop [my trivial_basic_loop $tac]
			# now create usage tree of variables
			set usage [dict create $rvar "return"]
			dict for {ip loop} $tac_bloop {
				dict for {var _} [dict get $loop loop input] {
					dict lappend usage $var $ip
				}
			}
			# now go over the code and combine 
			# - bloops with bloops
			# - bloops with reductions
			while true {
				set ips [dict keys $tac_bloop]
				# not: dict for {ip loop} $tac_bloop 
				# because we are modifying tac_bloop downstream
				foreach ip $ips {
					if {![dict exists $tac_bloop $ip]} { continue }
					
					set nochange true
					set loop [dict get $tac_bloop $ip]
					set type [dict get $loop type]
					if {$type ne "bloop"} { continue }

					set output [dict get $loop loop output]
					set uses [my dict_get_default $usage $output {}]
					switch [llength $uses] {
						0 { 
							# dead code, remove
							# puts "Dead code: $ip"
							dict unset tac_bloop $ip
							set nochange false
						}

						1 {
							# only a single use - combine
							set useip [lindex $uses 0]
							if {$useip eq "return"} { continue }
							set useloop [dict get $tac_bloop $useip loop]
							set usetype [dict get $tac_bloop $useip type]

							if {$usetype ne "bloop"} { continue }
							# don't do reductions yet
							#puts "Loop fusion: $ip $useip"
							set loop1 [dict get $loop loop]
							set combined_loop [my fuse_basic_loops $loop1 $useloop]
							#puts "$loop1 + $useloop"
							#puts "->$combined_loop"
							dict unset tac_bloop $ip
							dict set tac_bloop $useip loop $combined_loop
							dict unset usages $output
							set nochange false
						}

						default {
							# multiple uses - leave. Could later decide 
							# for inlining
						}
					}
				}

				if {$nochange} { break }
			}

			return [list $rvar $tac_bloop]
		}


		method codegen {rvar tac} {
			# generate C code - for now just print three address code
			set code [join $tac \n]
			append code "\nTcl_SetObjResult(interp, $rvar)\n"
		}

		#######################################
		# machinery to traverse the parse tree
		#######################################
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
				lassign $arg name type
				set symbol [list Argument $name]
				dict set typetable $symbol $type
				lassign [my assignvar $name $symbol] tvar tcode
				lappend resultcode $tcode
			}
			puts "Typetable $typetable"
			
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
			set symbol [list Literal $lcount]
			dict set literals $symbol $value
			return $symbol
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
		set code [vectcl::jitproc {{N {int 1}}}	{
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


		set code [vectcl::jitproc {{xv {double n}} {yv {double n}}}	{
			xm=mean(xv); ym=mean(yv)
			beta=sum((xv-xm).*(yv-ym))./sum((xv-xm).^2)
			alpha=ym-beta*xm
			list(alpha, beta)
		}]
		puts $code
		set code [vectcl::jitproc {{xv {double n}} {yv {double n}}}	{
			xv.*xv+yv.*yv
		}]
		puts $code
	}

}
	
