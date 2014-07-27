package require Tk
package require vectcl
namespace import vectcl::*

# this demonstration shows how to use VecTcl
# to perform an orthogonal 3D transform on a set of points
# and display it on a canvas.

set w .cube
catch {destroy $w}
toplevel $w

# create random lines
set	data {}
for {set i 0} {$i<1000} {incr i} {
	set r1 [expr {rand()-0.5}]
	set r2 [expr {rand()-0.5}]
	set r3 [expr {rand()-0.5}]
	lappend data [list $r1 $r2 $r3]
}

vproc eulerx {phi} {
	list( \
		list(1.0, 0.0, 0.0), \
		list(0.0, cos(phi), sin(phi)), \
		list(0.0, -sin(phi), cos(phi)))
}

vproc eulery {phi} {
	list( \
		list(cos(phi), 0.0, sin(phi)), \
		list(0.0, 1.0, 0.0), \
		list(-sin(phi), 0.0, cos(phi)))
}

vproc eulerz {phi} {
	list( \
		list(cos(phi), sin(phi), 0.0), \
		list(-sin(phi), cos(phi), 0.0), \
		list(0.0, 0.0, 1.0))
}

vproc euler {phi chi psi} {
	# this function returns the 
	# subsequent rotation around the axis x,y,z
	# with angles phi, chi, psi
	# it is slow, but only called once for every frame
	eulerz(psi)*eulery(chi)*eulerx(phi)
}

# create a canvas 
canvas $w.c -width 500 -height 500
# four sliders
ttk::scale $w.s -variable s -from 10.0 -to 500.0 -command updatePlot
ttk::scale $w.phi -variable phi -from 0.0 -to 6.28 -command updatePlot
ttk::scale $w.chi -variable chi -from 0.0 -to 6.28 -command updatePlot
ttk::scale $w.psi -variable psi -from 0.0 -to 6.28 -command updatePlot

set s 300.0
set phi 0.5
set chi 0.12
set psi 0.0

grid $w.s -sticky nsew
grid $w.phi -sticky nsew
grid $w.chi -sticky nsew
grid $w.psi -sticky nsew
grid $w.c -sticky nsew

grid rowconfigure $w 0 -weight 1
grid columnconfigure $w 0 -weight 1


proc updatePlot {args} {
	variable w
	set width [winfo width $w.c]
	set height [winfo height $w.c]

	lassign [do3DTransform $::data $width $height] x y
	
	$w.c delete all
	# create lines in the canvas - shimmers to list
	foreach {x1 x2} $x {y1 y2} $y {
		$w.c create line [list $x1 $y1 $x2 $y2]
	}
}

vproc do3DTransform {data width height} {
	T = euler(::phi, ::chi, ::psi)
	Tx = ::s*(T*::data')'
	x = Tx[:, 0]+width/2
	y = -Tx[:, 1]+height/2
	list(x,y)
}

update; # let the geometry propagate
updatePlot
