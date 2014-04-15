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

set key top left reverse Left

set style line 1 lt 1 lc rgb 'black' pt 7 ps 1.6 lw 3
set style line 2 lt 1 lc rgb 'blue' pt 5 ps 1.6 lw 3
set style line 3 lt 1 lc rgb 'red' pt 9 ps 1.6 lw 3
set style line 4 lt 1 lc rgb 'red' pt 11 ps 1.6 lw 3
set style line 5 lt 1 lc rgb '#008000' pt 10 ps 1.6 lw 3
set style line 6 lt 1 lc rgb '#008000' pt 6 ps 1.6 lw 3
set style line 7 lt 1 lc rgb '#D00080' pt 8 ps 1.6 lw 3

set title "Only computation"
set ylabel "Speed (MSamples/s)"
set xlabel "# of datapoints"

set yrange [0.05:500]
set terminal post col sol eps enh "Times-Roman" 20
set out "linreg_comp.eps"
plot \
	"benchresult-1397542004.dat" u 1:(1.0/$2) w lp ls 1 title "Tcl", \
	"" u 1:(1.0/$14) w lp ls 7 title "C", \
	"" u 1:(1.0/$4) w lp ls 2 title "BLT::vector", \
	"" u 1:(1.0/$6) w lp ls 3 title "NAP", \
	"" u 1:(1.0/$8) w lp ls 4 title "NAP LS", \
	"" u 1:(1.0/$10) w lp ls 5 title "VecTcl", \
	"" u 1:(1.0/$12) w lp ls 6 title "VecTcl LS"
set out

set yrange [0.01:10]
set title "Total (setup + computation)"
set out "linreg_total.eps"
plot \
	"benchresult-1397542004.dat" u 1:(1.0/$2) w lp ls 1 title "Tcl", \
	"" u 1:(1.0/($4+$5)) w lp ls 2 title "BLT::vector", \
	"" u 1:(1.0/($6+$7)) w lp ls 3 title "NAP", \
	"" u 1:(1.0/($8+$9)) w lp ls 4 title "NAP LS", \
	"" u 1:(1.0/($10+$11)) w lp ls 5 title "VecTcl", \
	"" u 1:(1.0/($12+$11)) w lp ls 6 title "VecTcl LS"
set out

set yrange [0.05:50]
set title "Vectors"
set xlabel "Array size (bytes)"
set ylabel "Memory bandwidth (GB/s)"
set out "memcpy_vectors.eps"
set key bottom right
i=0
plot \
	"benchmemcpy-1397384533.dat" u 1:2 index i w lp ls 1 title "memcpy", \
	"" u 1:3 index i w lp ls 2 title "C loop x+=y", \
	"" u 1:4 index i w lp ls 3 title "sum(x)", \
	"" u 1:5 index i w lp ls 4 title "x[:]=y", \
	"" u 1:6 index i w lp ls 5 title "x+=y", \
	"" u 1:7 index i w lp ls 6 title "-x", \
	"" u 1:8 index i w lp ls 7 title "x+y"
set out

i=1
set title "Square matrices (N x N)"
set out "memcpy_square.eps"
replot

i=2
set title "Wide matrices (2 x N)"
set out "memcpy_wide.eps"
replot

i=3
set title "Tall matrices (N x 2)"
set out "memcpy_tall.eps"
replot
set out

! convert -density 150 linreg_comp.eps -flatten linreg_comp.png
! rm linreg_comp.eps

! convert -density 150 linreg_total.eps -flatten linreg_total.png
! rm linreg_total.eps

! convert -density 150 memcpy_vectors.eps -flatten memcpy_vectors.png
! rm memcpy_vectors.eps

! convert -density 150 memcpy_square.eps -flatten memcpy_square.png
! rm memcpy_square.eps

! convert -density 150 memcpy_wide.eps -flatten memcpy_wide.png
! rm memcpy_wide.eps

! convert -density 150 memcpy_tall.eps -flatten memcpy_tall.png
! rm memcpy_tall.eps
#    EOF
