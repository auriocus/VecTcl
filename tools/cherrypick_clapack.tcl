# copy any C files from the CLAPACK sources
# that are required to create the functions listed
# in the arguments. i.e. a "source code linker"

set CLAPACK_DIR /Users/chris/Sources/CLAPACK-3.2.1/

set tooldir [file dirname [info script]]
set destdir [file join [file dirname $tooldir] clackap_cutdown]

catch {file mkdir $destdir}

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

	# a regexp matching any of f2c's return types /* Subroutine */ int 
	set rdtypes {(\minteger|\mlogical|\mreal|\mdoublereal|\mcomplex|\mdoublecomplex|/\* Subroutine \*/ int)\M}
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
	set rfullfuncdecl "${rdtypes}\\s+$rident${rarglist}"
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
				append constants "static $type $prefid = $value;\n"
				puts "Renaming $id -> $prefid"

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
				
				set start [string length $all]
				set matches [regexp -all -inline -start $start $rpartfuncdecl $extrn]
				puts "$matches"
				foreach {-> ename eargs} $matches {
					dict set externals $ename type $etype
					dict set externals $ename args $eargs
				}

			} \
			"^$brc" {
				# the only place an unindented brace can be
				# is the end of a toplevel subroutine

				print
				puts "Function $name, returns $rtype, args $arglist"
				puts "Depending on [dict keys $externals]"
				# put into global function list
				dict set functions $name type $rtype
				dict set functions $name args $arglist
				dict set functions $name depends [dict keys $externals]
				dict set functions $name constants $constants
				dict set functions $name body $body
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
	set linktable {}
	foreach fun $args {
		dict set linktable $fun undefined
	}

	while true {
		set final true
		dict for {fun included} $linktable {
			if {$included ne "undefined"} continue
			puts "Linking $fun"
			if {[catch {dict get $functions $fun} def]} {
				puts stderr "Warning: $fun not found"
				dict set linktable $fun external
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
		dict set linktable $fun exported
	}
	return $linktable
}

