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
 */
#ifndef __GLES2_DEBUGGER_EXTENSIONS_INCLUDE_H__
#define __GLES2_DEBUGGER_EXTENSIONS_INCLUDE_H__

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES2/gl2platform.h>

#include "miscs.h"

// simple extensions manager
class Extensions
{
public:
	// -----------------------------------------------------------------
	// functions -------------------------------------------------------
	
	/*! OpenGL base start
	 * return true when correctly initialized, else false.
	 * Must be called one time at the beg. of the app
	 */
	static const bool init();

	// load EGL extensions ----------------------------------------------
	static bool initEgl(EGLDisplay display);

	/** return true is an extension "name" exist, else false. */
	static const bool has(const String& name);

	/** same as above, with hash. */
	static const bool has(const hash_t& hash);

protected:
	// -----------------------------------------------------------------
	// variables -------------------------------------------------------
	static hash_v _extensions;

}; // end of namespace Extensions

#endif // __GLES2_DEBUGGER_EXTENSIONS_INCLUDE_H__
