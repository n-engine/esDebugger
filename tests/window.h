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
#ifndef __WINDOW_INCLUDE_H__
#define __WINDOW_INCLUDE_H__

class Window
{
public:
	static bool init(int w, int h, int flags);
	static void shutdown();

	/** */
	static void update();
	static void swap();

	/** check if window is terminated : error, escape, etc... */
	static bool isDone() { return _is_done; };

	/** set state */
	static void setIsDone(const bool& state) { _is_done = state; }
	/* set window pause (pause the loop) **/
	static void setPause(const bool& state) { _is_paused = state; }
	/** get the pause state */
	static bool isPaused() { return _is_paused; }

#if defined(__WIN32__)
	/** create a console window where areal time log is sent */
	static void createConsole(uint idDialog, uint idEdit, HWND parent);

	/** set the content of the console */
	static void setConsole(const String& text);

	static HWND getHwnd() { return _hwnd; }
	static HDC getHdc() { return _hdc; }
	static HGLRC getHrc() { return _hrc; }
protected:
	static void setDialogItemText(int id, const char* text);
	static bool baseWindow(int w, int h);
	static BOOL dialogProc(HWND,UINT,WPARAM,LPARAM);

protected:
	static HWND _hwnd;
	static HDC _hdc;
	static HGLRC _hrc;
	static HWND _dlg_hwnd;
#else
	static bool baseWindow(int w, int h) { return false; }
#endif // __WIN32__

protected:

	static bool _is_done;
	static bool _is_paused;
	static int _width, _height;

	static String	gl_info;
	static String	gl_version;
	static int _v_major, _v_minor;

	static EGLContext _context;
	static EGLDisplay _display;
	static EGLConfig _config;
	static EGLSurface _surface;

}; // end of class Window

#endif // __WINDOW_INCLUDE_H__
