---
title: VecTcl
layout: default
subtitle: Experiments with JIT compilation
documentation: true
home: false
toplink: false
---


## Designing a JIT compiler

The current implementation of VecTcl fairs quite well performancewise, but it hits two limits
which can hardly be overcome using the current scheme of implementation. As explained in the [design
document]({{ site.baseurl }}/design.html), the speed and memory usage of complex vector
expressions is constrained by the use of temporaries, which store the whole vector instead of one
component at a time; and the speed of tight low-level loops is bound by the execution speed of the
Tcl bytecode engine, which is several orders of magnitude slower than an equivalent loop in native
code. These issues can only be overcome by compiling into native code at runtime, which is possible
via C using the [tcc4tcl](https://chiselapp.com/user/rkeene/repository/tcc4tcl) extension. 
This document describes the experiments carried out in this direction.

### Obstacles against static compilation

The dynamic nature of (Vec)Tcl enables many metaprogramming abilities which hinder static
compilation, or native compilation at all. This section tries to enumerate and analyse these
features in order to define a subset which can be compiled. The goal is to leave out as little as
possible, until code generation is possible.

#### upvar and uplevel

A procedure called as a function in VecTcl can access the variables local to the caller via `upvar`
or executing code in the scope of the caller via uplevel. Consider

{% highlight tcl %}
{% raw %}
proc setx {v} {
	upvar 1 x x 
	set x $v
}

vproc test {y} {
	setx(y)
	3*x
}
{% endraw %}
{% endhighlight %}

In this case, it would be impossible to compile the vproc `test`. A static compiler with no insight
into the Tcl procedure `setx` cannot know, that the variable `x` is created by calling `setx`. 

#### break and continue return codes

A command which returns a break or continue code can modify the control flow in the caller in an
unpredictable way. Consider

{% highlight tcl %}
{% raw %}
proc mysurprise {i} {
	if {$i > 3} { return -code break }
	expr {$i*2}
}

vproc test {} {
	x=0
	for i=1:10 {
		x=x+mysurprise(i)
	}	
	x
}
{% endraw %}
{% endhighlight %}

The result of calling test is well defined, the loop terminates in the 4th iteration and the
result is $$12=2+4+6$$. This surprising control flow change introduces additional joins in the control
flow graph and complicates (prohibits?) the correct compilation into SSA form.

#### Redefinition of a command during execution

Currently, the Tcl compiled code invokes commands in the namespace numarray::, which is not
protected by any means from being redefined or deleted. In fact, this can even be exploited to
overload the operators, for example to do automatic differentiation. Since the speed advantage from
compilation into native code comes from native execution of the mathematical operators, enabling
overloading in compiled code would eithe require the code to check for overloading at runtime, or
effectively reduce the compilation into a chain of function calls at the C level. The latter cannot
be much more efficient than the current design, which employs the Tcl bytecode interpreter. The same
argument applies to execution traces on the internal commands.

#### Variable traces

If a variable trace is attached to a variable inside a vector expression, the trace fires, possibly
multiple times, when the expression is executed. In an expression like
{% highlight tcl %}
{% raw %}
vexpr {
	a=x-2
	b=a*a
}
{% endraw %}
{% endhighlight %}

a write trace on `a` would trigger, followed by two read traces on `a`, and before `b` is set. The
optimal native translation of this expression would write to `a` only once, but never read from it:

{% highlight c %}
{% raw %}

Tcl_Obj *aobj = NumArrayNewVector(N);
Tcl_Obj *bobj = NumArrayNewVector(N);
double *a=NumArrayGetPtrFromObj(aobj);
double *b=NumArrayGetPtrFromObj(bobj);
double *x=NumArrayGetPtrFromObj(xobj);
for (size_t i=0; i<N; i++) {
	a[i]=x[i]-2;
	b[i]=a[i]*a[i];
}
Tcl_ObjSetVar2(interp, "a", NULL, aObj, TCL_LEAVE_ERR_MSG);
Tcl_ObjSetVar2(interp, "b", NULL, bObj, TCL_LEAVE_ERR_MSG);
{% endraw %}
{% endhighlight %}

For local variables inside a `vproc`, an associated Tcl variable would not even exist. Again, trying
to implement trace semantics would probably prohibit effective compilation and force a certain
non-optimal sequence of the operations. 

### Projected implementation of the tracing JIT

The idea of the JIT compiler is to assign undefined behaviour to all of the above. In numerics-heavy
code, it is very unlikely that the metaprogramming tricks using `upvar`, custom control constructs,
dynamic redefinition of commands or variable traces are needed. Therefore, the compiler can assume
that the built-in operators, pointwise functions (like `sin`, `exp` etc.) and reductions have the usual
meaning. For custom functions, the compiled code will call `Tcl_EvalObj`, i.e. the associated Tcl
proc is called, which includes tracing etc. However, the proc will be called in the global namespace
and will have no access (via upvar) to the variables in the outer scope. 

#### vectcl::jitproc
The first implementation will not replace `vexpr`, this is cumbersome because of the linkage of
variables inside the expression and the surrounding Tcl code. Instead, an alternative to `vproc`
will be provided. The interface between the embedding Tcl code and a vproc is well defined: data
can only be passed in via arguments, and passed back only via return values. This means, that the variables
inside the vector expression are local, and need not (will not) correspond to real Tcl variables.

Still, the language is dynamic to an extent which prohibits full static compilation: the data type of the
variables (int, double, complex) and the shape (scalar, vector, 2D, 3D...) varies the semantics of
the operators. Type inference can provide some information, for a given type of the arguments, but
if the jitproc calls into an arbitrary Tcl function, the return type cannot be infered at compile
time.

The idea is to compile every expression twice. First, a trivial compilation into library calls,
which can handle any type, and second a compilation into code specialized to the assumed data type.
At runtime, checkpoint code switches both versions according to the actual type. If the general
version is called, it records the type of the expression that failed, and after return from the
jitproc, recompilation is triggered using the now correct type. In the highly likely case that the
data type of a given variable does not change often at runtime, 
the native code will run for any subsequent invocations of the jitproc.

#### Static Single Assignment (SSA)

The compiler will first compile the given expressions into SSA form, using the algorithm from
(Brandis and
Mössenböck)[http://people.via.ecp.fr/~stilgar/doc/compilo/Single-pass%20generation%20of%20static%20single-assignment%20form%20for%20structured%20languages.pdf].
A compiler for a minimal subset of the VecTcl language can be found in the jit branch of the
repository. It only supports simple assignments, `if`, `while`, and all operators besides array
slicling. Supporting all language features (with the exception of the restrictions discussed before)
should be no problem in the frontend. This frontend will be used to compile two types of benchmark
functions: a vectorized function (the linear regression benchmark), and a function which computes
the cycle length of the 3n+1 sequence as an example of a tight low-level loop, which cannot be vectorized.

{% highlight tcl %}
{% raw %}
vectcl::jitproc {{xv {double n}} {yv {double n}}}	{
	xm=mean(xv); ym=mean(yv)
	beta=sum((xv-xm).*(yv-ym))./sum((xv-xm).^2)
	alpha=ym-beta*xm
	list(alpha, beta)
}
{% endraw %}
{% endhighlight %}

This function compiles into the following code
{% highlight tcl %}
{% raw %}
= {Tempvar 1} {Argument xv}
= {Tempvar 2} {Argument yv}
{CALL mean} {Tempvar 4} {Tempvar 1}
{CALL mean} {Tempvar 6} {Tempvar 2}
- {Tempvar 7} {Tempvar 1} {Tempvar 4}
- {Tempvar 8} {Tempvar 2} {Tempvar 6}
.* {Tempvar 9} {Tempvar 7} {Tempvar 8}
{CALL sum} {Tempvar 10} {Tempvar 9}
- {Tempvar 11} {Tempvar 1} {Tempvar 4}
.^ {Tempvar 12} {Tempvar 11} {Literal 1}
{CALL sum} {Tempvar 13} {Tempvar 12}
./ {Tempvar 15} {Tempvar 10} {Tempvar 13}
* {Tempvar 16} {Tempvar 15} {Tempvar 4}
- {Tempvar 18} {Tempvar 6} {Tempvar 16}
{CALL list} {Tempvar 19} {Tempvar 18} {Tempvar 15}
Tcl_SetObjResult(interp, Tempvar 19)

Literals: 
Literal 1 2
Symbols: 
xv Tempvar 1
yv Tempvar 2
xm Tempvar 4
ym Tempvar 6
beta Tempvar 15
alpha Tempvar 18
Types: 
Argument xv double n
Argument yv double n
Literal 1 int 1
Tempvar 1 double n
Tempvar 2 double n
Tempvar 4 double 1
Tempvar 6 double 1
Tempvar 7 double n
Tempvar 8 double n
Tempvar 9 double n
Tempvar 10 double 1
Tempvar 11 double n
Tempvar 12 double n
Tempvar 13 double 1
Tempvar 15 double 1
Tempvar 16 double 1
Tempvar 18 double 1
Tempvar 19 Any
{% endraw %}
{% endhighlight %}

This is a form of three-address code, the first item in every list is the operator, the second the
destination, the rest are the arguments.

Since there are no branches, the SSA form does not contain any phi functions. Many assignments are
superfluous: Tempvar 2 could simply be replaced by Tempvar 1. This transformation can be done in a
separate optimization phase. 

The benchmark function for the cycle length looks like this:

{% highlight tcl %}
{% raw %}
vectcl::jitproc {{N {int 1}}}	{
	i=0
	while N != 1 {
		if (N%2 == 1) {
			N=3*N+1
		} else {
			N=N/2
		}
		i=i+1
	}
	i
}
{% endraw %}
{% endhighlight %}

The resulting SSA code is this:
{% highlight tcl %}
{% raw %}
= {Tempvar 1} {Argument N}
= {Tempvar 2} {Literal 1}
While {} {}
Phi {Tempvar 14} {Tempvar 1} {Tempvar 11}
Phi {Tempvar 15} {Tempvar 2} {Tempvar 13}
!= {Tempvar 3} {Tempvar 14} {Literal 2}
Do {} {Tempvar 3}
% {Tempvar 4} {Tempvar 14} {Literal 3}
== {Tempvar 5} {Tempvar 4} {Literal 4}
If {} {Tempvar 5}
* {Tempvar 6} {Literal 5} {Tempvar 14}
+ {Tempvar 8} {Tempvar 6} {Literal 6}
Else {} {}
/ {Tempvar 10} {Tempvar 14} {Literal 7}
EndIf {} {}
Phi {Tempvar 11} {Tempvar 10} {Tempvar 8}
+ {Tempvar 13} {Tempvar 15} {Literal 8}
EndWhile {} {}
Tcl_SetObjResult(interp, Tempvar 15)

Literals: 
Literal 1 0
Literal 2 1
Literal 3 2
Literal 4 1
Literal 5 3
Literal 6 1
Literal 7 2
Literal 8 1
Symbols: 
N Tempvar 1
i Tempvar 2
N Tempvar 8
N Tempvar 10
N Tempvar 11
i Tempvar 13
N Tempvar 14
i Tempvar 15
Types: 
Argument N int 1
Literal 1 int 1
Literal 2 int 1
Literal 3 int 1
Literal 4 int 1
Literal 5 int 1
Literal 6 int 1
Literal 7 int 1
Literal 8 int 1
Tempvar 1 int 1
Tempvar 2 int 1
 Any
Tempvar 14 int 1
Tempvar 15 int 1
Tempvar 3 int 1
Tempvar 4 int 1
Tempvar 5 int 1
Tempvar 6 int 1
Tempvar 8 int 1
Tempvar 10 int 1
Tempvar 11 int 1
Tempvar 13 int 1

{% endraw %}
{% endhighlight %}
Due to the loops and branches, this translation contains many phi functions.

#### Basic Loops
The SSA code could be trivially translated into C; for example, the instruction
{% highlight tcl %}
{% raw %}
- {Tempvar 5} {Argument xv} {Tempvar 2}
{% endraw %}
{% endhighlight %}
becomes 
{% highlight C %}
{% raw %}
Tcl_Obj *tempvar5;
if (NumArrayMinus(xv, tempvar2, &tempvar5) != TCL_OK) {
	NumArrayDecrRefcount(tempvar2);
	Tcl_SetObjResult(interp, tempvar5);
	return TCL_ERROR;
}
NumArrayDecrRefcount(tempvar2);

{% endraw %}
{% endhighlight %}
This code works for any data type or shape of the involved variables.
However, this provides only a marginal speed improvement over the Tcl compiled version; most of the
time is spent to allocate and free the memory for the temporary variables. In order to approach the
speed of native code, it is necessary to identify computations which can be performed together in a single
loop, reducing temporary variables to scalars. For example, assuming that `xv` and `yv` are vectors
of doubles, these three instructions could be performed in one loop:

{% highlight tcl %}
{% raw %}
- {Tempvar 5} {Argument xv} {Tempvar 2}
- {Tempvar 6} {Argument yv} {Tempvar 4}
.* {Tempvar 7} {Tempvar 5} {Tempvar 6}
{% endraw %}
{% endhighlight %}

which looks approximately like this
{% highlight C %}
{% raw %}
Tcl_Obj *tempvar7 = NumArrayNewVector(N);
double *tempvar7ptr = NumArrayGetPtrFfromObj(tempvar7);
for (size_t ind=0; ind++; ind<N) {
	double tempvar5 = xvptr[ind]-tempvar2;
	double tempvar6 = yvptr[ind]-tempvar2;
	tempvar7ptr[ind] = tempvar5 * tempvar6;
}

{% endraw %}
{% endhighlight %}

This transformation can be done for any subexpression which involves only elementwise operations,
and possibly a reduction at the root of the expression. This should be called a _basic loop_ in this
docuent. Suitable subexpressions could be identified
from the parse tree; however this is non-optimal in case of variables holding intermediate results:

{% highlight tcl %}
{% raw %}
vectcl::jit x {
	xs = x-mean(x)
	sum(xs.^2)
}
{% endraw %}
{% endhighlight %}

This code can be executed in two loops: the first computes `mean(x)`, the second computes the
result. Storing the assigned `xs` into a temporary vector makes this code slower. Therefore, it is
better to recombine the expressions from the SSA code into basic loops, rather than reading the
basic loops out of the parse tree.

The algorithm to recognize basic loops has not yet been written. Before the definition of the resulting
data structure, code generation with native data types is impossible. 

#### Runtime type checking



