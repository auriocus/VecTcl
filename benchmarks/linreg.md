---
layout: default
title: VecTcl
bench: true
subtitle: Linear regression benchmark
home: false
---

Given 2 Tcl lists with doubles x and y, compute the linear regression
line $$y\approx \alpha+\beta x$$. VecTcl is the clear winner in this contest:

![Linear regression, computation only]({{ site.baseurl }}/images/linreg_comp.png "Computation only")
![Linear regression, setup included]({{ site.baseurl }}/images/linreg_total.png "Total")

The first image shows the performance (measured in number of datapoints processed per second) for the 
computation only; the second includes also the time of converting from Tcl lists to the internal 
data structure. VecTcl is the clear winner in this contest, both for the
computation itself as for the conversion of the data. The code in VecTcl looks
like this:

{% highlight tcl %}
{% raw %}
vexpr { 
	xm=mean(xv); ym=mean(yv)
	beta=sum((xv-xm).*(yv-ym))./sum((xv-xm).^2)
	alpha=ym-beta*xm
}
{% endraw %}
{% endhighlight %}
	

And here comes the math:

There are two possible ways to complete this task.

* Using the direct formula
{% raw %}
<div>
$$
\begin{align} 
	\bar x &= \frac{1}{N} \sum x_i\\
	\bar y &= \frac{1}{N} \sum y_i\\
	\beta  &= \frac{\sum_i(x_i-\bar x)\cdot(y_i-\bar y)}{\sum_i (x_i-\bar x).^2}\\
	\alpha &= \bar y-\beta\bar x
\end{align}
$$
</div>
{% endraw %}
* Computing the least-squares solution to the system
{% raw %}
<div>
$$
\begin{align}
	A p &= y\\ 
	A &= 
	\begin{pmatrix} 
		1 & x_1\\
		1 & x_2\\
		\vdots & \vdots\\
		1 & x_N
	\end{pmatrix}\\
	p&=\begin{pmatrix} \alpha & \beta \end{pmatrix}
\end{align}
$$
</div>
{% endraw %}

Only the solution "VecTcl LS" uses the second formula, all others compute the
direct formula. 
