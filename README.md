# esDebugger
---

## Overview
esDebbuger is a simple OpenGL ES 2.0 debugger.
It is a collection of function to help debugging your code
when you work on OpenGL ES 2.0. It can help you find error more easily
and with a bit more "explicit error message".

## Table of Contents
* [Why esDebugger](#Why)
* [Usage](#Usage)
* [Reporting example](#Reporting)
* [Licence](#Licence)
* [Speed issue](#SpeedIssue)
* [Note](#Note)
* [Todo list](#TODO)
* [Screenshot](#Screenshot)
* [Changelog](#Changelog)
* [Upcoming change](#Upcoming)

<A NAME="Why">
## Why esDebugger :

1. I spent alot of time debugging a simple error on my engine.<br/>
Lost alot of time and energy to understand what was wrong with my code.<br/>
After too much the same error code like "GL_INVALID_OPERATION" I needed<br/>
something more usefull, like a direct assert on any error and any wrong<br/>
situation of using OpenGL (changing the same state 36 times for a frame<br/>
is not a good thing). So I made this *small* piece of code to help me<br/>
debugging my engine.<br/>
2. There is no real tool out for debugging like gDebugger, codeXL<br/>
for OpenGL ES 2.0 on Windows/Linux (or I dont known the existing tool)<br/>
3. Because there is alot of emulator for OpenGL es on Windows and Linux<br/>
But these tools doesnt helped me at all and I was very frustrated<br/>
not finding a simple bug ! With these tool, no problem (For my need)<br/>

<A NAME="Usage">
## Usage :
Take a look into the directory tests.<br />
In most case, when you need some help to debug your code.<br />
Just replace the standard opengl headers with these headers :<br />

* Replace:
 
```
#include <EGL/egl.h>
#include <GLES2/gl2.h>
```

* With :

```
#include <debugger.h>
#include <extensions.h>
```
the debugger need two step to be initialized :<br/>
first one : when you have created your display, call<br/>
```
Extensions::initEgl( display );
```
And when you have all your display and surface ready, call<br/>
```
Extensions::init( );
```

I made a Window class function to show you how to use it.<br/>
I tried to keep it simple and small.<br/>

You can use the example code, as you want.<br/>
You can replace all the base class used inside the whole debugger.cxx<br/>
or debugger.h easily (look at the miscs.cxx and miscs.h file)<br/>

The code will report every call to console or to a log (it is up to you <br/>
to give a log system) By default the output is for the console<br/>
and the standard output (std::cout)<br/>

<A NAME="Reporting">
## Reporting example :

Frame ID - Calls function - where it was called @ which line<br/>

```
1 glClearColor( 0.533333, 0.545098, 0.552941, 1 ) (ogles2/renderer.cxx@597)
1 glDepthMask( flag:GL_TRUE ) (ogles2/renderer.cxx@481)
1 glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT ) (ogles2/renderer.cxx@652)
1 glEnable( GL_BLEND ) (ogles2/renderer.cxx@624)
1 glUseProgram( program:1 ) (ogles2/shader.cxx@1009)
1 glActiveTexture( GL_TEXTURE0 ) (ogles2/texture.cxx@1484)
1 glBindTexture( target:GL_TEXTURE_2D, texture:1 ) (ogles2/texture.cxx@1527)
1 glUniform1i( location:1, v0:0 ) (ogles2/shader.cxx@811)
1 glUniformMatrix4fv( location:0 count:1 transpose:GL_FALSE value[0]:1.81066 ) (ogles2/shader.cxx@1122)
1 glEnableVertexAttribArray( index:0 ) (ogles2/vertexbuffer.cxx@163)
1 glEnableVertexAttribArray( index:2 ) (ogles2/vertexbuffer.cxx@163)
```
.... and so one.


The code is not thread safe and alot buggy, so pardon me if you found<br/>
something strange =)<br/>

<A NAME="Licence">
## Licence :
The licence was selected for the open source community.<br/>

<A NAME="SpeedIssue">
## Speed issue :
Dont forget, it is a debugging tool.<br />
It is slow as hell when you have 300 function calls becase the lib<br />
report every function you call to a string buffer and report it<br />
to a dialog (for now).<br />

<A NAME="Note">
## Note :
The OpenGL function was exported automatically with a bash script.<br/>
So, all opengl function is not added to calls function history or error reporting, only these who I managed to work on.<br/>

I used [ANGLE](https://github.com/google/angle) for the dll, and some piece of code<br/>
thanks for the author of ANGLE ! (The licence is not the same, and I dont known<br/>
if i can mixe licence, let me known if I made something bad)<br/>
<br/>
I hope the esDebugger would be helpfull for someone else. Because it was for me.<br/>
Feel free to report bug, error, patch, request...<br/>

<A NAME="TODO">
## Todo list :
- [ ] Add more checkup on function
- [ ] A texture viewer (dialog based ?)
- [ ] A buffer viewer (same question as above)
- [ ] Finish simple verification on all function
- [ ] Move example to gtk or fltk

<A NAME="Screenshot">
## Screenshot

![Screenshot](/screenshot.jpg?raw=true "Screenshot")

<A NAME="Changelog">
## Changelog
tbd

<A NAME="Upcoming">
## Upcoming change
Currently, I am working on the texture side (On dialog, on Windows for now, but soon on fltk.)
Added the capability to watch every texture sent to opengl (ditto for mipmap level)
