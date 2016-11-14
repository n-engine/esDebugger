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

#include "extensions.h"
#include "debugger.h"


using namespace Debugger;

// ---------------------------------------------------------------------
// variables -----------------------------------------------------------
// ---------------------------------------------------------------------

hash_v Extensions::_extensions;

// ---------------------------------------------------------------------
// functions -----------------------------------------------------------
// ---------------------------------------------------------------------

const bool Extensions::init()
{
	if(!gl::init())
	{
		TRACE_ERROR("Can't init OpenGL ES 2.0 Debugger.");
		return false;
	}

	/** proccess extensions */
	{
		string_v result;
		const char* cstr = (const char*)glGetString(GL_EXTENSIONS);
		if (!cstr)
		{
			TRACE_ERROR("glGetString returned a null ptr.");
			return false;
		}

		const String ext( cstr );

		if (ext.size() && Explode(result, ext) )
		{
			string_v::Iterator iter;
			foreach(result)
			{
				_extensions.push_back( hash( iter->c_str() ) );
			}
		}
		else
		{
			TRACE_ERROR("Invalid Explode nor getString value");
			return false;
		}
	}

	return true;
}

// load EGL extensions ------------------------------------------------------
bool Extensions::initEgl(EGLDisplay display)
{
	string_v result;

	const String ext( eglQueryString(display, EGL_EXTENSIONS) );

	if (Explode(result, ext))
	{
		string_v::Iterator iter;
		foreach(result)
		{
			_extensions.push_back(hash(iter->c_str()));
		}
	}
	else
	{
		TRACE_ERROR("Invalid Explode nor eglQueryString value");
		return false;
	}

	return true;
}

const bool Extensions::has(const String& name)
{
	if(!_extensions.size())
		return false;

	const hash_t hh = hash( name.c_str() );
	hash_v::Iterator iter;

	foreach(_extensions)
	{
		if ( hh == *iter ) return true;
	}

	return false;
}

const bool Extensions::has(const hash_t& hh)
{
	if(!_extensions.size())
		return false;

	hash_v::Iterator iter;

	foreach(_extensions)
	{
		if ( hh == *iter ) return true;
	}

	return false;
}
