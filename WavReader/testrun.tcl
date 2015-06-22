lappend auto_path .. .
package require vectcl
package require WavReader

namespace import vectcl::*

set wavinput [numarray::readwav /Users/chris/Audio/tinnitus1.wav]
set data [dict get $wavinput data]

puts "Read data, [numarray info $data]"

set samplerate [dict get $wavinput samplerate]
set nsamples [dict get $wavinput nsamples]

set framesize 20 ;# milliseconds

set framesamples [expr {$framesize*$samplerate/1000}]

vexpr {
	# create vector to hold result
	rms=zeros(nsamples/framesamples+1)
	j=0
	for i=0:nsamples-framesamples-1:framesamples {
		frame=data[i:i+framesamples-1,0]
		loud=frame'*frame
		rms[j]=loud
		j=j+1
	}
}

set fd [open loudness.dat w]
puts $fd [join $rms \n]
close $fd
