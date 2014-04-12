---
layout: default
title: VecTcl
bench: true
subtitle: Memory bandwidth benchmark
home: false
---

Adding to large arrays is mostly limited by memory bandwidth; this
microbenchmark compares the speed of various operations against memcpy and
a simple C-coded loop that adds to vectors. memcpy outperforms the C-coded loop by a
factor of 2. VecTcl assignment operators (i.e., `x+=y`) should ideally approach the C-coded loop. For
vectors, we are almost there. Tall matrices are worst. Reductions (i.e., `sum(x)`)
should be improved.


![Memory bandwidth test, vectors]({{ site.baseurl }}/images/memcpy_vectors.png)
![Memory bandwidth test, square matrices]({{ site.baseurl }}/images/memcpy_square.png)
![Memory bandwidth test, wide matrices]({{ site.baseurl }}/images/memcpy_wide.png)
![Memory bandwidth test, tall matrices]({{ site.baseurl }}/images/memcpy_tall.png)

