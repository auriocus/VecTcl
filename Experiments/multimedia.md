---
title: VecTcl
layout: default
subtitle: Multimedia processing using VecTcl
documentation: true
home: false
toplink: false
---


## Sound and image processing

Array languages such as Matlab and NumPy are often used to perform digital sound
and image processing. Therefore these systems include facilities to read and
write data from and to multimedia files and to display or play the data. In
conjunction with a talk at [EuroTcl 2015](http://www.eurotcl.tcl3d.org/), 
some image and sound processing experiments
were performed to demonstrate the suitability of VecTcl for these tasks and to
discover possible limitations.

## Image processing 
For the talk, a [demo application](https://github.com/auriocus/VecTcl/blob/master/demo/vectcl2015demo.tcl) was prepared
which allows to manipulate an image by a VecTcl expression. To try it, open a
small image, select a code preset (e.g. Daytime or Wiggle) and pull the sliders
at the bottom underneath the code window. 

In order to read and write image data, a bridge was created between a Tk photo image and a VecTcl
array as a VecTcl extension
([vectk](https://github.com/auriocus/VecTcl/blob/master/TkBridge/)). This
extension must be loaded after VecTcl and Tk. In this
way, VecTcl itself does not link against Tk and continues to work in a pure Tcl.
The extension provides two new commands, `numarray::fromPhoto` and
`numarray::toPhoto`, which convert numerical arrays to and from a Tk photo
image. A grayscale image is stored as a 2D array of doubles, ranging from 0 to
1. The first coordinate represents the vertical and second coordinate the
horizontal axis. This convention was chosen such that it agrees with the
standard notation of matrices. A color image has an additional third dimension,
containing the red, green, blue, and alpha planes.

The demo program simply presets a few variables like `height`, `width`, and
`input` with the ipnut image and its dimensions. `x` and `y` are preset with
arrays of the same dimension as the input. This allows to use the demo as a 2D
function plotter, by evaluating an expression like

{% highlight matlab %}
{% raw %}
	sin(x).*cos(y)
{% endraw %}
{% endhighlight %}

For a demonstration of the manipulation of an image, try the Daylight or Wiggle
preset, which manipulate the color planes and distort the image, respectively.
The code is fast enough to process images of small size in real time. 

### Missing functionality and bugs

During the implementation of the demo script, a few limitations of VecTcl
surfaced. The first is the defect of an integer casting function, to be able to
compute coordinates within VecTcl. This could be easily changed, and an `int()`
downcasting function was added.

Another missing functionality is vector indexing. VecTcl currently supports
indexing like in `a[3]` by translating it into a slice reaching from 3 to 3
(`3:3:1`). This makes it possible to use the efficient slicing infrastracture
(`NumArrayIterator`) on indices and partial indices like `a[3,2:5]`. But there is
no way to express an arbitrary sequence of non-contiguous indices. Such a
feature is provided by other array languages and could be used to implement
image distortion very easily, like in the Wiggle example

{% highlight matlab %}
{% raw %}
	ind=int(x+10*sin(y))
	input[ind]
{% endraw %}
{% endhighlight %}

The current code uses a for loop to iterate over the lines. It is still fast
enough, but doesn't generalize for arbitrary deformations, for instance it
would not be possible to have the waves travel diagonally or do magnifications
and such. Implementing indexing would require a major rethinking of the slicing
infrastructure, but is certainly worth it. A Tcl level loop running over all
pixels would be an order of magnitude slower than this. 

## Sound processing
At the [EuroTcl 2015 event](http://www.eurotcl.tcl3d.org/), the talks have been
recorded using a Canon EOS 550D DSLR. Sound was additionally captured by a
portable Zoom H2n recorder, attached to the speaker using a belt clip. While the
DLSR provides excellent video quality, the sound is poor due to the large
distance to the speaker. Additionally, recording stops after ~15 min and resumes
only a few seconds later. In order to synchronize the externally recorded sound
with the video, almost 40 movies need to be aligned to the soundtrack. 

As an experiment, a program was developed using VecTcl which automatically
synchronizes the videos to the external soundtrack based on the matching to
the internal sound. 
Before the computation can be done, the sound data must be converted into a
VecTcl array. This is accomplished by another extension, which reads in WAVE
files, [WavReader](https://github.com/auriocus/VecTcl/blob/master/WavReader/).
This is extension is currently restricted to little endian machines and 16 bit
PCM wave files, however that is the most common case.

Pattern matching in signal processing is usually done using some kind of
correlation function. In this case, it makes no sense to do a direct
cross-correlation of the raw audio data, because the phase of the sound between both microphones
is necessarily different. Instead, 
[the matching program](https://github.com/auriocus/VecTcl/blob/master/WavReader/testrun.tcl)
first reduces the audio data into a loudness profile, by dividing the track into
frames of 20 ms and computing the RMS value of each frame. The RMS data is then
matched (with an accuracy of 20 ms) between both recorded tracks using shifted
normalized cross-correlation. This works extremely well for the setting.

![cross correlation between two sound snippets]({{site.baseurl}}/images/crosscorr_sound.png)

The above figure displays the cross correlation of one of the recorded movies
fragments to the separate soundtrack. The sharp spike at 1104 s corresponds to
the shift to match both sound tracks. It can be easily detected by finding the
maximum, and is accurate to within one frame. 
