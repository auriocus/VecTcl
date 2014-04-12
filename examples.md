---
title: VecTcl
layout: default
subtitle: Code examples
home: false
---
	
	{% highlight tcl %}
	{% raw %}
    # create a matrix
    set A {{2.0 3.0} {5.0 6.0} {7.0 8.0}}
    vexpr { p = A\x ;# solve p = A^-1 x
	    # in the least squares sense if m>n
    }
    # 0.23684210526315772 0.15789473684210545
	{% endraw %}
	{% endhighlight %}

array slicing, shaping and reductions

	{% highlight tcl %}
	{% raw %}
    vexpr { A[:,1] = {9 10 11} }
    # { {2.0 9.0} {5.0 10.0} {7.0 11.0} }
    vexpr { A=hstack(x, x.^2) }
    # {1.0 1.0} {2.0 4.0} {3.0 9.0}
    vexpr { sum(x.^2)}
    # 14.0
	{% endraw %}
	{% endhighlight %}


and complex numbers

	{% highlight tcl %}
	{% raw %}
    vexpr { list((1+2i)*(3+4i), sinh(2+3i) }
    # -5.0+10.0i -3.5905645899857794+0.5309210862485197i
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



