package require Tk
package require vectcl
namespace import vectcl::*

# this demonstration shows how to use VecTcl
# to perform an orthogonal 3D transform on a set of points
# and display it on a canvas.



# define the coordinates for a cube with size 2 in the
# center of the coordsys
# each two coordinates are connected with a line
# first top, then bottom, finally the 4 columns between 
# ceiling and bottom
set	cube {
		{1 -1 1}
		{1 1 1}
		{1 1 1}
		{-1 1 1}
		{-1 1 1}
		{-1 -1 1}
		{-1 -1 1}
		{1 -1 1}
		
		{1 -1 -1}
		{1 1 -1}
		{1 1 -1}
		{-1 1 -1}
		{-1 1 -1}
		{-1 -1 -1}
		{-1 -1 -1}
		{1 -1 -1}

		{1 -1 -1}
		{1 -1  1}
		{-1 -1 -1}
		{-1 -1 1}
		{-1 1 -1}
		{-1 1  1}
		{1 1  -1}
		{1 1   1}
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
canvas .c -width 500 -height 500
# four sliders
ttk::scale .s -variable s -from 1.0 -to 250.0 -command updatePlot
ttk::scale .phi -variable phi -from 0.0 -to 6.28 -command updatePlot
ttk::scale .chi -variable chi -from 0.0 -to 6.28 -command updatePlot
ttk::scale .psi -variable psi -from 0.0 -to 6.28 -command updatePlot

set s 100.0
set phi 0.5
set chi 0.12
set psi 0.0

grid .s -sticky nsew
grid .phi -sticky nsew
grid .chi -sticky nsew
grid .psi -sticky nsew
grid .c -sticky nsew

grid rowconfigure . 0 -weight 1
grid columnconfigure . 0 -weight 1

proc updatePlot {args} {
	.c delete all
	set width [winfo width .c]
	set height [winfo height .c]
	vexpr {
		T = euler(::phi, ::chi, ::psi)
		Tx = ::s*(T*::cube')'
		x = Tx[:, 0]+width/2
		y = -Tx[:, 1]+height/2
	}
	# create lines in the canvas
	foreach {x1 x2} $x {y1 y2} $y {
		.c create line [list $x1 $y1 $x2 $y2]
	}
}

update; # let the geometry propagate
updatePlot
