# create the parser from the grammar using pt
# if it was changed.

set toolpath [file dirname [info script]]

package require textutil::expander
package require fileutil

if {[llength $argv]!=2} {
	puts stderr "Usage: $argv0 input.tcl.c output.c"
	exit -1
}

lassign $argv templatefn resultfn

set template [fileutil::cat $templatefn]
set templatepath [file dirname $templatefn]

set expander [textutil::expander %AUTO%]
$expander setbrackets "\${" "\$}"

proc C {text} {
	$::expander cappend [$::expander expand $text]
	return ""
}

source [file join $templatepath defs.tcl]

fileutil::writeFile $resultfn [$expander expand $template]
