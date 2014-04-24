---
title: VecTcl
layout: default
subtitle: Tutorial
home: false
permalink: "tutorial.html"
---

Basic VecTcl usage
-------------------------
In VecTcl, there is no distinction between a Tcl list and a vector or matrix. 
They are created by setting a variable with a list of doubles:

{% highlight tcl %}
{% raw %}
# create a vector
set x { 1 2 3 }
# create a matrix
set A {{1.0 2.0 3.0} {4.0 5.0 6.0} {7.0 8.0 9.0}}
{% endraw %}
{% endhighlight %}

Of course, list commands such as `list`, `lappend`, `linsert`, `lrepeat` etc. 
can also be used. To evaluate an expression involving vector operations, pass
the expression to `vexpr`:

{% highlight tcl %}
{% raw %}
vexpr { A*x } ;# compute the matrix-vector product
# 14.0 32.0 50.0
{% endraw %}
{% endhighlight %}

In order for this to work, you must first load the package and import the commands:
{% highlight tcl %}
{% raw %}
package require vectcl
namespace import vectcl::*
{% endraw %}
{% endhighlight %}

Vectors can contain integers, floating-point values or complex numbers:
{% highlight tcl %}
{% raw %}
set x {1 2 3} ;# an integer vector
set y {2.0 3.0 5.0} ;# a floating-point vector
set z {0+1i 2+3.5i 3.0+0i} ;# a complex vector
{% endraw %}
{% endhighlight %}

VecTcl includes support for linear equation solving 
{% highlight tcl %}
{% raw %}
vexpr { x = A\y ;# solve A x = y for x
	# in the least squares sense if m>n
}
{% endraw %}
{% endhighlight %}

array slicing, shaping and reductions
{% highlight tcl %}
{% raw %}
# define a vector with 3 elements 
set x {1 2 3}
# ...and a 3x2 matrix 
set A {{2.0 3.0} {5.0 6.0} {7.0 8.0}}

# replace column 1 in A with {9 10 11}
# indices start from 0
vexpr { A[:,1] = {9 10 11} }
# { {2.0 9.0} {5.0 10.0} {7.0 11.0} }

# create a matrix with columns x and x.^2
vexpr { A=hstack(x, x.^2) }
# {1.0 1.0} {2.0 4.0} {3.0 9.0}
vexpr { sum(x.^2)}
# 14.0
{% endraw %}
{% endhighlight %}

and elementary transcendental functions
{% highlight tcl %}
{% raw %}
vexpr { sinh(2+3i) } ;# complex hyperbolic sine
# -3.5905645899857794+0.5309210862485197i
{% endraw %}
{% endhighlight %}

Any Tcl command can be called as a function
{% highlight tcl %}
{% raw %}
set x {1 2 3}
vexpr { n=llength(x); puts(n) } 
# writes 3 to stdout
# Caveat: llength(x) is inefficient, it
# involves a conversion to a list. Use rows(x) instead.
{% endraw %}
{% endhighlight %}

Not only short expressions are supported. Looping and branching make it possible 
to write larger math functions in a single expression
{% highlight tcl %}
{% raw %}
vexpr { 
	for i=1:5 {
		if i!=2 {
			puts(i)
		}
	}
} 
{% endraw %}
{% endhighlight %}

A second command, `vproc` defines a procedure fully in terms of a VecTcl expression
{% highlight tcl %}
{% raw %}
vproc rms {x} {
	# compute the root mean square
	xm=mean(x)
	sqrt(mean((x-xm).^2))
} 
{% endraw %}
{% endhighlight %}

Vector expressions are compiled into Tcl procedures; the curious can peek into
the compiler output

{% highlight tcl %}
{% raw %}
vectcl::compile {
	x, y = list(y, x) ;# swap x and y
	A= -3*x
}
# this outputs:
upvar 1 y y
upvar 1 x x
upvar 1 A A
set __temp1 [list [set y] [set x]]
lassign $__temp1 x y

set A [numarray::neg [numarray::* 3 [set x]]]
{% endraw %}
{% endhighlight %}



