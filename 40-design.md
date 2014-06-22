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

### The NumArray Tcl_ObjType
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
set a_11 [lindex $A 2 0] ;# 5
vexpr { a_11=A[2,0] } ;# 5
{% endraw %}
{% endhighlight %}

For incomplete indices, a slice is returned
{% highlight tcl %}
{% raw %}
set A {{1 2} {3 4} {5 6}} 
set a_1 [lindex $A 2] ;# {5 6}
vexpr { a_1=A[2] } ;# should also be {5 6}
 # doesn't work currently, BUG
{% endraw %}
{% endhighlight %}
Of course, `lindex` causes shimmering of the NumArray to the list representation, which is
particularly inefficient if the list patch is not applied (see next section). 

### vexpr, the Tcl level compiler and execution engine
Using the commands in the `numarray` ensemble, it is possible to perform computations on numerical
arrays in prefix form:
{% highlight tcl %}
{% raw %}
set a {1 2 3}
set c [numarray neg [numarray * 2 [numarray + $a {4 5 6}]]]
{% endraw %}
{% endhighlight %}
But most mathematics is actually better expressed in infix form; the above sequence of commands is
equal to 
{% highlight tcl %}
{% raw %}
vexpr {
	a={1 2 3}
	c=-2*(a+{4 5 6})
}
{% endraw %}
{% endhighlight %}
Of course, both forms can be freely mixed, there is no perceived distinction at the script level between variables
accessed and modified within a `vexpr` or outside. Therefore `vexpr` can be used in inline form
similar to `expr`
{% highlight tcl %}
{% raw %}
set c [vexpr {-2*(a+{4 5 6})}]
{% endraw %}
{% endhighlight %}
or as a self-standing command with a longer sequence of statements.
The language is quite complete and supports not only simple assignments, but [loops and
conditions as well.](tutorial.html)

The expression handed over to `vexpr` is compiled into the prefix form using a compiler based on 
[a small, but clever example compiler from the wiki by Donal Fellows](http://wiki.tcl.tk/39011). The
expression is not evaluated directly, but put into a proc under the namespace numarray. `vexpr`
maintains a cache of compiled expressions in a dict and compiles every expression only once. For
repeated invocation of the same expression, merely the associated proc is invoked via uplevel. For
every variable that is accessed in the expression, the compiler emits an `upvar` instruction so as
to access the variable in the callers scope. 

Loops, branches and function calls are translated to their Tcl equivalent. The execution under the
numarray namespace ensures that the commands contained there appear as built-in functions, but any
Tcl proc can be called as a function. This ensures compatibility also on the command level. As a
performance improvement,`vproc` allows to create a Tcl proc entirely defined by a single `vexpr`program. The
only difference is that the compiler doesn't emit upvar instructions for the local variables, and
that the `vexpr` call itself is circumvented. 


Performance considerations
--------------------------

### Shimmering performance
Due to the way the grammar is defined, a value to be interpreted as a NumArray must be parsed
recursively down to the level of individual elements at the moment of shimmering, i.e. in the
setFromAnyProc. At this moment, it must accept every possible number of dimensions or data type.
Since numerical objects can be very large, the setFromAnyProc tries to perform the conversion
without triggering a string representation. This is done by looking into the `typePtr` field of the
Tcl_Obj representing the input value. Only if the `typePtr` can neither be read as an integer,
double, complex, or list type, the string representation is examined. A pure sequence of
Tcl_GetFooFromObj() is insufficient to prevent the string conversion, possibly multiple times,
especially when non-core numeric datatypes like complex values are involved. Therefore, the
efficiency of converting a pure list of doubles into NumArray is dependent on the fact that the core
datatypes `int`, `double`, `list`, are registered using Tcl_RegisterObjType(). An alternative
interface satisfying the needs of NumArray's setFromAnyProc would be a Tcl_MaybeGetFooFromObj()
function which fails if the Tcl_Obj in question cannot be converted to the requested value type
without triggering a string conversion.

Of equal importance than the conversion of Tcl's native datatypes into NumArray is the other way
round. At some point, the data must leave the internal representation of NumArray. The only official
way to do this is the updateStringProc. But the string representation is not always the final step
in the conversion. If the NumArray is not scalar, then in most cases the code using he result
expects a list, and the generated string representation is just used to parse the values back into a
list. For instance, consider a program which wishes to draw a line on the canvas computed by VecTcl.

{% highlight tcl %}
{% raw %}
# compute a sine wave using VecTcl
vexpr { 
	x=linspace(0,10,1000)
	y=sin(x)*50
	coords = hstack(x*10+100, y+100)
	coords = reshape(coords, 2000)
}
# display on canvas
.c create line $coords
{% endraw %}
{% endhighlight %}

As long as the canvas is not rewritten to understand NumArrays directly,`coords` shimmers via string
to list of doubles. VecTcl contains an experimental feature to enable direct conversion from the
internal representation, enabled by `--enable-listpatch` during compilation. It requires a 
[small patch](https://github.com/auriocus/VecTcl/blob/master/tcl86_vectcl.patch) to the Tcl core to work. 
The patch replaces calls to setListFromAny in the core (single file tclListObj.c) with equivalent
calls to Tcl_ConvertToType() and removes the const qualifier from the list Tcl_ObjType. Thus VecTcl
can patch the list Tcl_ObjType() and wrap the setFromAny proc, such that NumArrays are checked
first. The conversion code constructs a list of scalar values from one-dimensional NumArrays.
Higher-dimensional NumArrays are returned as a list of slices into the original buffer. Thus, a
`lindex` into a matrix with 1000×1000 elements, shimmers the object into a list of 1000 slices, and
the accessed row into a list of 1000 doubles. In contrast, without the patch, it prints a million
doubles into ASCII. A cleaner way to achieve this goal would be, if the core provided a method to
register an optimized myObjTypetoList() function, or even more general, a way to register an A to B
transform. Even the core itself could profit from such an infrastructure, since, e.g., the dict and list 
types are converted circumventing the string representation, by making list and dict
setFromAny know of each other.

### Execution speed of vexpr
#### Compilation
Compilation speed of vexpr in its current implementation is satisfying. Simple statements compile
faster than a millisecond, and relatively long expressions or programs, such as the integration loop
in [jima's example](http://stevehavelka.com/blog/2014/06/guest-post-vectcl/), take a few
milliseconds to compile
{% highlight tcl %}
{% raw %}
vproc shipTrajectory {} {
    numSteps = 13000
    x = zeros(numSteps + 1, 2); # m
    v = zeros(numSteps + 1, 2); # m/s

    x[0, 0] = 15e6
    x[0, 1] = 1e6
    v[0, 0] = 2e3
    v[0, 1] = 4e3

    for i=0:numSteps-1 {
        a = acceleration(x[i,:])
        v[i+1,0] = v[i,0]+::h*a[0]
        v[i+1,1] = v[i,1]+::h*a[1]
        x[i+1,0] = x[i,0]+::h*v[i,0]
        x[i+1,1] = x[i,1]+::h*v[i,1]
    }
    list(x,v)
}
{% endraw %}
{% endhighlight %}

#### Execution speed of a sequence of commands
On the other hand, the execution speed of this code is very slow compared to the clock speed of the
computer. On the same machine on which the [benchmarks](benchmarks.html) are performed, it takes 600
milliseconds to integrate the 13,000 steps. When the updates in the inner loop are rewritten in
vector form, the execution speed nearly doubles. This code
{% highlight tcl %}
{% raw %}
v[i+1,:] = v[i,:]+::h*a
x[i+1,:] = x[i,:]+::h*v[i,:]
{% endraw %}
{% endhighlight %}
takes 350 milliseconds to complete 13,000 steps. The main reason for this difference is that the
time for actually performing this computation is insignificant. Calling a C coded command which
doesn't do anything in a for loop many times takes 200 ns per iteration, which is roughly 100 times
slower than performing a scalar computation on the machine. The second variant of the code simply runs faster
because it eliminates half of the command calls. To make this kind of code run fast, the speed of the
Tcl bytecode will have to be improved, or an alternative backend for the VecTcl compiler must be
sought. One possible alternative implementation could generate code in C, which still calls the same
underlying C commands, but does not dispatch through the Tcl interpreter. Such an implementation 
is within reach with the advent of the tcc4tcl extension, which can compile C code very fast into
memory and execute it from there. 

#### Execution speed of a single NumArray operation

Other implementation issues
--------------------------
