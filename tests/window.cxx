/* 
 * Simple OpenGL ES 2.0 debugger
 * 
 *  Copyright (C) 2016 ESTEVE Olivier
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * 
 * 
 */
#include "config.h"

#if defined(__WIN32__)
#include <windows.h>
#include "win32/resource.h"
#endif // __WIN32__

#include "extensions.h"
//#include "debugger.h"
#include "window.h"


//using namespace Debugger; // we are using gl:: and egl::

bool Window::_is_done = false;
bool Window::_is_paused = false;

int Window::_width = 0;
int Window::_height = 0;

EGLContext Window::_context = nullptr;
EGLDisplay Window::_display = nullptr;
EGLConfig Window::_config = nullptr;
EGLSurface Window::_surface = nullptr;

#if defined(__WIN32__)

HWND Window::_hwnd = nullptr;
HDC Window::_hdc = nullptr;
HGLRC Window::_hrc = nullptr;

static uint edit_control_id = 0;
static uint dialog_control_id = 0;

HWND Window::_dlg_hwnd = nullptr;

static LRESULT CALLBACK winProc(HWND hWnd,UINT msg,
	WPARAM wPar,LPARAM lPar)
{
	switch(msg)
	{
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
			if (wPar == VK_ESCAPE)
				Window::setIsDone( true );
		break;

		case WM_SYSKEYUP:
		case WM_KEYUP:
		return 0;
		case WM_CLOSE:
			Window::setIsDone( true );
		break;
	}

	return DefWindowProc(hWnd,msg,wPar,lPar);
}

bool Window::baseWindow(int width,int height)
{
	BYTE and_mask	= 0xff;
	BYTE xor_mask	= 0x00;
	WNDCLASS wc;
	DWORD dwExStyle;
	DWORD dwStyle;

	_width = width;
	_height = height;

	dwExStyle	= WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
	dwStyle		= WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;

	wc.style		= 0;
	wc.lpfnWndProc	= winProc;
	wc.cbClsExtra	= 0;
	wc.cbWndExtra	= 0;
	wc.hInstance	= GetModuleHandle(NULL);
	wc.hIcon		= CreateIcon(wc.hInstance,1,1,1,1,&and_mask,&xor_mask);
	wc.hCursor		= LoadCursor(0,IDC_ARROW);
	wc.hbrBackground= 0;
	wc.lpszMenuName	= 0;
	wc.lpszClassName= "esDebugger";

	if ( !RegisterClass(&wc) )
	{
		TRACE_ERROR("RegisterClass() failed");
		ne_assert(!"RegisterClass() failed");

		_is_done = true;
		
		return 0;
	}

	RECT windowRect = {0, 0, _width, _height };
	AdjustWindowRectEx(&windowRect,WS_OVERLAPPEDWINDOW,0,0);

	_width	= windowRect.right - windowRect.left;
	_height	= windowRect.bottom - windowRect.top;

	_hwnd = CreateWindowEx(dwExStyle,"esDebugger","esDebugger",dwStyle,
			CW_USEDEFAULT,CW_USEDEFAULT,_width,_height,0,0, wc.hInstance,0);

	if(_hwnd == 0)
	{
		TRACE_ERROR("CreateWindowEx() failed");
		ne_assert(!"CreateWindowEx() failed");
		_is_done = true;
		return 0;
	}

	_hdc = GetDC(_hwnd);

	if(_hdc == 0)
	{
		TRACE_ERROR("GetDC() failed");
		ne_assert(!"GetDC() failed");
		_is_done = true;
		return 0;
	}

	return 1;
}

#endif // __WIN32__

static const char* eglErrorString(int result)
{
	switch (result)
	{
		case EGL_SUCCESS:
		return "The last function succeeded without error.";
		case EGL_NOT_INITIALIZED:
		return "EGL is not initialized, or could not be initialized, for the "
			"specified EGL display connection.";
		case EGL_BAD_ACCESS:
		return "EGL cannot access a requested resource (for example a context "
			"is bound in another thread).";
		case EGL_BAD_ALLOC:
		return "EGL failed to allocate resources for the requested operation.";
		case EGL_BAD_ATTRIBUTE:
		return "An unrecognized attribute or attribute value was passed in"
			" the attribute list.";
		case EGL_BAD_CONTEXT:
		return "An EGLContext argument does not name a valid EGL rendering "
			"context.";
		case EGL_BAD_CONFIG:
		return "An EGLConfig argument does not name a valid EGL frame buffer "
			"configuration.";
		case EGL_BAD_CURRENT_SURFACE:
		return "The current surface of the calling thread is a window, pixel "
			"buffer or pixmap that is no longer valid.";
		case EGL_BAD_DISPLAY:
		return "An EGLDisplay argument does not name a valid EGL display "
			"connection.";
		case EGL_BAD_SURFACE:
		return "An EGLSurface argument does not name a valid surface (window, "
			"pixel buffer or pixmap) configured for GL rendering.";
		case EGL_BAD_MATCH:
		return "Arguments are inconsistent (for example, a valid context "
			"requires buffers not supplied by a valid surface).";
		case EGL_BAD_PARAMETER:
		return "One or more argument values are invalid.";
		case EGL_BAD_NATIVE_PIXMAP:
		return "A NativePixmapType argument does not refer to a valid native "
			"pixmap.";
		case EGL_BAD_NATIVE_WINDOW:
		return "A NativeWindowType argument does not refer to a valid native "
		"window.";
		case EGL_CONTEXT_LOST:
		return "A power management event has occurred. The application must "
			"destroy all contexts and reinitialise OpenGL ES state and "
			"objects to continue rendering";
	}
	return 0;
}


#define checkForEgl(function) checkForEgl_(function, __FILE__, __LINE__)

static void checkForEgl_(const char* function, const char* file, const int& line)
{
	EGLint lastError = eglGetError();

	if (lastError != EGL_SUCCESS)
	{
		TRACE_ERROR( 
			function << " errCode: " <<
			lastError << " errString: " << eglErrorString( lastError ) <<
			" in " << file << "(" << line << ')'
		);
		ne_assert(0);
	}
}

bool Window::init(int w, int h, int flags)
{
	Vector<EGLint> attribs;
	Vector<EGLint> contextAttribs;

	/** init base window */
	bool result = baseWindow(w, h);

	ne_assert(result);

	#define ADD(val_) contextAttribs.push_back(val_)

	ADD(EGL_CONTEXT_CLIENT_VERSION); ADD( 2 );
	ADD( EGL_NONE );

	#undef ADD
	#define ADD(val_) attribs.push_back(val_)

	// attribs
	ADD( EGL_SURFACE_TYPE );	ADD( EGL_WINDOW_BIT );
	ADD( EGL_RENDERABLE_TYPE );	ADD( EGL_OPENGL_ES2_BIT );
	/*ADD( EGL_RED_SIZE );		ADD(  8 );
	ADD( EGL_GREEN_SIZE );		ADD(  8 );
	ADD( EGL_BLUE_SIZE );		ADD(  8 );
	ADD( EGL_ALPHA_SIZE );		ADD(  8 );
	ADD( EGL_DEPTH_SIZE );		ADD( 24 );
	ADD( EGL_STENCIL_SIZE );	ADD(  8 );*/

	ADD( EGL_NONE );

	#undef ADD

	EGLint numConfigs;
	_display = eglGetDisplay(_hdc);

	if (_display == EGL_NO_DISPLAY)
	{
		TRACE_ERROR("Failed to get an EGLDisplay");
		return false;
	}
	EGLint major, minor;
	if (!eglInitialize(_display, &major, &minor))
	{
		TRACE_ERROR("Failed to initialise the EGLDisplay");
		return false;
	}

	if (!eglBindAPI(EGL_OPENGL_ES_API))
	{
		checkForEgl("eglBindAPI");
		return false;
	}

	/** display was init, start to get extension from egl */
	Extensions::initEgl(_display);

	_config = 0;

	// default config who match the simplest stuff
	if (!eglChooseConfig(_display, &attribs[0], &_config, 1, &numConfigs))
	{
		TRACE_ERROR("Failed to choose a suitable config.");
		return false;
	}

	ne_assert(_config);

	_context = eglCreateContext(_display, _config, EGL_NO_CONTEXT,
		&contextAttribs[0]);

	if (_context == EGL_NO_CONTEXT)
	{
		checkForEgl("eglCreateContext");
		return false;
	}

	_surface = eglCreateWindowSurface(_display, _config,
		(EGLNativeWindowType)_hwnd, NULL /*surfaceAttribList*/);

	if (_surface == EGL_NO_SURFACE)
	{
		checkForEgl("eglCreateWindowSurface");
		return false;
	}

	if (!eglMakeCurrent(_display, _surface, _surface, _context))
	{
		checkForEgl("eglMakeCurrent");
		return false;
	}

	/** start extensions */
	result = Extensions::init();

	ne_assert(result);

#if defined(__WIN32__)
	/** show window */
	ShowWindow(_hwnd, SW_SHOW);
	UpdateWindow(_hwnd);
	SetForegroundWindow(_hwnd);
	SetFocus(_hwnd);
#endif // __WIN32__

	return true;
}

void Window::shutdown()
{
	if (_display)
	{
		eglMakeCurrent(_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		eglTerminate(_display);
	}

#if defined(__WIN32__)
	if (_hwnd)
	{
		ReleaseDC(_hwnd, _hdc);
		DestroyWindow(_hwnd);
	}
#endif // #if defined(__WIN32__)


	_context = EGL_NO_CONTEXT;
	_display = EGL_NO_DISPLAY;
	_surface = EGL_NO_SURFACE;
}

void Window::swap()
{
	eglSwapBuffers(_display, _surface);
}

// ---------------------------------------------------------------------
// dialog console
// ---------------------------------------------------------------------
#if defined(__WIN32__)

void Window::update()
{
	MSG msg;

	while ( PeekMessage(&msg, 0, 0, 0, PM_NOREMOVE) )
	{
		if (!GetMessage(&msg, 0, 0, 0))
			break;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

void Window::setConsole(const String& text)
{
	if (!_dlg_hwnd||!text.size()) return;
	// set message
	SendDlgItemMessage(_dlg_hwnd, edit_control_id, WM_SETTEXT, 0, (LPARAM)text.c_str());
	// auto scroll down
	SendDlgItemMessage(_dlg_hwnd, edit_control_id, EM_LINESCROLL, 0, 65535);
}

void Window::createConsole(uint idDialog, uint idEdit, HWND parent)
{
	if (!idDialog || !idEdit) return;

	dialog_control_id = idDialog;
	edit_control_id = idEdit;

	_dlg_hwnd = CreateDialog(GetModuleHandle(NULL),
		MAKEINTRESOURCE(dialog_control_id),
		parent, (DLGPROC)dialogProc
	);

	if (!_dlg_hwnd)
		return;

	ShowWindow(_dlg_hwnd, SW_SHOW);
}

void Window::setDialogItemText(int id, const char* text)
{
	SendDlgItemMessage(_dlg_hwnd, id, WM_SETTEXT, 0, (LPARAM)text);
}

BOOL Window::dialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
		// todo - other control and action
		case WM_COMMAND:
		{
			switch (LOWORD(wParam))
			{
				case ID_PAUSE:
					Window::setPause(!Window::isPaused());
					if (Window::isPaused())
					{
						setDialogItemText(ID_PAUSE,"[Paused]");
					}
					else
					{
						setDialogItemText(ID_PAUSE, "Pause");
					}
				break;
			}
		}
		break;
	}
	return FALSE;
}

#else // todo

void Window::update() {}
void Window::setConsole(const char*) {}
void Window::createConsole() {}

#endif // __WIN32__
