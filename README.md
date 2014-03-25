VecTcl 
=====

A numerical array extension for Tcl

This package provides a numerical array extension for Tcl with support for
vectors, matrices and higher-rank tensors of integers, floating point and
complex numbers. It has builtin support for basic array shaping, slicing and
linear algebra subroutines and is designed to integrate seamlessly with Tcl. The
user interface consists of a single command, _vexpr_, which acts as an expression
evaluator similar to expr. The language supported by vexpr is inspired by
Matlab, which closely models the language used by textbook math.
    
    package require vectcl
    namespace import vectcl::vexpr


The conversion between VecTcl's numeric arrays and Tcl lists is transparently
handled through Tcl's object system:

    # create a vector and multiply by 3
    set x {1.0 2.0 3.0}
    vexpr {3*x}
    # 3.0 6.0 9.0

What already works now
----------------------

Vectcl has builtin support for linear system solving

    # create a matrix
    set A {{2.0 3.0} {5.0 6.0} {7.0 8.0}}
    vexpr { p = A\x ;# solve p = A^-1 x
	    # in the least squares sense if m>n
    }
    # 0.23684210526315772 0.15789473684210545

array slicing, shaping and reductions

    vexpr { A[:,1] = {9 10 11} }
    # {{2.0 9.0} {5.0 10.0} {7.0 11.0}}
    vexpr { A=hstack(x, x.^2) }
    # {1.0 1.0} {2.0 4.0} {3.0 9.0}
    vexpr { sum(x.^2)}
    # 14.0


and complex numbers

    vexpr { list((1+2i)*(3+4i), sinh(2+3i) }
    # -5.0+10.0i -3.5905645899857794+0.5309210862485197i

Vector expressions are compiled into Tcl procedures; the curious can peek into
the compiler output

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

Design decisions
---------------

The value semantics, i.e. implicit conversion from and to lists, has a number of advantages:

- automatic garbage collection by Tcl
- interoperability with commands working on lists, and math::linearalgebra
- the vector language can be extended by Tcl procs
- it is reasonable fast

Currently a (small) patch to the core is required to make shimmering to lists
efficient. It works without, but then the conversion from numeric arrays to
lists goes via the string representation. There are no other dependencies
besides the pt module of tcllib, which is required for the parser. The final
version could use a parser implemented in C. 

TODO
====
	* More examples
	* More linear algebra implementations (LU, eigenvalue, SVD, FFT)
	* Optional LAPACK integration
	* User documentation
	* Source code documentation    
