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
of the computation should be kept as efficient as possible, and in any case lower than what could be
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
`dims`, and the data type of the stored data `type`. 


Performance considerations
--------------------------

Other implementation issues
--------------------------
