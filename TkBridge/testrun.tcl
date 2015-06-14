lappend auto_path . ..
package require vectk

namespace import vectcl::*

image create photo display0
pack [label .l -image display0]

proc display {na} {
	# display a numarray for 1 s
	numarray::toPhoto $na display0
	after 1000 {set ::_ 1}
	vwait ::_
}


vexpr {
	x=ones(100)*linspace(0,10,100)'
	y=linspace(0,10,100)*ones(100)'
	z=0.5+0.5*sin(x+y).*cos(x)
}

# display the plot
display $z

# load image
set sgimg [image create photo -file somloi_galuska.png]
set sg [numarray::fromPhoto $sgimg]
display $sg

# procss the image
vexpr {
	z=sg*0.3; # darken 
	z[:,:,3]=1.0; # reset alpha
}
display $z

