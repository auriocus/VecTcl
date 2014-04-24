---
layout: default
title: VecTcl
subtitle: A numerical array extension for Tcl
home: true
---

This package provides a numerical array extension for Tcl with support for
vectors, matrices and higher-rank tensors of integers, floating point and
complex numbers. It has builtin support for basic array shaping, slicing and
linear algebra subroutines and is designed to integrate seamlessly with Tcl. The
user interface consists of a single command, `vexpr`, which acts as an expression
evaluator similar to `expr`. The language supported by `vexpr` is inspired by
Matlab, which closely models the language used by textbook math.

VecTcl has the following nice features, which sets it apart from similar packages:

- [easy to use]({{ site.baseurl }}/tutorial.html)
- [reasonably fast]({{ site.baseurl }}/benchmarks.html)
- interoperability with commands working on lists, and math::linearalgebra
- the vector language can be extended by Tcl procs
- automatic garbage collection by Tcl

Example
-------

{% highlight tcl %}
{% raw %}
package require vectcl
namespace import vectcl::vexpr

# create a vector and multiply by 3
set x {1 2 3}
set y [vexpr {3*x}]
# y is now {3 6 9}
{% endraw %}
{% endhighlight %}

Currently a (small) patch to the core is required to make shimmering to lists
efficient. It works without, but then the conversion from numeric arrays to
lists goes via the string representation. There are no other dependencies
besides the pt module of tcllib, which is required for the parser. The final
version could use a parser implemented in C. 

TODO
----
* More examples
* More linear algebra implementations (LU, eigenvalue, <s>SVD, FFT</s>)
* Optional LAPACK integration
* Various performance improvements
* User documentation
* Source code documentation    
