---
title: VecTcl
layout: default
subtitle: VecTcLab
home: false
permalink: "vectclab.html"
---

VecTcLab
-------------------------
VecTcLab is a try to emulate the way of working with linear algebra packages 
such as Matlab, octave, or SciLab, within a Tcl/VecTcl powered environment.
It consists of a modified tkcon, an interactive shell for Tcl, with all the necessary
packages preloaded to do vector math and simple plotting. It can be found on the download
page. 

When you fire up VecTcLab, a command window opens with an interactive prompt. 

First, we will do simple matrix calculations
{% highlight tcl %}
{% raw %}
(VecTcLab) 29 % x={1 2 3} 
{1 2 3}
(VecTcLab) 30 % A=x*x' 
{{1 2 3}
 {2 4 6}
 {3 6 9}}
(VecTcLab) 31 % A * x 
{14 28 42}
(VecTcLab) 32 % 
{% endraw %}
{% endhighlight %}

Commands entered are first evaluated as a VecTcl expressino, and on failure 
reevaluated as a Tcl command. This allows us to mix Tcl with math in VecTcl.

For instance, we construct a x-y dataseries for plotting
{% highlight tcl %}
{% raw %}
x=linspace(1,10,100); y=sin(x); 1
# Compute x-y data for a sine wave.
# the 1 is here to suppress the output
s=figure()
# a plot window appears
$s plot $x $y with linespoints pointtype squares
# the sine wave is plottedt with connected squares
{% endraw %}
{% endhighlight %}
The plot should look like this:
![Sine wave plot]({{ site.baseurl }}/images/firstplot.png)
Drag a rectangle with the left mouse button to zoom into this plot. Clicking
with the right mouse button takes you back. Several properties of the plot
can be manipulated with commands inspired by gnuplot. 

For instance, try
{% highlight tcl %}
{% raw %}
$s set log x
# switch x axis to logarithmic scale
$s set grid on
# display a regular grid of lines
y2=y.^2
$s plot $x $y2 with lines color blue title "Squared"
# plot another curve on top, with blue lines
# and a legend
$s set key bottom 
# move the legend to the bottom
$s update 0 title "Sine wave"
# add a legend to the first dataset. The 0 is the dataset identifier, 
# as returned from the plot command
$s saveAsPDF sinesqur.pdf
# write the current plot in PDF format to disk
{% endraw %}
{% endhighlight %}
Now a PDF should be generated which should look like this:
![Sine wave plot]({{ site.baseurl }}/images/sinesqur.png)



