lappend auto_path ~/Programmieren/VecTcl/ ~/Programmieren/VecTcl/TkBridge/

package require vectcl
namespace import vectcl::*

package require Tk
catch {package require Img}
package require vectcl::tk
package require snit
package require ctext

set samplecode {
	ParametricPlot {
		code {
			# square grid. Pull the sliders 1 and 2
			# to change the wavelength
			sin(r1.*x).*sin(r2.*y)
		}
		xmin -5 xmax 5
		ymin -5 ymax 5
		r1min 0.0 r1max 2.0 r2min 0.0 r2max 2.0
		normalize 1
	}

	PlotComplex {
		code {
			# plotting the complex square root
			# as red and green channels. Pull slider 1
			# to adjust the size
			
			z=sqrt(x/r1+y/r1*1i)
			display=ones(height,width,3)
			display[:,:,0]=real(z)
			display[:,:,1]=imag(z)
			display[:,:,2]=0
		}
		xmin -2 xmax 5 
		ymin -3 ymax 5
		r1min 1.0 r1max 5.0
		normalize 0
	}


	Polkadots {
		code {
			# Same plot as Parametric
			# Plot, but thresholded pull
			# slider 3 to change from
			# square grid to polka dot
			# pattern, and sliders 1 and
			# for wavelength
			double(sin(r1.*x).*sin(r2.*y) > r3)
		}
		xmin -10 xmax 10
		ymin -10 ymax 10
		r1min 0.0 r1max 2.0 r2min 0.0 r2max 2.0
		r3min 0.0 r3max 1.0
	}

	Wiggle {
		code {
			# simulate water waves. Try
			# animating slider 2 or 3.
			z=input
			for i=0:height-1 {
				xt=max(0,int(r1*(1+sin(r2*i+r3))))
				z[i,xt:-1,:]=input[i,0:-xt-1,:]
			}
			z
		}
		r1min 0.0 r1max 10.0 r1raw 0.2 r2min 0.05 r2max 0.5
		r3min 0.0 r3max 62.832 normalize 0

	}
	
	Alpha {
		code {
			# fading an image out using
			# the alpha channel
			z=input
			z[:,:,3].*=r1
		}
		r1min 0.0 r1max 2.0 normalize 0
	}

	Daytime {
		code {
			# simulate sunshine and
			# night shadow. 
			z=input
			z[:,:,0:1].*=r1
		}
		r1min 0.0 r1max 2.0 normalize 0
	}
}

#################
# main application - implemented as a widget 
# for toplevels. This way, we could later reuse it for 
# another toplevel (i.e. popup) easily
#########################

snit::widgetadaptor ImgCalculator { 
	
	component pathbtn 
	
	variable imgdisp
	variable xmin 0
	variable xmax 10
	variable ymin 0
	variable ymax 10
	variable width 300
	variable height 300

	variable r1min 0.0
	variable r1max 2.0
	variable r2min 0.0
	variable r2max 2.0
	variable r3min 0.0
	variable r3max 2.0

	variable r1raw 0.5
	variable r2raw 0.5
	variable r3raw 0.5

	variable code {}
	variable normalize 1

	# image input
	variable input {} 

	component codeedit
	component codeselect
	variable codeletname
	component runbtn
	component loadbtn

	component slider1
	component slider2
	component slider3

	variable playbuttons

	constructor {args} {
		installhull $win

		# for the main toplevel we must cheat
		if {$win=="."} { set wpref "" } else { set wpref $win }
		
		set mfr [ttk::frame $wpref.mfr]
		pack $mfr -expand yes -fill both

		set cntfr [ttk::frame $mfr.cntfr]
		
		set imgdisp [image create photo]
		set dsplbl [label $mfr.display -image $imgdisp]

		grid $cntfr $dsplbl -sticky nsew
		grid columnconfigure $mfr $cntfr -weight 1
		grid rowconfigure $mfr $cntfr -weight 1

		install codeselect using ttk::combobox $cntfr.codeselect -state readonly \
			-values [dict keys $::samplecode] -textvariable [myvar codeletname]

		bind $codeselect <<ComboboxSelected>> [mymethod loadsamplecode]
		
		set codefont [font create -size 20 -family Courier]
		install codeedit using ctext $cntfr.codeedit -font $codefont -width 40 -tabs 2c
		# create simple syntax highlighting
		ctext::addHighlightClass $codeedit keywords {#00A000} {if while for}
		ctext::addHighlightClassForRegexp $codeedit number {#FF80FF} {\y\d+(\.\d+)?((e|E)(\+|-)?\d+)?(i|I)?\y}
		ctext::addHighlightClassForRegexp $codeedit operator {#A02000} {(\.?[-+*/\\])|==|<|>|!=|<=|>=|%|not|\|\||&&}
		ctext::addHighlightClassForRegexp $codeedit comment {#3030A0} {#[^\n]*}
		ctext::addHighlightClassForRegexp $codeedit builtins {#3030FF} {\y(create|constfill|eye|info|dimensions|shape|reshape|transpose|adjoint|slice|concat|diag|int|double|complex|abs|sign|real|imag|arg|conj|sin|cos|tan|exp|log|log10|sqrt|sinh|cosh|tanh|asin|acos|atan|asinh|acosh|atanh|qreco|eigv|eig|svd1|svd|schur|sum|min|max|mean|std|std1|all|any|fft|ifft)\y}
		# color linenumbers
		$codeedit configure -linemapfg #B06000 -linemapbg grey90 -highlightthickness 0 -padx 3

		set btnbar [ttk::frame $cntfr.btnbar]
		# also show a run button
		set runBtn [ttk::button $btnbar.runbtn -text "Run!" -command [mymethod Run]]
		set loadBtn [ttk::button $btnbar.loadbtn -text "Open..." -command [mymethod OpenGUI]]
		set normBtn [ttk::checkbutton $btnbar.norm -text "Normalize" -variable [myvar normalize]]
		pack $runBtn $loadBtn $normBtn -side left


		# Control for x- and y-ranges
		
		set rngfr [ttk::frame $cntfr.rangefr]

		set wlbl [ttk::label $rngfr.wlbl -text "Width"]
		set went [ttk::entry $rngfr.went -textvariable [myvar width] -width 10]
		
		set xminent [ttk::entry $rngfr.xminent -textvariable [myvar xmin]]
		set xlbl [ttk::label $rngfr.xlbl -text "<= x <="]
		set xmaxent [ttk::entry $rngfr.xmaxent -textvariable [myvar xmax] -width 10]

		set hlbl [ttk::label $rngfr.hlbl -text "Height"]
		set hent [ttk::entry $rngfr.hent -textvariable [myvar height] -width 10]
		
		set yminent [ttk::entry $rngfr.yminent -textvariable [myvar ymin] -width 10]
		set ylbl [ttk::label $rngfr.ylbl -text "<= y <="]
		set ymaxent [ttk::entry $rngfr.ymaxent -textvariable [myvar ymax] -width 10]

		grid $wlbl $went $xminent $xlbl $xmaxent -sticky nsew
		grid $hlbl $hent $yminent $ylbl $ymaxent -sticky nsew

		# create sliders
		set slfr [ttk::frame $cntfr.slfr]
		foreach sl {1 2 3} {
			ttk::entry $slfr.rmin$sl -textvariable [myvar r${sl}min] -width 10
			ttk::entry $slfr.rmax$sl -textvariable [myvar r${sl}max] -width 10
			install slider$sl using ttk::scale $slfr.$sl -from 0.0 -to 1.0 \
				-variable [myvar r${sl}raw] -command [mymethod Run]
			lappend playbuttons [ttk::button $slfr.ranim$sl -text "\u25b6" -command [mymethod AnimateSwitch r${sl}] -style Toolbutton]
			grid $slfr.rmin$sl $slfr.$sl $slfr.ranim$sl $slfr.rmax$sl -sticky nsew 
		}
		grid columnconfigure $slfr 1 -weight 1

		grid $codeselect -sticky nsew
		grid $codeedit -sticky nsew
		grid $btnbar -sticky nsew
		grid $rngfr -sticky nsew
		grid $slfr -sticky nsew
		
		grid columnconfigure $cntfr 0 -weight 1
		grid rowconfigure $cntfr $codeedit -weight 1
		
		# now load the hugarian desert image
		$self Open [file join [file dirname [info script]] somloi_galuska.png]
	}

	destructor {
		$self AnimateStop
	}

	method Run {args} {
		# compute value for sliders
		vexpr {
			r1 = r1min+(r1max-r1min)*r1raw
			r2 = r2min+(r2max-r2min)*r2raw
			r3 = r3min+(r3max-r3min)*r3raw
			x=ones(height)*linspace(xmin,xmax,width)'
			y=linspace(ymin,ymax,height)*ones(width)'
		}
		
		set code [$codeedit get 1.0 end]

		set time [time {
			set result [vexpr $code]
		}]
		
		if {$normalize} {
			vexpr {
				zmin=min(min(result))
				zmax=max(max(result))
				result -= zmin
				result ./= (zmax-zmin)
			}
		}
		#puts $time
		numarray::toPhoto $result $imgdisp

	}


	method Open {fn} {
		$imgdisp configure -width 0 -height 0
		$imgdisp read -shrink $fn
		set input [numarray::fromPhoto $imgdisp]

		set width [image width $imgdisp]
		set height [image height $imgdisp]
		set normalize 0
		
		vexpr {
			xmin=0
			xmax=width-1
			ymin=0
			ymax=height-1
		}
	}

	method OpenGUI {} {
		set fn [tk_getOpenFile -title {select any image, it should fit on the screen}]
		if {$fn ne {}} { $self Open $fn }
	}	

	method loadsamplecode {} {
		set codelet [dict get $::samplecode $codeletname]
		# assign all variables from this code - I like the danger :)
		dict for {variable value} $codelet {
			set $variable $value
		}
			
		# remove superfluous indentation
		set codeline [lrange [split $code \n] 1 end]
		regexp {^\s+} [lindex $codeline 0] ws
		set skiplen [string length $ws]
		set code {}
		foreach line $codeline {
			set wstart [string range $line 0 $skiplen-1]
			if {$wstart eq $ws} {
				set line [string range $line $skiplen end]
			} else {
				# puts ">$wstart< != >$ws<"
			}
			append code "$line\n"
		}

		$codeedit delete 1.0 end
		$codeedit insert end $code
		$codeedit highlight 1.0 end
		$self Run
	}

	variable AnimateID {}
	method Animate {register} {
		upvar 0 ${register}raw reg
		set reg [expr {$reg+0.01}]
		if {$reg > 1.0} { set reg 0.0 }
		set AnimateID [after 50 [mymethod Animate $register]]
		if {[catch {$self Run} err]} {
			$self AnimateStop
			error $err
		}
		update idletask
	}
	
	method AnimateStop {} {
		after cancel $AnimateID
		set AnimateID {}
	}

	method AnimateSwitch {reg} {
		if {$AnimateID eq {}} {
			foreach btn $playbuttons {
				$btn configure -text "\u2161"
			}
			$self Animate $reg
		} else {
			$self AnimateStop
			foreach btn $playbuttons {
				$btn configure -text \u25b6
			}
		}
	}
}

# now instantiate the main application

if {[wm state .] eq "normal"} {
	ImgCalculator .
} else {
	toplevel .imgcalc
	ImgCalculator .imgcalc
}

