# copy any C files from the CLAPACK sources
# that are required to create the functions listed
# in the arguments. i.e. a "source code linker"

set CLAPACK_DIR /Users/chris/Sources/CLAPACK-3.2.1/

set tooldir [file normalize [file dirname [info script]]]
set destfile [file normalize [file join $tooldir ../generic/clapack_cutdown.c]]

# helper procs for parsing
proc shift {} {
	upvar 1 i i
	upvar 1 line line
	upvar 1 lines lines
	incr i
	set line [lindex $lines $i]
	return $line
}

proc putback {} {
    upvar 1 i i
    incr i -1
}

proc print {} {
	upvar 1 line line
	upvar 1 body body
	upvar 1 constantmap constantmap
	# map all redefined constants within line
	set mline $line
	foreach {old new} $constantmap {
		set mline [regsub -all "\\m${old}\\M" $mline $new]
	}
	append body "$mline\n"
}

proc issubroutine {type} {
    expr {
	$type eq "/* Subroutine */ int"
    }
}

proc insinterp {arglist if} {
    if {$if} {
	regsub {\(} {(Tcl_Interp *interp, } $arglist
    } else {
	return $arglist
    }
}

proc parsef2csource {f} {
	# read a CLAPACK source file
	# and rip the pieces apart
	# such that the functions can be freely
	# combined in a singe source file

	variable functions
	variable constantdefs
	variable ignore

	# a regexp matching any of f2c's return types /* Subroutine */ int 
	set rdtypes {(\minteger|\mlogical|\mreal|\mdoublereal|\mcomplex|\mdoublecomplex|/\* Subroutine \*/ int|/\* Double Complex \*/ VOID|/\* Complex \*/ VOID)\M}
	# regexp matching an identifier
	set rident  {\m([a-zA-Z0-9_]+)\M}
	# regexes for f2c's markers
	set rconstants {/\* Table of constant values \*/}
	set rbuiltins {/\* Builtin functions \*/}
	set rlocals {/\* Local variables \*/}
	set rfuncdecl "$rdtypes\\s+$rident\\("
	set rexternfunc "extern $rfuncdecl"
	set rarglist "(\\(\[^)\]+\\))"
	set rpartfuncdecl "$rident${rarglist}"
	set rfullfuncdecl ".*${rdtypes}\\s+$rident${rarglist}"
	set rmainfuncdecl "^${rfuncdecl}(.*)"
	set rconstant "^static\\s+$rdtypes\\s+$rident\\s*=\\s*(.*);"
	set rcommentonly {^\s*/\*.*\*/\s*$}
	set rfuncall "^\\s*$rident\\("
	set rreturnok "return\\s+0;"
	
	# read in the file
	set fd [open $f r]
	set lines [split [read $fd] \n]
	close $fd

	set prefix [file tail [file rootname $f]]_

	set bro \{
	set brc \}
	# here comes a state machine parse the f2c source
	set infunction false
	set inheader true
	set externals {}
	set name {}
	set rtype {}
	set arglist {}
	set body {}
	set constantmap {}
	set constants {}

	for {set i 0} {$i<[llength $lines]} {incr i} {
		set line [lindex $lines $i]
		 
		switch -regexp -matchvar match -- $line \
			$rconstants { 
				set inconstants true
			} \
			$rbuiltins  { 
				set inbuiltins true
			} \
			$rlocals {
				set inlocals true
			} \
			$rconstant {
				lassign $match -> type id value
				set prefid "$prefix$id"
				dict set constantmap $id $prefid
				dict set constantdefs $prefid "static $type $prefid = $value;"
				lappend constants $prefid
				#puts "Renaming $id -> $prefid"

			} \
			$rmainfuncdecl {
				#puts "Found match $match"
				lassign $match -> rtype name arglist
				set infunction true
				# read anything up to the trailing brace
				shift
				while {![string match "$bro*" $line]} {
					append arglist $line
					shift
				}
				print
				set mainsubroutine [issubroutine $rtype]
				if {$mainsubroutine} {
				    set arglist "(Tcl_Interp *interp, $arglist"
				} else {
				    set arglist "($arglist"
				}
				# the leading parenthesis was gobbled up
				# by rmainfuncdecl
			} \
			$rexternfunc {
				# gobble up until we hit a line with ;
				set extrn $line
				while {[string first ";" $line]<0} {
					shift
					append extrn $line
				}
				
				# puts "Found $extrn\n\n"
				# now we must parse the extern declarations. 
				# There can be more than one function on the same
				# extern line
				regexp  $rfullfuncdecl $extrn all etype ename eargs
				
				set ip [issubroutine $etype]

				if {![dict exists $ignore $ename]} {
					dict set externals $ename type $etype
					dict set externals $ename args [insinterp $eargs $ip]
				}
				#puts $all 
				set start [string length $all]
				set matches [regexp -all -inline -start $start $rpartfuncdecl $extrn]
				#puts "$matches"
				foreach {-> ename eargs} $matches {
					if {![dict exists $ignore $ename]} {
						dict set externals $ename type $etype
						dict set externals $ename args [insinterp $eargs $ip]
					}
				}

			} \
			"^$brc" {
				# the only place an unindented brace can be
				# is the end of a toplevel subroutine

				print
				#puts "Function $name, returns $rtype"
				#puts "Depending on [dict keys $externals]"
				# put into global function list
				dict set functions $name type $rtype
				dict set functions $name args $arglist
				dict set functions $name depends [dict keys $externals]
				dict set functions $name constants $constants
				dict set functions $name body $body

				# now put the external declarations into the functions table,
				dict for {name def} $externals {
					if {![dict exists $functions $name]} {
						# careful not to overwrite previous definitions
						# declarations must agree
						dict set functions $name $def
					} else {
						# check for redefinition - lots of false alarms
						set previous [dict get $functions $name]
						if {$previous != $def} {
							#puts stderr "Warning, declarations differ for $name"
						}
					}
				}
				set infunction false
				set externals {}
				set body {}
				set arglist {}
			} \
			$rcommentonly { 
				# strip the extensive comments to save space
				#puts $line
			} \
			$rfuncall {
			    # it is a void call to a function. Check that it was declared
			    # as subroutine int, then insert the Tcl_Interp as the first 
			    # parameter.
			    
			    lassign $match -> fname
			    
			    if {$fname eq "xerbla_"} {
				# replace call to xerbla by tcl_xerbla
				# and report error
				set call [regsub {xerbla_\s*\(} $line {vectcl_xerbla(interp, }]
				set line "$call\n"
				append line "return TCL_ERROR;\n"
				print
				continue
			    }

			    if {[dict exists $externals $fname] && 
				[issubroutine [dict get $externals $fname type]] } {
				# gobble up until ;
				set call $line
				while {[string first ";" $line]<0} {
					shift
					append call "$line\n"
				}
				
				if {$mainsubroutine} {
				    # we have the interp available
				    # insert Tcl_Interp, check for return value 
				    set call [regsub {\(} $call {(interp, }]
				    set call [regsub {;} $call {!=TCL_OK) { return TCL_ERROR; }}]
				    set call [regsub {^(\s*)} $call {\1if (}]
				    set line "$call\n"
				} else {
				    # Oh. we don't have the interp available, 
				    # but call a subroutine (which needs it)
				    # pass NULL and hope it works. It will error out
				    # at compile time in the dependent subroutine, if the interp 
				    # is really needed.

				    set line [regsub {\(} $call {(NULL, }]
				}
			    }
			    print
			    
			} \
			$rreturnok {
			    # replace by returning TCL_OK
			    if {$infunction && [issubroutine $rtype]} {
				append body "return TCL_OK;\n"
			    } else {
				print
			    }
			} \
			default {
				if {$infunction} {
					print
				}
			} 
		
	}
}

proc do_linkage {args} {
	variable functions
	variable constantdefs

	set linktable {}
	foreach fun $args {
		dict set linktable $fun undefined
	}

	while true {
		set final true
		dict for {fun included} $linktable {
			if {$included ne "undefined"} continue
			puts "Linking $fun"
			set def [dict get $functions $fun]
			if {![dict exists $def body]} {
				puts stderr "Warning: $fun undefined"
				dict set linktable $fun extern
				continue
			}
			set depends [dict get $def depends]
			foreach f $depends {
				if {![dict exists $linktable $f]} {
					dict set linktable $f undefined
					set final false
				}
			}
			dict set linktable $fun static
		}
		if {$final} break
	}
	
	foreach fun $args {
		dict set linktable $fun MODULE_SCOPE
	}
	return $linktable
}

proc generate_code {f args} {
	# write out code as an amalgamated big C file
	variable functions
	variable constantdefs

	# link the functions
	set subroutines $args
	set linktable [do_linkage {*}$subroutines]

	set fh [file rootname $f].h
	
	set declarations {}
	set funcdefinitions {}
	set constantdecls {}

	set header {}

	dict for {name ltype} $linktable {
		set def [dict get $functions $name]
		dict with def {
			set defline  "$ltype $type $name $args"
		}
		switch $ltype {

			extern {
				# it's not defined here. Just write a declaration
				append declarations "$defline;\n"
			} 
			
			static {
				append declarations "$defline;\n"

				append funcdefinitions "$defline\n"
				append funcdefinitions $body
				
				foreach c $constants {
					dict set constantdecls $c [dict get $constantdefs $c]
				}
			}

			MODULE_SCOPE {
				append header "$defline;\n"

				append funcdefinitions "$defline\n"
				append funcdefinitions $body

				foreach c $constants {
					dict set constantdecls $c [dict get $constantdefs $c]
				}
			}
		}
	}
		
	
	set fdc [open $f w]
	set fdh [open $fh w]

	puts $fdc {/* Generated code. Do not edit. See cherrypick_lapack.tcl */}
	puts $fdc {/* This file contains a subset of LAPACK for use with Tcl/VecTcl */}
	puts $fdc "/* available subroutines: $subroutines  */"
	puts $fdc "#include \"[file tail $fh]\""
	puts $fdc "#include \"f2c_mathlib.h\""
	puts $fdc "/* Declaring the Tcl replacement for xerbla */"
	
	puts $fdc { \
MODULE_SCOPE int vectcl_xerbla(Tcl_Interp *interp, char* func, integer *info);
#pragma clang diagnostic ignored "-Wshift-op-parentheses"
#pragma clang diagnostic ignored "-Wlogical-op-parentheses"
#pragma clang diagnostic ignored "-Wsometimes-uninitialized"}
	# shut up compiler with operator precedence warnings. 
	# f2c really DOES know the operator precedence in C

	puts $fdc "/* declared functions */"
	puts $fdc $declarations

	puts $fdc "/* defined constants */"
	puts $fdc [join [dict values $constantdecls] \n]

	puts $fdc "/* defined functions */"
	puts $fdc $funcdefinitions
	puts $fdc

	puts $fdh {#include "f2c.h"}
	puts $fdh {#include <tcl.h>}
	puts $fdh $header
	close $fdc
	close $fdh
}

proc read_lapack {} {
	variable CLAPACK_DIR
	# these functions should be ignored everywhere
	variable ignore {xerbla_ 1 dlamch_ 1 dlamc3_ 1}
	
	# read in LAPACK
	foreach f [glob -directory $CLAPACK_DIR SRC/*.c] {
		parsef2csource $f
	}
	# read BLAS
	foreach f [glob -directory $CLAPACK_DIR BLAS/SRC/*.c] {
		parsef2csource $f
	}

}

proc run_generator {} {
    #
    variable destfile
    read_lapack
    generate_code $destfile  \
	dgesdd_ zgesdd_ dgemm_ zgemm_ \
	dsyevr_ zheevr_ dgeev_ zgeev_ \
	dgelss_ zgelss_ dgelsy_ zgelsy_ \
	dgesv_ zgesv_ dgesvx_ zgesvx_ \
	dgees_ zgees_ 
}
