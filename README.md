VecTcl 
=====

A numerical array extension for Tcl

This package provides a numerical array extension for Tcl with support for
vectors, matrices and higher-rank tensors of integers, floating point and
complex numbers. It has builtin support for basic array shaping, slicing and
linear algebra subroutines and is designed to integrate seamlessly with Tcl. The
user interface consists of a single command, `vexpr`, which acts as an expression
evaluator similar to `expr`. The language supported by `vexpr` is inspired by
Matlab, which closely models the language used by textbook math.

Example:

    package require vectcl
    namespace import vectcl::vexpr
    # create a vector and multiply by 3
    set x {1 2 3}
    set y [vexpr {3*x}]
    # y is now {3 6 9}

For further documentation, see [the home page on GitHub pages](http://auriocus.github.io/VecTcl/)
