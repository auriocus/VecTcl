# Small script to assemble the separate chapters to a single file
#
proc putfile {filename} {
   set infile [open $filename]
   puts $::outfile [read $infile]
   close $infile
}

set outfile [open "teadoc.man" "w"]

puts $outfile \
{[manpage_begin {TEA documentation} n 0.2]
[moddesc TEA]
[titledesc {TEA documentation}]}

putfile introduction.txt
putfile design.txt
putfile codingstyle.txt
putfile packages.txt
putfile stubs.txt
putfile makefiles.txt
putfile writingtests.txt
putfile writingdocs.txt
putfile app_makefiles.txt
putfile app_config_options.txt

puts $outfile {[manpage_end]}
close $outfile

