# copy any C files from the CLAPACK sources
# that are required to create the functions listed
# in the arguments. i.e. a "source code linker"

set CLAPACK_DIR /Users/chris/Sources/CLAPACK-3.2.1/

set tooldir [file dirname [info script]]
set destdir [file join [file dirname $tooldir] clackap_cutdown]

# catch {file mkdir $destdir}

# helper procs for parsing
proc shift {} {
	upvar 1 i i
	upvar 1 line line
	upvar 1 lines lines
	incr i
	set line [lindex $lines $i]
	return $line
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


proc parsef2csource {f} {
	# read a CLAPACK source file
	# and rip the pieces apart
	# such that the functions can be freely
	# combined in a singe source file

	variable functions
	variable constantdefs

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
				set arglist "($arglist"
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

				# now we must parse the extern declarations. 
				# There can be more than one function on the same
				# extern line
				regexp  $rfullfuncdecl $extrn all etype ename eargs
				
				dict set externals $ename type $etype
				dict set externals $ename args $eargs 
				
				#puts $all 
				set start [string length $all]
				set matches [regexp -all -inline -start $start $rpartfuncdecl $extrn]
				#puts "$matches"
				foreach {-> ename eargs} $matches {
					dict set externals $ename type $etype
					dict set externals $ename args $eargs
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
			#file copy -force $src $destdir
		}
		if {$final} break
	}
	
	foreach fun $args {
		dict set linktable $fun MODULE_SCOPE
	}
	return $linktable
}

proc generate_code {f linktable} {
	# write out code as an amalgamated big C file
	variable functions
	variable constantdefs

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
	puts $fdc "#include \"$fh\""
	puts $fdc "#include \"f2c_mathlib.h\""
	
	puts $fdc "/* declared functions */"
	puts $fdc $declarations

	puts $fdc "/* defined constants */"
	puts $fdc [join [dict values $constantdecls] \n]

	puts $fdc "/* defined functions */"
	puts $fdc $funcdefinitions
	puts $fdc

	puts $fdh {#include "f2c.h"}
	puts $fdh $header
	close $fdc
	close $fdh
}

proc read_lapack {} {
	variable CLAPACK_DIR
	# read in LAPACK
	foreach f [glob -directory $CLAPACK_DIR SRC/*.c] {
		parsef2csource $f
	}
	# read BLAS
	foreach f [glob -directory $CLAPACK_DIR BLAS/SRC/*.c] {
		parsef2csource $f
	}
}


