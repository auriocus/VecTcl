lappend auto_path [file dirname [info script]]
package require pt::pgen

# This tokenization step is needed
# because regexps contain character classes
# don't know how to express
# [[:alpha:]] in PEG without listing
# character ranges
namespace eval vectcl {
	variable ns [namespace current]
	
	interp alias {} ::numarray::ones {} ::numarray::constfill 1.0
	interp alias {} ::numarray::zeros {} ::numarray::constfill 0.0

	set testscripts {
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
	}

	set vmtokens {
		{\\\n} WHITESPACE
		{[[:space:]]*\n} NEWLINE
		{#([^\n]*)(\n|$)} DROP
		{[[:space:]]+}  WHITESPACE
		{[[:digit:]]+(\.[[:digit:]]+)?((e|E)[+-]?[[:digit:]]+)?[iI]} IMAGINARYNUMBER
		{[[:digit:]]+(\.[[:digit:]]+)?((e|E)[+-]?[[:digit:]]+)?} REALNUMBER
		{=} ASSIGN
		{(\+=|\.+=|-=|\.-=)} ASSIGNOP
		{(\.\*=|\./=)} ASSIGNOP
		{(\^=|\*\*=)} ASSIGNOP
		{\+} PLUS
		{-} MINUS
		{'} TRANSPOSE
		{(\.\^|\.\*\*)} OPERATORPOW
		{(\.\+|\.-)} OPERATORADD
		{(\*|\.\*|\/|\./|\\|%)} OPERATORMUL
		{[,:;(){}\[\]]} COPY
		{for\M} FOR
		{[[:alpha:]_][[:alnum:]_]*} IDENTIFIER
	}  

	set tokenmap {
		WHITESPACE " "
		REALNUMBER n
		IMAGINARYNUMBER I
		ASSIGN =
		ASSIGNOP !
		PLUS p
		MINUS m
		TRANSPOSE T
		OPERATORADD +
		OPERATORMUL *
		OPERATORPOW ^
		IDENTIFIER i
		NEWLINE ;
		FOR F
	}

	
	
	
	proc compile {xprstring} {
		variable compiler
		$compiler compile $xprstring
	}

	proc tokenize {xprstring} {
		# fully lex an expression string 
		variable vmtokens
		variable tokenmap
		set restokens {}
		set reslexems {}
		while {$xprstring != "" } {
			lassign [lex $vmtokens $xprstring] tok val xprstring
			if {$tok eq "COPY"} {
				append restokens $val
				lappend reslexems $val
			} elseif {$tok eq "DROP"} {
				# token is comment and ignored
			} else {
				append restokens [dict get $tokenmap $tok]
				lappend reslexems $val
			}
		}
		return [list $restokens $reslexems]
	}

	proc lex {tokens input} {
		# try to parse the next bit of input 
		# with all regexps from tokens
		# and return the first hit
		foreach {rexp token} $tokens {
			set fits [regexp -- ^$rexp $input match]
			if {$fits} {
				# got a match
				break
			}
		}
		if {![info exists match]} {
			# no pattern match
			error "Tokenizer: Cannot parse $input"
		}
		set matchlen [string length $match]
		set newinput [string range $input $matchlen end]
		return [list $token $match $newinput]
	}

	proc untok {AST lexems} {
		set args [lassign $AST type from to]
		set result [list $type [lrange $lexems $from $to]]
		foreach arg $args {
			lappend result [untok $arg $lexems]
		}
		return $result
	}

	variable VMathGrammar {
	PEG VMath (Sequence)
		Program      <- ' '* (Sequence / ForLoop )*;
		Sequence     <- ' '* Statement (' '* ';' ' '* Statement)* ' '*;
		Statement    <- Assignment / OpAssignment / Expression /Empty;
		ForLoop      <- 'F' ' '* Var ' '* '=' ' '* Expression ' '* '{' Sequence '}';
		Empty		 <- ' '*;
		Expression   <- Term (' '* AddOp ' '* Term)*;
		Assignment   <- VarSlice ( ' '* ',' ' '* VarSlice)* ' '* '=' ' '* Expression;
		OpAssignment <- VarSlice ' '* AssignOp ' '* Expression;

		Term         <- ( Factor (' '* MulOp ' '* Factor)* ) / Sign Factor (' '* MulOp ' '* Factor)*;
		Factor       <- Transpose ' '* PowOp ' '* Factor / Transpose;

		Transpose    <- Fragment TransposeOp / Fragment;
		Fragment     <- Number / '(' ' '* Expression ' '*  ')' / Function / VarSlice/Literal;

		Function     <- FunctionName '(' ( Expression (',' ' '* Expression)* )? ')';
		
		VarSlice     <- Var ( ' '* '[' ' '* SliceExpr ( ',' ' '* SliceExpr )* ' '* ']' )?;
		SliceExpr    <- IndexExpr  ' '* (':' ' '* IndexExpr ' '* ( ':' ' '* IndexExpr )? )? / ':';
		
		IndexExpr	 <- Sign? RealNumber;
		SignedNumber <- Sign? RealNumber;

		Literal      <- '{' ' '* ( ComplexNumber / Literal )  (' '+ (ComplexNumber / Literal))* ' '* '}';
		ComplexNumber  <- Sign? RealNumber ( Sign ImaginaryNumber)?;

		TransposeOp  <- 'T';
		AssignOp     <- '!';
		RealNumber    <- 'n';
		ImaginaryNumber <- 'I';
		Number       <- ImaginaryNumber / RealNumber;
		Sign         <- 'p' / 'm';
		Var          <- 'i';
		FunctionName <- 'i';
		MulOp        <- '*';
		AddOp        <- '+'/'p'/'m';
		PowOp        <- '^';
	END;
	}

	proc Init {} {
		variable VMathGrammar
		variable ns
		# create parser 
		catch [pt::pgen peg $VMathGrammar snit -class ${ns}::VMathParser -name VMathGrammar]
		variable parser [VMathParser]
		# create compiler
		variable compiler [CompileVMath new $parser]
		# clean cache
		variable proccache {}
		variable cachecount 0

		namespace export vexpr
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

	oo::class create ${ns}::CompileVMath {
		variable tokens lexems varrefs tempcount parser
		variable purefunctions

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
				std std1 sum tan tanh transpose list
			}
		}

		method compile {script} {
			# Instantiate the parser
			lassign [vectcl::tokenize $script] tokens lexems
			set varrefs {}
			set tempcount 0

			return [my {*}[$parser parset $tokens]]
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
			dict set varrefs $var 1
		}

		method getvarrefs {} {
			dict keys $varrefs
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

			if {$sign eq "-"} {
				return "numarray::neg [my bracket $value]"
			} else {
				return $value
			}
				
		}
		
		variable resultvar
		method Sequence {from to args} {
			# sequence of statements
			# first check if we parsed the full program
			# if not, there was an error...
			if {$to + 1 < [llength $lexems]} {
				return -code error "Parse error near [join [lrange $lexems $to [expr {$to+3}]]]"
			}
			# every arg represents a Statement. Compile everything in sequence

			set resultvar {}
			set body {}
			foreach stmt $args {
				set stmtcompile [my {*}$stmt]
				if {$stmtcompile ne ""} {
					append body "[my constantfold $stmtcompile]\n"
				}
			}

			# resultvar is set, if a statement
			# stores an intermediate computation as the result (ListAssignment)
			if {$resultvar ne ""} {
				append body "return \$[list $resultvar]"
			}

			# create upvars and prepend to body
			set upvars {}
			foreach ref [my getvarrefs] {
				append upvars [list upvar 1 $ref $ref ]\n
			}

			return $upvars$body

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
			my varref $var
			
			set op [my {*}$assignop]
			set value [my bracket [my {*}$expression]]
			
			if {$slice=={}} {
				# simple assignment
				set resultvar {}
				return "[list {*}$op $var] $value"
			} else {
				# assignment to slice
				set resultvar {}
				return "[list {*}$op $var $slice] $value"
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
				my varref $var
				if {$slice=={}} {
					# simple assignment
					set resultvar {}
					return "[list set $var] $value"
				} else {
					# assignment to slice
					set resultvar {}
					return "[list numarray::= $var $slice] $value"
				}
			} else {
				# list assignment
				set assignvars {}
				set slicevars {}
				foreach varslice [lrange $args 0 end-1] {	
					lassign [my analyseslice $varslice] var slice
					my varref $var
					if {$slice=={}} {
						# simple assignment
						lappend assignvars $var
					} else {
						# assignment to slice
						set temp [my alloctemp]
						lappend assignvars $temp
						lappend slicevars "[list numarray::= $var $slice] \$$temp"
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
			# puts "Slices: $slices ([llength $slices])"
			set slices [lmap slice $slices "my {*}\$slice"]
			
			return [list $var $slices]
		}

		method SliceExpr {from to args} {
			if {[llength $args] == 0} {
				# it is a single :, meaning all
				return {0 -1 1}
			}
			
			# otherwise at max 3 IndexExpr
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

			return $result
		}

		method Literal {from to args} {
			# A complex literal number is used in literal arrays
			return "I [join [lrange $lexems $from $to] ""]"
		}
		
		method Verbatim {from to args} {
			# A complex literal number is used in literal arrays
			return [list I [join [lrange $lexems $from $to] ""]]
		}
		
		forward Number    my Verbatim
		forward SignedNumber my Verbatim
		forward ComplexNumber my Verbatim

		forward IndexExpr	my SignedNumber

		forward Expression   my Expression-Compound
		forward Factor       my Expression-Compound
		forward Fragment     my Expression-Compound

		method Expression-Operator {from to args} {
			return [list numarray::[lindex $lexems $from]]
		}

		forward AddOp        my Expression-Operator
		forward MulOp        my Expression-Operator
		forward PowOp        my Expression-Operator
		forward AssignOp     my Expression-Operator

		method Var {from to args} {
			list set [lindex $lexems $from]
		}

		method VarSlice {from to args} {
			 lassign \
				[my analyseslice [list VarSlice $from $to {*}$args]] \
				var slices

			my varref $var
			if {$slices eq {}} {
				return [list set $var]
				# alternative:
				# return [list I "\$[list $var]"]
			} else {
				return "numarray::slice \[set [list $var]\] [list $slices]"
			}
		}

		method VarRef {Var from to args} {
			lindex $lexems $from
		}

		method FunctionName {from to args} {
			lindex $lexems $from
		}
		
		method Sign {from to args} {
			# unary plus or minus
			# no-op for +, neg for minus
			lindex $lexems $from
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

	proc testcompiler {} {
		variable testscripts
		variable compiler
		variable parser

		foreach script $testscripts {
			puts " === $script"
			lassign [tokenize $script] tokens lexems
			set parsed [$parser parset $tokens]
			puts [untok $parsed $lexems]

			if {[catch {$compiler compile $script} compiled]} {
				puts stderr "Error: $compiled"
			} else {
				puts stdout "======Compiled: \n$compiled\n====END==="
			}
		}
	}
	
	Init
}

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
