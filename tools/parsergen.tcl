# create the parser from the grammar using pt
# if it was changed.
package require pt::pgen

if {[llength $argv]!=2} {
	puts stderr "Usage: $argv0 <grammar.peg> <parserules.h>"
	exit -1
}

lassign $argv grammarfn parserfn

set fd [open $grammarfn r]
set grammar [read $fd]
close $fd

set parser [pt::pgen peg $grammar c -main StartSymbol]

set fd [open $parserfn w]
puts $fd $parser
close $fd

