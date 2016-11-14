# esDebugger

### What is it ? :

   esDebbuger is a simple OpenGL ES 2.0 debugger.
   It is a collection of function to help debugging your code
   when you work on OpenGL ES 2.0. It can help you find error more easily
   and with a bit more "explicit error message".


### Why :

I spent alot of time debugging a simple error on my engine.
Lost alot of time and energy to understand what was wrong with my code.
After too much the same error code like "GL_INVALID_OPERATION" I needed
something more usefull, like a direct assert on any error and any wrong
situation of using OpenGL (changing the same state 36 times for a frame
is not a good thing). So I made this *small* piece of code to help me
debugging my engine.
- There is no real tool out for debugging like gDebugger, codeXL
for OpenGL ES 2.0 on Windows/Linux (or I dont known the existing tool)
- Because there is alot of emulator for OpenGL es on Windows and Linux
But these tools doesnt helped me at all and I was very frustrated
not finding a simple bug ! With these tool, no problem (For my need)

### What is not ? :
   esDebugger is not a fully fonctionnal tool like "gDebugger", it is
   more like a library to use for debugging and reporting what is doing 
   and what was called with which define.
   I was missing time to make a Linux or Mac version of the reporting 
   dialog tool. So for now, there is only a Win32 version.

### Speed issue :
   Dont forget, it is a debugging tool.
   It is slow as hell when you have 300 function calls becase the lib
   report every function you call to a string buffer and report it
   to a dialog (for now).

### Usage :
   Take a look into the directory tests.
   In most case, when you need some help to debug your code.
   Just replace the standard opengl headers with these headers :

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
the debugger need two step to be initialized :
first one : when you have created your display, call
```
Extensions::initEgl( display );
```
And when you have all your display and surface ready, call
```
Extensions::init( );
```

I made a Window class function to show you how to use it.
I tried to keep it simple and small.

You can use the example code, as you want.
You can replace all the base class used inside the whole debugger.cxx
or debugger.h easily (look at the miscs.cxx and miscs.h file)

The code will report every call to console or to a log (it is up to you 
to give a log system) By default the output is for the console
and the standard output (std::cout)

### Reporting example :

Frame ID - Calls function - where it was called @ which line

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


The code is not thread safe and alot buggy, so pardon me if you found
something strange =)

### The Licence :
The licence was selected for the open source community, use it, share it
update it and make it better.

### Note :
The OpenGL function was exported automatically with a bash script.
So, all opengl function is not added to calls function history or error reporting, only these who I managed to work on.

I used [ANGLE](https://github.com/google/angle) for the dll, and some piece of code, thanks for the author of ANGLE ! (The licence is not the same, and I dont known if i can mixe licence, let me known if I made something bad)

I hope the esDebugger would be helpfull for someone else. Because it was for me.
Feel free to report bug, error, patch, request...

### TODO : 
Add more checkup on function
A texture viewer (dialog based ?)
A buffer viewer (same question as above)
Finish simple verification on all function

![Screenshot](/screenshot.jpg?raw=true "Screenshot")
