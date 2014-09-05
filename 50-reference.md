---
title: VecTcl
layout: default
subtitle: A guided tour to VecTcl / basic reference
documentation: true
home: false
toplink: false
---


## Introduction.

The present document aims at giving a glimpse of all the functionality it conveys in a guided form and by means of using small examples.

**VecTcl** functionality may be used by invoking a **Tcl** command (`vexpr`) that offers the possibility of using a similar syntax to that of Matlab with infix form operators, assignments, no need to use `$` for variable access, calling **Tcl** commands disguised as functions, indexing and slicing as in `A[:,1]`, special forms for conditional branching and looping, etc...

Another way of invoking **VecTcl** functionality is by accessing the **Tcl** commands disposed under the `numarray` namespace. This form might also come handy on occasions.

We will see examples of both ways of using **VecTcl** throughout this document.

There are probably things that won't be well covered in the first versions of this document... we will strive to improve it in the future.

At the end of the document we will collect random notes of things that might be worked upon.

So let's **VecTcl**...

## Creating a numeric array.

Let's start by the most basic task, creating numeric arrays.

Numeric arrays can be created from **Tcl** by using `set`.

{% highlight tcl %}
{% raw %}
set A { {1. 2. 3.} {4. 5. 6.} {7. 8. 9.} }
{% endraw %}
{% endhighlight %}

We can use this variable in further `vexpr` invokations. This one for instance uses the elementwise operator `.+` in a way that adds `1` to every element of the newly created `A` array to have a `B` array. See that this also means that numeric arrays can also be created by using the assignment operator.

{% highlight tcl %}
{% raw %}
vexpr {
    B = A .+ 1
}
{% endraw %}
{% endhighlight %}

With these commands we would have had the equivalent to having defined in mathematical notation (which we will abuse throughout the document):

{% raw %}
<div>
$$
A = \begin{bmatrix}
1.0 & 2.0 & 3.0 \\
4.0 & 5.0 & 6.0 \\
7.0 & 8.0 & 9.0
\end{bmatrix}
\;
B = \begin{bmatrix}
2.0 & 3.0 & 4.0 \\
5.0 & 6.0 & 7.0 \\
8.0 & 9.0 & 10.0
\end{bmatrix}
$$
</div>
{% endraw %}

We can also use the function `list` from within a `vexpr`. Functions invoked from within `vexpr` have their arguments separated by commas. In this case `list` has the following signature:

{% raw %}
list(valueList)
{% endraw %}

where `list` invokations typically nest to represent more than one dimensional arrays.

Example:

{% highlight tcl %}
{% raw %}
vexpr {
    A = list( list(1.0,2.0,3.0), list(4.0,5.0,6.0), list(7.0,8.0,9.0) )
}
{% endraw %}
{% endhighlight %}

This would also give the same matrix `A`:

{% raw %}
<div>
$$
A = \begin{bmatrix}
1.0 & 2.0 & 3.0 \\
4.0 & 5.0 & 6.0 \\
7.0 & 8.0 & 9.0
\end{bmatrix}
$$
</div>
{% endraw %}

We can also use function `create` from within `vexpr` to create the same matrix `A`.

The function `create` has signature:

{% raw %}
create(valueList)
{% endraw %}

Example:

{% highlight tcl %}
{% raw %}
vexpr {
    A = create({ {1.0 2.0 3.0} {4.0 5.0 6.0} {7.0 8.0 9.0} })
}
{% endraw %}
{% endhighlight %}

Or, lastly, we could also the same function directly from **Tcl** following the signature:

{% highlight tcl %}
{% raw %}
numarray::create valueList
{% endraw %}
{% endhighlight %}

Example:

{% highlight tcl %}
{% raw %}
set A [numarray::create { {1. 2. 3.} {4. 5. 6.} {7. 8. 9.} }]
{% endraw %}
{% endhighlight %}

### Creating special numeric arrays.

There are ways to create some specialized arrays.

#### Filling an array with a constant.

The function to use is `constfill` with signature:

{% raw %}
constfill(value,dim1,?dim2 ...?)
{% endraw %}

Example:

{% highlight tcl %}
{% raw %}
vexpr {
    A = constfill(5.0,2,2)
}
{% endraw %}
{% endhighlight %}

Which would give us something equivalent to the following:

{% raw %}
<div>
$$
A = \begin{bmatrix}
5.0 & 5.0 \\
5.0 & 5.0
\end{bmatrix}
$$
</div>
{% endraw %}

We could also have used the alternative form:

{% raw %}
numarray::constfill value dim1 ?dim2 ...?
{% endraw %}

The following example creates a numerical array of dimension 3:

{% highlight tcl %}
{% raw %}
set A [numarray::constfill 5. 2 2 2]
{% endraw %}
{% endhighlight %}

Giving, in nested list string representation:

{% highlight tcl %}
{% raw %}
{{{5.0 5.0} {5.0 5.0}} {{5.0 5.0} {5.0 5.0}}}
{% endraw %}
{% endhighlight %}

#### Creating an identity matrix.

The function to use is `eye` whose signatures are:

{% raw %}
eye(dim1?,dim2?)
numarray::eye dim ?dim2?
{% endraw %}

Examples:

{% highlight tcl %}
{% raw %}
vexpr {
    I2 = eye(2);
}
{% endraw %}
{% endhighlight %}

This would give:

{% raw %}
<div>
$$
\begin{bmatrix}
1 & 0 \\
0 & 1
\end{bmatrix}
$$
</div>
{% endraw %}

{% highlight tcl %}
{% raw %}
set I23 [numarray::eye 2 3]
{% endraw %}
{% endhighlight %}

This would give:

{% raw %}
<div>
$$
\begin{bmatrix}
1 & 0 & 0 \\
0 & 1 & 0
\end{bmatrix}
$$
</div>
{% endraw %}

### Creating a vector of linearly spaced numbers.

With `linspace`, a **Tcl** convenience procedure, we can create a vector of linearly spaced reals. The procedure needs the initial and the final points of the interval to be returned and the number of divisions to establish in it.

{% raw %}
numarray::linspace start stop n
linspace(start,stop,n)
{% endraw %}

An example:

{% highlight tcl %}
{% raw %}
vexpr {
    linspace(0,9,10)
}
{% endraw %}
{% endhighlight %}

This will just return a vector `0.0 1.0 2.0 3.0 4.0 5.0 6.0 7.0 8.0 9.0`.

## Destroying numeric arrays.

One of the many good things of **VecTcl** is that, being numeric arrays first class citizens in the realm of **Tcl** objects, the user has not to care when to dispose of a numeric array, the system does that in automagical fashion.

In any case, users may use **Tcl** command `unset` at anytime, like in:

{% highlight tcl %}
{% raw %}
vexpr {
    A=eye(2)
}
unset A
{% endraw %}
{% endhighlight %}

## Asking for metadata of a numeric array.

There are some pieces of information associated to each numeric array like the number of its dimensions and such. We call these pieces the array's metadata.

We will see now some ways to ask **VecTcl** about the metadata of a numeric array.

### Info.

This command is used mainly for debug purposes. To avoid collisions with the **Tcl** `info` command this form is used:

{% raw %}
numarray info numArrayVal
{% endraw %}

{% highlight tcl %}
{% raw %}
vexpr {
  Z = eye(2)
}
puts [numarray info $Z]
{% endraw %}
{% endhighlight %}

This will produce:

{% raw %}
dimensions {2 2} offset 0 pitches {16 8} canonical 1 bufsize 32 refcount 1 type 1
{% endraw %}

### Dimensions.

Signatures:

{% raw %}
dimensions(numArrayVar)
numarray::dimensions numArrayVal
{% endraw %}

As examples, taking the `I23` array from previous example we should get `2` as answer to the following queries:

{% highlight tcl %}
{% raw %}
vexpr {
    dimensions(I23)
}
numarray::dimensions $I23
{% endraw %}
{% endhighlight %}

### Shape.

The shape of a numeric array is a list giving the size of each dimension of the array. Signatures for `shape`:

{% raw %}
shape(numArrayVar)
numarray::shape numArrayVal
{% endraw %}

As examples, taking the `I23` array from previous example we should get the list `2 3` as answer to the following queries:

{% highlight tcl %}
{% raw %}
vexpr {
    shape(I23)
}
numarray::shape $I23
{% endraw %}
{% endhighlight %}

### Rows and columns.

For vectors and matrices there is a pair of **Tcl** convenience procedures called `rows` and `cols`.

{% raw %}
rows(numArrayVar)
numarray::rows numArrayVal

cols(numArrayVar)
numarray::cols numArrayVal
{% endraw %}

Continuing with the `I23`example, we, naturally, should get `2` if we ask for its `rows` and 3` if we ask for its `cols`.

## Changing the shape of a numeric array.

If you consider a numeric array as a set of contiguous numbers with an associated legend (or metadata) that allows you to interpret them as, let's say, a column vector or a two by two matrix then it is conceivable that we could use a function to define the metadata so as to change our interpretation of the array.

### Reshape.

The `reshape` function allows to redefine the shape of a numeric array. Its signatures:

{% raw %}
reshape(numArrayVar,dim1?,dim2 ...?)
numarray::reshape numArrayVal dim1 ?dim2 ...?
{% endraw %}

An example:

{% highlight tcl %}
{% raw %}
set A {1 2 3 4 5 6 7 8}
vexpr {
    reshape(A,2,4)
}
{% endraw %}
{% endhighlight %}

Which would transform `A` into `{% raw %}{1 2 3 4} {5 6 7 8}{% endraw %}` in list form.

Doing

{% highlight tcl %}
{% raw %}
vexpr {
    reshape(A, 2,2,2)
}
{% endraw %}
{% endhighlight %}

Would transform `A` into `{% raw %}{{1 2} {3 4}} {{5 6} {7 8}}{% endraw %}` in nested list form.

### Transposition.

One very common way of reshaping a numeric array is the transposition operation with which the reader maybe familiar in the cases of vectors and matrices.

There are two ways of asking **VecTcl** to transpose a numeric array for us.

One is using the usual "prime" syntax:

{% highlight tcl %}
{% raw %}
vexpr {
    A = list(1,2,3)
    B = A'
}
{% endraw %}
{% endhighlight %}

This would be akin to having:

{% raw %}
<div>
$$
A = \begin{bmatrix}
1 \\
2 \\
3
\end{bmatrix}
\;
B = \begin{bmatrix}
1 & 2 & 3
\end{bmatrix}
$$
</div>
{% endraw %}

If the reader is familiar with the transpose of a matrix or a vector, it maybe familiar with the adjoint of a matrix or a vector of complex numbers.

For this there is the `adjoint` function (which transposes the numeric array and flips the sign of the imaginary parts of its elements). This function is useful when dealing with numeric arrays of complex numbers. The "prime" syntax, when applied to a complex numeric array gets mapped to the `adjoint` function.

{% highlight tcl %}
{% raw %}
vexpr {
    A = list(1+3i,2+2i,3+1i)
    B = A'
}
{% endraw %}
{% endhighlight %}

This would be akin to having:

{% raw %}
<div>
$$
A = \begin{bmatrix}
1.0+3.0i \\
2.0+2.0i \\
3.0+1.0i
\end{bmatrix}
\;
B = \begin{bmatrix}
1.0+3.0i & 2.0+2.0i & 3.0+1.0i
\end{bmatrix}
$$
</div>
{% endraw %}

Notice that, by design, **VecTcl** parser enforces a canonical representation for complex numbers that forces us to say things like `0+1i` instead of just `i`.

The second way of asking for a `transpose` or an `adjoint` of a numeric array would be invoking the functions directly without using the "prime" syntax according to these signatures:

{% raw %}
transpose(numArrayVar)
numarray::transpose numArrayVal

adjoint(numArrayVar)
numarray::adjoint numArrayVal
{% endraw %}

By the way, using this form we would be, at least partly, freed from **VecTcl** way of parsing complex numbers, so we could say `numarray::adjoint {4i 3+1i 5}`.

Lastly, notice that the `transpose` function can be applied to a complex numeric array too (but, of course, it won't flip signs of the imaginary parts of its elements).

## Recap on shapes.

Out of curiosity, let's see the dimensions and shapes of different numeric arrays and their transposes.

Let's begin with humble scalars. Dimension 1 everywhere we look at.

| created as | dims | shape | dims of transpose | shape of transpose |
|---|---|---|---|---|
| A = 5 | 1 | 1 | 1 | 1 |

Let's go for a column vector which is seen either as a 1 dimensional array with dimension size greater than one (as can be seen in the first line of the following table) or a 2 dimensional array with its last dimension having size 1 (look in the second line of table). If the column vector has one row, it just "collapses" to be an scalar as we can see in the third line of the following table.

| created as | dims | shape | dims of transpose | shape of transpose |
|---|---|---|---|---|
| A = constfill(5,2) | 1 | 2 | 2 | 1 2 |
| A = constfill(5,2,1) | 1 | 2 | 2 | 1 2 |
| A = constfill(5,1,1) | 1 | 1 | 1 | 1 |

The transpose of a column vector is a row vector.

| created as | dims | shape | dims of transpose | shape of transpose |
|---|---|---|---|---|
| A = constfill(5,1,2) | 2 | 1 2 | 1 | 2 |

A matrix is a rank 2 numeric array. You can have square matrices or not...

| created as | dims | shape | dims of transpose | shape of transpose |
|---|---|---|---|---|
| A = constfill(5,2,2) | 2 | 2 2 | 2 | 2 2 |
| A = constfill(5,2,3) | 2 | 2 3 | 2 | 3 2 |

A slightly more convoluted case as we enter dimensions greater than 2. Here, having the last dimension being 1 makes us say that the shape is one `1 1` array stacked in a pile of 1 along a third dimension. This would be akin to stacking a single row vector of one column, which in reality is just a scalar as the transpose reveals.

| created as | dims | shape | dims of transpose | shape of transpose |
|---|---|---|---|---|
| A = constfill(5,1,1,1) | 2 | 1 1 | 1 | 1 |

The same logic, although the transpose being less revealing, applies here:

| created as | dims | shape | dims of transpose | shape of transpose |
|---|---|---|---|---|
| A = constfill(5,1,1,1,1) | 3 | 1 1 1 | 3 | 1 1 1 |

But let's admit that keep filling up dimensions of size 1 is not a normal case by any means, this would be like keep nesting a single element in lists of lists and so forth in **Tcl**.

For more normal cases, consider the following ones. Here we have a stack of 3 1 by 2 row vectors in the first line of the table and a stack of 3 4 by 2 matrices in the second line. One can see the effect of transposition taking place in both first dimensions in each case so that gives us a stack of 3 2 by 1 column vectors in the first line of the table and a stack of 3 2 by 4 matrices in the second.

| created as | dims | shape | dims of transpose | shape of transpose |
|---|---|---|---|---|
| A = constfill(5,1,2,3) | 3 | 1 2 3 | 3 | 2 1 3 |
| A = constfill(5,4,2,3) | 3 | 4 2 3 | 3 | 2 4 3 |

And, lastly, here we have a stack of 1 2 by 3 matrix in the first line of the table which is treated as a 2 dimensional array by **VecTcl** and gets transposed as such. On the second line we have a stack of 4 2 by 3 matrices that gets transposed into a stack of 4 3 by 2 matrices.

| created as | dims | shape | dims of transpose | shape of transpose |
|---|---|---|---|---|
| A = constfill(5,2,3,1) | 2 | 2 3 | 2 | 3 2 |
| A = constfill(5,2,3,4) | 3 | 2 3 4 | 3 | 3 2 4 |

To keep on building up on dimensions is an exercise left to the curious reader.

## Composing numeric arrays.

There is a general function for concatenating numeric arrays that goes by the following signatures:

{% raw %}
concat(numArrVar1,numArrVar2,axis)
numarray::concat numArrVal1 numArrVal2 axis
{% endraw %}

Let's try an example:

{% highlight tcl %}
{% raw %}
set A {  { {1 2} {3 4} } { {5 6} {7 8} }  }
set B {  { {10 20} {30 40} } { {50 60} {70 80} }  }
vexpr {
    C = concat(A,B,0)
    D = concat(A,B,1)
    E = concat(A,B,2)
}
{% endraw %}
{% endhighlight %}

This gives, in nested list form:

{% raw %}
C -- { {1 2} {3 4} } { {5 6} {7 8} } { {10 20} {30 40} } { {50 60} {70 80} }
D -- { {1 2} {3 4} {10 20} {30 40} } { {5 6} {7 8} {50 60} {70 80} }
E -- { {1 2 10 20} {3 4 30 40} } { {5 6 50 60} {7 8 70 80} }
{% endraw %}

But, to simplify things for the users, there are two convenience **Tcl** procedures for stacking a number of arrays vertically or horizontally, which are common cases for matrices and vectors.

{% raw %}
vstack(numArrVar1,numArrVar2 ?,numArrVar3 ...?)
numarray::vstack numArrVal1 numArrVal2 ?numArrVal3 ...?

vstack(numArrVar1,numArrVar2 ?,numArrVar3 ...?)
numarray::vstack numArrVal1 numArrVal2 ?numArrVal3 ...?
{% endraw %}

{% highlight tcl %}
{% raw %}
set A {  {1 2} {3 4} {5 6}  }
set B {  {10 20} {30 40} }
set C { 100 200 300 }
vexpr {
    D = vstack(A,B)
    E = hstack(A,C)
    F = vstack(A,C)
}
{% endraw %}
{% endhighlight %}

The hypothetical combination `C = hstack(A,B)` is signalled by **VecTcl** as an error due to a missmatch in the dimensions of the arrays `A` and `B` for horizontal stacking (`B` has less values than required).

For the rest, we would have, in nested list form:

{% raw %}
D -- {1 2} {3 4} {5 6} {10 20} {30 40}
E -- {1 2 100} {3 4 200} {5 6 300}
F -- {1 2} {3 4} {5 6} {100 200}
{% endraw %}

Notice (at `F`) how part of `C` has been lost when concatenating it vertically to `A`. Interestingly too `C` being naturally thought of a column vector in **VecTcl** has been transposed to make it match `A` row structure.

## Accessing the elements of a numeric array.

We shall start little, we will try to access to one particular element to begin with. Later on we will see how to get to entire ranges of elements (called slices).

The first way to get or set a particular element of a numeric array is by using brackets. Let's see an example:

{% highlight tcl %}
{% raw %}
vexpr {
    A = eye(2);
}
{% endraw %}
{% endhighlight %}

Get the value of a given position by using brackets:

{% highlight tcl %}
{% raw %}
puts [vexpr {A[1,1]}]
{% endraw %}
{% endhighlight %}

This naturally gives `1`.

Notice that indices start by `0` in **VecTcl**... This means the bottom right element of matrix `A` is given by `A[1,1]` if we are using the indices from the start of the array (we could use them from its end too, we shall cover this later on when dealing with slices).

Let us change the value of a given position and check that it has effectively changed:

{% highlight tcl %}
{% raw %}
vexpr {A[1,1] = 2}
puts [vexpr {A[1,1]}]
{% endraw %}
{% endhighlight %}

Now we get our `2` out of the last expression.

Other ways to accomplish this:

{% raw %}
numarray::get numArray indexDim0 ?indexDim1 ...?
numarray set numArray indexDim0 ?indexDim1 ...? value
{% endraw %}

So the following example would print `3` for us:

{% highlight tcl %}
{% raw %}
numarray set A 1 1 3
puts [numarray::get $A 1 1]
{% endraw %}
{% endhighlight %}

## Slicing a numeric array.

Apart from accessing particular elements within a numeric array whole ranges can be "sliced" out of it by using the slicing syntax, borrowed from **Python**.

See different examples of possible accesses and slices. We will use the following matrix:

{% highlight tcl %}
{% raw %}
set A {{1 2 3 4 5 6} {7 8 9 10 11 12} {13 14 15 16 17 18}}
{% endraw %}
{% endhighlight %}

Note that indices in **VecTcl** start at 0.

| slice | result | explanation |
|---|---|---|
| A[0,0] | 1 | top left element |
| A[-1,-1] | 18 | bottom right element |
| A[0,-2] | 5 | first row, second column from the end  |
| A[:,:] | {1 2 3 4 5 6} {7 8 9 10 11 12} {13 14 15 16 17 18} | whole array |
| A[-2,:] | 7 8 9 10 11 12 | second row from the end |
| A[:,0] | 1 7 13 | first column |
| A[-3:-2,:] | {1 2 3 4 5 6} {7 8 9 10 11 12} | third to second row from the end |
| A[:,3:5] | {4 5 6} {10 11 12} {16 17 18} | third to fith columns |
| A[-3:-2,3:5] | {4 5 6} {10 11 12} | combine last two results |
| A[:,3:5:2] | {4 6} {10 12} {16 18} | third to fith columns with step 2 |
| A[:,-1:-3] |   | nothing from last col. to third from the end with step 1 |
| A[:,-1:-3:-1] |   | this isn't the way to express it either |
| A[:,-3:-1:-1] | {6 5 4} {12 11 10} {18 17 16} | from last col. to third from the end backwards |

In principle, **VecTcl** may report an index out of bounds if user misspecifies the slice, sometimes, as we have seen, **VecTcl** just returns an empty array if there is nothing to give back.

## Diagonal.

To extract the diagonal of a numeric array could perhaps be seen as a specialized form of slicing.

{% highlight tcl %}
{% raw %}
set A {{1 2 3} {4 5 6} {7 8 9}}
vexpr {
    B = diag(A)
    C = diag(constfill(4,2,3))
    D = diag(constfill(4,3,2))
}
{% endraw %}
{% endhighlight %}

This would give:

{% raw %}
<div>
$$
B = \begin{bmatrix}
1 \
5 \
9
\end{bmatrix}
\;
C = D = \begin{bmatrix}
4 \
4
\end{bmatrix}
$$
</div>
{% endraw %}

Final note: function `diag` is not defined for dimensions higher than 2.

## Elementwise operations.

Elementwise operations are those performed over all elements of a numeric array.

### Unary operators.

The most elementary one is the negation operator `-` or, in function form, `neg`. This negates all elements of a numeric array. Take the following example:

{% highlight tcl %}
{% raw %}
vexpr {
    A = eye(2)
    B = -A
    C = neg(A)
}
{% endraw %}
{% endhighlight %}

This would give:

{% raw %}
<div>
$$
B = C = \begin{bmatrix}
-1.0 & -0.0 \\
-0.0 & -1.0
\end{bmatrix}
$$
</div>
{% endraw %}

If you think that something funky happens in the output with `-0.0` rest assured this is ok regarding **IEEE** standards. By the way, **Tcl** itself produces those negative zeros for certain operations.

As you probably can guess by now, we could also have used the form:

{% raw %}
numarray::neg numArrayVal
{% endraw %}

### Binary operators.

Let's now see binary operators.

For instance, let's add a number to all elements of a numeric array with `+` or `.+`:

Examples:

{% highlight tcl %}
{% raw %}
vexpr {
    A = eye(2)
    B = A + 5
    C = 5 + A
    D = A .+ 5
    E = 5 .+ A
}
{% endraw %}
{% endhighlight %}

This would give us:

{% raw %}
<div>
$$
B = C = D = E = \begin{bmatrix}
6 & 5 \\
5 & 6
\end{bmatrix}
$$
</div>
{% endraw %}

This does not work only for adding a scalar to every element of a numeric array. These operators combine two numeric arrays. See this example:

{% highlight tcl %}
{% raw %}
vexpr {
    A1 = eye(2)
    A2 = constfill(5,2,2)
    B = A1 + A2
    C = A2 + A1
    D = A1 .+ A2
    E = A2 .+ A1
}
{% endraw %}
{% endhighlight %}

Notice that in this case we need both arrays, `A1` and `A2`, to have the same shape. If we try to sum arrays of different dimensions we will get an error message.

Finally, notice that we could also have used the forms:

{% raw %}
numarray::+ numArray1Val numArray2Val
numarray::.+ numArray1Val numArray2Val
{% endraw %}

The substraction operator (`-` or `.-`) works the same way as the addition one.

The elementwise multiplication operator is just `.*` (as `*` would be the multiplication defined among numerical arrays not the elementwise one), but it works in the same way as the addition or substration ones do.

All these three operators, being commutative, don't care much about the order of the operands.

Now let us see the elementwise power operator `.^`. Notice that in this case the order of the operands matter.

{% highlight tcl %}
{% raw %}
vexpr {
    A = 2 .* eye(2)
    B = A .^ 2
    C = 2 .^ A
}
{% endraw %}
{% endhighlight %}

If we do this we would get something equivalent to:

{% raw %}
<div>
$$
B = \begin{bmatrix}
4.0 & 0.0 \\
0.0 & 4.0
\end{bmatrix}
\;
C = \begin{bmatrix}
4.0 & 1.0 \\
1.0 & 4.0
\end{bmatrix}
$$
</div>
{% endraw %}

In the first case we have told **VecTcl** to raise all elements of `A` to the second power, notice that `0^2` is `0`. In the second case **VecTcl** raises `2` to each of the elements of `A` to produce matrix `C`.

Another operator for which order is important is the remainder operator `%` although this one needs one of its operands to be a scalar (real or integer).

Example:

{% highlight tcl %}
{% raw %}
set A {{1 2 3} {4 5 6} {7 8 9}}
vexpr {
  B = A % 2
  C = 9 % A
}
{% endraw %}
{% endhighlight %}

{% raw %}
<div>
$$
B = \begin{bmatrix}
1 & 0 & 1 \\
0 & 1 & 0 \\
1 & 0 & 1
\end{bmatrix}
\;
C = \begin{bmatrix}
0 & 1 & 0 \\
1 & 4 & 3 \\
2 & 1 & 0
\end{bmatrix}
$$
</div>
{% endraw %}

There are a couple of elementwise operators that might need clarification: `./` (right divide) and `.\\` (left divide). The order implied by the operator matters.

Let's see an example of right divide.

{% highlight tcl %}
{% raw %}
vexpr {
    A1 = constfill(2,2,2)
    A2 = constfill(4,2,2)
    B = A1 ./ A2
    C = A2 ./ A1
}
{% endraw %}
{% endhighlight %}

This gives:

{% raw %}
<div>
$$
B = \begin{bmatrix}
0.5 & 0.5 \\
0.5 & 0.5
\end{bmatrix}
\;
C = \begin{bmatrix}
2.0 & 2.0 \\
2.0 & 2.0
\end{bmatrix}
$$
</div>
{% endraw %}

This is what we would expect in both cases dividing elementwise the first matrix by the second one.

Left divide (`.\\`) is not recognized yet by the **VecTcl** parser. But, in essence, it would go like this: `A .\\ B` would be equivalent of saying `B ./ A`.

After having seen all these elementwise operators, see that `A += 2` is a simplified way to write `A = A + 2`. Equivalently you can do the same for the following operators: `.+=`, `-=`, `.-=`, `.*=`, `./=`, (I'm not counting on `.\\=` just yet), `.^=`.

To form a new numeric array by selecting, in an elementwise manner, the minimum or the maximum of two numeric arrays we have:

{% raw %}
binarymin(numArrVar1,numArrVar2)
numarray::binarymin numArrVal1 numArrVal2

binarymax(numArrVar1,numArrVar2)
numarray::binarymax numArrVal1 numArrVal2
{% endraw %}

A few examples of using these functions with the minimum:

{% highlight tcl %}
{% raw %}
set A { {1 2 3} {4 5 6} {7 8 9} }
set B { {6 5 4} {3 2 1} {9 8 7} }
vexpr {
  C0 = binarymin(A,B)
  C1 = binarymax(A,5)
}
set C2 [numarray::binarymin $A $B]
{% endraw %}
{% endhighlight %}

Which would give:

{% raw %}
<div>
$$
C0 = C2 = \begin{bmatrix}
1 & 2 & 3 \\
3 & 2 & 1 \\
7 & 8 & 9
\end{bmatrix}
\;
C1 = \begin{bmatrix}
5 & 5 & 5 \\
5 & 5 & 6 \\
7 & 8 & 9
\end{bmatrix}
$$
</div>
{% endraw %}

There is another set of operators that produce a numeric array of `0` or `1` depending on some condition that is stated elementwise between two numeric arrays.

These are the relation operators: `>`, `<`, `>=`, `<=`, `==` and `!=`.

An example, that uses as starting point the numeric arrays `A` and `B` of the previous example:

{% highlight tcl %}
{% raw %}
vexpr {
    C0 = A < B
    C1 = A < 5
}
set C2 [numarray::< $A $B]
{% endraw %}
{% endhighlight %}

{% raw %}
<div>
$$
C0 = C2 = \begin{bmatrix}
1 & 1 & 1 \\
0 & 0 & 0 \\
1 & 0 & 0
\end{bmatrix}
\;
C1 = \begin{bmatrix}
1 & 1 & 1 \\
1 & 0 & 0 \\
0 & 0 & 0
\end{bmatrix}
$$
</div>
{% endraw %}

If we consider the notion of numeric array made of `0` and `1` as logical values we might find interesting to use the boolean operators to, elementwise, process numeric arrays in pairs.

The boolean operators are `not` (which is unary) and the binary ones given by `&&` (and) and `||` (or).

Using `C0` and `C1` from the previous example:

{% highlight tcl %}
{% raw %}
vexpr {
    CAnd = C0 && C1
    COr = C0 || C1
}
{% endraw %}
{% endhighlight %}

Will give:

{% raw %}
<div>
$$
CAnd = \begin{bmatrix}
1 & 1 & 1 \\
0 & 0 & 0 \\
0 & 0 & 0
\end{bmatrix}
\;
COr = \begin{bmatrix}
1 & 1 & 1 \\
1 & 0 & 0 \\
1 & 0 & 0
\end{bmatrix}
$$
</div>
{% endraw %}

### Elementwise functions.

Apart from the elementwise operators that we have just seen there are functions that are applied to each element of a numeric array.

These are the elementwise elementary transcendental functions: `sin`, `cos`, `tan`, `exp`, `log`, `sqrt`, `sinh`, `cosh`, `tanh`.

We can also include here the inverse circular and hyperbolic functions: `asin`, `acos`, `atan`, `asinh`, `acosh`, `atanh`.

And finally, there are some functions which apply to numeric arrays of complex numbers: `abs`, `sign`, `real`, `imag`, `arg` and `conj`.

Let's see an example with just one of them as they all work similarly.

{% highlight tcl %}
{% raw %}
set A { {1+3i 2-4i} {4-3i 1-1i} {2+3i 4} }
vexpr {
    B = imag(A)
}
{% endraw %}
{% endhighlight %}

{% raw %}
<div>
$$
B = \begin{bmatrix}
3.0 & -4.0 \\
-3.0 & -1.0 \\
3.0 & 0.0
\end{bmatrix}
$$
</div>
{% endraw %}

### A further example.

As `log10` function does not exist at time of writing this document let's use it as an example of creating our own functions in **VecTcl** with `vproc`.

{% highlight tcl %}
{% raw %}
set A {{0 1 10} {100 1000 0}}
vproc log10 x { log(x)./log(10.0) }
vexpr {
    B = log10(A)
}
{% endraw %}
{% endhighlight %}

Now `B` is:

{% raw %}
<div>
$$
B = \begin{bmatrix}
-NaN & 0.0 & 1.0 \\
2.0 & 2.9999999999999996 & -Inf
\end{bmatrix}
$$
</div>
{% endraw %}

Notice the entities `NaN` and `Inf`.

## Operations defined over numeric arrays.

The most used operator is probably `*`, which allows for the standard "dot" product defined over numeric arrays.

Examples:

{% highlight tcl %}
{% raw %}
set A {1 2 3}
vexpr {
    B = A'
    C = A * B
    D = B * A
    E = C * B
}
{% endraw %}
{% endhighlight %}

Which gives:

{% raw %}
<div>
$$
C = \begin{bmatrix}
1 & 2 & 3 \\
2 & 4 & 6 \\
3 & 6 & 9
\end{bmatrix}
\;
D = 14
\;
E = \begin{bmatrix}
14 \\
28 \\
42
\end{bmatrix}
$$
</div>
{% endraw %}

Raising a numeric array to a power is also a defined operation which can be invoked by `^` or `**` operators. At the moment this operation has not been implemented in **VecTcl** yet.

There are two array division operators. One is `/` and the other is `\`.

At the moment operator ´/´ is yet to be implemented in **VecTcl**.

With `\` one can solve the linear system `A x = y` for `x` (or give the least squares sense solution for it if the number of rows of `A` exceed the number of its columns) like:

{% highlight tcl %}
{% raw %}
set A { {0 0 1} {0 1 0} {1 0 0} }
vexpr {
    y = list(10,20,30)
    x = A\y
}
{% endraw %}
{% endhighlight %}

Will give:

{% raw %}
<div>
$$
x = \begin{bmatrix}
30.0 \\
20.0 \\
10.0
\end{bmatrix}
$$
</div>
{% endraw %}

### More auxiliary **Tcl** procedures.

Related to the `\` operator is the `inv` procedure that compute the inverse of a square matrix.

{% raw %}
inv(squareMatrix)
numarray::inv squareMatrix
{% endraw %}

For instance:

{% highlight tcl %}
{% raw %}
set A { {0 0 1} {0 1 0} {1 0 0} }
vexpr {
    B = inv(A)
}
{% endraw %}
{% endhighlight %}

This will give currently an error `invalid command name "vexpr"`.

There are also other convenience **Tcl** procedures that make `binarymin` and `binarymax` and easier to use.

{% raw %}
min(numArrVar1,?numArrVar2 ...?)
numarray::min numArrVal1 ?numArrVal2 ...?

max(numArrVar1,?numArrVar2 ...?)
numarray::max numArrVal1 ?numArrVal2 ...?
{% endraw %}

An example:

{% highlight tcl %}
{% raw %}
set A { {1 20} {3 40} {5 60} }
set B { {10 200} {30 400} {50 600} }
set C { {100 2} {300 4} {500 6} }
vexpr {
    D = min(A,B,C)
    E = max(A,B,C
}
{% endraw %}
{% endhighlight %}

This will naturally give:

{% raw %}
<div>
$$
D = \begin{bmatrix}
1 & 2 \\
3 & 4 \\
5 & 6
\end{bmatrix}
\;
E = \begin{bmatrix}
100 & 200 \\
300 & 400 \\
500 & 600
\end{bmatrix}
$$
</div>
{% endraw %}

## Aggregation functions.

The concept of aggregation in a numeric array is similar to the concept of aggregating in **SQL** by using `GROUP BY` sentences.

In general, the signature for an aggregation function in **VecTcl** is, using `sum` for instance:

{% raw %}
sum(numArrayVar,?axis?)
numarray::sum numArrayVal ?axis?
{% endraw %}

The presently supported aggregator commands in **VecTcl** are: `sum`, `axismin`, `axismax`, `mean`, `std`, `std1`, `all`, `any`.

Let's see some examples.

{% highlight tcl %}
{% raw %}
set A { {1. 2. 3.} {4. 5. 6.} {7. 8. 9.} }
B = sum(A)
C = sum(A,1)
{% endraw %}
{% endhighlight %}

This stores the column-wise sum of elements in `B` and the row-wise sum of elements in `C`. Or equivalently in our abused mathematical notation:

{% raw %}
<div>
$$
B = \begin{bmatrix}
12.0 & 15.0 & 18.0
\end{bmatrix}
\;
C = \begin{bmatrix}
6.0 \\ 15.0 \\ 24.0
\end{bmatrix}
$$
</div>
{% endraw %}

There are many things to note from this example.

For starters, the second argument passed to `C=sum(A,1)` is the index of the dimension to be used in the aggregation. The default value of this argument is `0` which stands for the first dimension (columns). We could have also said `B=sum(A,0)` explicitly.

Notice that the results are given in a way that reflects in which dimension the aggregation took place. `B` is a row vector (of two dimensions with shape `1 3`) obtained after having done a colum-wise (along dimension 0) aggregation of `A`. `C` is a column vector (of one dimension with shape `3`) obtained after having done a row-wise (along dimension 1) aggregation of `A`. The fact that `C` is a column vector and its shape is given by just one dimension makes us say the columns conform the first dimension.

Let us see how things would go for higher dimensional numeric arrays. Now, let's do:

{% highlight tcl %}
{% raw %}
set A {  { {1 2} {3 4} } { {10 20} {30 40} }  }
vexpr {
    B = sum(A)
    C = sum(A,1)
    D = sum(A,2)
}
{% endraw %}
{% endhighlight %}

As discussed previously `A` can be seen as a stack of 2 2 by 2 matrices.

Asking to sum on the stack dimension (dimension 0) gives us `B` which is a stack of 1 2 by 2 matrix given by `{ {11 22} {33 44} }`.

Asking to sum in dimension 1 (along the rows of the matrices of the stack) gives us `C` which is a two by two matrix given by `{4 6} {40 60}`.

Asking to sum in dimension 2 (along the columns of the matrices of the stack) gives us `D` which is a two by two matrix given by `{3 7} {30 70}`.

After having seen in detail how one of these aggregator functions works we will finish this section by clarifying what the rest of them do.

Take `axismin` and `axismax`. These compute the minimum or the maximum value of the numeric array respectively along the desired dimension.

Function `mean` computes the mean value of the numeric array along the desired dimension.

Functions `std` and `std1` compute the standard deviation along the desired dimension. The difference between them is that in the computation of `std` `1` is substracted from the number of elements when doing the computation (a detail of statistical meaning).

Function `all` returns `1` is all elements of a numeric array are different from zero along the desired dimension.

Function `any` returns `1` if any element of a numeric array is different from zero along the desired dimension..

(**Is `all` and `any`depictions correct ???**)

## Guest functions.

**VecTcl** aims to be a complete package with minimum dependencies. Apart from its many core functionalities it borrows from other places useful stuff adapted to its internals.

We will give here quickly the relevant signatures with some comments for these methods.

### Matrix decompositions.

To perform the QR decomposition of a matrix (provided that it does not have more columns than rows) use `qreco`.

{% raw %}
qreco(matrixVar)
numarray::qreco matrixVal
{% endraw %}

To perform the eigen decomposition of a matrix use either `eig` or `eigv`.

The first one will give the eigenvalues and the eigenvectors of the matrix.

{% raw %}
eig(squareMatrixVar)
numarray::eig squareMatrixVal
{% endraw %}

The second one will only give the eigenvalues of the matrix.

{% raw %}
eigv(squareMatrixVar)
numarray::eigv squareMatrixVal
{% endraw %}

To compute the singular value decomposition of matrix use either `svd` or `svd1`.

The first one will give the singular values and the singular vectors.

{% raw %}
svd(matrixVar)
numarray::svd matrixVal
{% endraw %}

The second one will only give the singular values.

{% raw %}
svd1(matrixVar)
numarray::svd1 matrixVal
{% endraw %}

The `schur` decomposition that returns the Schur vectors and the Schur form.

{% raw %}
schur(squareMatrixVar)
numarray::schur squareMatrixVal
{% endraw %}

### Fast Fourier transform and inverse.

The relevant signatures:

{% raw %}
fft(numArrayVar)
numarray::fft numArrayVal

ifft(numArrayVar)
numarray::ifft numArrayVal
{% endraw %}

## Benchmark functions.

Finally, some functions, namely `fastcopy`, `fastadd` and `linreg` are benchmark tools. They shouldn't be used, they probably break **EIAS** and they are largely useless except perhaps to core **VecTcl** developers.

## Final notes.

### Parsing literal doubles.

Inside `vexpr` a double literal with unspecified decimal part (like `1.`) gives syntax error due to ambiguity with elementwise operators (as in `1.+3`). The decimal part is required, beause otherwise the elementwise operators cannot be disambiguated: `1 .* a` versus `1. * a`

