---
layout: default
title: VecTcl
subtitle: Benchmarks
home: false
---

Benchmarks were run on a 2.4 GHz Core i5 with 8GB RAM and 8MB L3 cache.

{% for p in site.pages %}
{% if p.bench %}
* [{{ p.subtitle }}]({{ site.baseurl }}{{ p.url }})
{% endif %}
{% endfor %}

The code for the benchmarks is available in the source tree for VecTcl

