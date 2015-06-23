lappend auto_path .. .
package require vectcl
package require WavReader
package require fileutil

namespace import vectcl::*

proc loudness {fn} {
	set wavinput [wavreader::readwav $fn]
	set data [dict get $wavinput data]

	puts "Read data, [numarray info $data]"

	set samplerate [dict get $wavinput samplerate]
	set nsamples [dict get $wavinput nsamples]
	set channels [dict get $wavinput channels]
	set framesize 20 ;# milliseconds

	set framesamples [expr {$framesize*$samplerate/1000}]

	puts "samplerate=$samplerate, framesamples=$framesamples, channels=$channels, nsamples=$nsamples"

	vexpr {
		# create vector to hold result
		rms=zeros(nsamples/framesamples+1)
		j=0
		for i=0:nsamples-framesamples-1:framesamples {
			frame=data[i:i+framesamples-1,0]
			loud=sqrt(frame'*frame)/framesamples
			rms[j]=loud
			j=j+1
		}
	}
	set ::data $data
	return $rms
}

set zoomdata [loudness /Users/chris/Video/EuroTcl2015/05-AlexandruDadalau/SR001XY.WAV]
fileutil::writeFile zoomloud.dat [join $zoomdata \n]
set vid1data [loudness /Users/chris/Video/EuroTcl2015/05-AlexandruDadalau/MVI_9905.wav]
fileutil::writeFile vid1loud.dat [join $vid1data \n]

# computing the crosscorrelation
set corrsize 1000 ;# 1000 frames = 20 seconds
set offset 500 ;# ten seconds after start, to remove powering noises

vexpr {
	fingerprint = (vid1data[offset:offset+corrsize-1])'
	fingerprint -= mean(fingerprint,1)
	fingerprint ./= sqrt(fingerprint*fingerprint')
	
	maxind=0
	max=0
	corrdata=zeros(rows(zoomdata))
	for i=0:rows(zoomdata)-offset-corrsize-1 {
		test=zoomdata[i+offset:i+offset+corrsize-1]
		test-=mean(test)
		# normalized correlation
		n=sqrt(test'*test)
		if n==0 { puts(test) } else {
		crosscorr=fingerprint*test ./ sqrt(test'*test)
		corrdata[i]=crosscorr }
		if crosscorr > max {
			max = crosscorr
			maxind=i
		}
	}
}


fileutil::writeFile crosscorr.dat [join $corrdata \n]

puts "Offset: [expr {$maxind*0.02}] s"
puts "Length: [vexpr {rows(vid1data)*0.02}]"
