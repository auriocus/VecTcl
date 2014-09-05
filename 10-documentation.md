---
layout: default
title: VecTcl
subtitle: Documentation
home: false
toplink: true
permalink: "documentation.html"
---

**VecTcl** is a **Tcl** extension which allows users to operate, with a reasonable efficiency, with numeric arrays of arbitrary dimensions within **Tcl**. 

For a concise introduction into the usage of VecTcl, please refer to the [getting started tutorial]({{ site.baseurl }}/tutorial.html).

A high level description of the projected functionality and implementational details is given in the [design document]({{ site.baseurl }}/design.html), and ultimately, the [source code](https://github.com/auriocus/VecTcl) is currently the most accurate reference.

List of all available documentation

{% for p in site.pages %} {% if p.documentation %}
* [{{ p.subtitle }}]({{ site.baseurl }}{{ p.url }}) {% endif %} {% endfor %}

