lappend auto_path . ~/Library/Tcl/
package require vectcl
package require tcc4tcl

namespace eval vectcl {
	namespace export jit
	
	set ipath [file join [file dirname [info script]] generic]

	proc dict_retrieve {dict args} {
		set keys [lrange $args 0 end-1]
		upvar 1 [lindex $args end] var
		if {[dict exists $dict {*}$keys]} {
			set var [dict get $dict {*}$keys]
			return true
		} else {
			return false
		}
	}

	proc jitproc {name arg xprstring} {
		variable parser
		if {![info exists jitcompiler]} {
			set jitcompiler [CompileJIT new $parser]
		}
		$jitcompiler compile $xprstring -args $arg -name $name
		return
	}

	oo::class create CompileJIT {
		variable tokens script varrefs tempcount parser
		variable purefunctions
		variable errpos

		variable debugsymbols

		constructor {p} {
			set parser $p
			my init_instr_class
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
		variable opts
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

			# check arguments
			set opts {-args {} -name {compiledproc}}; set nopts [dict size $opts]
			foreach {arg val} $args {
			    dict set opts $arg $val
			}
			if {[dict size $opts] != $nopts} { return -code error "Unkown option(s) $args" }

			# store common signatures as a lookup table
			# these functions are also pure and can be constant-folded
			# or optimized away in dead-code elimination
			#
			# TODO % and .^ are a lie - % only works on integer 
			# and .^ really upcasts to float
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
			# parse optional arguments

			# first pass: generate three-address code (SSA)
			set TAC [my {*}$ast]
			
			puts "SSA code "
			puts "[lindex $TAC 0] [join [lindex $TAC 1] \n]"	
			# optimization
			set TAC_opt [my optimize {*}$TAC]
			puts "SSA code "
			puts "[lindex $TAC_opt 0] [join [lindex $TAC_opt 1] \n]"	

			# type inference
			set TAC_annot [my infer_type {*}$TAC_opt]
			
			puts "Typetable $typetable"
			puts "SSA code "
			puts "[lindex $TAC_annot 0] [join [lindex $TAC_annot 1] \n]"	
			
			set TAC_bloop [my infer_basic_loop {*}$TAC_annot]
			# code generation
			set TAC_dephi [my dephi {*}$TAC_bloop]
			# code generation
			set ccode [my codegen {*}$TAC_dephi]
			
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
			
			# last stage: compile resulting C code and link

			my machinecodegen $ccode
			return $ccode
		}

		method machinecodegen {ccode} {
			puts "Compiling C code: "
			puts $ccode
			# rework literal table into list form suitable for 
			# the object command
			set litcdata [dict values $literals] ;# as easy?
			puts "Literals: $litcdata"
			# now call tcc
			if {[catch {
				set handle [tcc4tcl::new]
				$handle add_include_path $vectcl::ipath
				$handle ccode "#include <vectclInt.h>"
				$handle ccode $ccode
				$handle linktclcommand [dict get $opts -name] $cname $litcdata
				$handle go
			} err]} { puts stderr "Error calling tcc: $err" }
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
		
		method iscomplex {sym} {
			my iscomplextype [dict get $typetable $sym]
		}

		method isscalar {sym} {
			my isscalartype [dict get $typetable $sym]
		}

		method iscomplextype {stype} {
			set type [lindex $stype 0]
			expr {$type eq "complex"}
		}

		method isscalartype {stype} {
			lassign $stype type shape
			expr {$shape == 1}
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
					if {$shape1 == 1} { 
						set shape $shape2
					} elseif {$shape2 == 1} {
						set shape $shape1
					} else {
						set shape [list {*}$shape1 {*}[lrange $shape2 1 end]]
					}
					return [list $type $shape]
				}
					
				/ { 
					set type [my upcast $type1 $type2]
					if {$type eq "Any"} { return Any }
					if {$shape1 == 1} { 
						set shape $shape2
					} elseif {$shape2 == 1} {
						set shape $shape1
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
		
		method reduce2scalar {symbol} {
			lassign [dict get $typetable $symbol] dtype shape
			if {$dtype ne "Any"} {
				set shape {1}
			}
			dict set typetable $symbol [list $dtype $shape]
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
			set temp [dict merge [dict get $b1 temp] [dict get $b2 temp]]
			dict set temp $b1out 1
			# reduce the temporary variable to scalar
			my reduce2scalar $b1out

			set b [dict create \
				input $bin \
				output $b2out \
				code [list {*}$b1code {*}$b2code] \
				type [dict get $b2 type] \
				temp $temp
				]

			return $b
			
		}
		
		variable instr_class
		
		method init_instr_class {} {
			foreach opcode {If Else EndIf While Do EndWhile} {
				dict set instr_class $opcode control
			}

			dict set instr_class Phi phi
			
			foreach opcode {+ - .+ .- .* ./ .^ % == < > <= >= !=} {
				dict set instr_class $opcode bloop
			}

			dict set instr_class = copy
			
			foreach opcode {* / \\} {
				dict set instr_class $opcode matrixmult
			}
			
			foreach opcode {sin cos tan exp log} {
				dict set instr_class [list CALL $opcode] bloop
			}
			
			foreach opcode {sum mean std} {
				dict set instr_class [list CALL $opcode] reduction
			}
		}

		method isargument {symbol} {
			expr {[lindex $symbol 0] eq "Argument"}
		}

		method b_from_SSA {instr} {
			# convert a single instruction into the equivalent basic loop
			
			foreach input [lassign $instr opcode dest] {
				dict set inp $input 1
			}

			set type [my dict_get_default $instr_class $opcode call]

			# in case of matrix multiplication op,
			# reduce to bloop if one of the operands is scalar
			if {$type eq "matrixmult"} {
				lassign $instr _ _ op1 op2
				# puts "$instr\n$inp"
				if {[my isscalar $op1] || [my isscalar $op2]} {
					# it is not a true matrix op
					# and can be converted into an equivalent bloop
					set opcode .$opcode
					lset instr 0 $opcode
					set type bloop
				} else {
					# true matrix op
					set type call
				}
			}
			
			if {$type eq "copy"} {
				# convert into a load instruction,
				# if it merely stores an argument
				# otherwise, convert to bloop
				lassign $instr _ dest src
				if {[my isargument $src]} {
					set type load
				} else {
					set type bloop
				}
			}
			
			dict set b input $inp
			dict set b output $dest
			dict set b code [list $instr]
			dict set b temp {}
			dict set b type $type
		}

		method trivial_basic_loop {tac} {
			# turn any instruction into a basic loop
			# except for control constructs
			set result {}
		
			set ip 0
			foreach instr $tac {
				set loop [my b_from_SSA $instr]
	
				dict set result $ip $loop
				incr ip
			}
			return $result
		}
		
		# usage and definition are dicts that map tempvars
		# to the IPs in the code which uses them
		variable usage
		variable definition
		method infer_basic_loop {rvar tac} {
			# try to move around instructions such that 
			# we can infer the basic loops
			set tac_bloop [my trivial_basic_loop $tac]
			# now create usage tree of variables
			set usage [dict create $rvar "return 1"]
			dict for {ip loop} $tac_bloop {
				dict for {var _} [dict get $loop input] {
					dict set usage $var $ip 1
				}
				dict lappend definition [dict get $loop output] $ip

			}

			# now go over the code and combine 
			# - bloops with bloops
			# - bloops with reductions (TODO)
			# for {set iter 0} {$iter<10} {incr iter} {}
			while true {
				set ips [dict keys $tac_bloop]
				set nochange true
				
				# iterate over the instruction dict
				# not: dict for {ip loop} $tac_bloop 
				# because we are modifying tac_bloop in the loop
				foreach ip $ips {
					if {![dict exists $tac_bloop $ip]} { continue }
					
					set loop [dict get $tac_bloop $ip]
					set type [dict get $loop type]
					if {($type ne "bloop") && ($type ne "load")} { continue }

					set output [dict get $loop output]
					set uses [my dict_get_default $usage $output {}]
					switch [dict size $uses] {
						0 { 
							# dead code, remove
							puts "Dead code: $ip"
							dict unset tac_bloop $ip
							dict unset definition $output
							set nochange false
						}

						1 {
							if  {($type ne "bloop")} { continue }
							# only a single use - combine
							set useip [lindex [dict keys $uses] 0]
							if {$useip eq "return"} { continue }
							set useloop [dict get $tac_bloop $useip]
							set usetype [dict get $tac_bloop $useip type]

							if {$usetype ne "bloop"} { continue }
							# don't do reductions yet
							# puts "Loop fusion: $ip $useip"
							# puts "$loop + $useloop"
							set combined_loop [my fuse_basic_loops $loop $useloop]
							# puts "->$combined_loop"
							dict unset tac_bloop $ip
							dict set tac_bloop $useip $combined_loop
							# remove the usage of the output of the first loop
							dict unset usage $output
							dict unset definition $output
							# transfer the inputs of the first loop to the combined loop
							dict for {var _} [dict get $loop input] {
								dict unset usage $var $ip
								dict set usage $var $useip 1
							}
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
		
		method alias {rvar bloopcode var1 var2} {
			# replace all occurences of var1 with var2
			if {$rvar eq $var1} {set rvar $var2}
			# find the statements assigning to var1
			# Note: Since we are deconstructing SSA, there could be multiple assignments!
			foreach assip [dict get $definition $var1] {
				# fix code. Only the last instruction should
				# assign here, but who knows
				set newcode {}
				#puts stderr [join $bloopcode \n]
				set code [dict get $bloopcode $assip code]
				foreach instr $code {
					set inp [lassign $instr opcode dest]
					if {$dest eq $var1} { set dest $var2 }
					lappend newcode [list $opcode $dest {*}$inp]
				}
				#puts "$assip: $code -> $newcode"
				dict set bloopcode $assip code $newcode
				# fix output declaration
				dict set bloopcode $assip output $var2
			}
			dict unset definition $var1

			# find statements making use of var1
			dict for {useip _} [dict get $usage $var1] {
				# fix code. Only the last instruction should
				# assign here, but who knows
				
				# skip the instruction if it no longer exists (BUG in dephi??)
				# if {![dict exists $bloopcode $useip]} { continue }
				
				set newcode {}
				foreach instr [dict get $bloopcode $useip code] {
					set inp [lassign $instr opcode dest]
					set inp [lmap var $inp { expr {$var eq $var1 ? $var2 : $var }}]
					lappend newcode [list $opcode $dest {*}$inp]
				}
				dict set bloopcode $useip code $newcode

				# fix input declaration
				dict unset bloopcode $useip input $var1
				dict set bloopcode $useip input $var2 1
				dict unset usage $var1 $useip
			}
			return [list $rvar $bloopcode]
		}
		method print_bloop {bloopcode} {
			dict for {ip bloop} $bloopcode {
				puts "$ip: [dict get $bloop code]"
			}
		}

		variable usage
		method dephi {rvar bloopcode} {
			#
			my print_bloop $bloopcode
			# print usage information
			puts $usage
			set ips [dict keys $bloopcode]
			foreach ip $ips {
				# be careful, we are changing bloopcode on the way
				if {![dict exists $bloopcode $ip]} { continue }

				set bloop [dict get $bloopcode $ip]
				if {[dict get $bloop type] eq "phi"} {
					set phi [lindex [dict get $bloop code] 0]
					lassign $phi opcode dest op1 op2
					puts "Aliasing $op1 to $dest"
					lassign [my alias $rvar $bloopcode $op1 $dest] rvar bloopcode
					my print_bloop $bloopcode
					puts "Aliasing $op2 to $dest"
					lassign [my alias $rvar $bloopcode $op2 $dest] rvar bloopcode
					# kill the phi node
					dict unset bloopcode $ip
					my print_bloop $bloopcode
				}
			}		

			return [list $rvar $bloopcode]
		}

		variable cfunctable
		method makecfunctable {} {
			# create a table to look up function signatures
			set cfunctable {}

			set real_binop {
				+ +
				.+ +
				.- -
				- -
				* *
				.* *
				/ /
				./ /
			}
			
			foreach {t1 t2 tres} {
				int int int
				double double double
			} {
				foreach {op cop} $real_binop {
					dict lappend cfunctable $op [list infix $cop [list $t1 $t2] $tres]
				}
			}

			foreach relop {== != < > <= >=} {
				dict lappend cfunctable $relop [list infix $relop {int int} int]
				dict lappend cfunctable $relop [list infix $relop {double double} int]
			}
			
			dict lappend cfunctable % [list infix % {int int} int]

			# assignment operator
			foreach t1 {int double NumArray_Complex} {
				dict lappend cfunctable = [list infix "" $t1 $t1]
			}

			set complex_binop {
				+ NumArray_ComplexAdd
				.+ NumArray_ComplexAdd
				- NumArray_ComplexSubtract
				.- NumArray_ComplexSubtract
				* NumArray_ComplexMultiply
				.* NumArray_ComplexMultiply
				/ NumArray_ComplexDivide
				./ NumArray_ComplexDivide
			}
			
			set complex NumArray_Complex
			# unary real & complex functions.
			# complex results
			foreach {op cop} $complex_binop {
				dict lappend cfunctable $op [list fun $cop [list $complex $complex] $complex]
			}
			
		    foreach {fr fc} {
				sin NumArray_ComplexSin
				cos NumArray_ComplexCos
				tan NumArray_ComplexTan
				exp NumArray_ComplexExp
				log NumArray_ComplexLog
			} {
				dict lappend cfunctable [list CALL $fr] [list fun $fr double double]
				dict lappend cfunctable [list CALL $fr] [list fun $fc $complex $complex]
			}
			
			dict lappend cfunctable {.^} [list fun pow [list double double] double]
			dict lappend cfunctable {.^} [list fun NumArray_ComplexPow [list $complex $complex] $complex]
			
			dict lappend cfunctable {{CALL atan2}} [list fun atan2 [list double double] double]
		}
		
		method dtype2ctype {dtype} {
			switch $dtype {
				int - 
				double  { return $dtype }
				complex { return NumArray_Complex }
				default { return -code error "Unknown data type $dtype" }
			}
		}

		variable ctypetable
		method makectypetable {} {
			# iterate over the type table and create
			# corresponding C data type
			puts "building ctypetable"
			set ctypetable {}
			dict for {symbol type} $typetable {
				lassign $type dtype shape
				if {($type eq "Any") || ($shape != 1)} {
					set ctype "Tcl_Obj *"
				} else {
					# scalar
					set ctype [my dtype2ctype $dtype]
				}
				dict set ctypetable $symbol $ctype
			}
			puts "$ctypetable"
		}

		method allocsymbol4c {symbol {init ""}} {
			lassign [dict get $typetable $symbol] dtype shape
			set csymbol [my symbol2c $symbol]
			if {$dtype eq "Any"} {
				set code "Tcl_Obj *$csymbol";
				if {$init ne ""} {
					append code " = $init;\n"
					append code "Tcl_IncrRefCount($csymbol);\n"
					return $code
				} else {
					append code " = NULL;\n"
					return $code
				}
			}
			if {$shape != 1} {
				set code "Tcl_Obj *$csymbol";
				if {$init ne ""} {
					append code " = Tcl_DuplicateObj($init);\n"
					append code "Tcl_IncrRefCount($csymbol);\n"
					return $code
				} else {
					append code " = NULL;\n"
					return $code
				}
			}

			# shape == 1 , scalar values
			switch $dtype {
				int {
					set code "long $csymbol"
					if {$init ne ""} {
						append code " = $init"
					}
				}
				double {
					set code "double $csymbol"
					if {$init ne ""} {
						append code " = $init"
					}
					
				}
				complex {
					set code "NumArray_Complex $csymbol"
					if {$init ne ""} {
						append code " = $init"
					}
				}
				default {
					return -code error "Unknown data type $dtype"
				}
			}
			append code ";\n"
			return $code
		}

		method releasesymbol4c {symbol} {
			set csymbol [my symbol2c $symbol]
			set ctype [dict get $ctypetable $symbol]
			if {$ctype eq "Tcl_Obj *"} {
				# It is a Tcl_Obj*
				return "if ($csymbol) Tcl_DecrRefCount($csymbol);\n$csymbol = NULL;\n"
			} else {
				return ""
			}
		}	

		method symbol2c {symbol} {
			lassign $symbol type index
			switch $type {
				Tempvar {
					return "temp$index"
				}
				Argument {
					return "objv\[[expr {$index+1}]\]"
				}
				Literal {
					return "literals\[$index\]"
				}
			}
		}	

		method call2c {call} {
			# translate a single call instruction into C
			set code [lindex $call 0]
			set args [lassign $code fun dest]
			set fname [lindex $fun 1]
			set narg [expr {[llength $args]+1}] ;# including cmd name
			set ccode "\{\n"
			set cmdlit [my allocliteral $fname]
			
			set ccmdlit [my symbol2c $cmdlit]

			append ccode "Tcl_Obj *cmdwords\[[expr {$narg}]\];\n"
			append ccode "cmdwords\[0\] = $ccmdlit;\n"
			
			set i 1
			foreach arg $args {	
				set objvar "cmdwords\[$i\]" 
				append ccode "$objvar = [my castsymbol2ctype $arg "Tcl_Obj *"];\n"
				incr i
			}
			
			for {set i 0} {$i<$narg} {incr i} {
				set objvar "cmdwords\[$i\]" 
				append ccode "Tcl_IncrRefCount($objvar);\n"
			}

			append ccode "int code = Tcl_EvalObjv(interp, $narg, cmdwords, 0);\n"
			
			for {set i 0} {$i<$narg} {incr i} {
				set objvar "cmdwords\[$i\]" 
				append ccode "Tcl_DecrRefCount($objvar);\n"
			}

			append ccode " if (code != TCL_OK) { return TCL_ERROR; }\n"
			set cdest [my symbol2c $dest]
			append ccode "$cdest = Tcl_GetObjResult(interp);\n"
			append ccode "Tcl_IncrRefCount($cdest);\n"
			append ccode "\}\n"
			return $ccode
			
		}
		
		method ctypediff {t1 t2} {
			# compute "t1 - t2"
			set t1n [my ctype2numeric $t1]
			set t2n [my ctype2numeric $t2]
			expr {$t1n-$t2n}
		}

		method castsymbol2ctype {symbol destctype} {
			set csymbol [my symbol2c $symbol]
			set srcctype [dict get $ctypetable $symbol]
			my ctypecast $csymbol $srcctype $destctype
		}
		
		method ctypecast {csymbol srcctype destctype} {
			switch $destctype {
				{Tcl_Obj *} {
					switch $srcctype {
						{Tcl_Obj *} {
							return $csymbol
						}
						double {
							return "Tcl_NewDoubleObj($csymbol)"
						}
						int {
							return "Tcl_NewLongObj($csymbol)"
						}
						NumArray_Complex {
							return "NumArray_NewComplexObj($csymbol)"
						}
						default {
							return -code error "Unknown src type $srcctype"
						}
					}
				}

				NumArray_Complex {	
					switch $srcctype {
						{Tcl_Obj *} {
							return -code error $csymbol
							# can only construct it into a target
						}
						int - double {
							return "NumArray_mkComplex($csymbol, 0.0)"
						}
						NumArray_Complex {
							return $csymbol
						}
						default {
							return -code error "Unknown src type $srcctype"
						}
					}
				}

				double {	
					switch $srcctype {
						{Tcl_Obj *} - NumArray_Complex {
							return -code error "Can't downcast $csymbol to double"
							# can only construct it into a target
						}
						int {
							return "((double)$csymbol)"
						} 
						
						double {
							return "$csymbol" ;# compiler casts
						}
						default {
							return -code error "Unknown src type $srcctype"
						}
					}
				}
				
				int {	
					switch $srcctype {
						{Tcl_Obj *} - NumArray_Complex - double{
							return -code error "Can't downcast $csymbol to int"
							# can only construct it into a target
						}
						int {
							return "$csymbol"
						} 
						
						default {
							return -code error "Unknown src type $srcctype"
						}
					}
				}
				default { return -code error "Unknown target type $destctype" }
			}
		}
		
		method unboxtclobj {cexpr symbol} {
			# cexpr yields a Tcl_Obj*, extract the 
			# primitive dtype into symbol (if it matches)
			set csymbol [my symbol2c $symbol]
			set ctype [dict get $ctypetable $symbol]
			switch $ctype {
				{Tcl_Obj *} { return "$csymbol = $cexpr;\nTcl_IncrRefCount($csymbol);\n" }
				double { return "if (Tcl_GetDoubleFromObj(interp, $cexpr, &$csymbol) != TCL_OK) { goto error; };\n" }
				NumArray_Complex { return "if (NumArray_GetComplexFromObj(interp, $cexpr, &$csymbol) != TCL_OK) { goto error; };\n" }
				int { return "if (Tcl_GetLongFromObj(interp, $cexpr, &$csymbol) != TCL_OK) { goto error; };\n" }
				default { return -code error "Unknown type to unbox: $ctype" }
			}
		}
		
		method symbol2celement {symbol} {
			# input: symbol (e.g. "Tempvar 3"
			# output: [dict element eltype pitch alloc release iterator], 
			# e.g. *temp3ptr, double, temp3pitch, double *temp3ptr;,temp3_it
			set ctype [dict get $ctypetable $symbol]
			set csymbol [my symbol2c $symbol]
			lassign $symbol stype idx
			set result [dict create element "" eltype "" pitch "" alloc "" release "" iterator "" csymbol $csymbol]
			if {[my isscalar $symbol]} {
				if {$stype eq "Literal"} {
					set value [dict get $literals $symbol]
					# literals are special. For scalar ones, output literally to C
					# complex must be wrapped. For non-scalar, they are iterated
					switch $ctype {
						int -
						double { 
							dict set result element $value
							dict set result eltype $ctype 
						}

						NumArray_Complex {
							set real [vectcl::vexpr {real(value)}]
							set imag [vectcl::vexpr {imag(value)}]
							dict set result element "NumArray_mkComplex($real,$imag)"
							dict set result eltype $ctype
						}
						default { return -code error "Unknown ctype $ctype for literal $symbol" }
					}
				} else {
					# scalar non literal
					dict set result element $csymbol
					dict set result eltype $ctype
				}
			} else {
				# non-scalar. get scalar data type
				lassign [dict get $typetable $symbol] dtype shape
				set ctype [my dtype2ctype $dtype]
				dict set result eltype $ctype

				if {$stype eq "Literal"} {	
					# non-scalar literals. Create symbols 
					# create new symbols
					set stem "lit${idx}"
				} else {
					set stem $csymbol
				}
				
				set ptr "${stem}ptr"
				dict set result ptr $ptr
				dict set result element "(*${ptr})"
				dict set result pitch "${stem}pitch"
				dict set result iterator "${stem}_it"
				dict set result alloc "$ctype * ${ptr};"
			}

			return $result
		}
		
		method ctype2numeric {ctype} {
			dict get {int 0 double 1 NumArray_Complex 2} $ctype
		}
		
		method bloop2c {bloop} {
			# convert basic loop into C
			# first create C symbols for all variables
			puts "Converting bloop $bloop"
			set cinfo {}

			set allscalar true
			set tempvars [dict keys [dict get $bloop temp]]
			set in [dict keys [dict get $bloop input]]
			set out [dict get $bloop output]
			set inout [list {*}$in $out]
			set localvars [concat $tempvars $inout]
			
			
			foreach symbol $localvars {
				# compute csymbol for element-wise access
				# and corresponding increment
				set elprop [my symbol2celement $symbol]
				dict set cinfo $symbol $elprop

				if {![my isscalar $symbol]} { set allscalar false }
			}

			set ccode {}
			append ccode "\{\n"
			# block to enclose the temp variables
			set allocs ""
			set releases ""
			
			if {!$allscalar} {
				# find input with the highest number of dimensions
				set maxDim 0; set maxsymbol {}
				foreach symbol $in {
					lassign [dict get $typetable $symbol] dtype shape
					set dim [llength $shape]
					if {$dim > $maxDim && ($shape != 1)} {
						set maxDim $dim
						set maxsymbol $symbol
					}
				}
				
				set outctype [dict get $cinfo $out eltype]
				set outptr [dict get $cinfo $out ptr]
				set maxcsymbol [my symbol2c $maxsymbol]
				# create output buffer
				append ccode "
					NumArrayInfo *maxinfo = NumArrayGetInfoFromObj(interp, $maxcsymbol);
					if (maxinfo == NULL) { goto error; }
					NumArrayInfo *resultinfo;
					resultinfo = CreateNumArrayInfo(maxinfo -> nDim, maxinfo -> dims, NATYPE_FROM_C($outctype));
					
					/* allocate buffer of this size */
					NumArraySharedBuffer *sharedbuf = NumArrayNewSharedBuffer(resultinfo -> bufsize);
					$outctype *$outptr = NumArrayGetPtrFromSharedBuffer(sharedbuf);\n"
				# create iterators for all non-scalar inputs
				foreach symbol $in {
					dict with cinfo $symbol {
						if {$iterator ne ""} {
							append ccode "NumArrayIterator $iterator;\n"
							append ccode "NumArrayIteratorInitObj(interp, ${csymbol}, &${iterator});\n"
							append ccode "$eltype *$ptr = NumArrayIteratorDeRefPtr(&$iterator);\n"
							append ccode "const int $pitch = NumArrayIteratorRowPitchTyped(&$iterator);\n"
						}
					}
				}
				
				set maxit [dict get $cinfo $maxsymbol iterator]
				set maxptr [dict get $cinfo $maxsymbol ptr]
				append ccode "const int length = NumArrayIteratorRowLength(&$maxit);\n"
				append ccode "while ($maxptr) \{\n"
				append ccode "for (int i=length; i; i--) \{\n"
				
			
			}

			foreach symbol $tempvars {
				append ccode [my allocsymbol4c $symbol]
			}
			
			foreach instr [dict get $bloop code] {
				set operands [lassign $instr opcode dest]
				
				set candidates [dict get $cfunctable $opcode]
				set opctypes [lmap v $operands {dict get $cinfo $v eltype}]
				set best {}; set bestdiff Inf
				foreach cand $candidates {
					# compute argument type distance
					set diff 0
					set skip false
					lassign $cand cdispatch cfunc formalargs resarg
					#puts "instr $instr, Candidate $cand, formalargs $formalargs, opctypes $opctypes"
					foreach actualtype $opctypes formaltype $formalargs {
						set d1 [my ctypediff $formaltype $actualtype]
						if {$d1<0} { set skip true; break }
						incr diff $d1
					}
					if {$skip} { continue }
					if {$diff < $bestdiff} { 
						set bestdiff $diff
						set best $cand
					}
				}
				if {$best eq ""} { 
					return -code error "Cannot resolve overload for $instr, candidates are :\n[join $candidates \n]"
				}
				lassign $best cdispatch cfunc formalargs resctype
				set arglist [lmap v $operands actualtype $opctypes formaltype $formalargs {
					set csymbol [dict get $cinfo $v element]
					my ctypecast $csymbol $actualtype $formaltype
				}]
				
				switch $cdispatch {
					infix {
						set value [join $arglist $cfunc]
					}
					fun {
						set value "${cfunc}([join $arglist ,])"
					}
				}
				
				
				# append ccode "apply $best $in"
				set cdest [dict get $cinfo $dest element]
				set destctype [dict get $cinfo $dest eltype]
				append ccode "$cdest = [my ctypecast $value $resctype $destctype];\n"
										
			}

			if {!$allscalar} {
				append ccode "++$outptr;\n"
				set ptrs {}; set pitches {}; set iterators {}
				foreach symbol $in {
					dict with cinfo $symbol {
						if {$iterator ne ""} {
							lappend ptrs $ptr
							lappend pitches $pitch
							lappend iterators $iterator
						}
					}
				}
				
				foreach ptr $ptrs pitch $pitches {
					append ccode "$ptr += $pitch; \n"
				}
				append ccode "\};\n"
				
				foreach ptr $ptrs it $iterators {
					append ccode "$ptr = NumArrayIteratorAdvanceRow(&$it);\n"
				}
				
				append ccode "\};\n"
				
				foreach it $iterators {
					append ccode "NumArrayIteratorFree(&$it);\n"
				}
				
				set outsymbol [my symbol2c $out]
				append ccode "$outsymbol = Tcl_NewObj();\n"
				append ccode "NumArraySetInternalRep($outsymbol, sharedbuf, resultinfo);\n"
				append ccode "Tcl_IncrRefCount($outsymbol);\n"

			}	
			append ccode "\}\n"

			return $ccode
		}

		method control2c {code} {
			set instr [lindex $code 0]
			lassign $instr opcode _ src
			switch $opcode {
				If {
					return "if ([my symbol2c $src]) \{\n"
					# breaks if the symbol is not an int
				}
				Else {
					return "\} else \{\n"
				}
				EndIf {
					return "\}\n"
				}
				While {
					return "while (1) \{\n"
				}
				Do {
					return "if (![my symbol2c $src]) \{ break; \} \n"
				}
				EndWhile {
					return "\}\n"
				}

				default {
					return -code error "Unknown control construct: $opcode"
				}
			}

		}

		variable cname
		method codegen {rvar tac} {
			# generate C code - for now just print three address code
			set cname "VECTCLJIT_[dict get $opts -name]" ;# should add type signature for overloading

			my makectypetable
			my makecfunctable

			set code "int $cname (ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv) \{\n"
			append code "Tcl_Obj ** literals; int nLiterals;\n"
			append code "if (Tcl_ListObjGetElements(interp, (Tcl_Obj *)cdata, &nLiterals, &literals) != TCL_OK) \{\n"
			append code "return TCL_ERROR; /* internal error ! */\n"
			append code "\}\n"
			
			set bodycode {}
			set allocs {}
			set cleanups {}

			dict for {ip instr} $tac {
				set output [dict get $instr output]
				switch [dict get $instr type] {
					call {
						append bodycode "[my call2c [dict get $instr code]]\n"
					}
					load {
						# create native type from something
						lassign [lindex [dict get $instr code] 0] opcode dest src
						set cexpr [my symbol2c $src]
						append bodycode [my unboxtclobj $cexpr $dest]
					}
					
					control {
						# handle control constructs like if, while, (for)
						append bodycode [my control2c [dict get $instr code]]
					}
					bloop {
						append bodycode [my bloop2c $instr]
					}
					default {
						append bodycode "$instr\n"
					}
				}
				if {$output ne ""} {
					dict set allocs $output [my allocsymbol4c $output]
					dict set cleanups $output [my releasesymbol4c $output]
				}
			}

			set alloccode [join [dict values $allocs] ""]
			set cleanupcode [join [dict values $cleanups] ""]
			
			append code $alloccode
			append code $bodycode
			append code "\nTcl_SetObjResult(interp, [my castsymbol2ctype $rvar "Tcl_Obj *"]);\n"
			append code $cleanupcode
			append code "return TCL_OK\;\n"
			append code "error:\n"
			append code $cleanupcode
			append code "return TCL_ERROR;\n"
			append code "\}\n"
		}

		#######################################
		# machinery to traverse the parse tree
		#######################################
		method Program {from to sequence args} {
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
			set ind 0
			foreach arg [dict get $opts -args] {
				lassign $arg name type
				set symbol [list Argument $ind]
				dict set typetable $symbol $type
				lassign [my assignvar $name $symbol] tvar tcode
				lappend resultcode $tcode
				incr ind
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
			set symbol [list Literal $lcount]
			incr lcount
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
		vectcl::jitproc collatz {{N {int 1}}}	{
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
		}

		vectcl::jitproc linreg {{xv {double n}} {yv {double n}}}	{
			xm=mean(xv); ym=mean(yv)
			beta=sum((xv-xm).*(yv-ym))./sum((xv-xm).^2)
			alpha=ym-beta*xm
			list(alpha, beta)
		}

		vectcl::jitproc squares {{xv {double n}} {yv {double n}}}	{
			xv.*xv+yv.*yv
		}
		
	}

	proc benchjit {} {
		# translation of an integer benchmark
		jitproc random_run_vecjit {{N {int 1}}} {
			IM=139968
			IA=3877
			IC=29573
			last=42

			max=100.0
			while N {
				# gen_random_orig(100.0)
				# function calls are very slow (TclEval)
				last=(last * IA + IC) % IM
				result = max * last / IM
				N=N-1
			}
			puts(result)
		}

	# run a long-loop benchmark
		vproc square_tcl {x y} {
			x.*x+y.*y
		}

		jitproc square_tcc {{x {double n}} {y {double n}}} {
			x.*x+y.*y
		}
		
		set manual_tcc {
		
void inner_loop_asm(double *t1ptr, long t1pitch, double* t2ptr, long t2pitch, double* resptr, long length) {
	/* t1ptr = rdi, t1pitch=rsi, t2ptr=rdx, t2pitch=rcx, resptr=r8, length=r9 */
	/* Perform the equivalent of the innermost loop in SSE asm */
	#define MOV_X_XMM0 asm(".byte 0xf3,0x0f,0x7e,0x07\n");
	#define MOV_Y_XMM1 asm(".byte 0xf3,0x0f,0x7e,0x0a\n");
	#define MUL_XMM0_XMM0 asm(".byte 0xf2,0x0f,0x59,0xc0\n");
	#define MUL_XMM1_XMM1 asm(".byte 0xf2,0x0f,0x59,0xc9\n");
	#define ADD_XMM1_XMM0 asm(".byte 0xf2,0x0f,0x58,0xc1\n");
	#define MOV_XMM0_IR8 asm(".byte 0x66,0x41,0x0f,0xd6,0x00\n");
	#define INC_R8 asm(".byte 0x49,0xff,0xc0\n");
	#define ADD_R8_8 asm(".byte 0x49,0x83,0xc0,0x08\n");
	#define DEC_R9 asm(".byte 0x49,0xff,0xc9\n");

	/* Pitch is given in doubles, first convert to byte offset */
				asm("shlq $3,%rsi\n"
					"shlq $3,%rcx\n");
	top:
					MOV_X_XMM0
					MOV_Y_XMM1
					MUL_XMM0_XMM0
					MUL_XMM1_XMM1
					ADD_XMM1_XMM0
					MOV_XMM0_IR8
				asm("add %rsi, %rdi\n"
					"add %rcx, %rdx\n");
					ADD_R8_8
					DEC_R9
				asm("jnz top\n");
}


	int VECTCLJIT_manual_tcc (ClientData cdata, Tcl_Interp *interp, int objc, Tcl_Obj *const *objv) {
		Tcl_Obj *temp1 = NULL;
		Tcl_Obj *temp2 = NULL;
		Tcl_Obj *temp7 = NULL;
		temp1 = objv[1];
		Tcl_IncrRefCount(temp1);
		temp2 = objv[2];
		Tcl_IncrRefCount(temp2);
		{

			NumArrayInfo *maxinfo = NumArrayGetInfoFromObj(interp, temp1);
			if (maxinfo == NULL) { goto error; }
			NumArrayInfo *resultinfo;
			resultinfo = CreateNumArrayInfo(maxinfo -> nDim, maxinfo -> dims, NATYPE_FROM_C(double));
			
			/* allocate buffer of this size */
			NumArraySharedBuffer *sharedbuf = NumArrayNewSharedBuffer(resultinfo -> bufsize);
			double *temp7ptr = NumArrayGetPtrFromSharedBuffer(sharedbuf); 
			NumArrayIterator temp1_it;
			NumArrayIteratorInitObj(interp, temp1, &temp1_it);
			double *temp1ptr = NumArrayIteratorDeRefPtr(&temp1_it);
			const int temp1pitch = NumArrayIteratorRowPitchTyped(&temp1_it);
			NumArrayIterator temp2_it;
			NumArrayIteratorInitObj(interp, temp2, &temp2_it);
			double *temp2ptr = NumArrayIteratorDeRefPtr(&temp2_it);
			const int temp2pitch = NumArrayIteratorRowPitchTyped(&temp2_it);
			const int length = NumArrayIteratorRowLength(&temp1_it);
			
			while (temp1ptr) {
				inner_loop_asm(temp1ptr, temp1pitch, temp2ptr, temp2pitch,temp7ptr,length);
				temp1ptr = NumArrayIteratorAdvanceRow(&temp1_it);
				temp2ptr = NumArrayIteratorAdvanceRow(&temp2_it);
			};
			
			NumArrayIteratorFree(&temp1_it);
			NumArrayIteratorFree(&temp2_it);
			temp7 = Tcl_NewObj();
			NumArraySetInternalRep(temp7, sharedbuf, resultinfo);
			Tcl_IncrRefCount(temp7);
			}

			Tcl_SetObjResult(interp, temp7);
			if (temp1) Tcl_DecrRefCount(temp1);
			temp1 = NULL;
			if (temp2) Tcl_DecrRefCount(temp2);
			temp2 = NULL;
			if (temp7) Tcl_DecrRefCount(temp7);
			temp7 = NULL;
			return TCL_OK;
			error:
			if (temp1) Tcl_DecrRefCount(temp1);
			temp1 = NULL;
			if (temp2) Tcl_DecrRefCount(temp2);
			temp2 = NULL;
			if (temp7) Tcl_DecrRefCount(temp7);
			temp7 = NULL;
			return TCL_ERROR;
		}
	
		}
		
		set handle [tcc4tcl::new]
		$handle add_include_path $vectcl::ipath
		$handle ccode "#include <vectclInt.h>"
		$handle ccode $manual_tcc
		$handle linktclcommand manual_tcc VECTCLJIT_manual_tcc {}
		$handle go
		#rename $handle ""

		set coll_code {	
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
		}

		jitproc collatz {{N {int 1}}} $coll_code
		vproc vcollatz {N} $coll_code

		proc tclcollatz {N} {
			set i 0
			while {$N != 1} {
				if {$N%2 == 1} {
					set N [expr {3*$N+1}]
				} else {
					set N [expr {$N/2}]
				}
				incr i
			}
			return $i
		}

		# http://wiki.tcl.tk/1173
		# miguel's random number generator
		proc rand_seed_orig {} {
			variable IM 
			variable IA 
			variable IC 
			variable last
			set IM 139968
			set IA 3877
			set IC 29573
			set last 42
		}

		proc gen_random_orig {max} {
			variable IM 
			variable IA 
			variable IC 
			variable last
			set last [expr {($last * $IA + $IC) % $IM}]
			expr {$max * $last / $IM}
		}

		proc random_run_orig {N} {
			set result 0.0
			while {$N} {
				set result [gen_random_orig 100.0]
					incr N -1
			}
			puts [format "%.13f" $result]
		}

		proc rand_seed_miguel {} {

			set params {IM 139968 IA 3877 IC 29573}
			variable last
			set last 42
			set randBody [string map $params {
				expr {(100.0 * [set last [expr {($last * IA + IC) % IM}]]) / IM}
			}]

			set mainBody [string map [list randBody $randBody] {
				variable last
				set result 0.0
				incr N
				while {[incr N -1]} {
					set result [randBody]
				}
				puts [format "%.13f" $result]
			}]
			proc random_run_miguel {N} $mainBody
		}

		vproc random_run_vectcl {N} {
			IM=139968
			IA=3877
			IC=29573
			last=42

			max=100.0
			while N {
				# gen_random_orig(100.0)
				# function calls are very slow (TclEval)
				last=(last * IA + IC) % IM
				result = max * last / IM
				N-=1
			}
			puts(result)
		}
		
		# cosine function benchmark
		proc cos_tcl {x n} {
			set x [expr {double($x)}]
			set n [expr {int($n)}]
			set j 0
			set s 1.0
			set t 1.0
			set i 0
			while {[incr i] < $n} {
				set t [expr {-$t*$x*$x / [incr j] / [incr j]}]
				set s [expr {$s + $t}]
			}
			return $s
		}

		vproc cos_vectcl {x n} {
			j=0
			s=1.0
			t=1.0
			i=0
			while (i < n) {
				t=-t*x*x / (j+1) / (j+2)
				s =s + t
				j=j+2
				i=i+1
			}
			s
		}

		jitproc cos_jit {{x {double 1}} {n {int 1}}} {
			j=0
			s=1.0
			t=1.0
			i=0
			while (i < n) {
				t=0-t*x*x / (j+1) / (j+2)
				s =s + t
				j=j+2
				i=i+1
			}
			s
		}


		# run timings
		proc squaresbench {} {
			puts "Timing squares:"
			set fd [open squaresbench.dat w]
			foreach N {20 50 100 200 500 1000 2000 5000 10000 20000 50000 100000 200000 500000} {	
				set x {}; set y {}
				for {set i 0} {$i<$N} {incr i} {
					lappend x [expr {rand()}]
					lappend y [expr {rand()}]
				}
				
				set rep [expr {50000000/$N+1}]
				set res $N
				foreach benchproc {square_tcl square_tcc tcc_sljit manual_tcc} {
					# run once
					set r1 [$benchproc $x $y]
					lassign [time {$benchproc $x $y} $rep] ms
					puts "$benchproc $ms ms size $N, $rep repetitions"
					lappend res $ms
				}
				puts ""
				puts $fd $res
			}
			close $fd
		}

		proc collatzbench {} {
			puts "Timing collatz:"
			set coll_start 1537
			set c1 [collatz $coll_start]
			set c2 [vcollatz $coll_start]
			set c3 [tclcollatz $coll_start]
			puts "vproc [time {vcollatz $coll_start} 1000], result $c1"
			puts "jitproc [time {collatz $coll_start} 1000], result $c2"
			puts "Tcl [time {tclcollatz $coll_start} 1000], result $c3"
		}

		proc randombench {} {
			puts "Timing random number generator"
			set rep 300000
			rand_seed_orig
			puts "Original code [time {random_run_orig $rep}]"
			rand_seed_miguel
			puts "Miguel's code [time {random_run_miguel $rep}]"
			puts "VecTcl vproc [time {random_run_vectcl $rep}]"
			puts "VecTcl jitproc [time {random_run_vecjit $rep}]"
		}

		proc cosbench {} {
			puts "Timing cosine function"
			set rep 100000
			puts "Original code [time {cos_tcl 0.2 16} $rep]"
			puts "VecTcl vproc [time {cos_vectcl 0.2 16} $rep]"
			puts "VecTcl jitproc [time {cos_jit 0.2 16} $rep]"
		}

		#squaresbench
		#collatzbench
		#randombench
		
		cosbench

	}

}
	
