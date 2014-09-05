---
title: VecTcl
layout: default
subtitle: Example demonstrating array operations
documentation: true
home: false
toplink: false
---


## Vectcl's array features

One of the remarkable features of Vectcl is the way you can deal with
arrays of numerical data. This is akin to MATLAB, Numpy and Fortran 90
and the specific features include: operations on whole arrays, array
slices, constructing arrays etc. In this note we highlight these
features by means of two examples from completely different
numerical areas: data analysis and solving partial differential
equations.

### Example: Data analysis

Suppose you have a file that looks like this:

	Station,Date,Salinity,Temperature
	DO,2005-7-28,32.49,16.84
	OS30,2005-1-1,30.08,16.71
	NW2,2005-2-26,30.32,17.23
	DH,2005-1-19,31.88,16.61
	DO,2005-5-6,33.81,17.99
	DH,2005-6-10,28.60,18.78
	DH,2005-9-26,31.95,15.75
	...
	NW1,2005-11-7,28.70,19.09
	OS30,2005-12-12,32.85,16.20
	DO,2005-5-18,28.82,18.84

_Note:_ The data are completely arbitrary, they just have the typical
range for the salinity expressed in promilles and the temperature
expressed in degrees Celsius for a typical coastal region in a
temperate climate zone. The station labels are also completely
arbitrary.

Reading these data into a Tcl list of lists is a no-brainer:

{% highlight tcl %}
{% raw %}
set infile [open "somedata.csv"]
gets $infile

while { [gets $infile line] >= 0 } {
    lappend data [split $line ,]
}
{% endraw %}
{% endhighlight %}

Since Vectcl is very good at dealing with _numerical_ data, we would
like to use it to:

* determine the mean, standard deviation, minimum and maximum for the salinity and temperature
* examine the correlation between these two parameters

Unfortunately, Vectcl is not good at dealing with non-numerical data. So
the first step in our analysis is to get rid of the first two columns.
In this case it has no particular consequences, as we are not interested
in the data per station or possible trends over time.

It does have a different consequence though: within Tcl itself we often
use an empty string to indicate a value is missing (without indicating
why it is missing - but that is a different discussion). We can not do
that in Vectcl, as an empty string is not the representation of any
valid number. We need a different way to represent "missing values"!

Enter the infamous "not-a-number" number (or: NaN). I call it infamous
because it is a nasty entity to work with. For instance: $$x \leftarrow 1 + \mathrm{NaN}$$
gives $$x \leftarrow \mathrm{NaN}$$. Calculating with NaNs generally leads to new NaNs. You
could call it a viral number. Another peculiar property of this "number" is:
NaN is not equal to any number including itself! In fact this is the most
reliable way of determining whether you are dealing with "not-a-number" or not.

So, if one of our measurements got messed up and we could not include a
proper number, we could use the string `"NaN"` (which is
interpreted as the "not-a-number" number) instead to indicate
the missingness. One way of dealing with such gaps is shown below.

Okay, since our data are complete, we can use traditional Tcl code to
extract the two columns we need and ignore for now the issue of missing
values:

{% highlight tcl %}
{% raw %}
set table {}
foreach row $data {
    lappend table [lrange $row 2 3]
}
{% endraw %}
{% endhighlight %}

To work with the columns with salinity and temperature data we use this
code fragment (to save space we will leave out the `vexpr` part in the
remainder):

{% highlight tcl %}
{% raw %}
vexpr {
    salinity    = table[:,0]
    temperature = table[:,1]
}
{% endraw %}
{% endhighlight %}

Because it will be convenient later to have the salinity data in
one variable and the temperature data in another, we use _array slices_.
The result of taking such a slice is one-dimensional array, but we
can also take a small rectangular part:

    rectangle = table[3:10,:]

The colon (:) is an abbreviation for "the whole range at the given
dimension".

Introspection is provided by such functions as _shape()_.

Now let us determine some basic statistical properties of the salinity
and temperature data:

    salinity_mean = mean(salinity)

The function _mean()_ determines the mean value - by default by
examining the data along the first dimension. So if we apply this
function to the variable _table_ instead, we get a one-dimensional
array that contains the mean of the first column and the second column:

    table_mean = mean(table)

    puts(table_mean)  # Use the Tcl command puts to quickly print the result
    puts(shape(table_mean))

With the data in my example file, I get:

	31.346400000000006 17.65139999999999
	2

so indeed an array with two elements, the mean over the two columns.

If you want to average over the rows use:

    table_mean = mean(table,1)

Of course, with the two columns representing totally different physical
parameters that is nonsensical, but that is beyond Vectcl's concerns.

To compute the minimum and maximum we have the _min()_ and
_max()_ functions. With one argument they give the minimum and
maximum value in the first dimension. With two arguments they compare
their arguments. Finally to assign the results to variables with more
meaningful names, we pick individual array elements:

    tmean  = mean(table)
    tmin   = min(table)
    tmax   = max(table)

    salmean  = tmean[0]
    tmpmean  = tmean[1]

Now, for the standard deviation there are the functions _std()_ and
_std1_(), respectively the unbiased and the population standard
deviations. But we also want the correlation between salinity and
temperature and that is something we need to calculate explicitly.

Here are the expressions that gives us that:

    tstdev   = sqrt(mean(table.^2)-tmean.^2)
    salstdev = tstdev[0]
    tmpstdev = tstdev[1]

    corr     = mean((salinity-salmean).*(temperature-tmpmean))/salstdev/tmpstdev

Note the ".*" and ".^" operators - they are used to indicate that the
operation is to be applied element-wise. Multiplication "*" is
interpreted as a matrix-matrix or matrix-vector operation.

As you can see, Vectcl makes it possible to eliminate a lot of loops
that would otherwise obscure the code with irrelevant details. Well,
that is an exaggeration, but array operations can clarify the code
considerably.

### Loops over array elements and dealing with NaNs

Vectcl is still a package in development and we need to determine what
features are useful to be included in the core. One thing it does not
currently have is a function to remove array elements that do not
conform to a certain condition - a `filter` function.

The `vproc` command allows us to define just such a function. We
ignore the possibility of multidimensional arrays for the sake of
simplicity:

{% highlight tcl %}
{% raw %}
vproc filter {array condition} {
	sz        = shape(array)     # Assume a one-dimensional array
	new_array = zeros(sum(condition))

	j = 0
	for i=0:sz-1 {
		if (condition[i]) {
			new_array[j] = array[i]
			j = j + 1
		}
	}
	new_array     # Return the result
}
{% endraw %}
{% endhighlight %}

We can use this to get rid of those nasty NaNs like:

    array = filter(array,array==array)

For instance:

{% highlight tcl %}
{% raw %}
set v {1.0 2.0 NaN 1.2 3.4 NaN -0.2 1.3 4.3 -1.8}

vexpr {
    w = filter(v,v==v)
}
puts $w
{% endraw %}
{% endhighlight %}

which results in:

	1.0 2.0 1.2 3.4 -0.2 1.3 4.3 -1.8

You can use this technique to build all manner of functions, for
instance a function to calculate the sum of only those elements that
conform to a condition.

### Example: partial differential equations

The second example concerns solving mathematical problems that
are described via partial differential equations (PDE). Here the simpler
numerical methods use grid of data points and a series of algebraic
equations to replace the often intractable PDEs. A vast body of
literature is available on this topic, so we will concentrate on the
features that make Vectcl such a nice tool in this context.

Consider the following problem - well, it is not a practical one as far
as I know, I merely constructed it as something slightly more involved
than, say, heat conduction in a one-dimensional bar.

So, the problem:

We have a square sheet of inflammable material which is heated in the
centre. If the temperature of the material exceeds a certain threshold,
it catches fire and burns slowly. That is an additional source of
heat. At the edges the sheet is kept at a constant temperature.

What we want to know is: what is the temperature near the point where it
is heated and how large is the burned area?

The sheet is divided into a grid of 21 by 21 cells and for each cell we
set up a heat balance:

* Heat is conducted to surrounding cells if the temperature there is lower
* At the centre a constant amount of heat is added
* If the temperature is above the threshold, then the material starts to
  burn, producing heat. We assume all manner of simplifications, though,
  so just the concentration of the inflammable component gets lower.

The first step is simple enough:

The heat conducted into or out of the cell is the sum of the heat fluxes
over the four sides of the cell. Via so-called finite differences we can
calculate an approximation of the temperature gradient. It takes a few
steps, but the heat balance for a cell can be expressed as:

   dHeat = lambda * (Tleft + Tright + Tup + Tdown - 4*Tcell) / dx^2

where _lambda_ measures the conductivity and _dx_ the grid cell's size.

Special attention is required for the cells near the boundary: there we
have the boundary conditions to deal with.

Using Vectcl's array slicing techniques, the heat conduction in the
whole grid can be determined via this expression:

    dT[1:nb,1:nb] = a*(T[0:nbm,1:nb]+T[2:nmx,1:nb]+T[1:nb,0:nbm]+T[1:nb,2:nmx]-4.0*T[1:nb,1:nb])

where the auxiliary parameters _nb_, _nbm_ are related to the grid size
and the parameter _a_ comprises the heat conduction and the heat
capacity.

By judiciously selecting the bounds of the array slices we get the heat
conduction for all grid cells inside the actual grid. We exclude the
boundary cells.

To include the burning of the material, we determine how quickly it
burns:

    dC = -f * C .* (T>Tcrit)
    dT = dT - r * dC

where _f_ and _r_ are parameters that measure the rate of burning and
the amount of heat that is released during the burning.

_Note_: since we are dealing with two square two-dimensional arrays (_C
and the condition _T>Tcrit_) we need to be careful with the operators:
we need the element-wise multiplication. I fell into that trap myself
when developing this example.

The heating in the centre of the grid is a bit simpler:

    dT[nq,nq] = dT[nq,nq] + qheat

where _nq_ is (21-1)/2 = 10 - the index of the central grid cell.

Determining the temperature and concentration at the new time level is
also simple:

    T = T + dT * dt
    C = C + dC * dt

as _dT_ and _dC_ in the above description are calculated as proper
time-derivatives.

The complete code is given below. It contains some reasonable values for
the various physical parameters and it also combines several operations
in the actual parameters used - so that the change per time is
calculated directly and not the time-derivative.

{% highlight tcl %}
{% raw %}
#
# Set up the grid and some parameters
#
vexpr {
    rho      = 1.0e3  # kg/m3
    cp       = 1.0e4  # J/kg.K
    dx       = 0.02   # mm
    lambda   = 1.0    # W/m.K
    dt       = 0.1 * rho * cp * dx^2 / lambda # This is a stability criterium

    qheat    = 1.0e4 / rho / cp / dx^2 # W/m2 --> K/s

    fcombust = 8.0e-6 # Rate coefficient for combustion
    rcombust = 2.0e5  # Amount of heat produced
    Tcrit    = 9.0    # K - excess temperature

    Fcombust = fcombust * dt
    Rcombust = rcombust * Fcombust

    nsize    = 21     # Number of grid cells
    nq       = 10     # Location of the heated cell

    nmx      = nsize - 1 # Last cell (right boundary)
    nb       = nsize - 2 # Last active cell
    nbm      = nb    - 1 # Last "left" cell

    T        = constfill(0.0,nsize,nsize)
    dT       = constfill(0.0,nsize,nsize)
    C        = constfill(1.0,nsize,nsize)
    dC       = constfill(0.0,nsize,nsize)

    a        = lambda * dt / rho / cp / dx^2
}

proc report {t Th Tn Ch area} {
    puts "$t $Th $Tn $Ch $area"
}

#
# Loop over time
#
vexpr {
    for i=0:100 {
        #
        # Heat conduction
        #
        dT[1:nb,1:nb] = a*(T[0:nbm,1:nb]+T[2:nmx,1:nb]+T[1:nb,0:nbm]+T[1:nb,2:nmx]-4.0*T[1:nb,1:nb])

        #
        # Combustion
        #
        dC = -Fcombust * C .* (T>Tcrit)
        #dC = -Fcombust * (T>Tcrit)
        dT = dT - Rcombust * dC

        puts(dC[nq,nq])

        #
        # Heating in central point
        #
        dT[nq,nq] = dT[nq,nq] + qheat

        #
        # Update the temperature and the concentration
        #
        T = T + dT
        C = C + dC

        time = i * dt
        Th = T[nq,nq]
        Ch = C[nq,nq]
        Tn = T[nq+1,nq+1]

        # area where something is burning
        area = sum(sum(dC<0,1),0)

        report(time,Th,Tn,Ch,area)
    }
}
{% endraw %}
{% endhighlight %}

Experiment a bit with these parameters to see how sensitive the central
temperature is - choosing a larger combustion rate coefficient
_fcombust_ quickly lead to very high temperatures. Because of the
threshold for combustion, this problem is not analytically solvable.
Numerical simulations like these are one convenient way to study them
and with Vectcl Tclers have a powerful instrument at their disposal.
