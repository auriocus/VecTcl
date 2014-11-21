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

set key bottom right
set terminal post col dash dl 4 eps enh "Times-Roman" 20

set style line 1 lt 1 lc rgb 'black' pt 7 ps 1.6 lw 3
set style line 2 lt 1 lc rgb 'blue' pt 5 ps 1.6 lw 3
set style line 3 lt 1 lc rgb 'red' pt 9 ps 1.6 lw 3
set style line 4 lt 2 lc rgb 'red' pt 11 ps 1.6 lw 3
set style line 5 lt 1 lc rgb '#008000' pt 10 ps 1.6 lw 3
set style line 6 lt 1 lc rgb '#008000' pt 6 ps 1.6 lw 3
set style line 7 lt 1 lc rgb '#D00080' pt 8 ps 1.6 lw 3

set title "Compiler backends"
set ylabel "Speed (MSamples/s)"
set xlabel "# of datapoints"

set yrange [2:1000]
set out "compilerbench.eps"
plot \
	"squaresbench.dat" u 1:($1/$2) w lp ls 1 title "VecTcl", \
	"" u 1:($1/$3) w lp ls 4 title "VecTcl-tcc", \
	"" u 1:($1/$4) w lp ls 3 title "VecTcl-tcc-tweak", \
	"" u 1:($1/$5) w lp ls 2 title "VecTcl-gcc"
set out

! convert -density 150 compilerbench.eps -flatten compilerbench.png
! rm compilerbench.eps
#    EOF
