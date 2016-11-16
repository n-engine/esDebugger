#include "config.h"

#if defined(__WIN32__)
#include <windows.h>
#endif // __WIN32__

#include <extensions.h>
#include <debugger.h>
#include "window.h"

using namespace Debugger;

// #define TEST_ERROR_ONLY 1

#if defined(__WIN32__)

#define ID_DIALOG_CONSOLE 103 // resource.h
#define ID_EDIT 1002 // resource.h

// for testing a bad resquest (mixing header)
#define GL_MAX_3D_TEXTURE_SIZE 0x8073

int APIENTRY wWinMain(
	HINSTANCE hInstance,HINSTANCE hPrevInstance,
	LPWSTR lpCmdLine,int nCmdShow)
{
	bool result = Window::init(800,600,0/* flag is for future usage */);

	Window::createConsole(ID_DIALOG_CONSOLE, ID_EDIT,Window::getHwnd());

	gl::setConsoleCallback(Window::setConsole);

#if TEST_ERROR_ONLY
	/** enable break on error */
	Debugger::gl::setBreakOnError(true);
	/** enable break on warning */
	Debugger::gl::setBreakOnWarning(true);
	/** append all to log */
	Debugger::gl::setAppendToLogFunctionCalls(true);

	ne_assert(result);

	/** start to test the debugger */
	GLint value;

	// error test -------------------------------------------------
	glEnable( GL_TEXTURE_2D ); // must break on error : not allowed

	// do something not really standard (like calling a value from opengl es 3.0
	glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &value); // must break on error

	/** call glDrawElements() without having prepared something */
	glDrawElements(0, 1, 0, nullptr);

	/** todo: prepare shader, program, etc... */

	// warning test -----------------------------------------------
	// ok, first pass
	glEnable(GL_CULL_FACE); // no warning
	glEnable(GL_CULL_FACE); // must break on warning : already enabled
#else
	/** clear color : ~gray */
	glClearColor(0.533333f, 0.545098f, 0.552941f, 1.0f);

	const GLuint cl_flgs = GL_COLOR_BUFFER_BIT |
							GL_DEPTH_BUFFER_BIT |
							GL_STENCIL_BUFFER_BIT;

	/** main loop */
	while ( !Window::isDone() )
	{
		/** check for pause button */
		while (Window::isPaused() && !Window::isDone()) {
			Window::update();
		}

		gl::reset(); // clear old console content
		Window::update(); // get new message from the pool

		glClear(cl_flgs);

		if ( !glIsEnabled(GL_CULL_FACE) )
		{
			glEnable(GL_CULL_FACE); // no warning
		}

		Window::swap();
	}

#endif // TEST_ERROR_ONLY

	/* clean opengl and base window */
	Window::shutdown();

	return 0;
}

#else

int main(int argc, char* argv[]) { return 0; }

#endif // __WIN32__
