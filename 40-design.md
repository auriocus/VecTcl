---
layout: default
title: VecTcl
subtitle: Design and implementation issues
downloads: false
permalink: "design.html"
---

Design principles of VecTcl
--------------------------

VecTcl was designed in a way that integrates numerical computing tools as closely with Tcl as
possible. VecTcl was defined to adhere to these general objectives:

1. _Ease of use_ 
2. _Interoperability_
3. _Generality_
5. _Performance_

These terms are interpreted within VecTcl in the following sense:

1. _Ease of use._ The syntax should be as close to textbook notation of numerical mathematics as
possible. This goal is largely achieved by borrowing a set of syntactical elements and essential
functions from the array language supported by MATLAB and NumPy, two popular computational tools
outside the Tcl world.
2. _Interoperability._ Tcl already has a datatype suitable for dealing with sequences of numbers,
namely a list of integer or double values. Matrices and higher-rank tensors are most naturally
expressed as nested lists. It is desirable, that the vector type can be converted to and from the
list representation with ease, preferably without an explicit conversion step. In this way, code
using VecTcl can seamlessly interface to code written in pure Tcl, like math::linearalgebra, which
uses the same encoding, and to other packages supporting sequences of numbers. Value semantics also
allows Tcl procs to extend the language, pass vectors back and forth as first class objects, and
leave memory management to the Tcl interpreter.  It should be noted, that VecTcl does _NOT_ use the
list representation at the C level, nor is hidden information carried along that breaks the value
semantics. 
3. _Generality._ There should be no arbitrary limitation on the number of dimensions or the size of
the objects. The vector engine should not be limited to 3D vectors and support higher-rank tensors
both with syntax and in the backend as well as matrices and vectors.
4. _No external dependencies._ Besides what is required to compile Tcl itself, no external libraries
should be required to compile nor run the code.
This means that the code must be written in pure C, use TEA and stubs, generated code
must be included prebuilt within the package. A few 
[high-quality BSD compatible libraries](credits.html) have been incorporated into the project.
Faster alternatives for these libraries exist, but have been ruled out due to incompatible license
or codesize. Facultative dependency on these external libraries would be acceptable as a
compile-time choice.
5. _Performance._ In order to justify a C level extension, both memory footprint and execution speed
of the computation should be kept as efficient as possible, and in any case better than what could be
achieved with a Tcl level implementation.

It is clear, that these objectives compete, and so a certain compromise must be achieved. For
example, performance could be improved by adding external dependencies, or usage could be simplified
by loss of generality (e.g. restricting the values to two-dimensional matrices instead of N-rank
tensors). The following sections give an overview over the compromise sought, what is achieved and
what is planned or desirable. 

Implementation details
----------------------
VecTcl is implemented as a two-layered system. At the bottom lies a new Tcl_ObjType, NumArray,
implemented in C together with a number of commands to manipulate it. The commands live in the
namespace `numarray` and comprise elementary operations on numerical data like array shaping and
slicing, elementwise binary operators, logical comparisons, matrix product operations, linear
equation solving, elementary transcendental functions. On top of that, an expression compiler
written in Tcl transforms a sequence of mathematical expressions from infix notation into nested
function calls, which are then executed by Tcl. 

NumArray is a polymorphic datatype which represents an N-rank tensor of integer, floating point or
complex floating point type. The internal representation consists of two data structures, pointed to
by the twoPtrValue-fields in Tcl_Obj. The first structure `NumArraySharedBuffer` stores the data of
the NumArray in a contiguous memory buffer in native machine representation, refcounted and shared
between a NumArray and derived slices. The second structure `NumArrayInfo` contains the describing
metadata
{% highlight c %}
{% raw %}
typedef struct  {
	NumArrayType type;
	int nDim;
	int bufsize;
	int offset;
	int canonical;
	int *dims;
	int *pitches;
} NumArrayInfo;
{% endraw %}
{% endhighlight %}

The important fields in this structure are the number of dimensions `nDim`, 
number of elements in each dimension `dims`, the offset of the
first element into the buffer `offset`, the byte increments for advancing along each dimension
`pitches`, and the data type of the stored data `type`.

This representation, a contiguous memory buffer together with byte increments, is the most suitable
to do fast computations and to interface with existing libraries like BLAS and LAPACK. In the
implemented form, it allows for a wide range of array manipulations without touching the data
itself; for example, to select a subset of an array along one axis, merely the corresponding value
in `dims` needs to be adjusted and the offset recomputed, if the slice does not start from the
front. Likewise it is possible to select every second or third element in a dimension by increasing
the pitch value, and negative pitches can be used to reverse an array. Transposition can likewise 
be achieved by swapping the information of two axes. In accordance with the value semantics of Tcl,
VecTcl implements copy-on-write on these slices and creates shared buffers for all of the above
transformations.

The other face of NumArray is the string representation. NumArray was designed to reconcile the list
representation and the need of different dimensionality and numerical data types with EIAS. Contrary
to a common misbelief, EIAS does not exclude the existence of data types; it merely requires that an
unambiguous deserialization exists, such that a round trip from serialization/deserialization leads
to a value that behaves the same as the original, possibly having the same internal representation.
As an example, consider the standard `expr`. `expr`'s operators treat a string that parses as an
integer as an integer value, else interpret it as a double, and if that fails, treat it as a
non-numeric string error. In this way, every string can unambiguously interpreted as being an
integer, a floating point value or a general string, even though every integer parses correctly as a
floating point value. NumArray enhances these data types with N-rank tensors and complex values. A
NumArray can formally be described with reference to Tcl lists as follows:

A NumArray is one of the following:

1. An _empty list_
1. A list of values, which can all be parsed as _integers_
2. A list of values, which can all be parsed as _doubles_
3. A list of values, which can all be parsed as _complex values_
4. A list of _NumArrays_ which are not the empty list, all having the _same number of elements_

Using this grammar, the degree (i.e. number of dimensions), datatype and number of elements of a
NumArray can unambiguously derived. Examples for the interpretation are given below
{% highlight tcl %}
{% raw %}
set x {1 2 3} ;# an integer vector of length 3
set y {{1.0 3.0} {3.0 5.0}} ;# a 2x2 floating-point matrix
set z {0+1i 2+3.5i 3.0+0i} ;# a complex vector of length 3
set u {{1 2 3}} ;# a 3x1 integer matrix (a row vector)
set v {{{1 2} {3 4}} {{5 6} {7 8}}} ;# a 2x2x2 integer tensor

set e {1.0 2 3} ;# a floating point vector of length 3
set e1 {1.0 2 3a} ;# error: 3a can't be parsed as a number
set e2 {{1 2} 3 4} ;# error: Dimensions don't match
{% endraw %}
{% endhighlight %}

One peculiarity, which comes from the Tcl list representation,
needs further consideration: a value with no spaces in it can alternatively
interpreted as a list with a single value. And due to EIAS, a string containing a space is
indistinguishable from a list consisting of two elements. This has two consequences, first
single-element values (or scalars) may not contain spaces in their string representation. Otherwise,
the following two NumArrays could'nt be disamiguated:

{% highlight tcl %}
{% raw %}
set c1 {3.0+4.0i} ;# a complex number 
 # with real part 3.0 and imaginary part 4.0
set c2 {3.0 +4.0i} ;# a complex vector of length 2,
 # equal to {3.0+0.0i 0.0+4.0i}
{% endraw %}
{% endhighlight %}

Second, trailing singleton dimensions must be insignificant, i.e. a Nx1 matrix is identical to a
vector of length N, a scalar is identical to a vector of length 1 and a 1x1 matrix. Fortunately,
this coincides with the usual linear algebra interpretation of these entities. Following this
encoding, element retrieval from NumArrays can be done using `lindex` as well as with the indexing
operator of `vexpr`:
{% highlight tcl %}
{% raw %}
set A {{1 2} {3 4} {5 6}} 
set a11 [lindex $A 2 0] ;# 5
vexpr { a11=A[2,0] } ;# 5
{% endraw %}
{% endhighlight %}

For incomplete indices, a slice is returned
{% highlight tcl %}
{% raw %}
set A {{1 2} {3 4} {5 6}} 
set a1 [lindex $A 2] ;# {5 6}
vexpr { a1=A[2] } ;# doesn't work, BUG
{% endraw %}
{% endhighlight %}
Of course, `lindex` causes shimmering of the NumArray to the list representation, which is
particularly inefficient if the list patch is not applied (see next section). 

Due to the way the grammar is defined, a value to be interpreted as a NumArray must be
parsed recursively down to the level of individual elements, to decide on the data type, at the
moment of shimmering, i.e. in the setFromAnyProc. 


Performance considerations
--------------------------

Other implementation issues
--------------------------
