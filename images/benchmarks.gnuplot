#!/opt/local/bin/gnuplot -persist
#
#    
#    	G N U P L O T
#    	Version 4.4 patchlevel 4
#    	last modified November 2011
#    	System: Darwin 12.5.0
#    
#    	Copyright (C) 1986-1993, 1998, 2004, 2007-2011
#    	Thomas Williams, Colin Kelley and many others
#    
#    	gnuplot home:     http://www.gnuplot.info
#    	faq, bugs, etc:   type "help seeking-assistance"
#    	immediate help:   type "help"
#    	plot window:      hit 'h'
# set terminal x11  nopersist
# set output
set log xy

set style line 1 lt 1 lc rgb 'black' pt 7 ps 1.6 lw 3
set style line 2 lt 1 lc rgb 'blue' pt 5 ps 1.6 lw 3
set style line 3 lt 1 lc rgb 'red' pt 9 ps 1.6 lw 3
set style line 4 lt 1 lc rgb '#008000' pt 11 ps 1.6 lw 3
set style line 5 lt 1 lc rgb '#00A000' pt 10 ps 1.6 lw 3

set title "Only computation"
set ylabel "Speed (Msamples/s)"
set xlabel "# of datapoints"

set yrange [0.01:100]
set terminal post col sol eps enh "Times-Roman" 20
set out "linreg_comp.eps"
plot \
	"benchresult-1397299325.dat" u 1:(1.0/$2) w lp ls 1 title "Tcl", \
	"" u 1:(1.0/$4) w lp ls 2 title "BLT::vector", \
	"" u 1:(1.0/$6) w lp ls 3 title "NAP", \
	"" u 1:(1.0/$8) w lp ls 4 title "VecTcl", \
	"" u 1:(1.0/$10) w lp ls 5 title "VecTcl LS"
set out

set yrange [0.01:10]
set title "Total (setup + computation)"
set out "linreg_total.eps"
plot \
	"benchresult-1397299325.dat" u 1:(1.0/$2) w lp ls 1 title "Tcl", \
	"" u 1:(1.0/($4+$5)) w lp ls 2 title "BLT::vector", \
	"" u 1:(1.0/($6+$7)) w lp ls 3 title "NAP", \
	"" u 1:(1.0/($8+$9)) w lp ls 4 title "VecTcl", \
	"" u 1:(1.0/($10+$9)) w lp ls 5 title "VecTcl LS"
set out

! convert -density 150 linreg_comp.eps -flatten linreg_comp.png
! convert -density 150 linreg_total.eps -flatten linreg_total.png
! rm linreg_comp.eps
! rm linreg_total.eps
#    EOF
