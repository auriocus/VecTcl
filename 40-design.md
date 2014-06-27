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
4. _No external dependencies_
5. _Performance_

Read more about
{% for p in site.pages %} {% if p.design %}
* [{{ p.subtitle }}]({{ site.baseurl }}{{ p.url }}) {% endif %} {% endfor %}


