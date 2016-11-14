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
#include <config.h>

#if defined(__WIN32__)
#include <windows.h> // for the dialog
#endif // __WIN32__

#if defined(USE_DEBUGGER)

#include <extensions.h>
#include <debugger.h>

#if defined(max)
#undef max
#undef min
#endif // 

namespace Debugger {

#define INIT_POINTER =nullptr

//#define TRACE_FUNCTION TRACE_DEBUG
#define TRACE_FUNCTION(...)

#define RESERVED_SIZE 4096

const char* gl::invalid_framebuffer_operation =
	"GL_INVALID_FRAMEBUFFER_OPERATION";
const char* gl::out_of_memory = "GL_OUT_OF_MEMORY";
const char* gl::invalid_operation = "GL_INVALID_OPERATION";
const char* gl::invalid_value = "GL_INVALID_VALUE";
const char* gl::invalid_enum = "GL_INVALID_ENUM";
const char* gl::unknown_error = "Unknown error";

gl::Entry_v gl::_registered(RESERVED_SIZE);
gl::History_cb gl::_call_history;

uint gl::_program_bound = INVALID_BOUND;
gl::Texture_v gl::_textures(RESERVED_SIZE);
gl::Program_v gl::_programs(RESERVED_SIZE);
gl::Shader_v gl::_shaders(RESERVED_SIZE);
gl::Buffer_v gl::_buffers(RESERVED_SIZE);
gl::Buffer_t* gl::_bound_buffer[BUFFER_SIZE] = {0};
Map<uint, uchar> gl::_states;

uint_v gl::_allowed_enable;

bool gl::_dump_data = false;
bool gl::_break_on_error = true;
bool gl::_break_on_warning = false;
bool gl::_append_to_log_calls = true;

String gl::_output_buffer;
uint gl::frame = 0;

gl::fnc_console_cb gl::_console_cb = nullptr;

// ---------------------------------------------------------------------

void gl::reset()
{
	++frame;
	_call_history.reset();
	_output_buffer.clear();
}

// ---------------------------------------------------------------------

gl::Type gl::GenTypeInfo(GLuint bytes, bool specialInterpretation)
{
    Type info;
    info.bytes = bytes;
    GLuint i = 0;
    while ((1u << i) < bytes)
    {
        ++i;
    }
    info.bytesShift = i;
    ne_assert( (1u << info.bytesShift) == bytes );
    info.specialInterpretation = specialInterpretation;
    return info;
}

const gl::Type& gl::GetTypeInfo(GLenum type)
{
    switch (type)
    {
      case GL_UNSIGNED_BYTE:
      case GL_BYTE:
        {
            static const Type info = GenTypeInfo(1, false);
            return info;
        }
      case GL_UNSIGNED_SHORT:
      case GL_SHORT:
#if defined(GL_HALF_FLOAT)
      case GL_HALF_FLOAT:
#endif // GL_HALF_FLOAT
      case GL_HALF_FLOAT_OES:
        {
            static const Type info = GenTypeInfo(2, false);
            return info;
        }
      case GL_UNSIGNED_INT:
      case GL_INT:
      case GL_FLOAT:
        {
            static const Type info = GenTypeInfo(4, false);
            return info;
        }
      case GL_UNSIGNED_SHORT_5_6_5:
      case GL_UNSIGNED_SHORT_4_4_4_4:
      case GL_UNSIGNED_SHORT_5_5_5_1:
      case GL_UNSIGNED_SHORT_4_4_4_4_REV_EXT:
      case GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT:
        {
            static const Type info = GenTypeInfo(2, true);
            return info;
        }
#if defined(GL_UNSIGNED_INT_2_10_10_10_REV)
	  case GL_UNSIGNED_INT_2_10_10_10_REV:
      case GL_UNSIGNED_INT_24_8:
      case GL_UNSIGNED_INT_10F_11F_11F_REV:
      case GL_UNSIGNED_INT_5_9_9_9_REV:
        {
            ne_assert(GL_UNSIGNED_INT_24_8_OES == GL_UNSIGNED_INT_24_8);
            static const Type info = GenTypeInfo(4, true);
            return info;
        }
#endif // GL_UNSIGNED_INT_2_10_10_10_REV
#if defined(GL_FLOAT_32_UNSIGNED_INT_24_8_REV)
      case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
        {
            static const Type info = GenTypeInfo(8, true);
            return info;
        }
#endif // GL_FLOAT_32_UNSIGNED_INT_24_8_REV
	  default:
        {
            static const Type defaultInfo;
            return defaultInfo;
        }
    }
}

const char* gl::get_last_error()
{
	const GLenum err = gl::gl_GetError();

	switch (err)
	{
		case GL_NO_ERROR:
		return nullptr;

		case GL_OUT_OF_MEMORY:
		return out_of_memory;

		case GL_INVALID_OPERATION:
		return invalid_operation;

		case GL_INVALID_VALUE:
		return invalid_value;

		case GL_INVALID_ENUM:
		return invalid_enum;
	}

	return unknown_error;
}

bool gl::is_registered_texture(GLenum id)
{
	Texture_v::Iterator iter;
	foreach(_textures)
	{
		if ( (*iter).id == id )
			return true;
	}
	return false;
}

bool gl::is_registered_program(GLuint id)
{
	Program_v::Iterator iter;
	foreach(_programs)
	{
		if ((*iter).id == id)
			return true;
	}
	return false;
}

bool gl::is_registered_shader(GLuint id)
{
	Shader_v::Iterator iter;
	foreach(_shaders)
	{
		if ((*iter).id == id)
			return true;
	}
	return false;
}

bool gl::is_registered_buffer(GLuint id)
{
	Buffer_v::Iterator iter;
	foreach(_buffers)
	{
		if ((*iter).id == id)
			return true;
	}
	return false;
}

bool gl::is_cap_enabled(GLenum cap)
{
	Map<uint,uchar>::Iterator iter = 
		_states.find(static_cast<uint>(cap));

	if( iter != _states.end() && iter->second == 1 )
		return true;

	/** default disabled (excepter dither) */
	if( iter == _states.end() )
		_states[cap] = static_cast<uchar>(0);

	return false;
}

void gl::setStates(GLenum cap, uchar state)
{
	_states[cap] = state;
}

void gl::enableStates(GLenum cap)
{
	_states[cap] = static_cast<uchar>(1);
}

void gl::disableStates(GLenum cap)
{
	_states[cap] = static_cast<uchar>(0);
}

bool gl::is_allowed_capability_enable_disable(GLenum cap)
{
	uint_v::Iterator iter;

	foreach(_allowed_enable)
	{
		if (cap == *iter)
			return true;
	}

	return false;
}

const char* gl::getCapabilityName(GLenum cap)
{
	switch( cap )
	{
		case GL_BLEND: return "GL_BLEND";
		case GL_CULL_FACE: return "GL_CULL_FACE";
		case GL_DEPTH_TEST: return "GL_DEPTH_TEST";
		case GL_DITHER: return "GL_DITHER";
		case GL_POLYGON_OFFSET_FILL: return "GL_POLYGON_OFFSET_FILL";
		case GL_SAMPLE_ALPHA_TO_COVERAGE: return "GL_SAMPLE_ALPHA_TO_COVERAGE";
		case GL_SAMPLE_COVERAGE: return "GL_SAMPLE_COVERAGE";
		case GL_SCISSOR_TEST: return "GL_SCISSOR_TEST";
		case GL_STENCIL_TEST: return "GL_STENCIL_TEST";
		case GL_DEBUG_OUTPUT_KHR: return "GL_DEBUG_OUTPUT_KHR";
		case GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR: return "GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR";
	}

	return "GL_NONE";
}

const char* gl::getDefineName(GLenum name)
{
	const char* ret;

	/** EGL */
	ret = egl::is_define_egl_h(name);
	if ( ret ) return ret;

	/** GL2 */
	ret = is_define_gl2_h(name);

	return ret;
}

void gl::addCall(
	const char* err,	/* error message from opengl */
	const char* fnc,	/* function called */
	const char* file,	/* where we called the function */
	int line)			/* at line ... */
{
	String entry, filename(get_path(file));

	int id = _call_history.size();

	if ( id >= (RESERVED_SIZE-1) )
	{
		/** it doesnt matter at all, this is a circular buffer. */
		TRACE_WARNING("History buffer maximum reached");
	}

	/** append opengl call function to container */
	_call_history.append(
		History_t(id,err,fnc,file,line)
	);

	// format output message for console/log
	if ( err )
	{
		entry << format("%d %s (%s@%d) : %s",
			frame,fnc,filename.c_str(),line,err);
	}
	else
	{
		entry << format("%d %s (%s@%d)",
			frame,fnc,filename.c_str(),line);
	}

	/** dump to log */
	if ( _append_to_log_calls )
	{
		if ( err )
		{
			TRACE_ERROR( entry );
		}
		else
		{
			TRACE_DEBUG( entry );
		}
	}

	appendConsole( entry.c_str() );
}

void gl::appendConsole(const String& buffer)
{
	_output_buffer << buffer <<

#if defined(__WIN32__)
		"\r\n"; // Edit control need CRLF to make a correct return line
#else
		"\n";
#endif // __WIN32__

	setConsole( _output_buffer.c_str() );
}

void gl::breakOnError(const bool& value, const char* message)
{
	/** no error */
	if ( value )
		return;

	if (message)
	{
		if (_append_to_log_calls)
		{
			TRACE_ERROR(message);
		}
		appendConsole(message);
	}

	if(!_break_on_error)
	{
		if(!message && _append_to_log_calls)
		{
			TRACE_WARNING(
				"Error found, no message given and break is disabled!"
			);
		}

		return;
	}

	ne_assert( !"break on error :: check log" );
}

void gl::breakOnWarning(const bool& value, const char* message)
{
	/** no error */
	if (value)
		return;

	/** send log */
	if (message)
	{
		if (_append_to_log_calls)
		{
			TRACE_WARNING(message);
		}

		appendConsole(message);
	}

	if(!_break_on_warning)
	{
		if(!message && _append_to_log_calls)
		{
			TRACE_WARNING(
				"Warning found, no message given and break is disabled!"
			);
		}
		return;
	}

	ne_assert( !"break on warning :: check log" );
}

String gl::get_path(const String& file)
{
	static String r;
	static string_v array(10);

	if (!file.size())
		return r;

	array.clear();

	bool result = false;
	const int size = file.size();

	const char* t1 = Libc::strrchr(file, '/' );
	const char* t2 = !t1 ? Libc::strrchr(file, '\\') : 0;

	if(t1) result = Explode(array,file,'/');
	else if (t2) result = Explode(array,file,'\\');

	if( !result )
		return r;

	const int sz = array.size();

	if ( sz >= 2 )
	{
		r = format( "%s/%s", array[sz-2].c_str(), array[sz-1].c_str() );
	}
	else
	{
		r = format( "%s", array[sz-1].c_str() );
	}

	return r;
}

gl::Buffer_t* gl::getBuffer(uint target, uint id)
{
	if (target >= BUFFER_SIZE) ne_assert(!"local target only");

	Buffer_v::iterator iter;
	foreach(_buffers)
	{
		Buffer_t* buffer = &(*iter);
		if (
			(buffer->id == id) &&
			(
				(buffer->flags & NEW_BUFFER) ||
				(buffer->target == target)
			)
		)
		{
			return buffer;
		}
	}
	return nullptr;
}

// (un)register program -------------------------------------------------

bool gl::register_program(uint id)
{
	const bool is_valid = !is_registered_program(id);
	if (!is_valid)
		return false;
	Program_t o;
	o.id = id;
	_programs.push_back(o);
	return true;
}

bool gl::unregister_program(uint id)
{
	Program_v::iterator iter;
	foreach(_programs)
	{
		if (iter->id == id)
		{
			_programs.erase(iter);
			return true;
		}
	}
	return false;
}


// (un)register shader -------------------------------------------------

bool gl::register_shader(uint id)
{
	const bool is_valid = !is_registered_shader(id);
	if (!is_valid)
		return false;
	Shader_t o;
	o.id = id;
	_shaders.push_back(o);
	return true;
}

bool gl::unregister_shader(uint id)
{
	Shader_v::iterator iter;
	foreach(_shaders)
	{
		if (iter->id == id)
		{
			_shaders.erase(iter);
			return true;
		}
	}
	return false;
}


// (un)register buffer -------------------------------------------------
bool gl::register_buffer(uint id)
{
	const bool is_valid = !is_registered_buffer(id);

	if (!is_valid)
		return false;

	Buffer_t o;

	o.id = id;
	o.flags = NEW_BUFFER;
	_buffers.push_back(o);

	return true;
}

bool gl::unregister_buffer(uint id)
{
	Buffer_v::iterator iter;
	foreach(_buffers)
	{
		if (iter->id == id)
		{
			_buffers.erase(iter);
			return true;
		}
	}
	return false;
}

// (un)register textures -----------------------------------------------
bool gl::register_texture(uint id)
{
	const bool is_valid = !is_registered_texture(id);

	if (!is_valid)
		return false;

	Texture_t o;
	o.id = id;
	o.flags = NEW_TEXTURE;
	_textures.push_back(o);

	return true;
}

bool gl::unregister_texture(uint id)
{
	Texture_v::iterator iter;
	foreach(_textures)
	{
		if (iter->id == id)
		{
			_textures.erase(iter);
			return true;
		}
	}
	return false;
}

bool gl::setBuffer(uint target,uint id,uint size,const void* data)
{
	if (target >= BUFFER_SIZE) ne_assert(!"local target only");

	Buffer_t* buffer = getBuffer( target, id );

	// not registered
	if(!buffer)
		return false;

	buffer->target = target;
	buffer->size = size;
	buffer->data = data;
	buffer->flags &= ~NEW_BUFFER;

	return true;
}

void gl::setBoundBuffer(uint target,uint id)
{
	if (target >= BUFFER_SIZE) ne_assert(!"local target only");

	// unbind
	if (id == 0)
	{
		_bound_buffer[target] = nullptr;
		return;
	}

	Buffer_t* buffer = getBuffer(target, id);

	// newly created
	if(!buffer)
		buffer = getBuffer(INVALID_BUFFER_TARGET, id);

	breakOnError(MakeBool(buffer), "buffer not registered / Invalid buffer" );

	// bind
	_bound_buffer[target] = buffer;
}

void gl::setBoundBufferData(uint target, uint size, const void* data)
{
	breakOnError(
		MakeBool(_bound_buffer[target]), "buffer not registered / Invalid buffer");

	_bound_buffer[target]->target = target;
	_bound_buffer[target]->size = size;
	_bound_buffer[target]->data = data;
	_bound_buffer[target]->valid = (size >= 1 && data) ? 1 : 0;
}

uint gl::getBoundBufferId(uint target)
{
	if (target >= BUFFER_SIZE) ne_assert(!"local target only");
	if (!_bound_buffer[target]) return INVALID_BOUND;
	return _bound_buffer[target]->id;
}

// ---------------------------------------------------------------------
// gl2.h
// ---------------------------------------------------------------------

// GL_ES_VERSION_2_0

PFNGLACTIVETEXTUREPROC gl::gl_ActiveTexture INIT_POINTER;
void gl::ActiveTexture  (GLenum texture, const char* file, int line)
{
	TRACE_FUNCTION("glActiveTexture(...) called from " << get_path(file) << '(' << line << ')');

	gl_ActiveTexture( texture );

	const char* result = get_last_error();

	String sTexture;

	if (texture >= GL_TEXTURE0 && texture <= GL_TEXTURE31)
		sTexture = format("GL_TEXTURE%d", (texture - GL_TEXTURE0));
	else
		breakOnError(!"invalid texture unit");

	// add function to call list
	addCall( result, format( "glActiveTexture( %s )", sTexture.c_str() ), file, line );
	
	breakOnError( !result, result );
}

PFNGLATTACHSHADERPROC gl::gl_AttachShader INIT_POINTER;
void gl::AttachShader(GLuint program, GLuint shader, const char* file, int line)
{
	TRACE_FUNCTION("glAttachShader(...) called from " << get_path(file) << '(' << line << ')');

	// check for registered program / shader and valid (todo)
	bool p_is_valid = is_registered_program(program);
	bool s_is_valid = is_registered_shader(shader);

	gl_AttachShader(program,shader);

	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format("glAttachShader( program(valid:%s, id:%d), shader(valid:%s, id:%d) )",
			p_is_valid ? "true" : "false", program,
			s_is_valid ? "true" : "false", shader ),
		file, line
	);
	
	breakOnError( p_is_valid || s_is_valid );
	breakOnError( !result, result );
}

PFNGLBINDATTRIBLOCATIONPROC gl::gl_BindAttribLocation INIT_POINTER;
void gl::BindAttribLocation  (GLuint program, GLuint index, const GLchar *name, const char* file, int line)
{
	TRACE_FUNCTION("glBindAttribLocation(...) called from " << 
		get_path(file) << '(' << line << ')');

	const bool is_valid = is_registered_program(program);

	breakOnError( is_valid, "Invalid program" );

	gl_BindAttribLocation(program,index,name);

	const char* result = get_last_error();

	addCall(result,format("glBindAttribLocation(%d)",program),file,line);

	breakOnError( !result, result );
}

PFNGLBINDBUFFERPROC gl::gl_BindBuffer INIT_POINTER;
void gl::BindBuffer  (GLenum target, GLuint buffer, const char* file, int line)
{
	TRACE_FUNCTION("glBindBuffer(...) called from " << 
		get_path(file) << '(' << line << ')');

	breakOnError( target == GL_ARRAY_BUFFER ||
		target == GL_ELEMENT_ARRAY_BUFFER, "Invalid buffer" );

	const GLuint b = buffer;

	gl_BindBuffer( target, buffer );

	const bool is_valid = (0 == buffer) ?
							true : is_registered_buffer(buffer);

	// local bind
	const uint ltarget = target == GL_ARRAY_BUFFER ?
						ARRAY_BUFFER : ELEMENT_ARRAY_BUFFER;

	setBoundBuffer( ltarget, buffer );

	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format("glBindBuffer( target:%s, buffer:%d )",
		(target==GL_ARRAY_BUFFER) ? "GL_ARRAY_BUFFER" : 
			"GL_ELEMENT_ARRAY_BUFFER",b),
		file, line
	);

	breakOnError( is_valid , "buffer not generated / Invalid buffer" );
	breakOnError( !result, result );
}

PFNGLBINDFRAMEBUFFERPROC gl::gl_BindFramebuffer INIT_POINTER;
void gl::BindFramebuffer  (GLenum target, GLuint framebuffer, const char* file, int line)
{
	TRACE_FUNCTION("glBindFramebuffer(...) called from " << get_path(file) << '(' << line << ')');
	gl_BindFramebuffer(
		target,
		framebuffer);
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLBINDRENDERBUFFERPROC gl::gl_BindRenderbuffer INIT_POINTER;
void gl::BindRenderbuffer  (GLenum target, GLuint renderbuffer, const char* file, int line)
{
	TRACE_FUNCTION("glBindRenderbuffer(...) called from " << get_path(file) << '(' << line << ')');
	gl_BindRenderbuffer(
		target,
		renderbuffer);
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLBINDTEXTUREPROC gl::gl_BindTexture INIT_POINTER;
void gl::BindTexture  (GLenum target, GLuint texture, const char* file, int line)
{
	TRACE_FUNCTION("glBindTexture(...) called from " << get_path(file) << '(' << line << ')');

	breakOnError( target == GL_TEXTURE_2D ||
		target == GL_TEXTURE_CUBE_MAP );

	bool is_valid = texture == 0 ? true : is_registered_texture( texture );
                    
	gl_BindTexture( target, texture );
	
	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format("glBindTexture( target:%s, texture:%d )",
		target == GL_TEXTURE_2D ?
		"GL_TEXTURE_2D" : "GL_TEXTURE_CUBE_MAP",texture),
		file, line
	);

	breakOnError( !result, result );
	breakOnError( is_valid, "Invalid texture" );
}

PFNGLBLENDCOLORPROC gl::gl_BlendColor INIT_POINTER;
void gl::BlendColor  (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha, const char* file, int line)
{
	TRACE_FUNCTION("glBlendColor(...) called from " << get_path(file) << '(' << line << ')');
	gl_BlendColor(
		red,
		green,
		blue,
		alpha);
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLBLENDEQUATIONPROC gl::gl_BlendEquation INIT_POINTER;
void gl::BlendEquation  (GLenum mode, const char* file, int line)
{
	TRACE_FUNCTION("glBlendEquation(...) called from " << get_path(file) << '(' << line << ')');
	gl_BlendEquation(
		mode);
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLBLENDEQUATIONSEPARATEPROC gl::gl_BlendEquationSeparate INIT_POINTER;
void gl::BlendEquationSeparate  (GLenum modeRGB, GLenum modeAlpha, const char* file, int line)
{
	TRACE_FUNCTION("glBlendEquationSeparate(...) called from " << get_path(file) << '(' << line << ')');
	gl_BlendEquationSeparate(
		modeRGB,
		modeAlpha);
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLBLENDFUNCPROC gl::gl_BlendFunc INIT_POINTER;
void gl::BlendFunc  (GLenum sfactor, GLenum dfactor, const char* file, int line)
{
	TRACE_FUNCTION("glBlendFunc(...) called from " << get_path(file) << '(' << line << ')');
	gl_BlendFunc(
		sfactor,
		dfactor);
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLBLENDFUNCSEPARATEPROC gl::gl_BlendFuncSeparate INIT_POINTER;
void gl::BlendFuncSeparate  (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha, const char* file, int line)
{
	TRACE_FUNCTION("glBlendFuncSeparate(...) called from " << get_path(file) << '(' << line << ')');
	gl_BlendFuncSeparate(
		sfactorRGB,
		dfactorRGB,
		sfactorAlpha,
		dfactorAlpha);
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLBUFFERDATAPROC gl::gl_BufferData INIT_POINTER;
void gl::BufferData  (GLenum target, GLsizeiptr size, const void *data, GLenum usage, const char* file, int line)
{
	TRACE_FUNCTION("glBufferData(...) called from " << get_path(file) << '(' << line << ')');

	breakOnError(
		(target == GL_ARRAY_BUFFER) ||
		(target == GL_ELEMENT_ARRAY_BUFFER), "target : Invalid enum");

	breakOnError(
		(usage == GL_STREAM_DRAW ||
		usage == GL_STATIC_DRAW ||
		usage == GL_DYNAMIC_DRAW), "usage : Invalid enum"
	);          

	const uint ltarget = target == GL_ARRAY_BUFFER ?
		ARRAY_BUFFER : ELEMENT_ARRAY_BUFFER;

	const uint bound_id = getBoundBufferId( ltarget );

	breakOnError( bound_id != INVALID_BOUND, "no buffer bound" );

	/** set to opengl */
	gl_BufferData(target,size,data,usage);

	// set locally
	setBoundBufferData(ltarget, size, data);

	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format(
			"glBufferData( target:%s size:%d data:%s usage:%s )",
			(target==GL_ARRAY_BUFFER) ? "GL_ARRAY_BUFFER" : 
			"GL_ELEMENT_ARRAY_BUFFER", size, data ? "ok" : "ko",
			usage == GL_STREAM_DRAW ? "GL_STREAM_DRAW" : 
			usage == GL_STATIC_DRAW ? "GL_STATIC_DRAW" : "GL_DYNAMIC_DRAW"
		),
		file, line
	);

	if ( _dump_data && size && data )
	{
		TRACE_BUFFER( data, size );
	}

	breakOnError( !result, result );
}

PFNGLBUFFERSUBDATAPROC gl::gl_BufferSubData INIT_POINTER;
void gl::BufferSubData  (GLenum target, GLintptr offset, GLsizeiptr size, const void *data, const char* file, int line)
{
	TRACE_FUNCTION("glBufferSubData(...) called from " << get_path(file) << '(' << line << ')');
	gl_BufferSubData(
		target,
		offset,
		size,
		data);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLCHECKFRAMEBUFFERSTATUSPROC gl::gl_CheckFramebufferStatus INIT_POINTER;
GLenum gl::CheckFramebufferStatus  (GLenum target, const char* file, int line)
{
	TRACE_FUNCTION("glCheckFramebufferStatus(...) called from " << get_path(file) << '(' << line << ')');
	return gl_CheckFramebufferStatus(
		target);
}

PFNGLCLEARPROC gl::gl_Clear INIT_POINTER;
void gl::Clear  (GLbitfield mask, const char* file, int line)
{
	TRACE_FUNCTION("glClear(...) called from " << get_path(file) << '(' << line << ')');

	gl_Clear(mask);
	
	const char* result = get_last_error();

	String sMask;

	if ( mask & GL_COLOR_BUFFER_BIT )
		sMask << "GL_COLOR_BUFFER_BIT";

	if ( mask & GL_DEPTH_BUFFER_BIT && sMask.size() )
		sMask << " | GL_DEPTH_BUFFER_BIT";
	else if ( mask & GL_DEPTH_BUFFER_BIT )
		sMask << "GL_DEPTH_BUFFER_BIT";

	if ( mask & GL_STENCIL_BUFFER_BIT && sMask.size() )
		sMask << " | GL_STENCIL_BUFFER_BIT";
	else if ( mask & GL_STENCIL_BUFFER_BIT )
		sMask << "GL_STENCIL_BUFFER_BIT";

	if( !sMask.size() )
	{
		breakOnError( 0, "Invalid mask" );
	}

	// add function to call list
	addCall(result,
		format("glClear( %s )",sMask.c_str()),
		file, line
	);
	
	breakOnError( !result, result );
}

PFNGLCLEARCOLORPROC gl::gl_ClearColor INIT_POINTER;
void gl::ClearColor  (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha, const char* file, int line)
{
	TRACE_FUNCTION("glClearColor(...) called from " << get_path(file) << '(' << line << ')');

	gl_ClearColor( red, green, blue, alpha);
	
	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format("glClearColor( %g, %g, %g, %g )",red,green,blue,alpha),
		file, line
	);
	
	breakOnError( !result, result );
}

PFNGLCLEARDEPTHFPROC gl::gl_ClearDepthf INIT_POINTER;
void gl::ClearDepthf  (GLfloat d, const char* file, int line)
{
	TRACE_FUNCTION("glClearDepthf(...) called from " << get_path(file) << '(' << line << ')');

	gl_ClearDepthf(d);
	
	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format("glClearDepthf( %g )",d),
		file, line
	);
	
	breakOnError( !result, result );
}

PFNGLCLEARSTENCILPROC gl::gl_ClearStencil INIT_POINTER;
void gl::ClearStencil  (GLint s, const char* file, int line)
{
	TRACE_FUNCTION("glClearStencil(...) called from " << get_path(file) << '(' << line << ')');

	gl_ClearStencil( s );
	
	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format("glClearStencil( %d )",s),
		file, line
	);
	
	breakOnError( !result, result );
}

PFNGLCOLORMASKPROC gl::gl_ColorMask INIT_POINTER;
void gl::ColorMask  (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha, const char* file, int line)
{
	TRACE_FUNCTION("glColorMask(...) called from " << get_path(file) << '(' << line << ')');

	gl_ColorMask(red,green,blue,alpha);
	
	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format("glColorMask( %d, %d, %d, %d )",red,green,blue,alpha),
		file, line
	);
	
	breakOnError( !result, result );
}

PFNGLCOMPILESHADERPROC gl::gl_CompileShader INIT_POINTER;
void gl::CompileShader  (GLuint shader, const char* file, int line)
{
	TRACE_FUNCTION("glCompileShader(...) called from " << get_path(file) << '(' << line << ')');

	gl_CompileShader( shader );
}

PFNGLCOMPRESSEDTEXIMAGE2DPROC gl::gl_CompressedTexImage2D INIT_POINTER;
void gl::CompressedTexImage2D  (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data, const char* file, int line)
{
	TRACE_FUNCTION("glCompressedTexImage2D(...) called from " << get_path(file) << '(' << line << ')');
	gl_CompressedTexImage2D(
		target,
		level,
		internalformat,
		width,
		height,
		border,
		imageSize,
		data);
}

PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC gl::gl_CompressedTexSubImage2D INIT_POINTER;
void gl::CompressedTexSubImage2D  (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data, const char* file, int line)
{
	TRACE_FUNCTION("glCompressedTexSubImage2D(...) called from " << get_path(file) << '(' << line << ')');
	gl_CompressedTexSubImage2D(
		target,
		level,
		xoffset,
		yoffset,
		width,
		height,
		format,
		imageSize,
		data);
}

PFNGLCOPYTEXIMAGE2DPROC gl::gl_CopyTexImage2D INIT_POINTER;
void gl::CopyTexImage2D  (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border, const char* file, int line)
{
	TRACE_FUNCTION("glCopyTexImage2D(...) called from " << get_path(file) << '(' << line << ')');
	gl_CopyTexImage2D(
		target,
		level,
		internalformat,
		x,
		y,
		width,
		height,
		border);
}

PFNGLCOPYTEXSUBIMAGE2DPROC gl::gl_CopyTexSubImage2D INIT_POINTER;
void gl::CopyTexSubImage2D  (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height, const char* file, int line)
{
	TRACE_FUNCTION("glCopyTexSubImage2D(...) called from " << get_path(file) << '(' << line << ')');
	gl_CopyTexSubImage2D(
		target,
		level,
		xoffset,
		yoffset,
		x,
		y,
		width,
		height);
}

PFNGLCREATEPROGRAMPROC gl::gl_CreateProgram INIT_POINTER;
GLuint gl::CreateProgram  (const char* file, int line)
{
	TRACE_FUNCTION("glCreateProgram(...) called from " << get_path(file) << '(' << line << ')');

	GLuint ret = gl_CreateProgram();

	const char* result = get_last_error();

	// add function to call list
	addCall(result,"glCreateProgram()",file, line);

	/** register program */
	bool valid = register_program(ret);

	breakOnError(valid, "Invalid program / register" );

	breakOnError( !result, result );

	return ret;
}

PFNGLCREATESHADERPROC gl::gl_CreateShader INIT_POINTER;
GLuint gl::CreateShader  (GLenum type, const char* file, int line)
{
	TRACE_FUNCTION("glCreateShader(...) called from " << get_path(file) << '(' << line << ')');
	
	GLuint ret = gl_CreateShader(type);

	const char* result = get_last_error();

	String sType;

	if ( (type & GL_VERTEX_SHADER) || (type & GL_FRAGMENT_SHADER) )
	{
		if ( type & GL_VERTEX_SHADER )
			sType << "GL_VERTEX_SHADER";
		else if ( type & GL_FRAGMENT_SHADER )
			sType << "GL_FRAGMENT_SHADER";
	}
	else
	{
		breakOnError(!"Invalid type");
	}

	// add function to call list
	addCall(result,format("glCreateShader(%s)",sType.c_str()),file,line);

	/** register shaders */
	bool valid = register_shader(ret);

	breakOnError(valid, "Invalid shader / register");

	breakOnError(!result, result);

	return ret;
}

PFNGLCULLFACEPROC gl::gl_CullFace INIT_POINTER;
void gl::CullFace  (GLenum mode, const char* file, int line)
{
	TRACE_FUNCTION("glCullFace(...) called from " << get_path(file) << '(' << line << ')');
	gl_CullFace(mode);
}

PFNGLDELETEBUFFERSPROC gl::gl_DeleteBuffers INIT_POINTER;
void gl::DeleteBuffers  (GLsizei n, const GLuint *buffers, const char* file, int line)
{
	TRACE_FUNCTION("glDeleteBuffers(...) called from " << get_path(file) << '(' << line << ')');

	gl_DeleteBuffers(n,buffers);

	bool is_valid = n == 1 ? is_registered_buffer(*buffers) : true;

	const char* result = get_last_error();

	// add function to call list
	addCall(result, format("glDeleteBuffers(%d)", n), file, line);

	bool found = false;
	for (int x = 0; x < n; ++x)
	{
		found = unregister_buffer(buffers[x]);
		if (found)
			break;
	}

	breakOnError( found, "buffers not found/unregistered" );
	breakOnError(!result);
}

PFNGLDELETEFRAMEBUFFERSPROC gl::gl_DeleteFramebuffers INIT_POINTER;
void gl::DeleteFramebuffers  (GLsizei n, const GLuint *framebuffers, const char* file, int line)
{
	TRACE_FUNCTION("glDeleteFramebuffers(...) called from " << get_path(file) << '(' << line << ')');
	gl_DeleteFramebuffers(n,framebuffers);
}

PFNGLDELETEPROGRAMPROC gl::gl_DeleteProgram INIT_POINTER;
void gl::DeleteProgram  (GLuint program, const char* file, int line)
{
	TRACE_FUNCTION("glDeleteProgram(...) called from " << get_path(file) << '(' << line << ')');

	const GLuint p = program;
	gl_DeleteProgram( program );

	bool is_valid = is_registered_program(p);

	const char* result = get_last_error();

	// add function to call list
	addCall(result,format("glDeleteProgram(%d)",p),file,line);

	/** unregister program */
	
	bool found = unregister_program(p);

	breakOnError(found);
	breakOnError(is_valid);
	breakOnError( !result, result );
}

PFNGLDELETERENDERBUFFERSPROC gl::gl_DeleteRenderbuffers INIT_POINTER;
void gl::DeleteRenderbuffers  (GLsizei n, const GLuint *renderbuffers, const char* file, int line)
{
	TRACE_FUNCTION("glDeleteRenderbuffers(...) called from " << get_path(file) << '(' << line << ')');

	gl_DeleteRenderbuffers(n,renderbuffers);
}

PFNGLDELETESHADERPROC gl::gl_DeleteShader INIT_POINTER;
void gl::DeleteShader  (GLuint shader, const char* file, int line)
{
	TRACE_FUNCTION("glDeleteShader(...) called from " << get_path(file) << '(' << line << ')');

	const GLuint s = shader;
	gl_DeleteShader(shader);

	const char* result = get_last_error();

	// add function to call list
	addCall(result,format("glDeleteShader(%d)",s),file,line);

	/** unregister shader */
	bool valid = unregister_shader(s);

	breakOnError(valid, "Invalid shader / unregister");

	breakOnError( !result, result );
}

PFNGLDELETETEXTURESPROC gl::gl_DeleteTextures INIT_POINTER;
void gl::DeleteTextures  (GLsizei n, const GLuint *textures, const char* file, int line)
{
	TRACE_FUNCTION("glDeleteTextures(...) called from " << get_path(file) << '(' << line << ')');

	int x;

	breakOnError( n >= 1, "n : Invalid value (n<=0)" );

	/** send it to opengl */
	gl_DeleteTextures(n,textures);

	const char* result = get_last_error();

	String sTextures;
	for (x = 0; x < n; ++x)
	{
		sTextures << format("[%d],%d ",x,textures[x]);
	}

	// add function to call list
	addCall(result,
		format("glDeleteTextures( size:%d textures:%s )",n,
			sTextures.c_str()),
		file, line
	);

	bool found = false;
	for (x = 0; x < n; ++x)
	{
		found = unregister_texture(textures[x]);
		if (found)
			break;
	}

	breakOnError(found, "unregistered texture / invalid");
	breakOnError( !result, result );
}

// 512 <> 519
static const char* _allowed_depth_func_str[] = {
	"GL_NEVER",		// 512
	"GL_LESS",		// 513
	"GL_EQUAL",		// 514
	"GL_LEQUAL",	// 515
	"GL_GREATER",	// 516
	"GL_NOTEQUAL",	// 517
	"GL_GEQUAL",	// 518
	"GL_ALWAYS"		// 519
};

PFNGLDEPTHFUNCPROC gl::gl_DepthFunc INIT_POINTER;
void gl::DepthFunc  (GLenum func, const char* file, int line)
{
	TRACE_FUNCTION("glDepthFunc(...) called from " << get_path(file) << '(' << line << ')');

	breakOnError(
		(func == GL_NEVER ||
		func == GL_LESS ||
		func == GL_EQUAL ||
		func == GL_LEQUAL ||
		func == GL_GREATER ||
		func == GL_NOTEQUAL ||
		func == GL_GEQUAL ||
		func == GL_ALWAYS),
		"func : Invalid enum, must be one of : GL_NEVER, GL_LESS, " 
		"GL_EQUAL, GL_LEQUAL, GL_GREATER, GL_NOTEQUAL, GL_GEQUAL "
		"or GL_ALWAYS"
	);

	gl_DepthFunc(func);
	
	/** check for opengl error */
	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format( "glDepthFunc( func:%s )", 
			_allowed_depth_func_str[func - 512] ),
		file, line
	);

	breakOnError( !result, result );
}

PFNGLDEPTHMASKPROC gl::gl_DepthMask INIT_POINTER;
void gl::DepthMask  (GLboolean flag, const char* file, int line)
{
	TRACE_FUNCTION("glDepthMask(...) called from " << get_path(file) << '(' << line << ')');

	breakOnError(
		flag == GL_TRUE || flag == GL_FALSE,
		"flag : Invalid flag passed, must be GL_TRUE or GL_FALSE."
	);

	gl_DepthMask(flag);

	/** check for opengl error */
	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format("glDepthMask( flag:%s )",
			flag==GL_TRUE?"GL_TRUE":"GL_FALSE"),
		file, line
	);

	breakOnError( !result, result );
}

PFNGLDEPTHRANGEFPROC gl::gl_DepthRangef INIT_POINTER;
void gl::DepthRangef  (GLfloat n, GLfloat f, const char* file, int line)
{
	TRACE_FUNCTION("glDepthRangef(...) called from " << get_path(file) << '(' << line << ')');

	gl_DepthRangef(n,f);

	/** check for opengl error */
	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format("glDepthRangef( n:%g f:%g )",n,f),
		file, line
	);

	breakOnError( !result, result );
}

PFNGLDETACHSHADERPROC gl::gl_DetachShader INIT_POINTER;
void gl::DetachShader  (GLuint program, GLuint shader, const char* file, int line)
{
	TRACE_FUNCTION("glDetachShader(...) called from " << get_path(file) << '(' << line << ')');

	const bool p_is_valid = is_registered_program(program);
	const bool s_is_valid = is_registered_shader(shader);

	breakOnError( p_is_valid, "Invalid program" );
	breakOnError( s_is_valid, "Invalid shader" );

	/** send it to opengl */
	gl_DetachShader(program,shader);

	/** check for opengl error */
	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format("glDetachShader( program:%d shader:%d )",program,shader),
		file, line
	);

	breakOnError( !result, result );
}

PFNGLDISABLEPROC gl::gl_Disable INIT_POINTER;
void gl::Disable  (GLenum cap, const char* file, int line)
{
	TRACE_FUNCTION("glDisable(...) called from " << get_path(file) << '(' << line << ')');

	/** error found if cap is not allowed */
	breakOnError( is_allowed_capability_enable_disable(cap) );
	
	/** warning detected if cap is already disabled */
	breakOnWarning( is_cap_enabled(cap), "capability already disabled" );

	/** local disable */
	disableStates( cap );

	/** send it to opengl */
	gl_Disable(cap);

	/** check for opengl error */
	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format("glDisable( %s )",
			getCapabilityName(cap)),
		file, line
	);

	breakOnError( !result, result );
}

PFNGLDISABLEVERTEXATTRIBARRAYPROC gl::gl_DisableVertexAttribArray INIT_POINTER;
void gl::DisableVertexAttribArray  (GLuint index, const char* file, int line)
{
	TRACE_FUNCTION("glDisableVertexAttribArray(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_DisableVertexAttribArray(index);

	/** check for opengl error */
	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format("glDisableVertexAttribArray( index:%d )",index),
		file, line
	);

	breakOnError( !result, result );
}

// 0<>6
static const char* _allowed_draw_arrays_str[] = {
	"GL_POINTS",
	"GL_LINES",
	"GL_LINE_LOOP",
	"GL_LINE_STRIP",
	"GL_TRIANGLES",
	"GL_TRIANGLE_STRIP",
	"GL_TRIANGLE_FAN"
};

PFNGLDRAWARRAYSPROC gl::gl_DrawArrays INIT_POINTER;
void gl::DrawArrays  (GLenum mode, GLint first, GLsizei count, const char* file, int line)
{
	TRACE_FUNCTION("glDrawArrays(...) called from " << 
		get_path(file) << '(' << line << ')');

	breakOnError(
		(mode == GL_TRIANGLES ||
		mode == GL_TRIANGLE_STRIP ||
		mode == GL_TRIANGLE_FAN ||
		mode == GL_POINTS ||
		mode == GL_LINES ||
		mode == GL_LINE_STRIP ||
		mode == GL_LINE_LOOP),
		"mode : Invalid enum, must be one of : "
		" GL_POINTS, GL_LINE_STRIP, GL_LINE_LOOP, GL_LINES, "
		"GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN or GL_TRIANGLES"
	);

	breakOnError( count >= 1, "count <= 0" );

	gl_DrawArrays(mode,first,count);
	
	/** check for opengl error */
	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format("glDrawArrays( mode:%s first:%d count:%d )",
			_allowed_draw_arrays_str[mode], first, count
		),
		file, line
	);

	breakOnError( !result, result );
}

PFNGLDRAWELEMENTSPROC gl::gl_DrawElements INIT_POINTER;
void gl::DrawElements  (GLenum mode, GLsizei count, GLenum type, const void *indices, const char* file, int line)
{
	TRACE_FUNCTION("glDrawElements(...) called from " << 
		get_path(file) << '(' << line << ')');

    breakOnError(
		(mode == GL_TRIANGLES ||
		mode == GL_TRIANGLE_STRIP ||
		mode == GL_TRIANGLE_FAN ||
		mode == GL_POINTS ||
		mode == GL_LINES ||
		mode == GL_LINE_STRIP ||
		mode == GL_LINE_LOOP),
		"mode : Invalid enum, must be one of : "
		" GL_POINTS, GL_LINE_STRIP, GL_LINE_LOOP, GL_LINES, "
		"GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN or GL_TRIANGLES"
	);

	breakOnError(
		(type == GL_UNSIGNED_BYTE) ||
		(type == GL_UNSIGNED_SHORT), 
		"type: Invalid enum (unsigned int is not allowed)" ); 

	breakOnError( (count >= 1), "count: <= 0" );

	/** check for element array buffer */
	if (!indices && !elementArrayBuffer())
	{
		breakOnError(
			0,
			"No element array buffer or indices data"
		);
	}
	else if ( elementArrayBuffer() )
	{
		breakOnError(
			elementArrayBuffer()->id != INVALID_BOUND,
			"No element array buffer bound (you need to do a : "
			"glBindBuffer(...) before calling glDrawElements())"
		);
	
		/** todo check for transform feedback :
		* 
		 * It is an invalid operation to call DrawElements
		 * DrawRangeElements or DrawElementsInstanced
		 * while transform feedback is active, (3.0.2, section 2.14, pg 86)
		 */
		
		/** todo check mapped buffer 
		 * glMapBuffer(...)
		 */
		
		/** check for valid data */
		breakOnError( MakeBool(elementArrayBuffer()->valid), 
			"Buffer is not valid/set (set it with glBufferData()");

		/** check for request validity
		 * thanks to libANGLE author
		 * https://github.com/google/angle
		 */
		{
			const gl::Type &typeInfo = gl::GetTypeInfo(type);

			GLint64 offset = reinterpret_cast<GLint64>(indices);
			GLint64 byteCount = static_cast<GLint64>(typeInfo.bytes) * 
									static_cast<GLint64>(count)+offset;

			// check for integer overflows
			if (static_cast<GLuint>(count) > 
					(std::numeric_limits<GLuint>::max() / typeInfo.bytes) ||
				byteCount > 
					static_cast<GLint64>(std::numeric_limits<GLuint>::max()))
			{
				breakOnError( 0, "Integer overflows" );
			}

			//breakOnError( count * (is16 ? 2 : 1), "" );
			// Check for reading past the end of the bound buffer object
			if ( byteCount > elementArrayBuffer()->size )
			{
				breakOnError(0, "count : Invalid value");
			}
		}
	}
	// todo verify indices data

	/** send it to opengl */
	gl_DrawElements( mode, count, type, indices );

	/** check for opengl error */
	const char* result = get_last_error();

	/** formated output */
	const char* sMode;

	if      ( mode == GL_TRIANGLES )		sMode = "GL_TRIANGLES";
	else if ( mode == GL_POINTS )			sMode = "GL_POINTS";
	else if ( mode == GL_LINES )			sMode = "GL_LINES";
	else if ( mode == GL_TRIANGLE_STRIP )	sMode = "GL_TRIANGLE_STRIP";
	else if ( mode == GL_TRIANGLE_FAN )		sMode = "GL_TRIANGLE_FAN";
	else if ( mode == GL_LINE_STRIP )		sMode = "GL_LINE_STRIP";
	else if ( mode == GL_LINE_LOOP )		sMode = "GL_LINE_LOOP";

	// add function to call list
	addCall(result,
		format("glDrawElements( mode:%s count:%d type:%s indices:%s )",
			sMode, count,
			type==GL_UNSIGNED_BYTE?"GL_UNSIGNED_BYTE":"GL_UNSIGNED_SHORT",
			indices?"not null":"null"),
		file, line
	);

	breakOnError( !result, result );
}

PFNGLENABLEPROC gl::gl_Enable INIT_POINTER;
void gl::Enable  (GLenum cap, const char* file, int line)
{
	TRACE_FUNCTION("glEnable(...) called from " << 
		get_path(file) << '(' << line << ')');

	const bool is_active = is_cap_enabled(cap);
	const bool is_allowed = is_allowed_capability_enable_disable(cap);

	/** error found if cap is not allowed */
	breakOnError( is_allowed, 
		"invalid capability or not allowed inside glEnable." );

	/** warning detected if cap is already enable */
	breakOnWarning( !is_active, "capability already enabled" );

	/** opengl state */
	gl_Enable( cap );

	/** local enable */
	enableStates( cap );

	/** check for opengl error */
	const char* result = get_last_error();

	// add function to call list
	const char* sCap = getCapabilityName(cap);

	const String fmt( format("glEnable( %s )",
						sCap ? sCap : "unknown caps") );

	addCall(result, fmt, file, line );

	breakOnError( !result, result );
}

PFNGLENABLEVERTEXATTRIBARRAYPROC gl::gl_EnableVertexAttribArray INIT_POINTER;
void gl::EnableVertexAttribArray  (GLuint index, const char* file, int line)
{
	TRACE_FUNCTION("glEnableVertexAttribArray(...) called from " << 
		get_path(file) << '(' << line << ')');

	breakOnError( index < GL_MAX_VERTEX_ATTRIBS ,
		"Invalid vertex attrib" );

	gl_EnableVertexAttribArray( index );

	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format("glEnableVertexAttribArray( index:%d )",index),
		file, line
	);

	breakOnError( !result, result );
}

PFNGLFINISHPROC gl::gl_Finish INIT_POINTER;
void gl::Finish  (const char* file, int line)
{
	TRACE_FUNCTION("glFinish(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_Finish();

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLFLUSHPROC gl::gl_Flush INIT_POINTER;
void gl::Flush  (const char* file, int line)
{
	TRACE_FUNCTION("glFlush(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_Flush();

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLFRAMEBUFFERRENDERBUFFERPROC gl::gl_FramebufferRenderbuffer INIT_POINTER;
void gl::FramebufferRenderbuffer  (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer, const char* file, int line)
{
	TRACE_FUNCTION("glFramebufferRenderbuffer(...) called from " <<
		get_path(file) << '(' << line << ')');

	gl_FramebufferRenderbuffer(target,attachment,renderbuffertarget,renderbuffer);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLFRAMEBUFFERTEXTURE2DPROC gl::gl_FramebufferTexture2D INIT_POINTER;
void gl::FramebufferTexture2D  (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, const char* file, int line)
{
	TRACE_FUNCTION("glFramebufferTexture2D(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_FramebufferTexture2D(target,attachment,textarget,texture,level);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLFRONTFACEPROC gl::gl_FrontFace INIT_POINTER;
void gl::FrontFace  (GLenum mode, const char* file, int line)
{
	TRACE_FUNCTION("glFrontFace(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_FrontFace(mode);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLGENBUFFERSPROC gl::gl_GenBuffers INIT_POINTER;
void gl::GenBuffers  (GLsizei n, GLuint *buffers, const char* file, int line)
{
	int i;

	TRACE_FUNCTION("glGenBuffers(...) called from " << 
		get_path(file) << '(' << line << ')');

	breakOnError( n != 0 || !buffers );

	gl_GenBuffers( n, buffers );

	const char* result = get_last_error();

	String sBuffers;

	for ( i = 0; i < n; ++i)
	{
		sBuffers << format("%d ",buffers[i]);
	}

	// add function to call list
	addCall(result,
		format("glGenBuffers( size:%d, returned::buffers:%s )",
			n,sBuffers.c_str()),
		file, line
	);

	/** add buffers to registered */
	for ( i = 0; i < n; ++i )
	{
		// really ?
		breakOnError(
			register_buffer(buffers[i]),
			"Buffer already registered / Invalid buffer"
		);
	}

	breakOnError( !result, result );
}

PFNGLGENFRAMEBUFFERSPROC gl::gl_GenFramebuffers INIT_POINTER;
void gl::GenFramebuffers  (GLsizei n, GLuint *framebuffers, const char* file, int line)
{
	TRACE_FUNCTION("glGenFramebuffers(...) called from " << 
		get_path(file) << '(' << line << ')');

	breakOnError( n >= 1, "Invalid size requested n <= 0" );
	breakOnError( !framebuffers, "Invalid / empty destination pointer" );

	gl_GenFramebuffers(n,framebuffers);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLGENRENDERBUFFERSPROC gl::gl_GenRenderbuffers INIT_POINTER;
void gl::GenRenderbuffers  (GLsizei n, GLuint *renderbuffers, const char* file, int line)
{
	TRACE_FUNCTION("glGenRenderbuffers(...) called from " << 
		get_path(file) << '(' << line << ')');

	breakOnError(n != 0 || !renderbuffers);

	gl_GenRenderbuffers(n,renderbuffers);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLGENTEXTURESPROC gl::gl_GenTextures INIT_POINTER;
void gl::GenTextures  (GLsizei n, GLuint *textures, const char* file, int line)
{
	TRACE_FUNCTION("glGenTextures(...) called from " << 
		get_path(file) << '(' << line << ')');

	breakOnError( n != 0, "n : Invalid value" );
	breakOnError(MakeBool(textures), "textures : Invalid pointer" );

	/** send it to opengl */
	gl_GenTextures(n, textures);
	
	const char* result = get_last_error();

	/** format output */
	String sTextures;
	for(int i = 0; i < n; ++i)
	{
		sTextures << format( "[%d]: %d ", i, textures[i] );
	}

	// add function to call list
	addCall(result,
		format("glGenTextures( size:%d Textures: %s)",n,
		sTextures.c_str() ),
		file, line
	);

	/** add texture to registered */
	for (int i = 0; i < n; ++i)
	{
		register_texture(textures[i]);
	}
	
	breakOnError( !result, result );
}

PFNGLGENERATEMIPMAPPROC gl::gl_GenerateMipmap INIT_POINTER;
void gl::GenerateMipmap  (GLenum target, const char* file, int line)
{
	TRACE_FUNCTION("glGenerateMipmap(...) called from " << 
		get_path(file) << '(' << line << ')');

	breakOnError(
		(target == GL_TEXTURE_2D ||
		target == GL_TEXTURE_CUBE_MAP),
		"target : Invalid enum, must be one of : GL_TEXTURE_2D or "
		"GL_TEXTURE_CUBE_MAP"
	);

	gl_GenerateMipmap(target);

	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format("glGenerateMipmap( target:%s, buffers:%s)",
		target == GL_TEXTURE_2D ? "GL_TEXTURE_2D":"GL_TEXTURE_CUBE_MAP"
		),
		file, line
	);

	breakOnError( !result, result );
}

PFNGLGETACTIVEATTRIBPROC gl::gl_GetActiveAttrib INIT_POINTER;
void gl::GetActiveAttrib  (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name, const char* file, int line)
{
	TRACE_FUNCTION("glGetActiveAttrib(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_GetActiveAttrib(program,index,bufSize,length,size,type,name);

	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLGETACTIVEUNIFORMPROC gl::gl_GetActiveUniform INIT_POINTER;
void gl::GetActiveUniform  (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name, const char* file, int line)
{
	TRACE_FUNCTION("glGetActiveUniform(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_GetActiveUniform(program,index,bufSize,length,size,type,name);
	
	const char* result = get_last_error();

	breakOnError( !result, result );
}

PFNGLGETATTACHEDSHADERSPROC gl::gl_GetAttachedShaders INIT_POINTER;
void gl::GetAttachedShaders  (GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders, const char* file, int line)
{
	TRACE_FUNCTION("glGetAttachedShaders(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_GetAttachedShaders( program,maxCount,count,shaders );
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLGETATTRIBLOCATIONPROC gl::gl_GetAttribLocation INIT_POINTER;
GLint gl::GetAttribLocation  (GLuint program, const GLchar *name, const char* file, int line)
{
	TRACE_FUNCTION("glGetAttribLocation(...) called from " << 
		get_path(file) << '(' << line << ')');

	GLint r =  gl_GetAttribLocation(program,name);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
	
	return r;
}

PFNGLGETBOOLEANVPROC gl::gl_GetBooleanv INIT_POINTER;
void gl::GetBooleanv  (GLenum pname, GLboolean *data, const char* file, int line)
{
	TRACE_FUNCTION("glGetBooleanv(...) called from " << 
		get_path(file) << '(' << line << ')');

	const char* sPname = is_allowed_enum_get_function(pname);

	breakOnError( MakeBool(sPname), "pname : Invalid enum" );

	breakOnError( MakeBool(data), "data : Invalid pointer" );

	gl_GetBooleanv(pname,data);

	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format("glGetBooleanv( pname:%s, data:%d )",
		sPname, *data // can be an array of data...
		),
		file, line
	);
	
	breakOnError( !result, result );
}

PFNGLGETBUFFERPARAMETERIVPROC gl::gl_GetBufferParameteriv INIT_POINTER;
void gl::GetBufferParameteriv  (GLenum target, GLenum pname, GLint *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetBufferParameteriv(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_GetBufferParameteriv(target,pname,params);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLGETERRORPROC gl::gl_GetError INIT_POINTER;
GLenum gl::GetError  (const char* file, int line)
{
	TRACE_FUNCTION("glGetError(...) called from " << 
		get_path(file) << '(' << line << ')');

	GLenum r = gl_GetError();
	
	return r;
}

PFNGLGETFLOATVPROC gl::gl_GetFloatv INIT_POINTER;
void gl::GetFloatv  (GLenum pname, GLfloat *data, const char* file, int line)
{
	TRACE_FUNCTION("glGetFloatv(...) called from " << 
		get_path(file) << '(' << line << ')');

	const char* sPname = is_allowed_enum_get_function(pname);

	breakOnError(MakeBool(sPname), "pname : Invalid enum" );

	breakOnError(MakeBool(data), "data : Invalid pointer" );

	gl_GetFloatv(pname,data);

	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format("glGetFloatv( pname:%s, data:%f )",
		sPname, *data // can be an array of data...
		),
		file, line
	);

	breakOnError( !result, result );
}

PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC gl::gl_GetFramebufferAttachmentParameteriv INIT_POINTER;
void gl::GetFramebufferAttachmentParameteriv  (GLenum target, GLenum attachment, GLenum pname, GLint *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetFramebufferAttachmentParameteriv(...)"
		" called from " << get_path(file) << '(' << line << ')');

	gl_GetFramebufferAttachmentParameteriv(target,attachment,pname,
		params);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

const char* gl::is_allowed_enum_get_function(GLenum pname)
{
	switch(pname)
	{
		case GL_ACTIVE_TEXTURE: return "GL_ACTIVE_TEXTURE";
		case GL_ALIASED_LINE_WIDTH_RANGE: return "GL_ALIASED_LINE_WIDTH_RANGE";
		case GL_ALIASED_POINT_SIZE_RANGE: return "GL_ALIASED_POINT_SIZE_RANGE";
		case GL_ALPHA_BITS: return "GL_ALPHA_BITS";
		case GL_ARRAY_BUFFER_BINDING: return "GL_ARRAY_BUFFER_BINDING";
		case GL_BLEND: return "GL_BLEND";
		case GL_BLEND_COLOR: return "GL_BLEND_COLOR";
		case GL_BLEND_DST_ALPHA: return "GL_BLEND_DST_ALPHA";
		case GL_BLEND_DST_RGB: return "GL_BLEND_DST_RGB";
		case GL_BLEND_EQUATION_ALPHA: return "GL_BLEND_EQUATION_ALPHA";
		case GL_FUNC_REVERSE_SUBTRACT: return "GL_FUNC_REVERSE_SUBTRACT";
		case GL_BLEND_EQUATION_RGB: return "GL_BLEND_EQUATION_RGB";
		case GL_BLEND_SRC_ALPHA: return "GL_BLEND_SRC_ALPHA";
		case GL_BLEND_SRC_RGB: return "GL_BLEND_SRC_RGB";
		case GL_BLUE_BITS: return "GL_BLUE_BITS";
		case GL_COLOR_CLEAR_VALUE: return "GL_COLOR_CLEAR_VALUE";
		case GL_COLOR_WRITEMASK: return "GL_COLOR_WRITEMASK";
		case GL_COMPRESSED_TEXTURE_FORMATS: return "GL_COMPRESSED_TEXTURE_FORMATS";
		case GL_CULL_FACE: return "GL_CULL_FACE";
		case GL_CULL_FACE_MODE: return "GL_CULL_FACE_MODE";
		case GL_CURRENT_PROGRAM: return "GL_CURRENT_PROGRAM";
		case GL_DEPTH_BITS: return "GL_DEPTH_BITS";
		case GL_DEPTH_CLEAR_VALUE: return "GL_DEPTH_CLEAR_VALUE";
		case GL_DEPTH_FUNC: return "GL_DEPTH_FUNC";
		case GL_DEPTH_RANGE: return "GL_DEPTH_RANGE";
		case GL_DEPTH_TEST: return "GL_DEPTH_TEST";
		case GL_DEPTH_WRITEMASK: return "GL_DEPTH_WRITEMASK";
		case GL_DITHER: return "GL_DITHER";
		case GL_ELEMENT_ARRAY_BUFFER_BINDING: return "GL_ELEMENT_ARRAY_BUFFER_BINDING";
		case GL_FRAMEBUFFER_BINDING: return "GL_FRAMEBUFFER_BINDING";
		case GL_FRONT_FACE: return "GL_FRONT_FACE";
		case GL_GENERATE_MIPMAP_HINT: return "GL_GENERATE_MIPMAP_HINT";
		case GL_GREEN_BITS: return "GL_GREEN_BITS";
		case GL_IMPLEMENTATION_COLOR_READ_FORMAT: return "GL_IMPLEMENTATION_COLOR_READ_FORMAT";
		case GL_IMPLEMENTATION_COLOR_READ_TYPE: return "GL_IMPLEMENTATION_COLOR_READ_TYPE";
		case GL_LINE_WIDTH: return "GL_LINE_WIDTH";
		case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS: return "GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS";
		case GL_MAX_CUBE_MAP_TEXTURE_SIZE: return "GL_MAX_CUBE_MAP_TEXTURE_SIZE";
		case GL_MAX_FRAGMENT_UNIFORM_VECTORS: return "GL_MAX_FRAGMENT_UNIFORM_VECTORS";
		case GL_MAX_RENDERBUFFER_SIZE: return "GL_MAX_RENDERBUFFER_SIZE";
		case GL_MAX_TEXTURE_IMAGE_UNITS: return "GL_MAX_TEXTURE_IMAGE_UNITS";
		case GL_MAX_TEXTURE_SIZE: return "GL_MAX_TEXTURE_SIZE";
		case GL_MAX_VARYING_VECTORS: return "GL_MAX_VARYING_VECTORS";
		case GL_MAX_VERTEX_ATTRIBS: return "GL_MAX_VERTEX_ATTRIBS";
		case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS: return "GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS";
		case GL_MAX_VERTEX_UNIFORM_VECTORS: return "GL_MAX_VERTEX_UNIFORM_VECTORS";
		case GL_MAX_VIEWPORT_DIMS: return "GL_MAX_VIEWPORT_DIMS";
		case GL_NUM_COMPRESSED_TEXTURE_FORMATS: return "GL_NUM_COMPRESSED_TEXTURE_FORMATS";
		case GL_NUM_SHADER_BINARY_FORMATS: return "GL_NUM_SHADER_BINARY_FORMATS";
		case GL_PACK_ALIGNMENT: return "GL_PACK_ALIGNMENT";
		case GL_POLYGON_OFFSET_FACTOR: return "GL_POLYGON_OFFSET_FACTOR";
		case GL_POLYGON_OFFSET_FILL: return "GL_POLYGON_OFFSET_FILL";
		case GL_POLYGON_OFFSET_UNITS: return "GL_POLYGON_OFFSET_UNITS";
		case GL_RED_BITS: return "GL_RED_BITS";
		case GL_RENDERBUFFER_BINDING: return "GL_RENDERBUFFER_BINDING";
		case GL_SAMPLE_ALPHA_TO_COVERAGE: return "GL_SAMPLE_ALPHA_TO_COVERAGE";
		case GL_SAMPLE_BUFFERS: return "GL_SAMPLE_BUFFERS";
		case GL_SAMPLE_COVERAGE: return "GL_SAMPLE_COVERAGE";
		case GL_SAMPLE_COVERAGE_INVERT: return "GL_SAMPLE_COVERAGE_INVERT";
		case GL_SAMPLE_COVERAGE_VALUE: return "GL_SAMPLE_COVERAGE_VALUE";
		case GL_SAMPLES: return "GL_SAMPLES";
		case GL_SCISSOR_BOX: return "GL_SCISSOR_BOX";
		case GL_SCISSOR_TEST: return "GL_SCISSOR_TEST";
		case GL_SHADER_BINARY_FORMATS: return "GL_SHADER_BINARY_FORMATS";
		case GL_SHADER_COMPILER: return "GL_SHADER_COMPILER";
		case GL_STENCIL_BACK_FAIL: return "GL_STENCIL_BACK_FAIL";
		case GL_STENCIL_BACK_FUNC: return "GL_STENCIL_BACK_FUNC";
		case GL_STENCIL_BACK_PASS_DEPTH_FAIL: return "GL_STENCIL_BACK_PASS_DEPTH_FAIL";
		case GL_STENCIL_BACK_PASS_DEPTH_PASS: return "GL_STENCIL_BACK_PASS_DEPTH_PASS";
		case GL_STENCIL_BACK_REF: return "GL_STENCIL_BACK_REF";
		case GL_STENCIL_BACK_VALUE_MASK: return "GL_STENCIL_BACK_VALUE_MASK";
		case GL_STENCIL_BACK_WRITEMASK: return "GL_STENCIL_BACK_WRITEMASK";
		case GL_STENCIL_BITS: return "GL_STENCIL_BITS";
		case GL_STENCIL_CLEAR_VALUE: return "GL_STENCIL_CLEAR_VALUE";
		case GL_STENCIL_FAIL: return "GL_STENCIL_FAIL";
		case GL_STENCIL_FUNC: return "GL_STENCIL_FUNC";
		case GL_STENCIL_PASS_DEPTH_FAIL: return "GL_STENCIL_PASS_DEPTH_FAIL";
		case GL_STENCIL_PASS_DEPTH_PASS: return "GL_STENCIL_PASS_DEPTH_PASS";
		case GL_STENCIL_REF: return "GL_STENCIL_REF";
		case GL_STENCIL_TEST: return "GL_STENCIL_TEST";
		case GL_STENCIL_VALUE_MASK: return "GL_STENCIL_VALUE_MASK";
		case GL_STENCIL_WRITEMASK: return "GL_STENCIL_WRITEMASK";
		case GL_SUBPIXEL_BITS: return "GL_SUBPIXEL_BITS";
		case GL_TEXTURE_BINDING_2D: return "GL_TEXTURE_BINDING_2D";
		case GL_TEXTURE_BINDING_CUBE_MAP: return "GL_TEXTURE_BINDING_CUBE_MAP";
		case GL_UNPACK_ALIGNMENT: return "GL_UNPACK_ALIGNMENT";
		case GL_VIEWPORT: return "GL_VIEWPORT";
	}

	return is_allowed_get_function_in_extensions(pname);
}

const char* gl::is_allowed_get_function_in_extensions(GLenum pname)
{
	const char* ret;

	// -----------------------------------------------------------------
	// eglext.h --------------------------------------------------------
	// -----------------------------------------------------------------

#if defined(EGL_KHR_cl_event)
	if ( Extensions::has(hash_EGL_KHR_cl_event2) )
	{
		ret = egl::is_define_khr_cl_event(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_cl_event
#if defined(EGL_KHR_cl_event2)
	if ( Extensions::has(hash_EGL_KHR_cl_event2) )
	{
		ret = egl::is_define_khr_cl_event2(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_cl_event2
#if defined(EGL_KHR_client_get_all_proc_addresses)
	if ( Extensions::has(hash_EGL_KHR_client_get_all_proc_addresses) )
	{
		ret = egl::is_define_khr_client_get_all_proc_addresses(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_client_get_all_proc_addresses
#if defined(EGL_KHR_config_attribs)
	if ( Extensions::has(hash_EGL_KHR_config_attribs) )
	{
		ret = egl::is_define_khr_config_attribs(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_config_attribs
#if defined(EGL_KHR_context_flush_control)
	if ( Extensions::has(hash_EGL_KHR_context_flush_control) )
	{
		ret = egl::is_define_khr_context_flush_control(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_context_flush_control
#if defined(EGL_KHR_create_context)
	if ( Extensions::has(hash_EGL_KHR_create_context) )
	{
		ret = egl::is_define_khr_create_context(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_create_context
#if defined(EGL_KHR_create_context_no_error)
	if ( Extensions::has(hash_EGL_KHR_create_context_no_error) )
	{
		ret = egl::is_define_khr_create_context_no_error(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_create_context_no_error
#if defined(EGL_KHR_debug)
	if ( Extensions::has(hash_EGL_KHR_debug) )
	{
		ret = egl::is_define_khr_debug(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_debug
#if defined(EGL_KHR_fence_sync)
	if ( Extensions::has(hash_EGL_KHR_fence_sync) )
	{
		ret = egl::is_define_khr_fence_sync(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_fence_sync
#if defined(EGL_KHR_get_all_proc_addresses)
	if ( Extensions::has(hash_EGL_KHR_get_all_proc_addresses) )
	{
		ret = egl::is_define_khr_get_all_proc_addresses(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_get_all_proc_addresses
#if defined(EGL_KHR_gl_colorspace)
	if ( Extensions::has(hash_EGL_KHR_gl_colorspace) )
	{
		ret = egl::is_define_khr_gl_colorspace(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_gl_colorspace
#if defined(EGL_KHR_gl_renderbuffer_image)
	if ( Extensions::has(hash_EGL_KHR_gl_renderbuffer_image) )
	{
		ret = egl::is_define_khr_gl_renderbuffer_image(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_gl_renderbuffer_image
#if defined(EGL_KHR_gl_texture_2D_image)
	if ( Extensions::has(hash_EGL_KHR_gl_texture_2D_image) )
	{
		ret = egl::is_define_khr_gl_texture_2d_image(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_gl_texture_2D_image
#if defined(EGL_KHR_gl_texture_3D_image)
	if ( Extensions::has(hash_EGL_KHR_gl_texture_3D_image) )
	{
		ret = egl::is_define_khr_gl_texture_3d_image(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_gl_texture_3D_image
#if defined(EGL_KHR_gl_texture_cubemap_image)
	if ( Extensions::has(hash_EGL_KHR_gl_texture_cubemap_image) )
	{
		ret = egl::is_define_khr_gl_texture_cubemap_image(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_gl_texture_cubemap_image
#if defined(EGL_KHR_image)
	if ( Extensions::has(hash_EGL_KHR_image) )
	{
		ret = egl::is_define_khr_image(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_image
#if defined(EGL_KHR_image_base)
	if ( Extensions::has(hash_EGL_KHR_image_base) )
	{
		ret = egl::is_define_khr_image_base(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_image_base
#if defined(EGL_KHR_image_pixmap)
	if ( Extensions::has(hash_EGL_KHR_image_pixmap) )
	{
		ret = egl::is_define_khr_image_pixmap(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_image_pixmap
#if defined(EGL_KHR_lock_surface)
	if ( Extensions::has(hash_EGL_KHR_lock_surface) )
	{
		ret = egl::is_define_khr_lock_surface(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_lock_surface
#if defined(EGL_KHR_lock_surface2)
	if ( Extensions::has(hash_EGL_KHR_lock_surface2) )
	{
		ret = egl::is_define_khr_lock_surface2(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_lock_surface2
#if defined(EGL_KHR_lock_surface3)
	if ( Extensions::has(hash_EGL_KHR_lock_surface3) )
	{
		ret = egl::is_define_khr_lock_surface3(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_lock_surface3
#if defined(EGL_KHR_mutable_render_buffer)
	if ( Extensions::has(hash_EGL_KHR_mutable_render_buffer) )
	{
		ret = egl::is_define_khr_mutable_render_buffer(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_mutable_render_buffer
#if defined(EGL_KHR_no_config_context)
	if ( Extensions::has(hash_EGL_KHR_no_config_context) )
	{
		ret = egl::is_define_khr_no_config_context(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_no_config_context
#if defined(EGL_KHR_partial_update)
	if ( Extensions::has(hash_EGL_KHR_partial_update) )
	{
		ret = egl::is_define_khr_partial_update(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_partial_update
#if defined(EGL_KHR_platform_android)
	if ( Extensions::has(hash_EGL_KHR_platform_android) )
	{
		ret = egl::is_define_khr_platform_android(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_platform_android
#if defined(EGL_KHR_platform_gbm)
	if ( Extensions::has(hash_EGL_KHR_platform_gbm) )
	{
		ret = egl::is_define_khr_platform_gbm(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_platform_gbm
#if defined(EGL_KHR_platform_wayland)
	if ( Extensions::has(hash_EGL_KHR_platform_wayland) )
	{
		ret = egl::is_define_khr_platform_wayland(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_platform_wayland
#if defined(EGL_KHR_platform_x11)
	if ( Extensions::has(hash_EGL_KHR_platform_x11) )
	{
		ret = egl::is_define_khr_platform_x11(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_platform_x11
#if defined(EGL_KHR_reusable_sync)
	if ( Extensions::has(hash_EGL_KHR_reusable_sync) )
	{
		ret = egl::is_define_khr_reusable_sync(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_reusable_sync
#if defined(EGL_KHR_stream)
	if ( Extensions::has(hash_EGL_KHR_stream) )
	{
		ret = egl::is_define_khr_stream(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_stream
#if defined(EGL_KHR_stream_attrib)
	if ( Extensions::has(hash_EGL_KHR_stream_attrib) )
	{
		ret = egl::is_define_khr_stream_attrib(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_stream_attrib
#if defined(EGL_KHR_stream_consumer_gltexture)
	if ( Extensions::has(hash_EGL_KHR_stream_consumer_gltexture) )
	{
		ret = egl::is_define_khr_stream_consumer_gltexture(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_stream_consumer_gltexture
#if defined(EGL_KHR_stream_cross_process_fd)
	if ( Extensions::has(hash_EGL_KHR_stream_cross_process_fd) )
	{
		ret = egl::is_define_khr_stream_cross_process_fd(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_stream_cross_process_fd
#if defined(EGL_KHR_stream_fifo)
	if ( Extensions::has(hash_EGL_KHR_stream_fifo) )
	{
		ret = egl::is_define_khr_stream_fifo(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_stream_fifo
#if defined(EGL_KHR_stream_producer_aldatalocator)
	if ( Extensions::has(hash_EGL_KHR_stream_producer_aldatalocator) )
	{
		ret = egl::is_define_khr_stream_producer_aldatalocator(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_stream_producer_aldatalocator
#if defined(EGL_KHR_stream_producer_eglsurface)
	if ( Extensions::has(hash_EGL_KHR_stream_producer_eglsurface) )
	{
		ret = egl::is_define_khr_stream_producer_eglsurface(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_stream_producer_eglsurface
#if defined(EGL_KHR_surfaceless_context)
	if ( Extensions::has(hash_EGL_KHR_surfaceless_context) )
	{
		ret = egl::is_define_khr_surfaceless_context(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_surfaceless_context
#if defined(EGL_KHR_swap_buffers_with_damage)
	if ( Extensions::has(hash_EGL_KHR_swap_buffers_with_damage) )
	{
		ret = egl::is_define_khr_swap_buffers_with_damage(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_swap_buffers_with_damage
#if defined(EGL_KHR_vg_parent_image)
	if ( Extensions::has(hash_EGL_KHR_vg_parent_image) )
	{
		ret = egl::is_define_khr_vg_parent_image(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_vg_parent_image
#if defined(EGL_KHR_wait_sync)
	if ( Extensions::has(hash_EGL_KHR_wait_sync) )
	{
		ret = egl::is_define_khr_wait_sync(pname);
		if( ret ) return ret;
	}
#endif // EGL_KHR_wait_sync
#if defined(EGL_ANDROID_blob_cache)
	if ( Extensions::has(hash_EGL_ANDROID_blob_cache) )
	{
		ret = egl::is_define_android_blob_cache(pname);
		if( ret ) return ret;
	}
#endif // EGL_ANDROID_blob_cache
#if defined(EGL_ANDROID_create_native_client_buffer)
	if ( Extensions::has(hash_EGL_ANDROID_create_native_client_buffer) )
	{
		ret = egl::is_define_android_create_native_client_buffer(pname);
		if( ret ) return ret;
	}
#endif // EGL_ANDROID_create_native_client_buffer
#if defined(EGL_ANDROID_framebuffer_target)
	if ( Extensions::has(hash_EGL_ANDROID_framebuffer_target) )
	{
		ret = egl::is_define_android_framebuffer_target(pname);
		if( ret ) return ret;
	}
#endif // EGL_ANDROID_framebuffer_target
#if defined(EGL_ANDROID_front_buffer_auto_refresh)
	if ( Extensions::has(hash_EGL_ANDROID_front_buffer_auto_refresh) )
	{
		ret = egl::is_define_android_front_buffer_auto_refresh(pname);
		if( ret ) return ret;
	}
#endif // EGL_ANDROID_front_buffer_auto_refresh
#if defined(EGL_ANDROID_image_native_buffer)
	if ( Extensions::has(hash_EGL_ANDROID_image_native_buffer) )
	{
		ret = egl::is_define_android_image_native_buffer(pname);
		if( ret ) return ret;
	}
#endif // EGL_ANDROID_image_native_buffer
#if defined(EGL_ANDROID_native_fence_sync)
	if ( Extensions::has(hash_EGL_ANDROID_native_fence_sync) )
	{
		ret = egl::is_define_android_native_fence_sync(pname);
		if( ret ) return ret;
	}
#endif // EGL_ANDROID_native_fence_sync
#if defined(EGL_ANDROID_presentation_time)
	if ( Extensions::has(hash_EGL_ANDROID_presentation_time) )
	{
		ret = egl::is_define_android_presentation_time(pname);
		if( ret ) return ret;
	}
#endif // EGL_ANDROID_presentation_time
#if defined(EGL_ANDROID_recordable)
	if ( Extensions::has(hash_EGL_ANDROID_recordable) )
	{
		ret = egl::is_define_android_recordable(pname);
		if( ret ) return ret;
	}
#endif // EGL_ANDROID_recordable
#if defined(EGL_ANGLE_d3d_share_handle_client_buffer)
	if ( Extensions::has(hash_EGL_ANGLE_d3d_share_handle_client_buffer) )
	{
		ret = egl::is_define_angle_d3d_share_handle_client_buffer(pname);
		if( ret ) return ret;
	}
#endif // EGL_ANGLE_d3d_share_handle_client_buffer
#if defined(EGL_ANGLE_device_d3d)
	if ( Extensions::has(hash_EGL_ANGLE_device_d3d) )
	{
		ret = egl::is_define_angle_device_d3d(pname);
		if( ret ) return ret;
	}
#endif // EGL_ANGLE_device_d3d
#if defined(EGL_ANGLE_query_surface_pointer)
	if ( Extensions::has(hash_EGL_ANGLE_query_surface_pointer) )
	{
		ret = egl::is_define_angle_query_surface_pointer(pname);
		if( ret ) return ret;
	}
#endif // EGL_ANGLE_query_surface_pointer
#if defined(EGL_ANGLE_surface_d3d_texture_2d_share_handle)
	if ( Extensions::has(hash_EGL_ANGLE_surface_d3d_texture_2d_share_handle) )
	{
		ret = egl::is_define_angle_surface_d3d_texture_2d_share_handle(pname);
		if( ret ) return ret;
	}
#endif // EGL_ANGLE_surface_d3d_texture_2d_share_handle
#if defined(EGL_ANGLE_window_fixed_size)
	if ( Extensions::has(hash_EGL_ANGLE_window_fixed_size) )
	{
		ret = egl::is_define_angle_window_fixed_size(pname);
		if( ret ) return ret;
	}
#endif // EGL_ANGLE_window_fixed_size
#if defined(EGL_ARM_implicit_external_sync)
	if ( Extensions::has(hash_EGL_ARM_implicit_external_sync) )
	{
		ret = egl::is_define_arm_implicit_external_sync(pname);
		if( ret ) return ret;
	}
#endif // EGL_ARM_implicit_external_sync
#if defined(EGL_ARM_pixmap_multisample_discard)
	if ( Extensions::has(hash_EGL_ARM_pixmap_multisample_discard) )
	{
		ret = egl::is_define_arm_pixmap_multisample_discard(pname);
		if( ret ) return ret;
	}
#endif // EGL_ARM_pixmap_multisample_discard
#if defined(EGL_EXT_buffer_age)
	if ( Extensions::has(hash_EGL_EXT_buffer_age) )
	{
		ret = egl::is_define_ext_buffer_age(pname);
		if( ret ) return ret;
	}
#endif // EGL_EXT_buffer_age
#if defined(EGL_EXT_client_extensions)
	if ( Extensions::has(hash_EGL_EXT_client_extensions) )
	{
		ret = egl::is_define_ext_client_extensions(pname);
		if( ret ) return ret;
	}
#endif // EGL_EXT_client_extensions
#if defined(EGL_EXT_create_context_robustness)
	if ( Extensions::has(hash_EGL_EXT_create_context_robustness) )
	{
		ret = egl::is_define_ext_create_context_robustness(pname);
		if( ret ) return ret;
	}
#endif // EGL_EXT_create_context_robustness
#if defined(EGL_EXT_device_base)
	if ( Extensions::has(hash_EGL_EXT_device_base) )
	{
		ret = egl::is_define_ext_device_base(pname);
		if( ret ) return ret;
	}
#endif // EGL_EXT_device_base
#if defined(EGL_EXT_device_drm)
	if ( Extensions::has(hash_EGL_EXT_device_drm) )
	{
		ret = egl::is_define_ext_device_drm(pname);
		if( ret ) return ret;
	}
#endif // EGL_EXT_device_drm
#if defined(EGL_EXT_device_enumeration)
	if ( Extensions::has(hash_EGL_EXT_device_enumeration) )
	{
		ret = egl::is_define_ext_device_enumeration(pname);
		if( ret ) return ret;
	}
#endif // EGL_EXT_device_enumeration
#if defined(EGL_EXT_device_openwf)
	if ( Extensions::has(hash_EGL_EXT_device_openwf) )
	{
		ret = egl::is_define_ext_device_openwf(pname);
		if( ret ) return ret;
	}
#endif // EGL_EXT_device_openwf
#if defined(EGL_EXT_device_query)
	if ( Extensions::has(hash_EGL_EXT_device_query) )
	{
		ret = egl::is_define_ext_device_query(pname);
		if( ret ) return ret;
	}
#endif // EGL_EXT_device_query
#if defined(EGL_EXT_image_dma_buf_import)
	if ( Extensions::has(hash_EGL_EXT_image_dma_buf_import) )
	{
		ret = egl::is_define_ext_image_dma_buf_import(pname);
		if( ret ) return ret;
	}
#endif // EGL_EXT_image_dma_buf_import
#if defined(EGL_EXT_multiview_window)
	if ( Extensions::has(hash_EGL_EXT_multiview_window) )
	{
		ret = egl::is_define_ext_multiview_window(pname);
		if( ret ) return ret;
	}
#endif // EGL_EXT_multiview_window
#if defined(EGL_EXT_output_base)
	if ( Extensions::has(hash_EGL_EXT_output_base) )
	{
		ret = egl::is_define_ext_output_base(pname);
		if( ret ) return ret;
	}
#endif // EGL_EXT_output_base
#if defined(EGL_EXT_output_drm)
	if ( Extensions::has(hash_EGL_EXT_output_drm) )
	{
		ret = egl::is_define_ext_output_drm(pname);
		if( ret ) return ret;
	}
#endif // EGL_EXT_output_drm
#if defined(EGL_EXT_output_openwf)
	if ( Extensions::has(hash_EGL_EXT_output_openwf) )
	{
		ret = egl::is_define_ext_output_openwf(pname);
		if( ret ) return ret;
	}
#endif // EGL_EXT_output_openwf
#if defined(EGL_EXT_platform_base)
	if ( Extensions::has(hash_EGL_EXT_platform_base) )
	{
		ret = egl::is_define_ext_platform_base(pname);
		if( ret ) return ret;
	}
#endif // EGL_EXT_platform_base
#if defined(EGL_EXT_platform_device)
	if ( Extensions::has(hash_EGL_EXT_platform_device) )
	{
		ret = egl::is_define_ext_platform_device(pname);
		if( ret ) return ret;
	}
#endif // EGL_EXT_platform_device
#if defined(EGL_EXT_platform_wayland)
	if ( Extensions::has(hash_EGL_EXT_platform_wayland) )
	{
		ret = egl::is_define_ext_platform_wayland(pname);
		if( ret ) return ret;
	}
#endif // EGL_EXT_platform_wayland
#if defined(EGL_EXT_platform_x11)
	if ( Extensions::has(hash_EGL_EXT_platform_x11) )
	{
		ret = egl::is_define_ext_platform_x11(pname);
		if( ret ) return ret;
	}
#endif // EGL_EXT_platform_x11
#if defined(EGL_EXT_protected_content)
	if ( Extensions::has(hash_EGL_EXT_protected_content) )
	{
		ret = egl::is_define_ext_protected_content(pname);
		if( ret ) return ret;
	}
#endif // EGL_EXT_protected_content
#if defined(EGL_EXT_protected_surface)
	if ( Extensions::has(hash_EGL_EXT_protected_surface) )
	{
		ret = egl::is_define_ext_protected_surface(pname);
		if( ret ) return ret;
	}
#endif // EGL_EXT_protected_surface
#if defined(EGL_EXT_stream_consumer_egloutput)
	if ( Extensions::has(hash_EGL_EXT_stream_consumer_egloutput) )
	{
		ret = egl::is_define_ext_stream_consumer_egloutput(pname);
		if( ret ) return ret;
	}
#endif // EGL_EXT_stream_consumer_egloutput
#if defined(EGL_EXT_swap_buffers_with_damage)
	if ( Extensions::has(hash_EGL_EXT_swap_buffers_with_damage) )
	{
		ret = egl::is_define_ext_swap_buffers_with_damage(pname);
		if( ret ) return ret;
	}
#endif // EGL_EXT_swap_buffers_with_damage
#if defined(EGL_EXT_yuv_surface)
	if ( Extensions::has(hash_EGL_EXT_yuv_surface) )
	{
		ret = egl::is_define_ext_yuv_surface(pname);
		if( ret ) return ret;
	}
#endif // EGL_EXT_yuv_surface
#if defined(EGL_HI_clientpixmap)
	if ( Extensions::has(hash_EGL_HI_clientpixmap) )
	{
		ret = egl::is_define_hi_clientpixmap(pname);
		if( ret ) return ret;
	}
#endif // EGL_HI_clientpixmap
#if defined(EGL_HI_colorformats)
	if ( Extensions::has(hash_EGL_HI_colorformats) )
	{
		ret = egl::is_define_hi_colorformats(pname);
		if( ret ) return ret;
	}
#endif // EGL_HI_colorformats
#if defined(EGL_IMG_context_priority)
	if ( Extensions::has(hash_EGL_IMG_context_priority) )
	{
		ret = egl::is_define_img_context_priority(pname);
		if( ret ) return ret;
	}
#endif // EGL_IMG_context_priority
#if defined(EGL_IMG_image_plane_attribs)
	if ( Extensions::has(hash_EGL_IMG_image_plane_attribs) )
	{
		ret = egl::is_define_img_image_plane_attribs(pname);
		if( ret ) return ret;
	}
#endif // EGL_IMG_image_plane_attribs
#if defined(EGL_MESA_drm_image)
	if ( Extensions::has(hash_EGL_MESA_drm_image) )
	{
		ret = egl::is_define_mesa_drm_image(pname);
		if( ret ) return ret;
	}
#endif // EGL_MESA_drm_image
#if defined(EGL_MESA_image_dma_buf_export)
	if ( Extensions::has(hash_EGL_MESA_image_dma_buf_export) )
	{
		ret = egl::is_define_mesa_image_dma_buf_export(pname);
		if( ret ) return ret;
	}
#endif // EGL_MESA_image_dma_buf_export
#if defined(EGL_MESA_platform_gbm)
	if ( Extensions::has(hash_EGL_MESA_platform_gbm) )
	{
		ret = egl::is_define_mesa_platform_gbm(pname);
		if( ret ) return ret;
	}
#endif // EGL_MESA_platform_gbm
#if defined(EGL_MESA_platform_surfaceless)
	if ( Extensions::has(hash_EGL_MESA_platform_surfaceless) )
	{
		ret = egl::is_define_mesa_platform_surfaceless(pname);
		if( ret ) return ret;
	}
#endif // EGL_MESA_platform_surfaceless
#if defined(EGL_NOK_swap_region)
	if ( Extensions::has(hash_EGL_NOK_swap_region) )
	{
		ret = egl::is_define_nok_swap_region(pname);
		if( ret ) return ret;
	}
#endif // EGL_NOK_swap_region
#if defined(EGL_NOK_swap_region2)
	if ( Extensions::has(hash_EGL_NOK_swap_region2) )
	{
		ret = egl::is_define_nok_swap_region2(pname);
		if( ret ) return ret;
	}
#endif // EGL_NOK_swap_region2
#if defined(EGL_NOK_texture_from_pixmap)
	if ( Extensions::has(hash_EGL_NOK_texture_from_pixmap) )
	{
		ret = egl::is_define_nok_texture_from_pixmap(pname);
		if( ret ) return ret;
	}
#endif // EGL_NOK_texture_from_pixmap
#if defined(EGL_NV_3dvision_surface)
	if ( Extensions::has(hash_EGL_NV_3dvision_surface) )
	{
		ret = egl::is_define_nv_3dvision_surface(pname);
		if( ret ) return ret;
	}
#endif // EGL_NV_3dvision_surface
#if defined(EGL_NV_coverage_sample)
	if ( Extensions::has(hash_EGL_NV_coverage_sample) )
	{
		ret = egl::is_define_nv_coverage_sample(pname);
		if( ret ) return ret;
	}
#endif // EGL_NV_coverage_sample
#if defined(EGL_NV_coverage_sample_resolve)
	if ( Extensions::has(hash_EGL_NV_coverage_sample_resolve) )
	{
		ret = egl::is_define_nv_coverage_sample_resolve(pname);
		if( ret ) return ret;
	}
#endif // EGL_NV_coverage_sample_resolve
#if defined(EGL_NV_cuda_event)
	if ( Extensions::has(hash_EGL_NV_cuda_event) )
	{
		ret = egl::is_define_nv_cuda_event(pname);
		if( ret ) return ret;
	}
#endif // EGL_NV_cuda_event
#if defined(EGL_NV_depth_nonlinear)
	if ( Extensions::has(hash_EGL_NV_depth_nonlinear) )
	{
		ret = egl::is_define_nv_depth_nonlinear(pname);
		if( ret ) return ret;
	}
#endif // EGL_NV_depth_nonlinear
#if defined(EGL_NV_device_cuda)
	if ( Extensions::has(hash_EGL_NV_device_cuda) )
	{
		ret = egl::is_define_nv_device_cuda(pname);
		if( ret ) return ret;
	}
#endif // EGL_NV_device_cuda
#if defined(EGL_NV_native_query)
	if ( Extensions::has(hash_EGL_NV_native_query) )
	{
		ret = egl::is_define_nv_native_query(pname);
		if( ret ) return ret;
	}
#endif // EGL_NV_native_query
#if defined(EGL_NV_post_convert_rounding)
	if ( Extensions::has(hash_EGL_NV_post_convert_rounding) )
	{
		ret = egl::is_define_nv_post_convert_rounding(pname);
		if( ret ) return ret;
	}
#endif // EGL_NV_post_convert_rounding
#if defined(EGL_NV_post_sub_buffer)
	if ( Extensions::has(hash_EGL_NV_post_sub_buffer) )
	{
		ret = egl::is_define_nv_post_sub_buffer(pname);
		if( ret ) return ret;
	}
#endif // EGL_NV_post_sub_buffer
#if defined(EGL_NV_robustness_video_memory_purge)
	if ( Extensions::has(hash_EGL_NV_robustness_video_memory_purge) )
	{
		ret = egl::is_define_nv_robustness_video_memory_purge(pname);
		if( ret ) return ret;
	}
#endif // EGL_NV_robustness_video_memory_purge
#if defined(EGL_NV_stream_consumer_gltexture_yuv)
	if ( Extensions::has(hash_EGL_NV_stream_consumer_gltexture_yuv) )
	{
		ret = egl::is_define_nv_stream_consumer_gltexture_yuv(pname);
		if( ret ) return ret;
	}
#endif // EGL_NV_stream_consumer_gltexture_yuv
#if defined(EGL_NV_stream_metadata)
	if ( Extensions::has(hash_EGL_NV_stream_metadata) )
	{
		ret = egl::is_define_nv_stream_metadata(pname);
		if( ret ) return ret;
	}
#endif // EGL_NV_stream_metadata
#if defined(EGL_NV_stream_sync)
	if ( Extensions::has(hash_EGL_NV_stream_sync) )
	{
		ret = egl::is_define_nv_stream_sync(pname);
		if( ret ) return ret;
	}
#endif // EGL_NV_stream_sync
#if defined(EGL_NV_sync)
	if ( Extensions::has(hash_EGL_NV_sync) )
	{
		ret = egl::is_define_nv_sync(pname);
		if( ret ) return ret;
	}
#endif // EGL_NV_sync
#if defined(EGL_NV_system_time)
	if ( Extensions::has(hash_EGL_NV_system_time) )
	{
		ret = egl::is_define_nv_system_time(pname);
		if( ret ) return ret;
	}
#endif // EGL_NV_system_time
#if defined(EGL_TIZEN_image_native_buffer)
	if ( Extensions::has(hash_EGL_TIZEN_image_native_buffer) )
	{
		ret = egl::is_define_tizen_image_native_buffer(pname);
		if( ret ) return ret;
	}
#endif // EGL_TIZEN_image_native_buffer
#if defined(EGL_TIZEN_image_native_surface)
	if ( Extensions::has(hash_EGL_TIZEN_image_native_surface) )
	{
		ret = egl::is_define_tizen_image_native_surface(pname);
		if( ret ) return ret;
	}
#endif // EGL_TIZEN_image_native_surface

	// -----------------------------------------------------------------
	// gl2ext.h --------------------------------------------------------
	// -----------------------------------------------------------------

#if defined(GL_KHR_blend_equation_advanced)
	if ( Extensions::has(hash_GL_KHR_blend_equation_advanced_coherent) )
	{
		ret = is_define_khr_blend_equation_advanced(pname);
		if( ret ) return ret;
	}
#endif // GL_KHR_blend_equation_advanced
#if defined(GL_KHR_blend_equation_advanced_coherent)
	if ( Extensions::has(hash_GL_KHR_blend_equation_advanced_coherent) )
	{
		ret = is_define_khr_blend_equation_advanced_coherent(pname);
		if( ret ) return ret;
	}
#endif // GL_KHR_blend_equation_advanced_coherent
#if defined(GL_KHR_context_flush_control)
	if ( Extensions::has(hash_GL_KHR_context_flush_control) )
	{
		ret = is_define_khr_context_flush_control(pname);
		if( ret ) return ret;
	}
#endif // GL_KHR_context_flush_control
#if defined(GL_KHR_debug)
	if ( Extensions::has(hash_GL_KHR_debug) )
	{
		ret = is_define_khr_debug(pname);
		if( ret ) return ret;
	}
#endif // GL_KHR_debug
#if defined(GL_KHR_no_error)
	if ( Extensions::has(hash_GL_KHR_no_error) )
	{
		ret = is_define_khr_no_error(pname);
		if( ret ) return ret;
	}
#endif // GL_KHR_no_error
#if defined(GL_KHR_robust_buffer_access_behavior)
	if ( Extensions::has(hash_GL_KHR_robust_buffer_access_behavior) )
	{
		ret = is_define_khr_robust_buffer_access_behavior(pname);
		if( ret ) return ret;
	}
#endif // GL_KHR_robust_buffer_access_behavior
#if defined(GL_KHR_robustness)
	if ( Extensions::has(hash_GL_KHR_robustness) )
	{
		ret = is_define_khr_robustness(pname);
		if( ret ) return ret;
	}
#endif // GL_KHR_robustness
#if defined(GL_KHR_texture_compression_astc_hdr)
	if ( Extensions::has(hash_GL_KHR_texture_compression_astc_hdr) )
	{
		ret = is_define_khr_texture_compression_astc_hdr(pname);
		if( ret ) return ret;
	}
#endif // GL_KHR_texture_compression_astc_hdr
#if defined(GL_KHR_texture_compression_astc_ldr)
	if ( Extensions::has(hash_GL_KHR_texture_compression_astc_ldr) )
	{
		ret = is_define_khr_texture_compression_astc_ldr(pname);
		if( ret ) return ret;
	}
#endif // GL_KHR_texture_compression_astc_ldr
#if defined(GL_KHR_texture_compression_astc_sliced_3d)
	if ( Extensions::has(hash_GL_KHR_texture_compression_astc_sliced_3d) )
	{
		ret = is_define_khr_texture_compression_astc_sliced_3d(pname);
		if( ret ) return ret;
	}
#endif // GL_KHR_texture_compression_astc_sliced_3d
#if defined(GL_OES_EGL_image)
	if ( Extensions::has(hash_GL_OES_EGL_image) )
	{
		ret = is_define_oes_eis_define_image(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_EGL_image
#if defined(GL_OES_EGL_image_external)
	if ( Extensions::has(hash_GL_OES_EGL_image_external) )
	{
		ret = is_define_oes_eis_define_image_external(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_EGL_image_external
#if defined(GL_OES_EGL_image_external_essl3)
	if ( Extensions::has(hash_GL_OES_EGL_image_external_essl3) )
	{
		ret = is_define_oes_eis_define_image_external_essl3(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_EGL_image_external_essl3
#if defined(GL_OES_compressed_ETC1_RGB8_sub_texture)
	if ( Extensions::has(hash_GL_OES_compressed_ETC1_RGB8_sub_texture) )
	{
		ret = is_define_oes_compressed_etc1_rgb8_sub_texture(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_compressed_ETC1_RGB8_sub_texture
#if defined(GL_OES_compressed_ETC1_RGB8_texture)
	if ( Extensions::has(hash_GL_OES_compressed_ETC1_RGB8_texture) )
	{
		ret = is_define_oes_compressed_etc1_rgb8_texture(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_compressed_ETC1_RGB8_texture
#if defined(GL_OES_compressed_paletted_texture)
	if ( Extensions::has(hash_GL_OES_compressed_paletted_texture) )
	{
		ret = is_define_oes_compressed_paletted_texture(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_compressed_paletted_texture
#if defined(GL_OES_copy_image)
	if ( Extensions::has(hash_GL_OES_copy_image) )
	{
		ret = is_define_oes_copy_image(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_copy_image
#if defined(GL_OES_depth24)
	if ( Extensions::has(hash_GL_OES_depth24) )
	{
		ret = is_define_oes_depth24(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_depth24
#if defined(GL_OES_depth32)
	if ( Extensions::has(hash_GL_OES_depth32) )
	{
		ret = is_define_oes_depth32(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_depth32
#if defined(GL_OES_depth_texture)
	if ( Extensions::has(hash_GL_OES_depth_texture) )
	{
		ret = is_define_oes_depth_texture(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_depth_texture
#if defined(GL_OES_draw_buffers_indexed)
	if ( Extensions::has(hash_GL_OES_draw_buffers_indexed) )
	{
		ret = is_define_oes_draw_buffers_indexed(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_draw_buffers_indexed
#if defined(GL_OES_draw_elements_base_vertex)
	if ( Extensions::has(hash_GL_OES_draw_elements_base_vertex) )
	{
		ret = is_define_oes_draw_elements_base_vertex(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_draw_elements_base_vertex
#if defined(GL_OES_element_index_uint)
	if ( Extensions::has(hash_GL_OES_element_index_uint) )
	{
		ret = is_define_oes_element_index_uint(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_element_index_uint
#if defined(GL_OES_fbo_render_mipmap)
	if ( Extensions::has(hash_GL_OES_fbo_render_mipmap) )
	{
		ret = is_define_oes_fbo_render_mipmap(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_fbo_render_mipmap
#if defined(GL_OES_fragment_precision_high)
	if ( Extensions::has(hash_GL_OES_fragment_precision_high) )
	{
		ret = is_define_oes_fragment_precision_high(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_fragment_precision_high
#if defined(GL_OES_geometry_point_size)
	if ( Extensions::has(hash_GL_OES_geometry_point_size) )
	{
		ret = is_define_oes_geometry_point_size(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_geometry_point_size
#if defined(GL_OES_geometry_shader)
	if ( Extensions::has(hash_GL_OES_geometry_shader) )
	{
		ret = is_define_oes_geometry_shader(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_geometry_shader
#if defined(GL_OES_get_program_binary)
	if ( Extensions::has(hash_GL_OES_get_program_binary) )
	{
		ret = is_define_oes_get_program_binary(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_get_program_binary
#if defined(GL_OES_gpu_shader5)
	if ( Extensions::has(hash_GL_OES_gpu_shader5) )
	{
		ret = is_define_oes_gpu_shader5(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_gpu_shader5
#if defined(GL_OES_mapbuffer)
	if ( Extensions::has(hash_GL_OES_mapbuffer) )
	{
		ret = is_define_oes_mapbuffer(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_mapbuffer
#if defined(GL_OES_packed_depth_stencil)
	if ( Extensions::has(hash_GL_OES_packed_depth_stencil) )
	{
		ret = is_define_oes_packed_depth_stencil(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_packed_depth_stencil
#if defined(GL_OES_primitive_bounding_box)
	if ( Extensions::has(hash_GL_OES_primitive_bounding_box) )
	{
		ret = is_define_oes_primitive_bounding_box(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_primitive_bounding_box
#if defined(GL_OES_required_internalformat)
	if ( Extensions::has(hash_GL_OES_required_internalformat) )
	{
		ret = is_define_oes_required_internalformat(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_required_internalformat
#if defined(GL_OES_rgb8_rgba8)
	if ( Extensions::has(hash_GL_OES_rgb8_rgba8) )
	{
		ret = is_define_oes_rgb8_rgba8(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_rgb8_rgba8
#if defined(GL_OES_sample_shading)
	if ( Extensions::has(hash_GL_OES_sample_shading) )
	{
		ret = is_define_oes_sample_shading(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_sample_shading
#if defined(GL_OES_sample_variables)
	if ( Extensions::has(hash_GL_OES_sample_variables) )
	{
		ret = is_define_oes_sample_variables(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_sample_variables
#if defined(GL_OES_shader_image_atomic)
	if ( Extensions::has(hash_GL_OES_shader_image_atomic) )
	{
		ret = is_define_oes_shader_image_atomic(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_shader_image_atomic
#if defined(GL_OES_shader_io_blocks)
	if ( Extensions::has(hash_GL_OES_shader_io_blocks) )
	{
		ret = is_define_oes_shader_io_blocks(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_shader_io_blocks
#if defined(GL_OES_shader_multisample_interpolation)
	if ( Extensions::has(hash_GL_OES_shader_multisample_interpolation) )
	{
		ret = is_define_oes_shader_multisample_interpolation(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_shader_multisample_interpolation
#if defined(GL_OES_standard_derivatives)
	if ( Extensions::has(hash_GL_OES_standard_derivatives) )
	{
		ret = is_define_oes_standard_derivatives(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_standard_derivatives
#if defined(GL_OES_stencil1)
	if ( Extensions::has(hash_GL_OES_stencil1) )
	{
		ret = is_define_oes_stencil1(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_stencil1
#if defined(GL_OES_stencil4)
	if ( Extensions::has(hash_GL_OES_stencil4) )
	{
		ret = is_define_oes_stencil4(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_stencil4
#if defined(GL_OES_surfaceless_context)
	if ( Extensions::has(hash_GL_OES_surfaceless_context) )
	{
		ret = is_define_oes_surfaceless_context(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_surfaceless_context
#if defined(GL_OES_tessellation_point_size)
	if ( Extensions::has(hash_GL_OES_tessellation_point_size) )
	{
		ret = is_define_oes_tessellation_point_size(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_tessellation_point_size
#if defined(GL_OES_tessellation_shader)
	if ( Extensions::has(hash_GL_OES_tessellation_shader) )
	{
		ret = is_define_oes_tessellation_shader(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_tessellation_shader
#if defined(GL_OES_texture_3D)
	if ( Extensions::has(hash_GL_OES_texture_3D) )
	{
		ret = is_define_oes_texture_3d(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_texture_3D
#if defined(GL_OES_texture_border_clamp)
	if ( Extensions::has(hash_GL_OES_texture_border_clamp) )
	{
		ret = is_define_oes_texture_border_clamp(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_texture_border_clamp
#if defined(GL_OES_texture_buffer)
	if ( Extensions::has(hash_GL_OES_texture_buffer) )
	{
		ret = is_define_oes_texture_buffer(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_texture_buffer
#if defined(GL_OES_texture_compression_astc)
	if ( Extensions::has(hash_GL_OES_texture_compression_astc) )
	{
		ret = is_define_oes_texture_compression_astc(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_texture_compression_astc
#if defined(GL_OES_texture_cube_map_array)
	if ( Extensions::has(hash_GL_OES_texture_cube_map_array) )
	{
		ret = is_define_oes_texture_cube_map_array(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_texture_cube_map_array
#if defined(GL_OES_texture_float)
	if ( Extensions::has(hash_GL_OES_texture_float) )
	{
		ret = is_define_oes_texture_float(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_texture_float
#if defined(GL_OES_texture_float_linear)
	if ( Extensions::has(hash_GL_OES_texture_float_linear) )
	{
		ret = is_define_oes_texture_float_linear(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_texture_float_linear
#if defined(GL_OES_texture_half_float)
	if ( Extensions::has(hash_GL_OES_texture_half_float) )
	{
		ret = is_define_oes_texture_half_float(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_texture_half_float
#if defined(GL_OES_texture_half_float_linear)
	if ( Extensions::has(hash_GL_OES_texture_half_float_linear) )
	{
		ret = is_define_oes_texture_half_float_linear(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_texture_half_float_linear
#if defined(GL_OES_texture_npot)
	if ( Extensions::has(hash_GL_OES_texture_npot) )
	{
		ret = is_define_oes_texture_npot(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_texture_npot
#if defined(GL_OES_texture_stencil8)
	if ( Extensions::has(hash_GL_OES_texture_stencil8) )
	{
		ret = is_define_oes_texture_stencil8(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_texture_stencil8
#if defined(GL_OES_texture_storage_multisample_2d_array)
	if ( Extensions::has(hash_GL_OES_texture_storage_multisample_2d_array) )
	{
		ret = is_define_oes_texture_storage_multisample_2d_array(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_texture_storage_multisample_2d_array
#if defined(GL_OES_texture_view)
	if ( Extensions::has(hash_GL_OES_texture_view) )
	{
		ret = is_define_oes_texture_view(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_texture_view
#if defined(GL_OES_vertex_array_object)
	if ( Extensions::has(hash_GL_OES_vertex_array_object) )
	{
		ret = is_define_oes_vertex_array_object(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_vertex_array_object
#if defined(GL_OES_vertex_half_float)
	if ( Extensions::has(hash_GL_OES_vertex_half_float) )
	{
		ret = is_define_oes_vertex_half_float(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_vertex_half_float
#if defined(GL_OES_vertex_type_10_10_10_2)
	if ( Extensions::has(hash_GL_OES_vertex_type_10_10_10_2) )
	{
		ret = is_define_oes_vertex_type_10_10_10_2(pname);
		if( ret ) return ret;
	}
#endif // GL_OES_vertex_type_10_10_10_2
#if defined(GL_AMD_compressed_3DC_texture)
	if ( Extensions::has(hash_GL_AMD_compressed_3DC_texture) )
	{
		ret = is_define_amd_compressed_3dc_texture(pname);
		if( ret ) return ret;
	}
#endif // GL_AMD_compressed_3DC_texture
#if defined(GL_AMD_compressed_ATC_texture)
	if ( Extensions::has(hash_GL_AMD_compressed_ATC_texture) )
	{
		ret = is_define_amd_compressed_atc_texture(pname);
		if( ret ) return ret;
	}
#endif // GL_AMD_compressed_ATC_texture
#if defined(GL_AMD_performance_monitor)
	if ( Extensions::has(hash_GL_AMD_performance_monitor) )
	{
		ret = is_define_amd_performance_monitor(pname);
		if( ret ) return ret;
	}
#endif // GL_AMD_performance_monitor
#if defined(GL_AMD_program_binary_Z400)
	if ( Extensions::has(hash_GL_AMD_program_binary_Z400) )
	{
		ret = is_define_amd_program_binary_z400(pname);
		if( ret ) return ret;
	}
#endif // GL_AMD_program_binary_Z400
#if defined(GL_ANDROID_extension_pack_es31a)
	if ( Extensions::has(hash_GL_ANDROID_extension_pack_es31a) )
	{
		ret = is_define_android_extension_pack_es31a(pname);
		if( ret ) return ret;
	}
#endif // GL_ANDROID_extension_pack_es31a
#if defined(GL_ANGLE_depth_texture)
	if ( Extensions::has(hash_GL_ANGLE_depth_texture) )
	{
		ret = is_define_angle_depth_texture(pname);
		if( ret ) return ret;
	}
#endif // GL_ANGLE_depth_texture
#if defined(GL_ANGLE_framebuffer_blit)
	if ( Extensions::has(hash_GL_ANGLE_framebuffer_blit) )
	{
		ret = is_define_angle_framebuffer_blit(pname);
		if( ret ) return ret;
	}
#endif // GL_ANGLE_framebuffer_blit
#if defined(GL_ANGLE_framebuffer_multisample)
	if ( Extensions::has(hash_GL_ANGLE_framebuffer_multisample) )
	{
		ret = is_define_angle_framebuffer_multisample(pname);
		if( ret ) return ret;
	}
#endif // GL_ANGLE_framebuffer_multisample
#if defined(GL_ANGLE_instanced_arrays)
	if ( Extensions::has(hash_GL_ANGLE_instanced_arrays) )
	{
		ret = is_define_angle_instanced_arrays(pname);
		if( ret ) return ret;
	}
#endif // GL_ANGLE_instanced_arrays
#if defined(GL_ANGLE_pack_reverse_row_order)
	if ( Extensions::has(hash_GL_ANGLE_pack_reverse_row_order) )
	{
		ret = is_define_angle_pack_reverse_row_order(pname);
		if( ret ) return ret;
	}
#endif // GL_ANGLE_pack_reverse_row_order
#if defined(GL_ANGLE_program_binary)
	if ( Extensions::has(hash_GL_ANGLE_program_binary) )
	{
		ret = is_define_angle_program_binary(pname);
		if( ret ) return ret;
	}
#endif // GL_ANGLE_program_binary
#if defined(GL_ANGLE_texture_compression_dxt3)
	if ( Extensions::has(hash_GL_ANGLE_texture_compression_dxt3) )
	{
		ret = is_define_angle_texture_compression_dxt3(pname);
		if( ret ) return ret;
	}
#endif // GL_ANGLE_texture_compression_dxt3
#if defined(GL_ANGLE_texture_compression_dxt5)
	if ( Extensions::has(hash_GL_ANGLE_texture_compression_dxt5) )
	{
		ret = is_define_angle_texture_compression_dxt5(pname);
		if( ret ) return ret;
	}
#endif // GL_ANGLE_texture_compression_dxt5
#if defined(GL_ANGLE_texture_usage)
	if ( Extensions::has(hash_GL_ANGLE_texture_usage) )
	{
		ret = is_define_angle_texture_usage(pname);
		if( ret ) return ret;
	}
#endif // GL_ANGLE_texture_usage
#if defined(GL_ANGLE_translated_shader_source)
	if ( Extensions::has(hash_GL_ANGLE_translated_shader_source) )
	{
		ret = is_define_angle_translated_shader_source(pname);
		if( ret ) return ret;
	}
#endif // GL_ANGLE_translated_shader_source
#if defined(GL_APPLE_clip_distance)
	if ( Extensions::has(hash_GL_APPLE_clip_distance) )
	{
		ret = is_define_apple_clip_distance(pname);
		if( ret ) return ret;
	}
#endif // GL_APPLE_clip_distance
#if defined(GL_APPLE_color_buffer_packed_float)
	if ( Extensions::has(hash_GL_APPLE_color_buffer_packed_float) )
	{
		ret = is_define_apple_color_buffer_packed_float(pname);
		if( ret ) return ret;
	}
#endif // GL_APPLE_color_buffer_packed_float
#if defined(GL_APPLE_copy_texture_levels)
	if ( Extensions::has(hash_GL_APPLE_copy_texture_levels) )
	{
		ret = is_define_apple_copy_texture_levels(pname);
		if( ret ) return ret;
	}
#endif // GL_APPLE_copy_texture_levels
#if defined(GL_APPLE_framebuffer_multisample)
	if ( Extensions::has(hash_GL_APPLE_framebuffer_multisample) )
	{
		ret = is_define_apple_framebuffer_multisample(pname);
		if( ret ) return ret;
	}
#endif // GL_APPLE_framebuffer_multisample
#if defined(GL_APPLE_rgb_422)
	if ( Extensions::has(hash_GL_APPLE_rgb_422) )
	{
		ret = is_define_apple_rgb_422(pname);
		if( ret ) return ret;
	}
#endif // GL_APPLE_rgb_422
#if defined(GL_APPLE_sync)
	if ( Extensions::has(hash_GL_APPLE_sync) )
	{
		ret = is_define_apple_sync(pname);
		if( ret ) return ret;
	}
#endif // GL_APPLE_sync
#if defined(GL_APPLE_texture_format_BGRA8888)
	if ( Extensions::has(hash_GL_APPLE_texture_format_BGRA8888) )
	{
		ret = is_define_apple_texture_format_bgra8888(pname);
		if( ret ) return ret;
	}
#endif // GL_APPLE_texture_format_BGRA8888
#if defined(GL_APPLE_texture_max_level)
	if ( Extensions::has(hash_GL_APPLE_texture_max_level) )
	{
		ret = is_define_apple_texture_max_level(pname);
		if( ret ) return ret;
	}
#endif // GL_APPLE_texture_max_level
#if defined(GL_APPLE_texture_packed_float)
	if ( Extensions::has(hash_GL_APPLE_texture_packed_float) )
	{
		ret = is_define_apple_texture_packed_float(pname);
		if( ret ) return ret;
	}
#endif // GL_APPLE_texture_packed_float
#if defined(GL_ARM_mali_program_binary)
	if ( Extensions::has(hash_GL_ARM_mali_program_binary) )
	{
		ret = is_define_arm_mali_program_binary(pname);
		if( ret ) return ret;
	}
#endif // GL_ARM_mali_program_binary
#if defined(GL_ARM_mali_shader_binary)
	if ( Extensions::has(hash_GL_ARM_mali_shader_binary) )
	{
		ret = is_define_arm_mali_shader_binary(pname);
		if( ret ) return ret;
	}
#endif // GL_ARM_mali_shader_binary
#if defined(GL_ARM_rgba8)
	if ( Extensions::has(hash_GL_ARM_rgba8) )
	{
		ret = is_define_arm_rgba8(pname);
		if( ret ) return ret;
	}
#endif // GL_ARM_rgba8
#if defined(GL_ARM_shader_framebuffer_fetch)
	if ( Extensions::has(hash_GL_ARM_shader_framebuffer_fetch) )
	{
		ret = is_define_arm_shader_framebuffer_fetch(pname);
		if( ret ) return ret;
	}
#endif // GL_ARM_shader_framebuffer_fetch
#if defined(GL_ARM_shader_framebuffer_fetch_depth_stencil)
	if ( Extensions::has(hash_GL_ARM_shader_framebuffer_fetch_depth_stencil) )
	{
		ret = is_define_arm_shader_framebuffer_fetch_depth_stencil(pname);
		if( ret ) return ret;
	}
#endif // GL_ARM_shader_framebuffer_fetch_depth_stencil
#if defined(GL_DMP_program_binary)
	if ( Extensions::has(hash_GL_DMP_program_binary) )
	{
		ret = is_define_dmp_program_binary(pname);
		if( ret ) return ret;
	}
#endif // GL_DMP_program_binary
#if defined(GL_DMP_shader_binary)
	if ( Extensions::has(hash_GL_DMP_shader_binary) )
	{
		ret = is_define_dmp_shader_binary(pname);
		if( ret ) return ret;
	}
#endif // GL_DMP_shader_binary
#if defined(GL_EXT_YUV_target)
	if ( Extensions::has(hash_GL_EXT_YUV_target) )
	{
		ret = is_define_ext_yuv_target(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_YUV_target
#if defined(GL_EXT_base_instance)
	if ( Extensions::has(hash_GL_EXT_base_instance) )
	{
		ret = is_define_ext_base_instance(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_base_instance
#if defined(GL_EXT_blend_func_extended)
	if ( Extensions::has(hash_GL_EXT_blend_func_extended) )
	{
		ret = is_define_ext_blend_func_extended(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_blend_func_extended
#if defined(GL_EXT_blend_minmax)
	if ( Extensions::has(hash_GL_EXT_blend_minmax) )
	{
		ret = is_define_ext_blend_minmax(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_blend_minmax
#if defined(GL_EXT_buffer_storage)
	if ( Extensions::has(hash_GL_EXT_buffer_storage) )
	{
		ret = is_define_ext_buffer_storage(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_buffer_storage
#if defined(GL_EXT_color_buffer_float)
	if ( Extensions::has(hash_GL_EXT_color_buffer_float) )
	{
		ret = is_define_ext_color_buffer_float(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_color_buffer_float
#if defined(GL_EXT_color_buffer_half_float)
	if ( Extensions::has(hash_GL_EXT_color_buffer_half_float) )
	{
		ret = is_define_ext_color_buffer_half_float(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_color_buffer_half_float
#if defined(GL_EXT_copy_image)
	if ( Extensions::has(hash_GL_EXT_copy_image) )
	{
		ret = is_define_ext_copy_image(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_copy_image
#if defined(GL_EXT_debug_label)
	if ( Extensions::has(hash_GL_EXT_debug_label) )
	{
		ret = is_define_ext_debug_label(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_debug_label
#if defined(GL_EXT_debug_marker)
	if ( Extensions::has(hash_GL_EXT_debug_marker) )
	{
		ret = is_define_ext_debug_marker(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_debug_marker
#if defined(GL_EXT_discard_framebuffer)
	if ( Extensions::has(hash_GL_EXT_discard_framebuffer) )
	{
		ret = is_define_ext_discard_framebuffer(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_discard_framebuffer
#if defined(GL_EXT_disjoint_timer_query)
	if ( Extensions::has(hash_GL_EXT_disjoint_timer_query) )
	{
		ret = is_define_ext_disjoint_timer_query(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_disjoint_timer_query
#if defined(GL_EXT_draw_buffers)
	if ( Extensions::has(hash_GL_EXT_draw_buffers) )
	{
		ret = is_define_ext_draw_buffers(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_draw_buffers
#if defined(GL_EXT_draw_buffers_indexed)
	if ( Extensions::has(hash_GL_EXT_draw_buffers_indexed) )
	{
		ret = is_define_ext_draw_buffers_indexed(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_draw_buffers_indexed
#if defined(GL_EXT_draw_elements_base_vertex)
	if ( Extensions::has(hash_GL_EXT_draw_elements_base_vertex) )
	{
		ret = is_define_ext_draw_elements_base_vertex(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_draw_elements_base_vertex
#if defined(GL_EXT_draw_instanced)
	if ( Extensions::has(hash_GL_EXT_draw_instanced) )
	{
		ret = is_define_ext_draw_instanced(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_draw_instanced
#if defined(GL_EXT_float_blend)
	if ( Extensions::has(hash_GL_EXT_float_blend) )
	{
		ret = is_define_ext_float_blend(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_float_blend
#if defined(GL_EXT_geometry_point_size)
	if ( Extensions::has(hash_GL_EXT_geometry_point_size) )
	{
		ret = is_define_ext_geometry_point_size(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_geometry_point_size
#if defined(GL_EXT_geometry_shader)
	if ( Extensions::has(hash_GL_EXT_geometry_shader) )
	{
		ret = is_define_ext_geometry_shader(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_geometry_shader
#if defined(GL_EXT_gpu_shader5)
	if ( Extensions::has(hash_GL_EXT_gpu_shader5) )
	{
		ret = is_define_ext_gpu_shader5(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_gpu_shader5
#if defined(GL_EXT_instanced_arrays)
	if ( Extensions::has(hash_GL_EXT_instanced_arrays) )
	{
		ret = is_define_ext_instanced_arrays(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_instanced_arrays
#if defined(GL_EXT_map_buffer_range)
	if ( Extensions::has(hash_GL_EXT_map_buffer_range) )
	{
		ret = is_define_ext_map_buffer_range(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_map_buffer_range
#if defined(GL_EXT_multi_draw_arrays)
	if ( Extensions::has(hash_GL_EXT_multi_draw_arrays) )
	{
		ret = is_define_ext_multi_draw_arrays(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_multi_draw_arrays
#if defined(GL_EXT_multi_draw_indirect)
	if ( Extensions::has(hash_GL_EXT_multi_draw_indirect) )
	{
		ret = is_define_ext_multi_draw_indirect(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_multi_draw_indirect
#if defined(GL_EXT_multisampled_compatibility)
	if ( Extensions::has(hash_GL_EXT_multisampled_compatibility) )
	{
		ret = is_define_ext_multisampled_compatibility(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_multisampled_compatibility
#if defined(GL_EXT_multisampled_render_to_texture)
	if ( Extensions::has(hash_GL_EXT_multisampled_render_to_texture) )
	{
		ret = is_define_ext_multisampled_render_to_texture(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_multisampled_render_to_texture
#if defined(GL_EXT_multiview_draw_buffers)
	if ( Extensions::has(hash_GL_EXT_multiview_draw_buffers) )
	{
		ret = is_define_ext_multiview_draw_buffers(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_multiview_draw_buffers
#if defined(GL_EXT_occlusion_query_boolean)
	if ( Extensions::has(hash_GL_EXT_occlusion_query_boolean) )
	{
		ret = is_define_ext_occlusion_query_boolean(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_occlusion_query_boolean
#if defined(GL_EXT_polygon_offset_clamp)
	if ( Extensions::has(hash_GL_EXT_polygon_offset_clamp) )
	{
		ret = is_define_ext_polygon_offset_clamp(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_polygon_offset_clamp
#if defined(GL_EXT_post_depth_coverage)
	if ( Extensions::has(hash_GL_EXT_post_depth_coverage) )
	{
		ret = is_define_ext_post_depth_coverage(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_post_depth_coverage
#if defined(GL_EXT_primitive_bounding_box)
	if ( Extensions::has(hash_GL_EXT_primitive_bounding_box) )
	{
		ret = is_define_ext_primitive_bounding_box(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_primitive_bounding_box
#if defined(GL_EXT_pvrtc_sRGB)
	if ( Extensions::has(hash_GL_EXT_pvrtc_sRGB) )
	{
		ret = is_define_ext_pvrtc_srgb(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_pvrtc_sRGB
#if defined(GL_EXT_raster_multisample)
	if ( Extensions::has(hash_GL_EXT_raster_multisample) )
	{
		ret = is_define_ext_raster_multisample(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_raster_multisample
#if defined(GL_EXT_read_format_bgra)
	if ( Extensions::has(hash_GL_EXT_read_format_bgra) )
	{
		ret = is_define_ext_read_format_bgra(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_read_format_bgra
#if defined(GL_EXT_render_snorm)
	if ( Extensions::has(hash_GL_EXT_render_snorm) )
	{
		ret = is_define_ext_render_snorm(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_render_snorm
#if defined(GL_EXT_robustness)
	if ( Extensions::has(hash_GL_EXT_robustness) )
	{
		ret = is_define_ext_robustness(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_robustness
#if defined(GL_EXT_sRGB)
	if ( Extensions::has(hash_GL_EXT_sRGB) )
	{
		ret = is_define_ext_srgb(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_sRGB
#if defined(GL_EXT_sRGB_write_control)
	if ( Extensions::has(hash_GL_EXT_sRGB_write_control) )
	{
		ret = is_define_ext_srgb_write_control(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_sRGB_write_control
#if defined(GL_EXT_separate_shader_objects)
	if ( Extensions::has(hash_GL_EXT_separate_shader_objects) )
	{
		ret = is_define_ext_separate_shader_objects(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_separate_shader_objects
#if defined(GL_EXT_shader_framebuffer_fetch)
	if ( Extensions::has(hash_GL_EXT_shader_framebuffer_fetch) )
	{
		ret = is_define_ext_shader_framebuffer_fetch(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_shader_framebuffer_fetch
#if defined(GL_EXT_shader_group_vote)
	if ( Extensions::has(hash_GL_EXT_shader_group_vote) )
	{
		ret = is_define_ext_shader_group_vote(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_shader_group_vote
#if defined(GL_EXT_shader_implicit_conversions)
	if ( Extensions::has(hash_GL_EXT_shader_implicit_conversions) )
	{
		ret = is_define_ext_shader_implicit_conversions(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_shader_implicit_conversions
#if defined(GL_EXT_shader_integer_mix)
	if ( Extensions::has(hash_GL_EXT_shader_integer_mix) )
	{
		ret = is_define_ext_shader_integer_mix(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_shader_integer_mix
#if defined(GL_EXT_shader_io_blocks)
	if ( Extensions::has(hash_GL_EXT_shader_io_blocks) )
	{
		ret = is_define_ext_shader_io_blocks(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_shader_io_blocks
#if defined(GL_EXT_shader_pixel_local_storage)
	if ( Extensions::has(hash_GL_EXT_shader_pixel_local_storage) )
	{
		ret = is_define_ext_shader_pixel_local_storage(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_shader_pixel_local_storage
#if defined(GL_EXT_shader_pixel_local_storage2)
	if ( Extensions::has(hash_GL_EXT_shader_pixel_local_storage2) )
	{
		ret = is_define_ext_shader_pixel_local_storage2(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_shader_pixel_local_storage2
#if defined(GL_EXT_shader_texture_lod)
	if ( Extensions::has(hash_GL_EXT_shader_texture_lod) )
	{
		ret = is_define_ext_shader_texture_lod(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_shader_texture_lod
#if defined(GL_EXT_shadow_samplers)
	if ( Extensions::has(hash_GL_EXT_shadow_samplers) )
	{
		ret = is_define_ext_shadow_samplers(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_shadow_samplers
#if defined(GL_EXT_sparse_texture)
	if ( Extensions::has(hash_GL_EXT_sparse_texture) )
	{
		ret = is_define_ext_sparse_texture(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_sparse_texture
#if defined(GL_EXT_tessellation_point_size)
	if ( Extensions::has(hash_GL_EXT_tessellation_point_size) )
	{
		ret = is_define_ext_tessellation_point_size(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_tessellation_point_size
#if defined(GL_EXT_tessellation_shader)
	if ( Extensions::has(hash_GL_EXT_tessellation_shader) )
	{
		ret = is_define_ext_tessellation_shader(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_tessellation_shader
#if defined(GL_EXT_texture_border_clamp)
	if ( Extensions::has(hash_GL_EXT_texture_border_clamp) )
	{
		ret = is_define_ext_texture_border_clamp(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_texture_border_clamp
#if defined(GL_EXT_texture_buffer)
	if ( Extensions::has(hash_GL_EXT_texture_buffer) )
	{
		ret = is_define_ext_texture_buffer(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_texture_buffer
#if defined(GL_EXT_texture_compression_dxt1)
	if ( Extensions::has(hash_GL_EXT_texture_compression_dxt1) )
	{
		ret = is_define_ext_texture_compression_dxt1(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_texture_compression_dxt1
#if defined(GL_EXT_texture_compression_s3tc)
	if ( Extensions::has(hash_GL_EXT_texture_compression_s3tc) )
	{
		ret = is_define_ext_texture_compression_s3tc(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_texture_compression_s3tc
#if defined(GL_EXT_texture_cube_map_array)
	if ( Extensions::has(hash_GL_EXT_texture_cube_map_array) )
	{
		ret = is_define_ext_texture_cube_map_array(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_texture_cube_map_array
#if defined(GL_EXT_texture_filter_anisotropic)
	if ( Extensions::has(hash_GL_EXT_texture_filter_anisotropic) )
	{
		ret = is_define_ext_texture_filter_anisotropic(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_texture_filter_anisotropic
#if defined(GL_EXT_texture_filter_minmax)
	if ( Extensions::has(hash_GL_EXT_texture_filter_minmax) )
	{
		ret = is_define_ext_texture_filter_minmax(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_texture_filter_minmax
#if defined(GL_EXT_texture_format_BGRA8888)
	if ( Extensions::has(hash_GL_EXT_texture_format_BGRA8888) )
	{
		ret = is_define_ext_texture_format_bgra8888(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_texture_format_BGRA8888
#if defined(GL_EXT_texture_norm16)
	if ( Extensions::has(hash_GL_EXT_texture_norm16) )
	{
		ret = is_define_ext_texture_norm16(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_texture_norm16
#if defined(GL_EXT_texture_rg)
	if ( Extensions::has(hash_GL_EXT_texture_rg) )
	{
		ret = is_define_ext_texture_rg(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_texture_rg
#if defined(GL_EXT_texture_sRGB_R8)
	if ( Extensions::has(hash_GL_EXT_texture_sRGB_R8) )
	{
		ret = is_define_ext_texture_srgb_r8(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_texture_sRGB_R8
#if defined(GL_EXT_texture_sRGB_RG8)
	if ( Extensions::has(hash_GL_EXT_texture_sRGB_RG8) )
	{
		ret = is_define_ext_texture_srgb_rg8(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_texture_sRGB_RG8
#if defined(GL_EXT_texture_sRGB_decode)
	if ( Extensions::has(hash_GL_EXT_texture_sRGB_decode) )
	{
		ret = is_define_ext_texture_srgb_decode(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_texture_sRGB_decode
#if defined(GL_EXT_texture_storage)
	if ( Extensions::has(hash_GL_EXT_texture_storage) )
	{
		ret = is_define_ext_texture_storage(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_texture_storage
#if defined(GL_EXT_texture_type_2_10_10_10_REV)
	if ( Extensions::has(hash_GL_EXT_texture_type_2_10_10_10_REV) )
	{
		ret = is_define_ext_texture_type_2_10_10_10_rev(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_texture_type_2_10_10_10_REV
#if defined(GL_EXT_texture_view)
	if ( Extensions::has(hash_GL_EXT_texture_view) )
	{
		ret = is_define_ext_texture_view(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_texture_view
#if defined(GL_EXT_unpack_subimage)
	if ( Extensions::has(hash_GL_EXT_unpack_subimage) )
	{
		ret = is_define_ext_unpack_subimage(pname);
		if( ret ) return ret;
	}
#endif // GL_EXT_unpack_subimage
#if defined(GL_FJ_shader_binary_GCCSO)
	if ( Extensions::has(hash_GL_FJ_shader_binary_GCCSO) )
	{
		ret = is_define_fj_shader_binary_gccso(pname);
		if( ret ) return ret;
	}
#endif // GL_FJ_shader_binary_GCCSO
#if defined(GL_IMG_framebuffer_downsample)
	if ( Extensions::has(hash_GL_IMG_framebuffer_downsample) )
	{
		ret = is_define_img_framebuffer_downsample(pname);
		if( ret ) return ret;
	}
#endif // GL_IMG_framebuffer_downsample
#if defined(GL_IMG_multisampled_render_to_texture)
	if ( Extensions::has(hash_GL_IMG_multisampled_render_to_texture) )
	{
		ret = is_define_img_multisampled_render_to_texture(pname);
		if( ret ) return ret;
	}
#endif // GL_IMG_multisampled_render_to_texture
#if defined(GL_IMG_program_binary)
	if ( Extensions::has(hash_GL_IMG_program_binary) )
	{
		ret = is_define_img_program_binary(pname);
		if( ret ) return ret;
	}
#endif // GL_IMG_program_binary
#if defined(GL_IMG_read_format)
	if ( Extensions::has(hash_GL_IMG_read_format) )
	{
		ret = is_define_img_read_format(pname);
		if( ret ) return ret;
	}
#endif // GL_IMG_read_format
#if defined(GL_IMG_shader_binary)
	if ( Extensions::has(hash_GL_IMG_shader_binary) )
	{
		ret = is_define_img_shader_binary(pname);
		if( ret ) return ret;
	}
#endif // GL_IMG_shader_binary
#if defined(GL_IMG_texture_compression_pvrtc)
	if ( Extensions::has(hash_GL_IMG_texture_compression_pvrtc) )
	{
		ret = is_define_img_texture_compression_pvrtc(pname);
		if( ret ) return ret;
	}
#endif // GL_IMG_texture_compression_pvrtc
#if defined(GL_IMG_texture_compression_pvrtc2)
	if ( Extensions::has(hash_GL_IMG_texture_compression_pvrtc2) )
	{
		ret = is_define_img_texture_compression_pvrtc2(pname);
		if( ret ) return ret;
	}
#endif // GL_IMG_texture_compression_pvrtc2
#if defined(GL_IMG_texture_filter_cubic)
	if ( Extensions::has(hash_GL_IMG_texture_filter_cubic) )
	{
		ret = is_define_img_texture_filter_cubic(pname);
		if( ret ) return ret;
	}
#endif // GL_IMG_texture_filter_cubic
#if defined(GL_INTEL_framebuffer_CMAA)
	if ( Extensions::has(hash_GL_INTEL_framebuffer_CMAA) )
	{
		ret = is_define_intel_framebuffer_cmaa(pname);
		if( ret ) return ret;
	}
#endif // GL_INTEL_framebuffer_CMAA
#if defined(GL_INTEL_performance_query)
	if ( Extensions::has(hash_GL_INTEL_performance_query) )
	{
		ret = is_define_intel_performance_query(pname);
		if( ret ) return ret;
	}
#endif // GL_INTEL_performance_query
#if defined(GL_NV_bindless_texture)
	if ( Extensions::has(hash_GL_NV_bindless_texture) )
	{
		ret = is_define_nv_bindless_texture(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_bindless_texture
#if defined(GL_NV_blend_equation_advanced)
	if ( Extensions::has(hash_GL_NV_blend_equation_advanced) )
	{
		ret = is_define_nv_blend_equation_advanced(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_blend_equation_advanced
#if defined(GL_NV_blend_equation_advanced_coherent)
	if ( Extensions::has(hash_GL_NV_blend_equation_advanced_coherent) )
	{
		ret = is_define_nv_blend_equation_advanced_coherent(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_blend_equation_advanced_coherent
#if defined(GL_NV_conditional_render)
	if ( Extensions::has(hash_GL_NV_conditional_render) )
	{
		ret = is_define_nv_conditional_render(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_conditional_render
#if defined(GL_NV_conservative_raster)
	if ( Extensions::has(hash_GL_NV_conservative_raster) )
	{
		ret = is_define_nv_conservative_raster(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_conservative_raster
#if defined(GL_NV_copy_buffer)
	if ( Extensions::has(hash_GL_NV_copy_buffer) )
	{
		ret = is_define_nv_copy_buffer(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_copy_buffer
#if defined(GL_NV_coverage_sample)
	if ( Extensions::has(hash_GL_NV_coverage_sample) )
	{
		ret = is_define_nv_coverage_sample(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_coverage_sample
#if defined(GL_NV_depth_nonlinear)
	if ( Extensions::has(hash_GL_NV_depth_nonlinear) )
	{
		ret = is_define_nv_depth_nonlinear(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_depth_nonlinear
#if defined(GL_NV_draw_buffers)
	if ( Extensions::has(hash_GL_NV_draw_buffers) )
	{
		ret = is_define_nv_draw_buffers(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_draw_buffers
#if defined(GL_NV_draw_instanced)
	if ( Extensions::has(hash_GL_NV_draw_instanced) )
	{
		ret = is_define_nv_draw_instanced(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_draw_instanced
#if defined(GL_NV_explicit_attrib_location)
	if ( Extensions::has(hash_GL_NV_explicit_attrib_location) )
	{
		ret = is_define_nv_explicit_attrib_location(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_explicit_attrib_location
#if defined(GL_NV_fbo_color_attachments)
	if ( Extensions::has(hash_GL_NV_fbo_color_attachments) )
	{
		ret = is_define_nv_fbo_color_attachments(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_fbo_color_attachments
#if defined(GL_NV_fence)
	if ( Extensions::has(hash_GL_NV_fence) )
	{
		ret = is_define_nv_fence(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_fence
#if defined(GL_NV_fill_rectangle)
	if ( Extensions::has(hash_GL_NV_fill_rectangle) )
	{
		ret = is_define_nv_fill_rectangle(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_fill_rectangle
#if defined(GL_NV_fragment_coverage_to_color)
	if ( Extensions::has(hash_GL_NV_fragment_coverage_to_color) )
	{
		ret = is_define_nv_fragment_coverage_to_color(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_fragment_coverage_to_color
#if defined(GL_NV_fragment_shader_interlock)
	if ( Extensions::has(hash_GL_NV_fragment_shader_interlock) )
	{
		ret = is_define_nv_fragment_shader_interlock(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_fragment_shader_interlock
#if defined(GL_NV_framebuffer_blit)
	if ( Extensions::has(hash_GL_NV_framebuffer_blit) )
	{
		ret = is_define_nv_framebuffer_blit(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_framebuffer_blit
#if defined(GL_NV_framebuffer_mixed_samples)
	if ( Extensions::has(hash_GL_NV_framebuffer_mixed_samples) )
	{
		ret = is_define_nv_framebuffer_mixed_samples(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_framebuffer_mixed_samples
#if defined(GL_NV_framebuffer_multisample)
	if ( Extensions::has(hash_GL_NV_framebuffer_multisample) )
	{
		ret = is_define_nv_framebuffer_multisample(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_framebuffer_multisample
#if defined(GL_NV_generate_mipmap_sRGB)
	if ( Extensions::has(hash_GL_NV_generate_mipmap_sRGB) )
	{
		ret = is_define_nv_generate_mipmap_srgb(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_generate_mipmap_sRGB
#if defined(GL_NV_geometry_shader_passthrough)
	if ( Extensions::has(hash_GL_NV_geometry_shader_passthrough) )
	{
		ret = is_define_nv_geometry_shader_passthrough(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_geometry_shader_passthrough
#if defined(GL_NV_image_formats)
	if ( Extensions::has(hash_GL_NV_image_formats) )
	{
		ret = is_define_nv_image_formats(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_image_formats
#if defined(GL_NV_instanced_arrays)
	if ( Extensions::has(hash_GL_NV_instanced_arrays) )
	{
		ret = is_define_nv_instanced_arrays(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_instanced_arrays
#if defined(GL_NV_internalformat_sample_query)
	if ( Extensions::has(hash_GL_NV_internalformat_sample_query) )
	{
		ret = is_define_nv_internalformat_sample_query(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_internalformat_sample_query
#if defined(GL_NV_non_square_matrices)
	if ( Extensions::has(hash_GL_NV_non_square_matrices) )
	{
		ret = is_define_nv_non_square_matrices(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_non_square_matrices
#if defined(GL_NV_path_rendering)
	if ( Extensions::has(hash_GL_NV_path_rendering) )
	{
		ret = is_define_nv_path_rendering(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_path_rendering
#if defined(GL_NV_path_rendering_shared_edge)
	if ( Extensions::has(hash_GL_NV_path_rendering_shared_edge) )
	{
		ret = is_define_nv_path_rendering_shared_edge(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_path_rendering_shared_edge
#if defined(GL_NV_polygon_mode)
	if ( Extensions::has(hash_GL_NV_polygon_mode) )
	{
		ret = is_define_nv_polygon_mode(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_polygon_mode
#if defined(GL_NV_read_buffer)
	if ( Extensions::has(hash_GL_NV_read_buffer) )
	{
		ret = is_define_nv_read_buffer(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_read_buffer
#if defined(GL_NV_read_buffer_front)
	if ( Extensions::has(hash_GL_NV_read_buffer_front) )
	{
		ret = is_define_nv_read_buffer_front(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_read_buffer_front
#if defined(GL_NV_read_depth)
	if ( Extensions::has(hash_GL_NV_read_depth) )
	{
		ret = is_define_nv_read_depth(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_read_depth
#if defined(GL_NV_read_depth_stencil)
	if ( Extensions::has(hash_GL_NV_read_depth_stencil) )
	{
		ret = is_define_nv_read_depth_stencil(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_read_depth_stencil
#if defined(GL_NV_read_stencil)
	if ( Extensions::has(hash_GL_NV_read_stencil) )
	{
		ret = is_define_nv_read_stencil(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_read_stencil
#if defined(GL_NV_sRGB_formats)
	if ( Extensions::has(hash_GL_NV_sRGB_formats) )
	{
		ret = is_define_nv_srgb_formats(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_sRGB_formats
#if defined(GL_NV_sample_locations)
	if ( Extensions::has(hash_GL_NV_sample_locations) )
	{
		ret = is_define_nv_sample_locations(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_sample_locations
#if defined(GL_NV_sample_mask_override_coverage)
	if ( Extensions::has(hash_GL_NV_sample_mask_override_coverage) )
	{
		ret = is_define_nv_sample_mask_override_coverage(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_sample_mask_override_coverage
#if defined(GL_NV_shader_noperspective_interpolation)
	if ( Extensions::has(hash_GL_NV_shader_noperspective_interpolation) )
	{
		ret = is_define_nv_shader_noperspective_interpolation(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_shader_noperspective_interpolation
#if defined(GL_NV_shadow_samplers_array)
	if ( Extensions::has(hash_GL_NV_shadow_samplers_array) )
	{
		ret = is_define_nv_shadow_samplers_array(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_shadow_samplers_array
#if defined(GL_NV_shadow_samplers_cube)
	if ( Extensions::has(hash_GL_NV_shadow_samplers_cube) )
	{
		ret = is_define_nv_shadow_samplers_cube(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_shadow_samplers_cube
#if defined(GL_NV_texture_border_clamp)
	if ( Extensions::has(hash_GL_NV_texture_border_clamp) )
	{
		ret = is_define_nv_texture_border_clamp(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_texture_border_clamp
#if defined(GL_NV_texture_compression_s3tc_update)
	if ( Extensions::has(hash_GL_NV_texture_compression_s3tc_update) )
	{
		ret = is_define_nv_texture_compression_s3tc_update(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_texture_compression_s3tc_update
#if defined(GL_NV_texture_npot_2D_mipmap)
	if ( Extensions::has(hash_GL_NV_texture_npot_2D_mipmap) )
	{
		ret = is_define_nv_texture_npot_2d_mipmap(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_texture_npot_2D_mipmap
#if defined(GL_NV_viewport_array)
	if ( Extensions::has(hash_GL_NV_viewport_array) )
	{
		ret = is_define_nv_viewport_array(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_viewport_array
#if defined(GL_NV_viewport_array2)
	if ( Extensions::has(hash_GL_NV_viewport_array2) )
	{
		ret = is_define_nv_viewport_array2(pname);
		if( ret ) return ret;
	}
#endif // GL_NV_viewport_array2
#if defined(GL_OVR_multiview)
	if ( Extensions::has(hash_GL_OVR_multiview) )
	{
		ret = is_define_ovr_multiview(pname);
		if( ret ) return ret;
	}
#endif // GL_OVR_multiview
#if defined(GL_OVR_multiview2)
	if ( Extensions::has(hash_GL_OVR_multiview2) )
	{
		ret = is_define_ovr_multiview2(pname);
		if( ret ) return ret;
	}
#endif // GL_OVR_multiview2
#if defined(GL_OVR_multiview_multisampled_render_to_texture)
	if ( Extensions::has(hash_GL_OVR_multiview_multisampled_render_to_texture) )
	{
		ret = is_define_ovr_multiview_multisampled_render_to_texture(pname);
		if( ret ) return ret;
	}
#endif // GL_OVR_multiview_multisampled_render_to_texture
#if defined(GL_QCOM_alpha_test)
	if ( Extensions::has(hash_GL_QCOM_alpha_test) )
	{
		ret = is_define_qcom_alpha_test(pname);
		if( ret ) return ret;
	}
#endif // GL_QCOM_alpha_test
#if defined(GL_QCOM_binning_control)
	if ( Extensions::has(hash_GL_QCOM_binning_control) )
	{
		ret = is_define_qcom_binning_control(pname);
		if( ret ) return ret;
	}
#endif // GL_QCOM_binning_control
#if defined(GL_QCOM_driver_control)
	if ( Extensions::has(hash_GL_QCOM_driver_control) )
	{
		ret = is_define_qcom_driver_control(pname);
		if( ret ) return ret;
	}
#endif // GL_QCOM_driver_control
#if defined(GL_QCOM_extended_get)
	if ( Extensions::has(hash_GL_QCOM_extended_get) )
	{
		ret = is_define_qcom_extended_get(pname);
		if( ret ) return ret;
	}
#endif // GL_QCOM_extended_get
#if defined(GL_QCOM_extended_get2)
	if ( Extensions::has(hash_GL_QCOM_extended_get2) )
	{
		ret = is_define_qcom_extended_get2(pname);
		if( ret ) return ret;
	}
#endif // GL_QCOM_extended_get2
#if defined(GL_QCOM_perfmon_global_mode)
	if ( Extensions::has(hash_GL_QCOM_perfmon_global_mode) )
	{
		ret = is_define_qcom_perfmon_global_mode(pname);
		if( ret ) return ret;
	}
#endif // GL_QCOM_perfmon_global_mode
#if defined(GL_QCOM_tiled_rendering)
	if ( Extensions::has(hash_GL_QCOM_tiled_rendering) )
	{
		ret = is_define_qcom_tiled_rendering(pname);
		if( ret ) return ret;
	}
#endif // GL_QCOM_tiled_rendering
#if defined(GL_QCOM_writeonly_rendering)
	if ( Extensions::has(hash_GL_QCOM_writeonly_rendering) )
	{
		ret = is_define_qcom_writeonly_rendering(pname);
		if( ret ) return ret;
	}
#endif // GL_QCOM_writeonly_rendering
#if defined(GL_VIV_shader_binary)
	if ( Extensions::has(hash_GL_VIV_shader_binary) )
	{
		ret = is_define_viv_shader_binary(pname);
		if( ret ) return ret;
	}
#endif // GL_VIV_shader_binary

	/** nothing found */
	return nullptr;
}

PFNGLGETINTEGERVPROC gl::gl_GetIntegerv INIT_POINTER;
void gl::GetIntegerv  (GLenum pname, GLint *data, const char* file, int line)
{
	TRACE_FUNCTION("glGetIntegerv(...) called from " << 
		get_path(file) << '(' << line << ')');

	const char* sPname = is_allowed_enum_get_function(pname);

	breakOnError(MakeBool(sPname), "pname : Invalid enum" );

	breakOnError(MakeBool(data), "data : Invalid pointer" );

	/** send it to opengl */
	gl_GetIntegerv(pname,data);

	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format("GetIntegerv( pname:%s, data:%d )",
		sPname, *data // can be an array of data...
		),
		file, line
	);

	breakOnError( !result, result );
}

PFNGLGETPROGRAMINFOLOGPROC gl::gl_GetProgramInfoLog INIT_POINTER;
void gl::GetProgramInfoLog  (GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog, const char* file, int line)
{
	TRACE_FUNCTION("glGetProgramInfoLog(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_GetProgramInfoLog( program, bufSize, length, infoLog );
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLGETPROGRAMIVPROC gl::gl_GetProgramiv INIT_POINTER;
void gl::GetProgramiv  (GLuint program, GLenum pname, GLint *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetProgramiv(...) called from " << 
		get_path(file) << '(' << line << ')');

	const bool is_valid = is_registered_program(program);

	breakOnError(is_valid, "Invalid program");

	gl_GetProgramiv(program,pname,params);

	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format("glGetProgramiv( program:%d pname:%s )", program, 
			getDefineName(pname) ),
		file, line
	);

	breakOnError( !result, result );
}

PFNGLGETRENDERBUFFERPARAMETERIVPROC gl::gl_GetRenderbufferParameteriv INIT_POINTER;
void gl::GetRenderbufferParameteriv  (GLenum target, GLenum pname, GLint *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetRenderbufferParameteriv(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_GetRenderbufferParameteriv(target,pname,params);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLGETSHADERINFOLOGPROC gl::gl_GetShaderInfoLog INIT_POINTER;
void gl::GetShaderInfoLog  (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog, const char* file, int line)
{
	TRACE_FUNCTION("glGetShaderInfoLog(...) called from " << 
		get_path(file) << '(' << line << ')');
	gl_GetShaderInfoLog(
		shader,
		bufSize,
		length,
		infoLog);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLGETSHADERPRECISIONFORMATPROC gl::gl_GetShaderPrecisionFormat INIT_POINTER;
void gl::GetShaderPrecisionFormat  (GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision, const char* file, int line)
{
	TRACE_FUNCTION("glGetShaderPrecisionFormat(...) called from " << 
		get_path(file) << '(' << line << ')');
	gl_GetShaderPrecisionFormat(
		shadertype,
		precisiontype,
		range,
		precision);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLGETSHADERSOURCEPROC gl::gl_GetShaderSource INIT_POINTER;
void gl::GetShaderSource  (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source, const char* file, int line)
{
	TRACE_FUNCTION("glGetShaderSource(...) called from " << 
		get_path(file) << '(' << line << ')');
	gl_GetShaderSource(
		shader,
		bufSize,
		length,
		source);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLGETSHADERIVPROC gl::gl_GetShaderiv INIT_POINTER;
void gl::GetShaderiv  (GLuint shader, GLenum pname, GLint *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetShaderiv(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_GetShaderiv(shader,pname,params);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLGETSTRINGPROC gl::gl_GetString INIT_POINTER;
const GLubyte* gl::GetString (GLenum name, const char* file, int line)
{
	TRACE_FUNCTION("glGetString(...) called from " << 
		get_path(file) << '(' << line << ')');

	const GLubyte* r = gl_GetString(name);
	
	const char* result = get_last_error();
	breakOnError( !result, result );

	return r;
}

PFNGLGETTEXPARAMETERFVPROC gl::gl_GetTexParameterfv INIT_POINTER;
void gl::GetTexParameterfv  (GLenum target, GLenum pname, GLfloat *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetTexParameterfv(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_GetTexParameterfv(target,pname,params);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLGETTEXPARAMETERIVPROC gl::gl_GetTexParameteriv INIT_POINTER;
void gl::GetTexParameteriv  (GLenum target, GLenum pname, GLint *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetTexParameteriv(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_GetTexParameteriv(target,pname,params);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLGETUNIFORMLOCATIONPROC gl::gl_GetUniformLocation INIT_POINTER;
GLint gl::GetUniformLocation  (GLuint program, const GLchar *name, const char* file, int line)
{
	TRACE_FUNCTION("glGetUniformLocation(...) called from " << 
		get_path(file) << '(' << line << ')');

	bool is_valid = is_registered_program(program);

	breakOnError(is_valid, "Invalid program");

	GLint r = gl_GetUniformLocation(program,name);

	const char* result = get_last_error();

	// add function to call list
	addCall(result,
	 format("glGetUniformLocation( program:%d, name:%s )",program,name),
	 file, line
	);

	breakOnError( !result, result );

	return r;
}

PFNGLGETUNIFORMFVPROC gl::gl_GetUniformfv INIT_POINTER;
void gl::GetUniformfv  (GLuint program, GLint location, GLfloat *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetUniformfv(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_GetUniformfv(program,location,params);

	const char* result = get_last_error();

	// add function to call list
	addCall(result,
	 format("glGetUniformfv( program:%d, location:%d params[0]:%d )",
	 program,location,*params),
	 file, line
	);

	breakOnError( !result, result );
}

PFNGLGETUNIFORMIVPROC gl::gl_GetUniformiv INIT_POINTER;
void gl::GetUniformiv  (GLuint program, GLint location, GLint *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetUniformiv(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_GetUniformiv( program, location, params );
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLGETVERTEXATTRIBPOINTERVPROC gl::gl_GetVertexAttribPointerv INIT_POINTER;
void gl::GetVertexAttribPointerv  (GLuint index, GLenum pname, void **pointer, const char* file, int line)
{
	TRACE_FUNCTION("glGetVertexAttribPointerv(...) called from " <<
		get_path(file) << '(' << line << ')');

	gl_GetVertexAttribPointerv( index, pname, pointer );

	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLGETVERTEXATTRIBFVPROC gl::gl_GetVertexAttribfv INIT_POINTER;
void gl::GetVertexAttribfv  (GLuint index, GLenum pname, GLfloat *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetVertexAttribfv(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_GetVertexAttribfv( index, pname, params );

	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLGETVERTEXATTRIBIVPROC gl::gl_GetVertexAttribiv INIT_POINTER;
void gl::GetVertexAttribiv  (GLuint index, GLenum pname, GLint *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetVertexAttribiv(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_GetVertexAttribiv( index, pname, params );

	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLHINTPROC gl::gl_Hint INIT_POINTER;
void gl::Hint  (GLenum target, GLenum mode, const char* file, int line)
{
	TRACE_FUNCTION("glHint(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_Hint( target, mode );

	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLISBUFFERPROC gl::gl_IsBuffer INIT_POINTER;
GLboolean gl::IsBuffer  (GLuint buffer, const char* file, int line)
{
	TRACE_FUNCTION("glIsBuffer(...) called from " << 
		get_path(file) << '(' << line << ')');

	GLboolean r = gl_IsBuffer(buffer);

	const char* result = get_last_error();
	breakOnError( !result, result );
	
	return r;
}

PFNGLISENABLEDPROC gl::gl_IsEnabled INIT_POINTER;
GLboolean gl::IsEnabled  (GLenum cap, const char* file, int line)
{
	TRACE_FUNCTION("glIsEnabled(...) called from " << 
		get_path(file) << '(' << line << ')');

	const char* sCap;

	if      ( cap == GL_BLEND )						sCap = "GL_BLEND";
	else if ( cap == GL_CULL_FACE )					sCap = "GL_CULL_FACE";
	else if ( cap == GL_DEPTH_TEST )				sCap = "GL_DEPTH_TEST";
	else if ( cap == GL_DITHER )					sCap = "GL_DITHER";
	else if ( cap == GL_POLYGON_OFFSET_FILL )		sCap = "GL_POLYGON_OFFSET_FILL";
	else if ( cap == GL_SAMPLE_ALPHA_TO_COVERAGE )	sCap = "GL_SAMPLE_ALPHA_TO_COVERAGE";
	else if ( cap == GL_SAMPLE_COVERAGE )			sCap = "GL_SAMPLE_COVERAGE";
	else if ( cap == GL_SCISSOR_TEST )				sCap = "GL_SCISSOR_TEST";
	else if ( cap == GL_STENCIL_TEST )				sCap = "GL_STENCIL_TEST";
	else
	{
		sCap = "";
		breakOnError( 0,
			"cap : Invalid enum, must be one of :"
			"GL_BLEND, GL_CULL_FACE, GL_DEPTH_TEST, GL_DITHER, "
			"GL_POLYGON_OFFSET_FILL, GL_SAMPLE_ALPHA_TO_COVERAGE, "
			"GL_SAMPLE_COVERAGE, GL_SCISSOR_TEST or GL_STENCIL_TEST"
		);
	}

	/** send it to opengl */
	GLboolean r = gl_IsEnabled(cap);

	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format("glIsEnabled( cap:%s, ret: %s )",
		sCap, r == GL_TRUE ? "GL_TRUE" : "GL_FALSE"
		),
		file, line
	);

	breakOnError( !result, result );

	return r;
}

PFNGLISFRAMEBUFFERPROC gl::gl_IsFramebuffer INIT_POINTER;
GLboolean gl::IsFramebuffer  (GLuint framebuffer, const char* file, int line)
{
	TRACE_FUNCTION("glIsFramebuffer(...) called from " << 
		get_path(file) << '(' << line << ')');

	GLboolean r = gl_IsFramebuffer(framebuffer);

	const char* result = get_last_error();
	breakOnError( !result, result );

	return r;
}

PFNGLISPROGRAMPROC gl::gl_IsProgram INIT_POINTER;
GLboolean gl::IsProgram  (GLuint program, const char* file, int line)
{
	TRACE_FUNCTION("glIsProgram(...) called from " << 
		get_path(file) << '(' << line << ')');

	GLboolean r = gl_IsProgram(program);

	const char* result = get_last_error();
	breakOnError( !result, result );

	return r;
}

PFNGLISRENDERBUFFERPROC gl::gl_IsRenderbuffer INIT_POINTER;
GLboolean gl::IsRenderbuffer  (GLuint renderbuffer, const char* file, int line)
{
	TRACE_FUNCTION("glIsRenderbuffer(...) called from " << 
		get_path(file) << '(' << line << ')');

	GLboolean r = gl_IsRenderbuffer(renderbuffer);
	
	const char* result = get_last_error();
	breakOnError( !result, result );

	return r;
}

PFNGLISSHADERPROC gl::gl_IsShader INIT_POINTER;
GLboolean gl::IsShader  (GLuint shader, const char* file, int line)
{
	TRACE_FUNCTION("glIsShader(...) called from " <<
		get_path(file) << '(' << line << ')');

	GLboolean r = gl_IsShader(shader);

	const char* result = get_last_error();
	breakOnError( !result, result );

	return r;
}

PFNGLISTEXTUREPROC gl::gl_IsTexture INIT_POINTER;
GLboolean gl::IsTexture  (GLuint texture, const char* file, int line)
{
	TRACE_FUNCTION("glIsTexture(...) called from " << 
		get_path(file) << '(' << line << ')');

	GLboolean r = gl_IsTexture(texture);
	
	const char* result = get_last_error();
	breakOnError( !result, result );

	return r;
}

PFNGLLINEWIDTHPROC gl::gl_LineWidth INIT_POINTER;
void gl::LineWidth  (GLfloat width, const char* file, int line)
{
	TRACE_FUNCTION("glLineWidth(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_LineWidth(width);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLLINKPROGRAMPROC gl::gl_LinkProgram INIT_POINTER;
void gl::LinkProgram  (GLuint program, const char* file, int line)
{
	TRACE_FUNCTION("glLinkProgram(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_LinkProgram(program);
	
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLPIXELSTOREIPROC gl::gl_PixelStorei INIT_POINTER;
void gl::PixelStorei  (GLenum pname, GLint param, const char* file, int line)
{
	TRACE_FUNCTION("glPixelStorei(...) called from " << 
		get_path(file) << '(' << line << ')');

	breakOnError(
		(pname == GL_PACK_ALIGNMENT || pname == GL_UNPACK_ALIGNMENT),
		"pname : Invalid enum"
	);

	breakOnError(
		(param == 1 || param == 2 || param == 4 || param == 8),
		"param: Invalid size must be 1,2,4 or 8"
	);

	gl_PixelStorei(	pname, param );

	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format("glPixelStorei( pname:%s param:%d )",
		pname == GL_PACK_ALIGNMENT ?
			"GL_PACK_ALIGNMENT":"GL_UNPACK_ALIGNMENT",
		param),
		file, line
	);

	breakOnError( !result, result );
}

PFNGLPOLYGONOFFSETPROC gl::gl_PolygonOffset INIT_POINTER;
void gl::PolygonOffset  (GLfloat factor, GLfloat units, const char* file, int line)
{
	TRACE_FUNCTION("glPolygonOffset(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_PolygonOffset( factor, units );
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLREADPIXELSPROC gl::gl_ReadPixels INIT_POINTER;
void gl::ReadPixels  (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels, const char* file, int line)
{
	TRACE_FUNCTION("glReadPixels(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_ReadPixels(x,y,width,height,format,type,pixels);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLRELEASESHADERCOMPILERPROC gl::gl_ReleaseShaderCompiler INIT_POINTER;
void gl::ReleaseShaderCompiler  (const char* file, int line)
{
	TRACE_FUNCTION("glReleaseShaderCompiler(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_ReleaseShaderCompiler();
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLRENDERBUFFERSTORAGEPROC gl::gl_RenderbufferStorage INIT_POINTER;
void gl::RenderbufferStorage  (GLenum target, GLenum internalformat, GLsizei width, GLsizei height, const char* file, int line)
{
	TRACE_FUNCTION("glRenderbufferStorage(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_RenderbufferStorage(target,internalformat,width,height);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLSAMPLECOVERAGEPROC gl::gl_SampleCoverage INIT_POINTER;
void gl::SampleCoverage  (GLfloat value, GLboolean invert, const char* file, int line)
{
	TRACE_FUNCTION("glSampleCoverage(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_SampleCoverage(value,invert);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLSCISSORPROC gl::gl_Scissor INIT_POINTER;
void gl::Scissor  (GLint x, GLint y, GLsizei width, GLsizei height, const char* file, int line)
{
	TRACE_FUNCTION("glScissor(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_Scissor(x,y,width,height);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLSHADERBINARYPROC gl::gl_ShaderBinary INIT_POINTER;
void gl::ShaderBinary  (GLsizei count, const GLuint *shaders, GLenum binaryformat, const void *binary, GLsizei length, const char* file, int line)
{
	TRACE_FUNCTION("glShaderBinary(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_ShaderBinary(count,shaders,binaryformat,binary,length);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLSHADERSOURCEPROC gl::gl_ShaderSource INIT_POINTER;
void gl::ShaderSource  (GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length, const char* file, int line)
{
	TRACE_FUNCTION("glShaderSource(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_ShaderSource(shader,count,string,length);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLSTENCILFUNCPROC gl::gl_StencilFunc INIT_POINTER;
void gl::StencilFunc  (GLenum func, GLint ref, GLuint mask, const char* file, int line)
{
	TRACE_FUNCTION("glStencilFunc(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_StencilFunc(func,ref,mask);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLSTENCILFUNCSEPARATEPROC gl::gl_StencilFuncSeparate INIT_POINTER;
void gl::StencilFuncSeparate  (GLenum face, GLenum func, GLint ref, GLuint mask, const char* file, int line)
{
	TRACE_FUNCTION("glStencilFuncSeparate(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_StencilFuncSeparate(face,func,ref,mask);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLSTENCILMASKPROC gl::gl_StencilMask INIT_POINTER;
void gl::StencilMask  (GLuint mask, const char* file, int line)
{
	TRACE_FUNCTION("glStencilMask(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_StencilMask(mask);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLSTENCILMASKSEPARATEPROC gl::gl_StencilMaskSeparate INIT_POINTER;
void gl::StencilMaskSeparate  (GLenum face, GLuint mask, const char* file, int line)
{
	TRACE_FUNCTION("glStencilMaskSeparate(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_StencilMaskSeparate(face,mask);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLSTENCILOPPROC gl::gl_StencilOp INIT_POINTER;
void gl::StencilOp  (GLenum fail, GLenum zfail, GLenum zpass, const char* file, int line)
{
	TRACE_FUNCTION("glStencilOp(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_StencilOp(fail,zfail,zpass);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLSTENCILOPSEPARATEPROC gl::gl_StencilOpSeparate INIT_POINTER;
void gl::StencilOpSeparate  (GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass, const char* file, int line)
{
	TRACE_FUNCTION("glStencilOpSeparate(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_StencilOpSeparate(face,sfail,dpfail,dppass);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLTEXIMAGE2DPROC gl::gl_TexImage2D INIT_POINTER;
void gl::TexImage2D  (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels, const char* file, int line)
{
	TRACE_FUNCTION("glTexImage2D(...) called from " << 
		get_path(file) << '(' << line << ')');

	breakOnError(
		(target == GL_TEXTURE_2D ||
		target == GL_TEXTURE_CUBE_MAP_POSITIVE_X ||
		target == GL_TEXTURE_CUBE_MAP_NEGATIVE_X ||
		target == GL_TEXTURE_CUBE_MAP_POSITIVE_Y ||
		target == GL_TEXTURE_CUBE_MAP_NEGATIVE_Y ||
		target == GL_TEXTURE_CUBE_MAP_POSITIVE_Z ||
		target == GL_TEXTURE_CUBE_MAP_NEGATIVE_Z),
		"target: Invalid enum"
	);
	breakOnError(	
		// uncompressed format
		(internalformat == GL_ALPHA ||
		internalformat == GL_LUMINANCE ||
		internalformat == GL_LUMINANCE_ALPHA ||
		internalformat == GL_RGB ||
		internalformat == GL_RGBA ||
		// compressed format
		internalformat == GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE
		// todo: other format
		),
		"internalformat : Invalid format"
	);
	// todo: to clarify : minimum "texels"
	/*
	 * cube map : width >= 16 || height >= 16
	 * 2d :  width >= 64 || height >= 64
	 */

	breakOnError(
		 (border == 0), "border : Invalid, must be zero"
	);

	breakOnError(
		(format == GL_ALPHA ||
		format == GL_RGB ||
		format == GL_RGBA ||
		format == GL_LUMINANCE ||
		format == GL_LUMINANCE_ALPHA),
		"format : Invalid enum"
	);

	breakOnError(
		(type == GL_UNSIGNED_BYTE ||
		type == GL_UNSIGNED_SHORT_5_6_5 ||
		type == GL_UNSIGNED_SHORT_4_4_4_4 ||
		type == GL_UNSIGNED_SHORT_5_5_5_1),
		"type : Invalid enum"
	);

	/** send it to opengl */
	gl_TexImage2D(target,level,internalformat,width,height,border,format,
		type,pixels);

	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		::format(
			"glTexImage2D( target:%s, level:%d, internalformat:%s, "
			"width:%d, height:%d, border:%d, "
			"format:%s, type:%s, pixels:%s )",
			getDefineName(target),
			level,getDefineName(internalformat),
			width,height,border,getDefineName(format),
			getDefineName(type),
			(pixels ? "not null" : "null")
		),
		file, line
	);

	breakOnError( !result, result );
}

PFNGLTEXPARAMETERFPROC gl::gl_TexParameterf INIT_POINTER;
void gl::TexParameterf  (GLenum target, GLenum pname, GLfloat param, const char* file, int line)
{
	TRACE_FUNCTION("glTexParameterf(...) called from " << get_path(file) << '(' << line << ')');
	
	breakOnError(
		(target == GL_TEXTURE_2D || target == GL_TEXTURE_CUBE_MAP),
		"target: Invalid enum must be one of : "
		"GL_TEXTURE_2D or GL_TEXTURE_CUBE_MAP"
	);
	breakOnError(
		(pname == GL_TEXTURE_MIN_FILTER || 
		pname == GL_TEXTURE_MAG_FILTER || 
		pname == GL_TEXTURE_WRAP_S || 
		pname == GL_TEXTURE_WRAP_T),
		"pname : Invalid enum, must be one of "
		"GL_TEXTURE_MIN_FILTER, GL_TEXTURE_MAG_FILTER, GL_TEXTURE_WRAP_S"
		" or GL_TEXTURE_WRAP_T"
	);

	/** send it to opengl */
	gl_TexParameterf(target,pname,param);

	/** check for allowed value */
	String sPname, sParam;

	switch(pname)
	{
		case GL_TEXTURE_MIN_FILTER:
			sPname = "GL_TEXTURE_MIN_FILTER";
			breakOnError(
				(param == GL_NEAREST ||
				param == GL_LINEAR ||
				param == GL_NEAREST_MIPMAP_NEAREST ||
				param == GL_LINEAR_MIPMAP_NEAREST ||
				param == GL_NEAREST_MIPMAP_LINEAR ||
				param == GL_LINEAR_MIPMAP_LINEAR),
				"param : Invalid enum must be one of : "
				"GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, "
				"GL_LINEAR_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR or "
				"GL_LINEAR_MIPMAP_LINEAR"
			);
			if ( param == GL_NEAREST )
				sParam = "GL_NEAREST";
			else if ( param == GL_LINEAR )
				sParam = "GL_LINEAR";
			else if ( param == GL_NEAREST_MIPMAP_NEAREST )
				sParam = "GL_NEAREST_MIPMAP_NEAREST";
			else if ( param == GL_LINEAR_MIPMAP_NEAREST )
				sParam = "GL_LINEAR_MIPMAP_NEAREST";
			else if ( param == GL_NEAREST_MIPMAP_LINEAR )
				sParam = "GL_NEAREST_MIPMAP_LINEAR";
			else if ( param == GL_LINEAR_MIPMAP_LINEAR )
				sParam = "GL_LINEAR_MIPMAP_LINEAR";
		break;

		case GL_TEXTURE_MAG_FILTER:
			sPname = "GL_TEXTURE_MAG_FILTER";
			breakOnError(
				param == GL_NEAREST ||
				param == GL_LINEAR,
				"param : Invalid enum, must be one of : "
				"GL_NEAREST or GL_LINEAR"
			);
			sParam = param == GL_NEAREST ? "GL_NEAREST":"GL_LINEAR";
		break;

		case GL_TEXTURE_WRAP_S:
		case GL_TEXTURE_WRAP_T:
			sPname = pname == GL_TEXTURE_WRAP_S  ?
				"GL_TEXTURE_WRAP_S":"GL_TEXTURE_WRAP_T";

			breakOnError(
				(param == GL_CLAMP_TO_EDGE ||
				param == GL_MIRRORED_REPEAT ||
				param == GL_REPEAT),
				"param : Invalid enum, must be one of : "
				"GL_CLAMP_TO_EDGE, GL_MIRRORED_REPEAT or GL_REPEAT"
			);

			if (param==GL_CLAMP_TO_EDGE)
				sParam = "GL_CLAMP_TO_EDGE";
			else if (param==GL_MIRRORED_REPEAT)
				sParam = "GL_MIRRORED_REPEAT";
			else if (param==GL_REPEAT)
				sParam = "GL_REPEAT";
		break;

		case GL_TEXTURE_MAX_ANISOTROPY_EXT:
			sPname = "GL_TEXTURE_MAX_ANISOTROPY_EXT";
			breakOnError(
				(param == 1 ||
				param == 2 ||
				param == 4 || 
				param == 8),
				"param : Invalid size, must be one of : 1,2,4 or 8"
			);
			sParam = toString(param);
		break;
	}

	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format("gl_TexParameterf( target:%s pname:%s param:%s )",
		target == GL_TEXTURE_2D ? "GL_TEXTURE_2D" : "GL_TEXTURE_CUBE_MAP",
		sPname, sParam),
		file, line
	);

	breakOnError( !result, result );
}

PFNGLTEXPARAMETERFVPROC gl::gl_TexParameterfv INIT_POINTER;
void gl::TexParameterfv  (GLenum target, GLenum pname, const GLfloat *params, const char* file, int line)
{
	TRACE_FUNCTION("glTexParameterfv(...) called from " << get_path(file) << '(' << line << ')');

	breakOnError(
		(target == GL_TEXTURE_2D || target == GL_TEXTURE_CUBE_MAP) ||
		(pname == GL_TEXTURE_MIN_FILTER || 
			pname == GL_TEXTURE_MAG_FILTER || 
			pname == GL_TEXTURE_WRAP_S || 
			pname == GL_TEXTURE_WRAP_T)
	);

	gl_TexParameterfv(target,pname,params);

	/** check for allowed value */
	//String sPname("todo"), sParam("todo");
	const char* sPname = "todo", *sParam = "todo";

	// check for allowed value
	/*switch(pname)
	{
		case GL_TEXTURE_MIN_FILTER:
			sPname = "GL_TEXTURE_MIN_FILTER";
			breakOnError(
				(param == GL_NEAREST ||
				param == GL_LINEAR ||
				param == GL_NEAREST_MIPMAP_NEAREST ||
				param == GL_LINEAR_MIPMAP_NEAREST ||
				param == GL_NEAREST_MIPMAP_LINEAR ||
				param == GL_LINEAR_MIPMAP_LINEAR),
				"param : Invalid enum must be one of : "
				"GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, "
				"GL_LINEAR_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR or "
				"GL_LINEAR_MIPMAP_LINEAR"
			);
			if ( param == GL_NEAREST )
				sParam = "GL_NEAREST";
			else if ( param == GL_LINEAR )
				sParam = "GL_LINEAR";
			else if ( param == GL_NEAREST_MIPMAP_NEAREST )
				sParam = "GL_NEAREST_MIPMAP_NEAREST";
			else if ( param == GL_LINEAR_MIPMAP_NEAREST )
				sParam = "GL_LINEAR_MIPMAP_NEAREST";
			else if ( param == GL_NEAREST_MIPMAP_LINEAR )
				sParam = "GL_NEAREST_MIPMAP_LINEAR";
			else if ( param == GL_LINEAR_MIPMAP_LINEAR )
				sParam = "GL_LINEAR_MIPMAP_LINEAR";
		break;

		case GL_TEXTURE_MAG_FILTER:
			sPname = "GL_TEXTURE_MAG_FILTER";
			breakOnError(
				param == GL_NEAREST ||
				param == GL_LINEAR,
				"param : Invalid enum, must be one of : "
				"GL_NEAREST or GL_LINEAR"
			);
			sParam = param == GL_NEAREST ? "GL_NEAREST":"GL_LINEAR";
		break;

		case GL_TEXTURE_WRAP_S:
		case GL_TEXTURE_WRAP_T:
			sPname = pname == GL_TEXTURE_WRAP_S  ?
				"GL_TEXTURE_WRAP_S":"GL_TEXTURE_WRAP_T";

			breakOnError(
				(param == GL_CLAMP_TO_EDGE ||
				param == GL_MIRRORED_REPEAT ||
				param == GL_REPEAT),
				"param : Invalid enum, must be one of : "
				"GL_CLAMP_TO_EDGE, GL_MIRRORED_REPEAT or GL_REPEAT"
			);

			if (param==GL_CLAMP_TO_EDGE)
				sParam = "GL_CLAMP_TO_EDGE";
			else if (param==GL_MIRRORED_REPEAT)
				sParam = "GL_MIRRORED_REPEAT";
			else if (param==GL_REPEAT)
				sParam = "GL_REPEAT";
		break;

		case GL_TEXTURE_MAX_ANISOTROPY_EXT:
			sPname = "GL_TEXTURE_MAX_ANISOTROPY_EXT";
			breakOnError(
				(param == 1 ||
				param == 2 ||
				param == 4 || 
				param == 8),
				"param : Invalid size, must be one of : 1,2,4 or 8"
			);
			sParam = String(param);
		break;
	}*/

	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format("glTexParameterf( target:%s pname:%s param:%s )",
		target == GL_TEXTURE_2D ? "GL_TEXTURE_2D" : "GL_TEXTURE_CUBE_MAP",
		sPname, sParam),
		file, line
	);

	breakOnError( !result, result );
}

PFNGLTEXPARAMETERIPROC gl::gl_TexParameteri INIT_POINTER;
void gl::TexParameteri  (GLenum target, GLenum pname, GLint param, const char* file, int line)
{
	TRACE_FUNCTION("glTexParameteri(...) called from " << get_path(file) << '(' << line << ')');
	
	breakOnError(
		(target == GL_TEXTURE_2D || target == GL_TEXTURE_CUBE_MAP) ||
		(pname == GL_TEXTURE_MIN_FILTER || 
			pname == GL_TEXTURE_MAG_FILTER || 
			pname == GL_TEXTURE_WRAP_S || 
			pname == GL_TEXTURE_WRAP_T)
	);

	gl_TexParameteri(target,pname,param);

	String sPname, sParam;

	/** check for allowed value */
	switch(pname)
	{
		case GL_TEXTURE_MIN_FILTER:
			sPname = "GL_TEXTURE_MIN_FILTER";
			breakOnError(
				(param == GL_NEAREST ||
				param == GL_LINEAR ||
				param == GL_NEAREST_MIPMAP_NEAREST ||
				param == GL_LINEAR_MIPMAP_NEAREST ||
				param == GL_NEAREST_MIPMAP_LINEAR ||
				param == GL_LINEAR_MIPMAP_LINEAR),
				"param : Invalid enum must be one of : "
				"GL_NEAREST, GL_LINEAR, GL_NEAREST_MIPMAP_NEAREST, "
				"GL_LINEAR_MIPMAP_NEAREST, GL_NEAREST_MIPMAP_LINEAR or "
				"GL_LINEAR_MIPMAP_LINEAR"
			);
			if ( param == GL_NEAREST )
				sParam = "GL_NEAREST";
			else if ( param == GL_LINEAR )
				sParam = "GL_LINEAR";
			else if ( param == GL_NEAREST_MIPMAP_NEAREST )
				sParam = "GL_NEAREST_MIPMAP_NEAREST";
			else if ( param == GL_LINEAR_MIPMAP_NEAREST )
				sParam = "GL_LINEAR_MIPMAP_NEAREST";
			else if ( param == GL_NEAREST_MIPMAP_LINEAR )
				sParam = "GL_NEAREST_MIPMAP_LINEAR";
			else if ( param == GL_LINEAR_MIPMAP_LINEAR )
				sParam = "GL_LINEAR_MIPMAP_LINEAR";
		break;

		case GL_TEXTURE_MAG_FILTER:
			sPname = "GL_TEXTURE_MAG_FILTER";
			breakOnError(
				param == GL_NEAREST ||
				param == GL_LINEAR,
				"param : Invalid enum, must be one of : "
				"GL_NEAREST or GL_LINEAR"
			);
			sParam = param == GL_NEAREST ? "GL_NEAREST":"GL_LINEAR";
		break;

		case GL_TEXTURE_WRAP_S:
		case GL_TEXTURE_WRAP_T:
			sPname = pname == GL_TEXTURE_WRAP_S  ?
				"GL_TEXTURE_WRAP_S":"GL_TEXTURE_WRAP_T";

			breakOnError(
				(param == GL_CLAMP_TO_EDGE ||
				param == GL_MIRRORED_REPEAT ||
				param == GL_REPEAT),
				"param : Invalid enum, must be one of : "
				"GL_CLAMP_TO_EDGE, GL_MIRRORED_REPEAT or GL_REPEAT"
			);

			if (param==GL_CLAMP_TO_EDGE)
				sParam = "GL_CLAMP_TO_EDGE";
			else if (param==GL_MIRRORED_REPEAT)
				sParam = "GL_MIRRORED_REPEAT";
			else if (param==GL_REPEAT)
				sParam = "GL_REPEAT";
		break;

		case GL_TEXTURE_MAX_ANISOTROPY_EXT:
			sPname = "GL_TEXTURE_MAX_ANISOTROPY_EXT";
			breakOnError(
				(param == 1 ||
				param == 2 ||
				param == 4 || 
				param == 8),
				"param : Invalid size, must be one of : 1,2,4 or 8"
			);
			sParam = toString(param);
		break;
	}

	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format("glTexParameteri( target:%s pname:%s param:%s )",
		target == GL_TEXTURE_2D ? "GL_TEXTURE_2D" : "GL_TEXTURE_CUBE_MAP",
		sPname.c_str(), sParam.c_str()),
		file, line
	);

	breakOnError( !result, result );
}

PFNGLTEXPARAMETERIVPROC gl::gl_TexParameteriv INIT_POINTER;
void gl::TexParameteriv  (GLenum target, GLenum pname, const GLint *params, const char* file, int line)
{
	TRACE_FUNCTION("glTexParameteriv(...) called from " <<
		get_path(file) << '(' << line << ')');

	gl_TexParameteriv(target,pname,params);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLTEXSUBIMAGE2DPROC gl::gl_TexSubImage2D INIT_POINTER;
void gl::TexSubImage2D  (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels, const char* file, int line)
{
	TRACE_FUNCTION("glTexSubImage2D(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_TexSubImage2D(target,level,xoffset,yoffset,width,height,format,
		type,pixels);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLUNIFORM1FPROC gl::gl_Uniform1f INIT_POINTER;
void gl::Uniform1f  (GLint location, GLfloat v0, const char* file, int line)
{
	TRACE_FUNCTION("glUniform1f(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_Uniform1f(location,v0);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLUNIFORM1FVPROC gl::gl_Uniform1fv INIT_POINTER;
void gl::Uniform1fv  (GLint location, GLsizei count, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glUniform1fv(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_Uniform1fv(location,count,value);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLUNIFORM1IPROC gl::gl_Uniform1i INIT_POINTER;
void gl::Uniform1i  (GLint location, GLint v0, const char* file, int line)
{
	TRACE_FUNCTION("glUniform1i(...) called from " << 
		get_path(file) << '(' << line << ')');

	const bool is_bound = is_program_bound();

	breakOnError(is_bound, "No program bound");

	gl_Uniform1i(location,v0);

	const char* result = get_last_error();

	// add function to call list
	addCall(result,
	 format("glUniform1i( location:%d, v0:%d )",location,v0),
	 file, line
	);

	breakOnError( !result, result );
}

PFNGLUNIFORM1IVPROC gl::gl_Uniform1iv INIT_POINTER;
void gl::Uniform1iv  (GLint location, GLsizei count, const GLint *value, const char* file, int line)
{
	TRACE_FUNCTION("glUniform1iv(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_Uniform1iv(location,count,value);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLUNIFORM2FPROC gl::gl_Uniform2f INIT_POINTER;
void gl::Uniform2f  (GLint location, GLfloat v0, GLfloat v1, const char* file, int line)
{
	TRACE_FUNCTION("glUniform2f(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_Uniform2f(location,v0,v1);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLUNIFORM2FVPROC gl::gl_Uniform2fv INIT_POINTER;
void gl::Uniform2fv  (GLint location, GLsizei count, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glUniform2fv(...) called from " << get_path(file) << '(' << line << ')');
	gl_Uniform2fv(
		location,
		count,
		value);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLUNIFORM2IPROC gl::gl_Uniform2i INIT_POINTER;
void gl::Uniform2i  (GLint location, GLint v0, GLint v1, const char* file, int line)
{
	TRACE_FUNCTION("glUniform2i(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_Uniform2i(location,v0,v1);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLUNIFORM2IVPROC gl::gl_Uniform2iv INIT_POINTER;
void gl::Uniform2iv  (GLint location, GLsizei count, const GLint *value, const char* file, int line)
{
	TRACE_FUNCTION("glUniform2iv(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_Uniform2iv(location,count,value);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLUNIFORM3FPROC gl::gl_Uniform3f INIT_POINTER;
void gl::Uniform3f  (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, const char* file, int line)
{
	TRACE_FUNCTION("glUniform3f(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_Uniform3f(location,v0,v1,v2);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLUNIFORM3FVPROC gl::gl_Uniform3fv INIT_POINTER;
void gl::Uniform3fv  (GLint location, GLsizei count, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glUniform3fv(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_Uniform3fv(location,count,value);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLUNIFORM3IPROC gl::gl_Uniform3i INIT_POINTER;
void gl::Uniform3i  (GLint location, GLint v0, GLint v1, GLint v2, const char* file, int line)
{
	TRACE_FUNCTION("glUniform3i(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_Uniform3i(location,v0,v1,v2);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLUNIFORM3IVPROC gl::gl_Uniform3iv INIT_POINTER;
void gl::Uniform3iv  (GLint location, GLsizei count, const GLint *value, const char* file, int line)
{
	TRACE_FUNCTION("glUniform3iv(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_Uniform3iv(location,count,value);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLUNIFORM4FPROC gl::gl_Uniform4f INIT_POINTER;
void gl::Uniform4f  (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3, const char* file, int line)
{
	TRACE_FUNCTION("glUniform4f(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_Uniform4f(location,v0,v1,v2,v3);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLUNIFORM4FVPROC gl::gl_Uniform4fv INIT_POINTER;
void gl::Uniform4fv  (GLint location, GLsizei count, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glUniform4fv(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_Uniform4fv(location,count,value);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLUNIFORM4IPROC gl::gl_Uniform4i INIT_POINTER;
void gl::Uniform4i  (GLint location, GLint v0, GLint v1, GLint v2, GLint v3, const char* file, int line)
{
	TRACE_FUNCTION("glUniform4i(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_Uniform4i(location,v0,v1,v2,v3);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLUNIFORM4IVPROC gl::gl_Uniform4iv INIT_POINTER;
void gl::Uniform4iv  (GLint location, GLsizei count, const GLint *value, const char* file, int line)
{
	TRACE_FUNCTION("glUniform4iv(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_Uniform4iv(location,count,value);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLUNIFORMMATRIX2FVPROC gl::gl_UniformMatrix2fv INIT_POINTER;
void gl::UniformMatrix2fv  (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glUniformMatrix2fv(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_UniformMatrix2fv(location,count,transpose,value);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLUNIFORMMATRIX3FVPROC gl::gl_UniformMatrix3fv INIT_POINTER;
void gl::UniformMatrix3fv  (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glUniformMatrix3fv(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_UniformMatrix3fv(location,count,transpose,value);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLUNIFORMMATRIX4FVPROC gl::gl_UniformMatrix4fv INIT_POINTER;
void gl::UniformMatrix4fv  (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glUniformMatrix4fv(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_UniformMatrix4fv(location,count,transpose,value);

	const char* result = get_last_error();

	// add function to call list
	addCall(result,
		format(
			"glUniformMatrix4fv( location:%d count:%d "
			"transpose:%s value[0]:%g )",
		location,count,transpose?"GL_TRUE":"GL_FALSE",value[0]),
		file, line
	);

	breakOnError( !result, result );
}

PFNGLUSEPROGRAMPROC gl::gl_UseProgram INIT_POINTER;
void gl::UseProgram  (GLuint program, const char* file, int line)
{
	TRACE_FUNCTION("glUseProgram(...) called from " << get_path(file) << '(' << line << ')');

	const bool is_valid = (0 == program) ?
							true : is_registered_program(program);

	breakOnError( is_valid, "Invalid program" );

	// break if program is valid and already bound
	breakOnWarning( 
		!(is_valid && (program == get_program_bound())),
		"program already bound"
	);

	/** send it to opengl */
	gl_UseProgram( program );

	// local
	setUseProgram( ( 0 == program ) ? INVALID_BOUND : program );

	const char* result = get_last_error();

	// add function to call list
	addCall(result,format("glUseProgram( program:%d )",
		program),file,line);

	breakOnError( !result, result );
}

PFNGLVALIDATEPROGRAMPROC gl::gl_ValidateProgram INIT_POINTER;
void gl::ValidateProgram  (GLuint program, const char* file, int line)
{
	TRACE_FUNCTION("glValidateProgram(...) called from " << 
		get_path(file) << '(' << line << ')');

	const bool is_valid = is_registered_program(program);

	breakOnError( is_valid, "Invalid program" );

	gl_ValidateProgram(program);

	const char* result = get_last_error();

	// add function to call list
	addCall(result,format("glValidateProgram( program:%d )",
		program),file,line);

	breakOnError( !result, result );
}

PFNGLVERTEXATTRIB1FPROC gl::gl_VertexAttrib1f INIT_POINTER;
void gl::VertexAttrib1f  (GLuint index, GLfloat x, const char* file, int line)
{
	TRACE_FUNCTION("glVertexAttrib1f(...) called from " << 
		get_path(file) << '(' << line << ')');

	gl_VertexAttrib1f(index,x);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLVERTEXATTRIB1FVPROC gl::gl_VertexAttrib1fv INIT_POINTER;
void gl::VertexAttrib1fv  (GLuint index, const GLfloat *v, const char* file, int line)
{
	TRACE_FUNCTION("glVertexAttrib1fv(...) called from " <<
		get_path(file) << '(' << line << ')');

	gl_VertexAttrib1fv(index,v);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLVERTEXATTRIB2FPROC gl::gl_VertexAttrib2f INIT_POINTER;
void gl::VertexAttrib2f  (GLuint index, GLfloat x, GLfloat y, const char* file, int line)
{
	TRACE_FUNCTION("glVertexAttrib2f(...) called from " <<
		get_path(file) << '(' << line << ')');

	gl_VertexAttrib2f(index,x,y);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLVERTEXATTRIB2FVPROC gl::gl_VertexAttrib2fv INIT_POINTER;
void gl::VertexAttrib2fv  (GLuint index, const GLfloat *v, const char* file, int line)
{
	TRACE_FUNCTION("glVertexAttrib2fv(...) called from " <<
		get_path(file) << '(' << line << ')');

	gl_VertexAttrib2fv(index,v);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLVERTEXATTRIB3FPROC gl::gl_VertexAttrib3f INIT_POINTER;
void gl::VertexAttrib3f  (GLuint index, GLfloat x, GLfloat y, GLfloat z, const char* file, int line)
{
	TRACE_FUNCTION("glVertexAttrib3f(...) called from " << get_path(file) << '(' << line << ')');

	gl_VertexAttrib3f(index,x,y,z);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLVERTEXATTRIB3FVPROC gl::gl_VertexAttrib3fv INIT_POINTER;
void gl::VertexAttrib3fv  (GLuint index, const GLfloat *v, const char* file, int line)
{
	TRACE_FUNCTION("glVertexAttrib3fv(...) called from " << get_path(file) << '(' << line << ')');

	gl_VertexAttrib3fv(index,v);
	
	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLVERTEXATTRIB4FPROC gl::gl_VertexAttrib4f INIT_POINTER;
void gl::VertexAttrib4f  (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w, const char* file, int line)
{
	TRACE_FUNCTION("glVertexAttrib4f(...) called from " << get_path(file) << '(' << line << ')');

	gl_VertexAttrib4f(index,x,y,z,w);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLVERTEXATTRIB4FVPROC gl::gl_VertexAttrib4fv INIT_POINTER;
void gl::VertexAttrib4fv  (GLuint index, const GLfloat *v, const char* file, int line)
{
	TRACE_FUNCTION("glVertexAttrib4fv(...) called from " << get_path(file) << '(' << line << ')');

	gl_VertexAttrib4fv(index,v);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLVERTEXATTRIBPOINTERPROC gl::gl_VertexAttribPointer INIT_POINTER;
void gl::VertexAttribPointer(GLuint index, GLint size, GLenum type,
			GLboolean normalized, GLsizei stride, const void *pointer,
			const char* file, int line)
{
	TRACE_FUNCTION("glVertexAttribPointer(...) called from " <<
		get_path(file) << '(' << line << ')');

	breakOnError(
		(size == 1 || size == 2 || size == 3 || size == 4) ||
		(type == GL_BYTE ||
			type == GL_UNSIGNED_BYTE ||
			type == GL_SHORT ||
			type == GL_UNSIGNED_SHORT ||
			type == GL_FIXED ||
			type == GL_FLOAT) ||
			(normalized == GL_TRUE || normalized == GL_FALSE) ||
		(index < GL_MAX_VERTEX_ATTRIBS)
	);

	breakOnError(
		_bound_buffer[ARRAY_BUFFER] ||
		_bound_buffer[ARRAY_BUFFER]->id != INVALID_BOUND
	);

	gl_VertexAttribPointer(index,size,type,normalized,stride,pointer);

	const char* result = get_last_error();
	breakOnError( !result, result );
}

PFNGLVIEWPORTPROC gl::gl_Viewport INIT_POINTER;
void gl::Viewport  (GLint x, GLint y, GLsizei width, GLsizei height, const char* file, int line)
{
	TRACE_FUNCTION("glViewport(...) called from " << get_path(file) << '(' << line << ')');

	gl_Viewport(x,y,width,height);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

// ---------------------------------------------------------------------
// gl2ext.h
// ---------------------------------------------------------------------

// GL_AMD_performance_monitor
PFNGLBEGINPERFMONITORAMDPROC gl::gl_BeginPerfMonitorAMD INIT_POINTER;
void gl::BeginPerfMonitorAMD  (GLuint monitor, const char* file, int line)
{
	TRACE_FUNCTION("glBeginPerfMonitorAMD(...) called from " << get_path(file) << '(' << line << ')');

	gl_BeginPerfMonitorAMD(monitor);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLDELETEPERFMONITORSAMDPROC gl::gl_DeletePerfMonitorsAMD INIT_POINTER;
void gl::DeletePerfMonitorsAMD  (GLsizei n, GLuint *monitors, const char* file, int line)
{
	TRACE_FUNCTION("glDeletePerfMonitorsAMD(...) called from " << get_path(file) << '(' << line << ')');

	gl_DeletePerfMonitorsAMD(n,monitors);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLENDPERFMONITORAMDPROC gl::gl_EndPerfMonitorAMD INIT_POINTER;
void gl::EndPerfMonitorAMD  (GLuint monitor, const char* file, int line)
{
	TRACE_FUNCTION("glEndPerfMonitorAMD(...) called from " << get_path(file) << '(' << line << ')');

	gl_EndPerfMonitorAMD(monitor);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLGENPERFMONITORSAMDPROC gl::gl_GenPerfMonitorsAMD INIT_POINTER;
void gl::GenPerfMonitorsAMD  (GLsizei n, GLuint *monitors, const char* file, int line)
{
	TRACE_FUNCTION("glGenPerfMonitorsAMD(...) called from " << get_path(file) << '(' << line << ')');

	gl_GenPerfMonitorsAMD(n,monitors);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLGETPERFMONITORCOUNTERDATAAMDPROC gl::gl_GetPerfMonitorCounterDataAMD INIT_POINTER;
void gl::GetPerfMonitorCounterDataAMD  (GLuint monitor, GLenum pname, GLsizei dataSize, GLuint *data, GLint *bytesWritten, const char* file, int line)
{
	TRACE_FUNCTION("glGetPerfMonitorCounterDataAMD(...) called from " << get_path(file) << '(' << line << ')');

	gl_GetPerfMonitorCounterDataAMD(monitor,pname,dataSize,data,bytesWritten);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLGETPERFMONITORCOUNTERINFOAMDPROC gl::gl_GetPerfMonitorCounterInfoAMD INIT_POINTER;
void gl::GetPerfMonitorCounterInfoAMD  (GLuint group, GLuint counter, GLenum pname, void *data, const char* file, int line)
{
	TRACE_FUNCTION("glGetPerfMonitorCounterInfoAMD(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetPerfMonitorCounterInfoAMD(
		group,
		counter,
		pname,
		data);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLGETPERFMONITORCOUNTERSTRINGAMDPROC gl::gl_GetPerfMonitorCounterStringAMD INIT_POINTER;
void gl::GetPerfMonitorCounterStringAMD  (GLuint group, GLuint counter, GLsizei bufSize, GLsizei *length, GLchar *counterString, const char* file, int line)
{
	TRACE_FUNCTION("glGetPerfMonitorCounterStringAMD(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetPerfMonitorCounterStringAMD(
		group,
		counter,
		bufSize,
		length,
		counterString);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLGETPERFMONITORCOUNTERSAMDPROC gl::gl_GetPerfMonitorCountersAMD INIT_POINTER;
void gl::GetPerfMonitorCountersAMD  (GLuint group, GLint *numCounters, GLint *maxActiveCounters, GLsizei counterSize, GLuint *counters, const char* file, int line)
{
	TRACE_FUNCTION("glGetPerfMonitorCountersAMD(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetPerfMonitorCountersAMD(
		group,
		numCounters,
		maxActiveCounters,
		counterSize,
		counters);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLGETPERFMONITORGROUPSTRINGAMDPROC gl::gl_GetPerfMonitorGroupStringAMD INIT_POINTER;
void gl::GetPerfMonitorGroupStringAMD  (GLuint group, GLsizei bufSize, GLsizei *length, GLchar *groupString, const char* file, int line)
{
	TRACE_FUNCTION("glGetPerfMonitorGroupStringAMD(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetPerfMonitorGroupStringAMD(
		group,
		bufSize,
		length,
		groupString);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLGETPERFMONITORGROUPSAMDPROC gl::gl_GetPerfMonitorGroupsAMD INIT_POINTER;
void gl::GetPerfMonitorGroupsAMD  (GLint *numGroups, GLsizei groupsSize, GLuint *groups, const char* file, int line)
{
	TRACE_FUNCTION("glGetPerfMonitorGroupsAMD(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetPerfMonitorGroupsAMD(
		numGroups,
		groupsSize,
		groups);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLSELECTPERFMONITORCOUNTERSAMDPROC gl::gl_SelectPerfMonitorCountersAMD INIT_POINTER;
void gl::SelectPerfMonitorCountersAMD  (GLuint monitor, GLboolean enable, GLuint group, GLint numCounters, GLuint *counterList, const char* file, int line)
{
	TRACE_FUNCTION("glSelectPerfMonitorCountersAMD(...) called from " << get_path(file) << '(' << line << ')');
	gl_SelectPerfMonitorCountersAMD(
		monitor,
		enable,
		group,
		numCounters,
		counterList);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

// GL_ANGLE_framebuffer_blit
PFNGLBLITFRAMEBUFFERANGLEPROC gl::gl_BlitFramebufferANGLE INIT_POINTER;
void gl::BlitFramebufferANGLE  (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter, const char* file, int line)
{
	TRACE_FUNCTION("glBlitFramebufferANGLE(...) called from " << get_path(file) << '(' << line << ')');
	gl_BlitFramebufferANGLE(
		srcX0,
		srcY0,
		srcX1,
		srcY1,
		dstX0,
		dstY0,
		dstX1,
		dstY1,
		mask,
		filter);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

// GL_ANGLE_framebuffer_multisample
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEANGLEPROC gl::gl_RenderbufferStorageMultisampleANGLE INIT_POINTER;
void gl::RenderbufferStorageMultisampleANGLE  (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, const char* file, int line)
{
	TRACE_FUNCTION("glRenderbufferStorageMultisampleANGLE(...) called from " << get_path(file) << '(' << line << ')');
	gl_RenderbufferStorageMultisampleANGLE(
		target,
		samples,
		internalformat,
		width,
		height);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

// GL_ANGLE_instanced_arrays
PFNGLDRAWARRAYSINSTANCEDANGLEPROC gl::gl_DrawArraysInstancedANGLE INIT_POINTER;
void gl::DrawArraysInstancedANGLE  (GLenum mode, GLint first, GLsizei count, GLsizei primcount, const char* file, int line)
{
	TRACE_FUNCTION("glDrawArraysInstancedANGLE(...) called from " << get_path(file) << '(' << line << ')');
	gl_DrawArraysInstancedANGLE(
		mode,
		first,
		count,
		primcount);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLDRAWELEMENTSINSTANCEDANGLEPROC gl::gl_DrawElementsInstancedANGLE INIT_POINTER;
void gl::DrawElementsInstancedANGLE  (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei primcount, const char* file, int line)
{
	TRACE_FUNCTION("glDrawElementsInstancedANGLE(...) called from " << get_path(file) << '(' << line << ')');
	gl_DrawElementsInstancedANGLE(
		mode,
		count,
		type,
		indices,
		primcount);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLVERTEXATTRIBDIVISORANGLEPROC gl::gl_VertexAttribDivisorANGLE INIT_POINTER;
void gl::VertexAttribDivisorANGLE  (GLuint index, GLuint divisor, const char* file, int line)
{
	TRACE_FUNCTION("glVertexAttribDivisorANGLE(...) called from " << get_path(file) << '(' << line << ')');
	gl_VertexAttribDivisorANGLE(
		index,
		divisor);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

// GL_ANGLE_translated_shader_source
PFNGLGETTRANSLATEDSHADERSOURCEANGLEPROC gl::gl_GetTranslatedShaderSourceANGLE INIT_POINTER;
void gl::GetTranslatedShaderSourceANGLE  (GLuint shader, GLsizei bufsize, GLsizei *length, GLchar *source, const char* file, int line)
{
	TRACE_FUNCTION("glGetTranslatedShaderSourceANGLE(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetTranslatedShaderSourceANGLE(
		shader,
		bufsize,
		length,
		source);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

// GL_APPLE_copy_texture_levels
PFNGLCOPYTEXTURELEVELSAPPLEPROC gl::gl_CopyTextureLevelsAPPLE INIT_POINTER;
void gl::CopyTextureLevelsAPPLE  (GLuint destinationTexture, GLuint sourceTexture, GLint sourceBaseLevel, GLsizei sourceLevelCount, const char* file, int line)
{
	TRACE_FUNCTION("glCopyTextureLevelsAPPLE(...) called from " << get_path(file) << '(' << line << ')');
	gl_CopyTextureLevelsAPPLE(
		destinationTexture,
		sourceTexture,
		sourceBaseLevel,
		sourceLevelCount);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

// GL_APPLE_framebuffer_multisample
PFNGLRENDERBUFFERSTORAGEMULTISAMPLEAPPLEPROC gl::gl_RenderbufferStorageMultisampleAPPLE INIT_POINTER;
void gl::RenderbufferStorageMultisampleAPPLE  (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, const char* file, int line)
{
	TRACE_FUNCTION("glRenderbufferStorageMultisampleAPPLE(...) called from " << get_path(file) << '(' << line << ')');
	gl_RenderbufferStorageMultisampleAPPLE(
		target,
		samples,
		internalformat,
		width,
		height);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLRESOLVEMULTISAMPLEFRAMEBUFFERAPPLEPROC gl::gl_ResolveMultisampleFramebufferAPPLE INIT_POINTER;
void gl::ResolveMultisampleFramebufferAPPLE  (const char* file, int line)
{
	TRACE_FUNCTION("glResolveMultisampleFramebufferAPPLE(...) called from " << get_path(file) << '(' << line << ')');
	gl_ResolveMultisampleFramebufferAPPLE();
	const char* result = get_last_error();
	breakOnError(!result, result);
}

// GL_APPLE_sync
PFNGLCLIENTWAITSYNCAPPLEPROC gl::gl_ClientWaitSyncAPPLE INIT_POINTER;
GLenum gl::ClientWaitSyncAPPLE  (GLsync sync, GLbitfield flags, GLuint64 timeout, const char* file, int line)
{
	TRACE_FUNCTION("glClientWaitSyncAPPLE(...) called from " << get_path(file) << '(' << line << ')');
	return gl_ClientWaitSyncAPPLE(
		sync,
		flags,
		timeout);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLDELETESYNCAPPLEPROC gl::gl_DeleteSyncAPPLE INIT_POINTER;
void gl::DeleteSyncAPPLE  (GLsync sync, const char* file, int line)
{
	TRACE_FUNCTION("glDeleteSyncAPPLE(...) called from " << get_path(file) << '(' << line << ')');
	gl_DeleteSyncAPPLE(
		sync);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLFENCESYNCAPPLEPROC gl::gl_FenceSyncAPPLE INIT_POINTER;
GLsync gl::FenceSyncAPPLE  (GLenum condition, GLbitfield flags, const char* file, int line)
{
	TRACE_FUNCTION("glFenceSyncAPPLE(...) called from " << get_path(file) << '(' << line << ')');
	return gl_FenceSyncAPPLE(
		condition,
		flags);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLGETINTEGER64VAPPLEPROC gl::gl_GetInteger64vAPPLE INIT_POINTER;
void gl::GetInteger64vAPPLE  (GLenum pname, GLint64 *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetInteger64vAPPLE(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetInteger64vAPPLE(
		pname,
		params);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLGETSYNCIVAPPLEPROC gl::gl_GetSyncivAPPLE INIT_POINTER;
void gl::GetSyncivAPPLE  (GLsync sync, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *values, const char* file, int line)
{
	TRACE_FUNCTION("glGetSyncivAPPLE(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetSyncivAPPLE(
		sync,
		pname,
		bufSize,
		length,
		values);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLISSYNCAPPLEPROC gl::gl_IsSyncAPPLE INIT_POINTER;
GLboolean gl::IsSyncAPPLE  (GLsync sync, const char* file, int line)
{
	TRACE_FUNCTION("glIsSyncAPPLE(...) called from " << get_path(file) << '(' << line << ')');

	GLboolean r = gl_IsSyncAPPLE(sync);

	const char* result = get_last_error();
	breakOnError(!result, result);

	return r;
}

PFNGLWAITSYNCAPPLEPROC gl::gl_WaitSyncAPPLE INIT_POINTER;
void gl::WaitSyncAPPLE  (GLsync sync, GLbitfield flags, GLuint64 timeout, const char* file, int line)
{
	TRACE_FUNCTION("glWaitSyncAPPLE(...) called from " << get_path(file) << '(' << line << ')');
	gl_WaitSyncAPPLE(
		sync,
		flags,
		timeout);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

// GL_EXT_base_instance
PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEEXTPROC gl::gl_DrawArraysInstancedBaseInstanceEXT INIT_POINTER;
void gl::DrawArraysInstancedBaseInstanceEXT  (GLenum mode, GLint first, GLsizei count, GLsizei instancecount, GLuint baseinstance, const char* file, int line)
{
	TRACE_FUNCTION("glDrawArraysInstancedBaseInstanceEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_DrawArraysInstancedBaseInstanceEXT(
		mode,
		first,
		count,
		instancecount,
		baseinstance);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEEXTPROC gl::gl_DrawElementsInstancedBaseInstanceEXT INIT_POINTER;
void gl::DrawElementsInstancedBaseInstanceEXT  (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLuint baseinstance, const char* file, int line)
{
	TRACE_FUNCTION("glDrawElementsInstancedBaseInstanceEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_DrawElementsInstancedBaseInstanceEXT(
		mode,
		count,
		type,
		indices,
		instancecount,
		baseinstance);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEEXTPROC gl::gl_DrawElementsInstancedBaseVertexBaseInstanceEXT INIT_POINTER;
void gl::DrawElementsInstancedBaseVertexBaseInstanceEXT  (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex, GLuint baseinstance, const char* file, int line)
{
	TRACE_FUNCTION("glDrawElementsInstancedBaseVertexBaseInstanceEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_DrawElementsInstancedBaseVertexBaseInstanceEXT(
		mode,
		count,
		type,
		indices,
		instancecount,
		basevertex,
		baseinstance);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

// GL_EXT_blend_func_extended
PFNGLBINDFRAGDATALOCATIONEXTPROC gl::gl_BindFragDataLocationEXT INIT_POINTER;
void gl::BindFragDataLocationEXT  (GLuint program, GLuint color, const GLchar *name, const char* file, int line)
{
	TRACE_FUNCTION("glBindFragDataLocationEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_BindFragDataLocationEXT(
		program,
		color,
		name);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLBINDFRAGDATALOCATIONINDEXEDEXTPROC gl::gl_BindFragDataLocationIndexedEXT INIT_POINTER;
void gl::BindFragDataLocationIndexedEXT  (GLuint program, GLuint colorNumber, GLuint index, const GLchar *name, const char* file, int line)
{
	TRACE_FUNCTION("glBindFragDataLocationIndexedEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_BindFragDataLocationIndexedEXT(
		program,
		colorNumber,
		index,
		name);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLGETFRAGDATAINDEXEXTPROC gl::gl_GetFragDataIndexEXT INIT_POINTER;
GLint gl::GetFragDataIndexEXT  (GLuint program, const GLchar *name, const char* file, int line)
{
	TRACE_FUNCTION("glGetFragDataIndexEXT(...) called from " << get_path(file) << '(' << line << ')');

	GLint r = gl_GetFragDataIndexEXT(program,name);

	const char* result = get_last_error();
	breakOnError(!result, result);

	return r;
}

PFNGLGETPROGRAMRESOURCELOCATIONINDEXEXTPROC gl::gl_GetProgramResourceLocationIndexEXT INIT_POINTER;
GLint gl::GetProgramResourceLocationIndexEXT  (GLuint program, GLenum programInterface, const GLchar *name, const char* file, int line)
{
	TRACE_FUNCTION("glGetProgramResourceLocationIndexEXT(...) called from " << get_path(file) << '(' << line << ')');

	GLint r = gl_GetProgramResourceLocationIndexEXT(program,programInterface,name);

	const char* result = get_last_error();
	breakOnError(!result, result);

	return r;
}

// GL_EXT_buffer_storage
PFNGLBUFFERSTORAGEEXTPROC gl::gl_BufferStorageEXT INIT_POINTER;
void gl::BufferStorageEXT  (GLenum target, GLsizeiptr size, const void *data, GLbitfield flags, const char* file, int line)
{
	TRACE_FUNCTION("glBufferStorageEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_BufferStorageEXT(
		target,
		size,
		data,
		flags);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

// GL_EXT_copy_image
PFNGLCOPYIMAGESUBDATAEXTPROC gl::gl_CopyImageSubDataEXT INIT_POINTER;
void gl::CopyImageSubDataEXT  (GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth, const char* file, int line)
{
	TRACE_FUNCTION("glCopyImageSubDataEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_CopyImageSubDataEXT(
		srcName,
		srcTarget,
		srcLevel,
		srcX,
		srcY,
		srcZ,
		dstName,
		dstTarget,
		dstLevel,
		dstX,
		dstY,
		dstZ,
		srcWidth,
		srcHeight,
		srcDepth);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

// GL_EXT_debug_label
PFNGLGETOBJECTLABELEXTPROC gl::gl_GetObjectLabelEXT INIT_POINTER;
void gl::GetObjectLabelEXT  (GLenum type, GLuint object, GLsizei bufSize, GLsizei *length, GLchar *label, const char* file, int line)
{
	TRACE_FUNCTION("glGetObjectLabelEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetObjectLabelEXT(
		type,
		object,
		bufSize,
		length,
		label);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLLABELOBJECTEXTPROC gl::gl_LabelObjectEXT INIT_POINTER;
void gl::LabelObjectEXT  (GLenum type, GLuint object, GLsizei length, const GLchar *label, const char* file, int line)
{
	TRACE_FUNCTION("glLabelObjectEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_LabelObjectEXT(
		type,
		object,
		length,
		label);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

// GL_EXT_debug_marker
PFNGLINSERTEVENTMARKEREXTPROC gl::gl_InsertEventMarkerEXT INIT_POINTER;
void gl::InsertEventMarkerEXT  (GLsizei length, const GLchar *marker, const char* file, int line)
{
	TRACE_FUNCTION("glInsertEventMarkerEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_InsertEventMarkerEXT(
		length,
		marker);
	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLPOPGROUPMARKEREXTPROC gl::gl_PopGroupMarkerEXT INIT_POINTER;
void gl::PopGroupMarkerEXT  (const char* file, int line)
{
	TRACE_FUNCTION("glPopGroupMarkerEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_PopGroupMarkerEXT();

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLPUSHGROUPMARKEREXTPROC gl::gl_PushGroupMarkerEXT INIT_POINTER;
void gl::PushGroupMarkerEXT  (GLsizei length, const GLchar *marker, const char* file, int line)
{
	TRACE_FUNCTION("glPushGroupMarkerEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_PushGroupMarkerEXT(length,marker);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

// GL_EXT_discard_framebuffer
PFNGLDISCARDFRAMEBUFFEREXTPROC gl::gl_DiscardFramebufferEXT INIT_POINTER;
void gl::DiscardFramebufferEXT  (GLenum target, GLsizei numAttachments, const GLenum *attachments, const char* file, int line)
{
	TRACE_FUNCTION("glDiscardFramebufferEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_DiscardFramebufferEXT(target,numAttachments,attachments);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

// GL_EXT_disjoint_timer_query
PFNGLBEGINQUERYEXTPROC gl::gl_BeginQueryEXT INIT_POINTER;
void gl::BeginQueryEXT  (GLenum target, GLuint id, const char* file, int line)
{
	TRACE_FUNCTION("glBeginQueryEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_BeginQueryEXT(target,id);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLDELETEQUERIESEXTPROC gl::gl_DeleteQueriesEXT INIT_POINTER;
void gl::DeleteQueriesEXT  (GLsizei n, const GLuint *ids, const char* file, int line)
{
	TRACE_FUNCTION("glDeleteQueriesEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_DeleteQueriesEXT(n,ids);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLENDQUERYEXTPROC gl::gl_EndQueryEXT INIT_POINTER;
void gl::EndQueryEXT  (GLenum target, const char* file, int line)
{
	TRACE_FUNCTION("glEndQueryEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_EndQueryEXT(target);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLGENQUERIESEXTPROC gl::gl_GenQueriesEXT INIT_POINTER;
void gl::GenQueriesEXT  (GLsizei n, GLuint *ids, const char* file, int line)
{
	TRACE_FUNCTION("glGenQueriesEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_GenQueriesEXT(n,ids);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLGETQUERYOBJECTI64VEXTPROC gl::gl_GetQueryObjecti64vEXT INIT_POINTER;
void gl::GetQueryObjecti64vEXT  (GLuint id, GLenum pname, GLint64 *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetQueryObjecti64vEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_GetQueryObjecti64vEXT(id,pname,params);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLGETQUERYOBJECTIVEXTPROC gl::gl_GetQueryObjectivEXT INIT_POINTER;
void gl::GetQueryObjectivEXT  (GLuint id, GLenum pname, GLint *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetQueryObjectivEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_GetQueryObjectivEXT(id,pname,params);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLGETQUERYOBJECTUI64VEXTPROC gl::gl_GetQueryObjectui64vEXT INIT_POINTER;
void gl::GetQueryObjectui64vEXT  (GLuint id, GLenum pname, GLuint64 *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetQueryObjectui64vEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_GetQueryObjectui64vEXT(id,pname,params);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLGETQUERYOBJECTUIVEXTPROC gl::gl_GetQueryObjectuivEXT INIT_POINTER;
void gl::GetQueryObjectuivEXT  (GLuint id, GLenum pname, GLuint *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetQueryObjectuivEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_GetQueryObjectuivEXT(id,pname,params);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLGETQUERYIVEXTPROC gl::gl_GetQueryivEXT INIT_POINTER;
void gl::GetQueryivEXT  (GLenum target, GLenum pname, GLint *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetQueryivEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_GetQueryivEXT(target,pname,params);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLISQUERYEXTPROC gl::gl_IsQueryEXT INIT_POINTER;
GLboolean gl::IsQueryEXT  (GLuint id, const char* file, int line)
{
	TRACE_FUNCTION("glIsQueryEXT(...) called from " << get_path(file) << '(' << line << ')');

	GLboolean r = gl_IsQueryEXT(id);

	const char* result = get_last_error();
	breakOnError(!result, result);

	return r;
}

PFNGLQUERYCOUNTEREXTPROC gl::gl_QueryCounterEXT INIT_POINTER;
void gl::QueryCounterEXT  (GLuint id, GLenum target, const char* file, int line)
{
	TRACE_FUNCTION("glQueryCounterEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_QueryCounterEXT(id,target);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

// GL_EXT_draw_buffers
PFNGLDRAWBUFFERSEXTPROC gl::gl_DrawBuffersEXT INIT_POINTER;
void gl::DrawBuffersEXT  (GLsizei n, const GLenum *bufs, const char* file, int line)
{
	TRACE_FUNCTION("glDrawBuffersEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_DrawBuffersEXT(n,bufs);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

// GL_EXT_draw_buffers_indexed
PFNGLBLENDEQUATIONSEPARATEIEXTPROC gl::gl_BlendEquationSeparateiEXT INIT_POINTER;
void gl::BlendEquationSeparateiEXT  (GLuint buf, GLenum modeRGB, GLenum modeAlpha, const char* file, int line)
{
	TRACE_FUNCTION("glBlendEquationSeparateiEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_BlendEquationSeparateiEXT(buf,modeRGB,modeAlpha);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLBLENDEQUATIONIEXTPROC gl::gl_BlendEquationiEXT INIT_POINTER;
void gl::BlendEquationiEXT  (GLuint buf, GLenum mode, const char* file, int line)
{
	TRACE_FUNCTION("glBlendEquationiEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_BlendEquationiEXT(buf,mode);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLBLENDFUNCSEPARATEIEXTPROC gl::gl_BlendFuncSeparateiEXT INIT_POINTER;
void gl::BlendFuncSeparateiEXT  (GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha, const char* file, int line)
{
	TRACE_FUNCTION("glBlendFuncSeparateiEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_BlendFuncSeparateiEXT(buf,srcRGB,dstRGB,srcAlpha,dstAlpha);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLBLENDFUNCIEXTPROC gl::gl_BlendFunciEXT INIT_POINTER;
void gl::BlendFunciEXT  (GLuint buf, GLenum src, GLenum dst, const char* file, int line)
{
	TRACE_FUNCTION("glBlendFunciEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_BlendFunciEXT(buf,src,dst);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLCOLORMASKIEXTPROC gl::gl_ColorMaskiEXT INIT_POINTER;
void gl::ColorMaskiEXT  (GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a, const char* file, int line)
{
	TRACE_FUNCTION("glColorMaskiEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_ColorMaskiEXT(index,r,g,b,a);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLDISABLEIEXTPROC gl::gl_DisableiEXT INIT_POINTER;
void gl::DisableiEXT  (GLenum target, GLuint index, const char* file, int line)
{
	TRACE_FUNCTION("glDisableiEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_DisableiEXT(target,index);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLENABLEIEXTPROC gl::gl_EnableiEXT INIT_POINTER;
void gl::EnableiEXT  (GLenum target, GLuint index, const char* file, int line)
{
	TRACE_FUNCTION("glEnableiEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_EnableiEXT(target,index);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLISENABLEDIEXTPROC gl::gl_IsEnablediEXT INIT_POINTER;
GLboolean gl::IsEnablediEXT  (GLenum target, GLuint index, const char* file, int line)
{
	TRACE_FUNCTION("glIsEnablediEXT(...) called from " << get_path(file) << '(' << line << ')');

	return gl_IsEnablediEXT(target,index);
}

// GL_EXT_draw_elements_base_vertex
PFNGLDRAWELEMENTSBASEVERTEXEXTPROC gl::gl_DrawElementsBaseVertexEXT INIT_POINTER;
void gl::DrawElementsBaseVertexEXT  (GLenum mode, GLsizei count, GLenum type, const void *indices, GLint basevertex, const char* file, int line)
{
	TRACE_FUNCTION("glDrawElementsBaseVertexEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_DrawElementsBaseVertexEXT(mode,count,type,indices,basevertex);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXEXTPROC gl::gl_DrawElementsInstancedBaseVertexEXT INIT_POINTER;
void gl::DrawElementsInstancedBaseVertexEXT  (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex, const char* file, int line)
{
	TRACE_FUNCTION("glDrawElementsInstancedBaseVertexEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_DrawElementsInstancedBaseVertexEXT(
		mode,count,type,indices,instancecount,basevertex);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLDRAWRANGEELEMENTSBASEVERTEXEXTPROC gl::gl_DrawRangeElementsBaseVertexEXT INIT_POINTER;
void gl::DrawRangeElementsBaseVertexEXT  (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices, GLint basevertex, const char* file, int line)
{
	TRACE_FUNCTION("glDrawRangeElementsBaseVertexEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_DrawRangeElementsBaseVertexEXT(
		mode,start,end,count,type,indices,basevertex);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLMULTIDRAWELEMENTSBASEVERTEXEXTPROC gl::gl_MultiDrawElementsBaseVertexEXT INIT_POINTER;
void gl::MultiDrawElementsBaseVertexEXT  (GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei primcount, const GLint *basevertex, const char* file, int line)
{
	TRACE_FUNCTION("glMultiDrawElementsBaseVertexEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_MultiDrawElementsBaseVertexEXT(
		mode,count,type,indices,primcount,basevertex);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

// GL_EXT_draw_instanced
PFNGLDRAWARRAYSINSTANCEDEXTPROC gl::gl_DrawArraysInstancedEXT INIT_POINTER;
void gl::DrawArraysInstancedEXT  (GLenum mode, GLint start, GLsizei count, GLsizei primcount, const char* file, int line)
{
	TRACE_FUNCTION("glDrawArraysInstancedEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_DrawArraysInstancedEXT(
		mode,start,count,primcount);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLDRAWELEMENTSINSTANCEDEXTPROC gl::gl_DrawElementsInstancedEXT INIT_POINTER;
void gl::DrawElementsInstancedEXT  (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei primcount, const char* file, int line)
{
	TRACE_FUNCTION("glDrawElementsInstancedEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_DrawElementsInstancedEXT(
		mode,count,type,indices,primcount);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

// GL_EXT_geometry_shader
PFNGLFRAMEBUFFERTEXTUREEXTPROC gl::gl_FramebufferTextureEXT INIT_POINTER;
void gl::FramebufferTextureEXT  (GLenum target, GLenum attachment, GLuint texture, GLint level, const char* file, int line)
{
	TRACE_FUNCTION("glFramebufferTextureEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_FramebufferTextureEXT(
		target,attachment,texture,level);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

// GL_EXT_instanced_arrays
PFNGLVERTEXATTRIBDIVISOREXTPROC gl::gl_VertexAttribDivisorEXT INIT_POINTER;
void gl::VertexAttribDivisorEXT  (GLuint index, GLuint divisor, const char* file, int line)
{
	TRACE_FUNCTION("glVertexAttribDivisorEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_VertexAttribDivisorEXT(index,divisor);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

// GL_EXT_map_buffer_range
PFNGLFLUSHMAPPEDBUFFERRANGEEXTPROC gl::gl_FlushMappedBufferRangeEXT INIT_POINTER;
void gl::FlushMappedBufferRangeEXT  (GLenum target, GLintptr offset, GLsizeiptr length, const char* file, int line)
{
	TRACE_FUNCTION("glFlushMappedBufferRangeEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_FlushMappedBufferRangeEXT(target,offset,length);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLMAPBUFFERRANGEEXTPROC gl::gl_MapBufferRangeEXT INIT_POINTER;
void gl::MapBufferRangeEXT  (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access, const char* file, int line)
{
	TRACE_FUNCTION("glMapBufferRangeEXT(...) called from " << get_path(file) << '(' << line << ')');

	gl_MapBufferRangeEXT(target,offset,length,access);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

// GL_EXT_multi_draw_arrays
PFNGLMULTIDRAWARRAYSEXTPROC gl::gl_MultiDrawArraysEXT INIT_POINTER;
void gl::MultiDrawArraysEXT  (GLenum mode, const GLint *first, const GLsizei *count, GLsizei primcount, const char* file, int line)
{
	TRACE_FUNCTION("glMultiDrawArraysEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_MultiDrawArraysEXT(
		mode,first,count,primcount);

	const char* result = get_last_error();
	breakOnError(!result, result);
}

PFNGLMULTIDRAWELEMENTSEXTPROC gl::gl_MultiDrawElementsEXT INIT_POINTER;
void gl::MultiDrawElementsEXT  (GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei primcount, const char* file, int line)
{
	TRACE_FUNCTION("glMultiDrawElementsEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_MultiDrawElementsEXT(
		mode,
		count,
		type,
		indices,
		primcount);
}

// GL_EXT_multi_draw_indirect
PFNGLMULTIDRAWARRAYSINDIRECTEXTPROC gl::gl_MultiDrawArraysIndirectEXT INIT_POINTER;
void gl::MultiDrawArraysIndirectEXT  (GLenum mode, const void *indirect, GLsizei drawcount, GLsizei stride, const char* file, int line)
{
	TRACE_FUNCTION("glMultiDrawArraysIndirectEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_MultiDrawArraysIndirectEXT(
		mode,
		indirect,
		drawcount,
		stride);
}

PFNGLMULTIDRAWELEMENTSINDIRECTEXTPROC gl::gl_MultiDrawElementsIndirectEXT INIT_POINTER;
void gl::MultiDrawElementsIndirectEXT  (GLenum mode, GLenum type, const void *indirect, GLsizei drawcount, GLsizei stride, const char* file, int line)
{
	TRACE_FUNCTION("glMultiDrawElementsIndirectEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_MultiDrawElementsIndirectEXT(
		mode,
		type,
		indirect,
		drawcount,
		stride);
}

// GL_EXT_multisampled_render_to_texture
PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC gl::gl_FramebufferTexture2DMultisampleEXT INIT_POINTER;
void gl::FramebufferTexture2DMultisampleEXT  (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples, const char* file, int line)
{
	TRACE_FUNCTION("glFramebufferTexture2DMultisampleEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_FramebufferTexture2DMultisampleEXT(
		target,
		attachment,
		textarget,
		texture,
		level,
		samples);
}

PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC gl::gl_RenderbufferStorageMultisampleEXT INIT_POINTER;
void gl::RenderbufferStorageMultisampleEXT  (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, const char* file, int line)
{
	TRACE_FUNCTION("glRenderbufferStorageMultisampleEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_RenderbufferStorageMultisampleEXT(
		target,
		samples,
		internalformat,
		width,
		height);
}

// GL_EXT_multiview_draw_buffers
PFNGLDRAWBUFFERSINDEXEDEXTPROC gl::gl_DrawBuffersIndexedEXT INIT_POINTER;
void gl::DrawBuffersIndexedEXT  (GLint n, const GLenum *location, const GLint *indices, const char* file, int line)
{
	TRACE_FUNCTION("glDrawBuffersIndexedEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_DrawBuffersIndexedEXT(
		n,
		location,
		indices);
}

PFNGLGETINTEGERI_VEXTPROC gl::gl_GetIntegeri_vEXT INIT_POINTER;
void gl::GetIntegeri_vEXT  (GLenum target, GLuint index, GLint *data, const char* file, int line)
{
	TRACE_FUNCTION("glGetIntegeri_vEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetIntegeri_vEXT(
		target,
		index,
		data);
}

PFNGLREADBUFFERINDEXEDEXTPROC gl::gl_ReadBufferIndexedEXT INIT_POINTER;
void gl::ReadBufferIndexedEXT  (GLenum src, GLint index, const char* file, int line)
{
	TRACE_FUNCTION("glReadBufferIndexedEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ReadBufferIndexedEXT(
		src,
		index);
}

// GL_EXT_polygon_offset_clamp
PFNGLPOLYGONOFFSETCLAMPEXTPROC gl::gl_PolygonOffsetClampEXT INIT_POINTER;
void gl::PolygonOffsetClampEXT  (GLfloat factor, GLfloat units, GLfloat clamp, const char* file, int line)
{
	TRACE_FUNCTION("glPolygonOffsetClampEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_PolygonOffsetClampEXT(
		factor,
		units,
		clamp);
}

// GL_EXT_primitive_bounding_box
PFNGLPRIMITIVEBOUNDINGBOXEXTPROC gl::gl_PrimitiveBoundingBoxEXT INIT_POINTER;
void gl::PrimitiveBoundingBoxEXT  (GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW, GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW, const char* file, int line)
{
	TRACE_FUNCTION("glPrimitiveBoundingBoxEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_PrimitiveBoundingBoxEXT(
		minX,
		minY,
		minZ,
		minW,
		maxX,
		maxY,
		maxZ,
		maxW);
}

// GL_EXT_raster_multisample
PFNGLRASTERSAMPLESEXTPROC gl::gl_RasterSamplesEXT INIT_POINTER;
void gl::RasterSamplesEXT  (GLuint samples, GLboolean fixedsamplelocations, const char* file, int line)
{
	TRACE_FUNCTION("glRasterSamplesEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_RasterSamplesEXT(
		samples,
		fixedsamplelocations);
}

// GL_EXT_robustness
PFNGLGETGRAPHICSRESETSTATUSEXTPROC gl::gl_GetGraphicsResetStatusEXT INIT_POINTER;
GLenum gl::GetGraphicsResetStatusEXT  (const char* file, int line)
{
	TRACE_FUNCTION("glGetGraphicsResetStatusEXT(...) called from " << get_path(file) << '(' << line << ')');
	return gl_GetGraphicsResetStatusEXT(
		);
}

PFNGLGETNUNIFORMFVEXTPROC gl::gl_GetnUniformfvEXT INIT_POINTER;
void gl::GetnUniformfvEXT  (GLuint program, GLint location, GLsizei bufSize, GLfloat *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetnUniformfvEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetnUniformfvEXT(
		program,
		location,
		bufSize,
		params);
}

PFNGLGETNUNIFORMIVEXTPROC gl::gl_GetnUniformivEXT INIT_POINTER;
void gl::GetnUniformivEXT  (GLuint program, GLint location, GLsizei bufSize, GLint *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetnUniformivEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetnUniformivEXT(
		program,
		location,
		bufSize,
		params);
}

PFNGLREADNPIXELSEXTPROC gl::gl_ReadnPixelsEXT INIT_POINTER;
void gl::ReadnPixelsEXT  (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void *data, const char* file, int line)
{
	TRACE_FUNCTION("glReadnPixelsEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ReadnPixelsEXT(
		x,
		y,
		width,
		height,
		format,
		type,
		bufSize,
		data);
}

// GL_EXT_separate_shader_objects
PFNGLACTIVESHADERPROGRAMEXTPROC gl::gl_ActiveShaderProgramEXT INIT_POINTER;
void gl::ActiveShaderProgramEXT  (GLuint pipeline, GLuint program, const char* file, int line)
{
	TRACE_FUNCTION("glActiveShaderProgramEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ActiveShaderProgramEXT(
		pipeline,
		program);
}

PFNGLBINDPROGRAMPIPELINEEXTPROC gl::gl_BindProgramPipelineEXT INIT_POINTER;
void gl::BindProgramPipelineEXT  (GLuint pipeline, const char* file, int line)
{
	TRACE_FUNCTION("glBindProgramPipelineEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_BindProgramPipelineEXT(
		pipeline);
}

PFNGLCREATESHADERPROGRAMVEXTPROC gl::gl_CreateShaderProgramvEXT INIT_POINTER;
GLuint gl::CreateShaderProgramvEXT  (GLenum type, GLsizei count, const GLchar **strings, const char* file, int line)
{
	TRACE_FUNCTION("glCreateShaderProgramvEXT(...) called from " << get_path(file) << '(' << line << ')');
	return gl_CreateShaderProgramvEXT(
		type,
		count,
		strings);
}

PFNGLDELETEPROGRAMPIPELINESEXTPROC gl::gl_DeleteProgramPipelinesEXT INIT_POINTER;
void gl::DeleteProgramPipelinesEXT  (GLsizei n, const GLuint *pipelines, const char* file, int line)
{
	TRACE_FUNCTION("glDeleteProgramPipelinesEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_DeleteProgramPipelinesEXT(
		n,
		pipelines);
}

PFNGLGENPROGRAMPIPELINESEXTPROC gl::gl_GenProgramPipelinesEXT INIT_POINTER;
void gl::GenProgramPipelinesEXT  (GLsizei n, GLuint *pipelines, const char* file, int line)
{
	TRACE_FUNCTION("glGenProgramPipelinesEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_GenProgramPipelinesEXT(
		n,
		pipelines);
}

PFNGLGETPROGRAMPIPELINEINFOLOGEXTPROC gl::gl_GetProgramPipelineInfoLogEXT INIT_POINTER;
void gl::GetProgramPipelineInfoLogEXT  (GLuint pipeline, GLsizei bufSize, GLsizei *length, GLchar *infoLog, const char* file, int line)
{
	TRACE_FUNCTION("glGetProgramPipelineInfoLogEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetProgramPipelineInfoLogEXT(
		pipeline,
		bufSize,
		length,
		infoLog);
}

PFNGLGETPROGRAMPIPELINEIVEXTPROC gl::gl_GetProgramPipelineivEXT INIT_POINTER;
void gl::GetProgramPipelineivEXT  (GLuint pipeline, GLenum pname, GLint *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetProgramPipelineivEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetProgramPipelineivEXT(
		pipeline,
		pname,
		params);
}

PFNGLISPROGRAMPIPELINEEXTPROC gl::gl_IsProgramPipelineEXT INIT_POINTER;
GLboolean gl::IsProgramPipelineEXT  (GLuint pipeline, const char* file, int line)
{
	TRACE_FUNCTION("glIsProgramPipelineEXT(...) called from " << get_path(file) << '(' << line << ')');
	return gl_IsProgramPipelineEXT(
		pipeline);
}

PFNGLPROGRAMPARAMETERIEXTPROC gl::gl_ProgramParameteriEXT INIT_POINTER;
void gl::ProgramParameteriEXT  (GLuint program, GLenum pname, GLint value, const char* file, int line)
{
	TRACE_FUNCTION("glProgramParameteriEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramParameteriEXT(
		program,
		pname,
		value);
}

PFNGLPROGRAMUNIFORM1FEXTPROC gl::gl_ProgramUniform1fEXT INIT_POINTER;
void gl::ProgramUniform1fEXT  (GLuint program, GLint location, GLfloat v0, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniform1fEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniform1fEXT(
		program,
		location,
		v0);
}

PFNGLPROGRAMUNIFORM1FVEXTPROC gl::gl_ProgramUniform1fvEXT INIT_POINTER;
void gl::ProgramUniform1fvEXT  (GLuint program, GLint location, GLsizei count, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniform1fvEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniform1fvEXT(
		program,
		location,
		count,
		value);
}

PFNGLPROGRAMUNIFORM1IEXTPROC gl::gl_ProgramUniform1iEXT INIT_POINTER;
void gl::ProgramUniform1iEXT  (GLuint program, GLint location, GLint v0, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniform1iEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniform1iEXT(
		program,
		location,
		v0);
}

PFNGLPROGRAMUNIFORM1IVEXTPROC gl::gl_ProgramUniform1ivEXT INIT_POINTER;
void gl::ProgramUniform1ivEXT  (GLuint program, GLint location, GLsizei count, const GLint *value, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniform1ivEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniform1ivEXT(
		program,
		location,
		count,
		value);
}

PFNGLPROGRAMUNIFORM1UIEXTPROC gl::gl_ProgramUniform1uiEXT INIT_POINTER;
void gl::ProgramUniform1uiEXT  (GLuint program, GLint location, GLuint v0, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniform1uiEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniform1uiEXT(
		program,
		location,
		v0);
}

PFNGLPROGRAMUNIFORM1UIVEXTPROC gl::gl_ProgramUniform1uivEXT INIT_POINTER;
void gl::ProgramUniform1uivEXT  (GLuint program, GLint location, GLsizei count, const GLuint *value, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniform1uivEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniform1uivEXT(
		program,
		location,
		count,
		value);
}

PFNGLPROGRAMUNIFORM2FEXTPROC gl::gl_ProgramUniform2fEXT INIT_POINTER;
void gl::ProgramUniform2fEXT  (GLuint program, GLint location, GLfloat v0, GLfloat v1, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniform2fEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniform2fEXT(
		program,
		location,
		v0,
		v1);
}

PFNGLPROGRAMUNIFORM2FVEXTPROC gl::gl_ProgramUniform2fvEXT INIT_POINTER;
void gl::ProgramUniform2fvEXT  (GLuint program, GLint location, GLsizei count, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniform2fvEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniform2fvEXT(
		program,
		location,
		count,
		value);
}

PFNGLPROGRAMUNIFORM2IEXTPROC gl::gl_ProgramUniform2iEXT INIT_POINTER;
void gl::ProgramUniform2iEXT  (GLuint program, GLint location, GLint v0, GLint v1, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniform2iEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniform2iEXT(
		program,
		location,
		v0,
		v1);
}

PFNGLPROGRAMUNIFORM2IVEXTPROC gl::gl_ProgramUniform2ivEXT INIT_POINTER;
void gl::ProgramUniform2ivEXT  (GLuint program, GLint location, GLsizei count, const GLint *value, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniform2ivEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniform2ivEXT(
		program,
		location,
		count,
		value);
}

PFNGLPROGRAMUNIFORM2UIEXTPROC gl::gl_ProgramUniform2uiEXT INIT_POINTER;
void gl::ProgramUniform2uiEXT  (GLuint program, GLint location, GLuint v0, GLuint v1, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniform2uiEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniform2uiEXT(
		program,
		location,
		v0,
		v1);
}

PFNGLPROGRAMUNIFORM2UIVEXTPROC gl::gl_ProgramUniform2uivEXT INIT_POINTER;
void gl::ProgramUniform2uivEXT  (GLuint program, GLint location, GLsizei count, const GLuint *value, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniform2uivEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniform2uivEXT(
		program,
		location,
		count,
		value);
}

PFNGLPROGRAMUNIFORM3FEXTPROC gl::gl_ProgramUniform3fEXT INIT_POINTER;
void gl::ProgramUniform3fEXT  (GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniform3fEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniform3fEXT(
		program,
		location,
		v0,
		v1,
		v2);
}

PFNGLPROGRAMUNIFORM3FVEXTPROC gl::gl_ProgramUniform3fvEXT INIT_POINTER;
void gl::ProgramUniform3fvEXT  (GLuint program, GLint location, GLsizei count, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniform3fvEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniform3fvEXT(
		program,
		location,
		count,
		value);
}

PFNGLPROGRAMUNIFORM3IEXTPROC gl::gl_ProgramUniform3iEXT INIT_POINTER;
void gl::ProgramUniform3iEXT  (GLuint program, GLint location, GLint v0, GLint v1, GLint v2, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniform3iEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniform3iEXT(
		program,
		location,
		v0,
		v1,
		v2);
}

PFNGLPROGRAMUNIFORM3IVEXTPROC gl::gl_ProgramUniform3ivEXT INIT_POINTER;
void gl::ProgramUniform3ivEXT  (GLuint program, GLint location, GLsizei count, const GLint *value, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniform3ivEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniform3ivEXT(
		program,
		location,
		count,
		value);
}

PFNGLPROGRAMUNIFORM3UIEXTPROC gl::gl_ProgramUniform3uiEXT INIT_POINTER;
void gl::ProgramUniform3uiEXT  (GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniform3uiEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniform3uiEXT(
		program,
		location,
		v0,
		v1,
		v2);
}

PFNGLPROGRAMUNIFORM3UIVEXTPROC gl::gl_ProgramUniform3uivEXT INIT_POINTER;
void gl::ProgramUniform3uivEXT  (GLuint program, GLint location, GLsizei count, const GLuint *value, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniform3uivEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniform3uivEXT(
		program,
		location,
		count,
		value);
}

PFNGLPROGRAMUNIFORM4FEXTPROC gl::gl_ProgramUniform4fEXT INIT_POINTER;
void gl::ProgramUniform4fEXT  (GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniform4fEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniform4fEXT(
		program,
		location,
		v0,
		v1,
		v2,
		v3);
}

PFNGLPROGRAMUNIFORM4FVEXTPROC gl::gl_ProgramUniform4fvEXT INIT_POINTER;
void gl::ProgramUniform4fvEXT  (GLuint program, GLint location, GLsizei count, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniform4fvEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniform4fvEXT(
		program,
		location,
		count,
		value);
}

PFNGLPROGRAMUNIFORM4IEXTPROC gl::gl_ProgramUniform4iEXT INIT_POINTER;
void gl::ProgramUniform4iEXT  (GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniform4iEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniform4iEXT(
		program,
		location,
		v0,
		v1,
		v2,
		v3);
}

PFNGLPROGRAMUNIFORM4IVEXTPROC gl::gl_ProgramUniform4ivEXT INIT_POINTER;
void gl::ProgramUniform4ivEXT  (GLuint program, GLint location, GLsizei count, const GLint *value, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniform4ivEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniform4ivEXT(
		program,
		location,
		count,
		value);
}

PFNGLPROGRAMUNIFORM4UIEXTPROC gl::gl_ProgramUniform4uiEXT INIT_POINTER;
void gl::ProgramUniform4uiEXT  (GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniform4uiEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniform4uiEXT(
		program,
		location,
		v0,
		v1,
		v2,
		v3);
}

PFNGLPROGRAMUNIFORM4UIVEXTPROC gl::gl_ProgramUniform4uivEXT INIT_POINTER;
void gl::ProgramUniform4uivEXT  (GLuint program, GLint location, GLsizei count, const GLuint *value, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniform4uivEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniform4uivEXT(
		program,
		location,
		count,
		value);
}

PFNGLPROGRAMUNIFORMMATRIX2FVEXTPROC gl::gl_ProgramUniformMatrix2fvEXT INIT_POINTER;
void gl::ProgramUniformMatrix2fvEXT  (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniformMatrix2fvEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniformMatrix2fvEXT(
		program,
		location,
		count,
		transpose,
		value);
}

PFNGLPROGRAMUNIFORMMATRIX2X3FVEXTPROC gl::gl_ProgramUniformMatrix2x3fvEXT INIT_POINTER;
void gl::ProgramUniformMatrix2x3fvEXT  (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniformMatrix2x3fvEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniformMatrix2x3fvEXT(
		program,
		location,
		count,
		transpose,
		value);
}

PFNGLPROGRAMUNIFORMMATRIX2X4FVEXTPROC gl::gl_ProgramUniformMatrix2x4fvEXT INIT_POINTER;
void gl::ProgramUniformMatrix2x4fvEXT  (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniformMatrix2x4fvEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniformMatrix2x4fvEXT(
		program,
		location,
		count,
		transpose,
		value);
}

PFNGLPROGRAMUNIFORMMATRIX3FVEXTPROC gl::gl_ProgramUniformMatrix3fvEXT INIT_POINTER;
void gl::ProgramUniformMatrix3fvEXT  (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniformMatrix3fvEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniformMatrix3fvEXT(
		program,
		location,
		count,
		transpose,
		value);
}

PFNGLPROGRAMUNIFORMMATRIX3X2FVEXTPROC gl::gl_ProgramUniformMatrix3x2fvEXT INIT_POINTER;
void gl::ProgramUniformMatrix3x2fvEXT  (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniformMatrix3x2fvEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniformMatrix3x2fvEXT(
		program,
		location,
		count,
		transpose,
		value);
}

PFNGLPROGRAMUNIFORMMATRIX3X4FVEXTPROC gl::gl_ProgramUniformMatrix3x4fvEXT INIT_POINTER;
void gl::ProgramUniformMatrix3x4fvEXT  (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniformMatrix3x4fvEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniformMatrix3x4fvEXT(
		program,
		location,
		count,
		transpose,
		value);
}

PFNGLPROGRAMUNIFORMMATRIX4FVEXTPROC gl::gl_ProgramUniformMatrix4fvEXT INIT_POINTER;
void gl::ProgramUniformMatrix4fvEXT  (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniformMatrix4fvEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniformMatrix4fvEXT(
		program,
		location,
		count,
		transpose,
		value);
}

PFNGLPROGRAMUNIFORMMATRIX4X2FVEXTPROC gl::gl_ProgramUniformMatrix4x2fvEXT INIT_POINTER;
void gl::ProgramUniformMatrix4x2fvEXT  (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniformMatrix4x2fvEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniformMatrix4x2fvEXT(
		program,
		location,
		count,
		transpose,
		value);
}

PFNGLPROGRAMUNIFORMMATRIX4X3FVEXTPROC gl::gl_ProgramUniformMatrix4x3fvEXT INIT_POINTER;
void gl::ProgramUniformMatrix4x3fvEXT  (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniformMatrix4x3fvEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniformMatrix4x3fvEXT(
		program,
		location,
		count,
		transpose,
		value);
}

PFNGLUSEPROGRAMSTAGESEXTPROC gl::gl_UseProgramStagesEXT INIT_POINTER;
void gl::UseProgramStagesEXT  (GLuint pipeline, GLbitfield stages, GLuint program, const char* file, int line)
{
	TRACE_FUNCTION("glUseProgramStagesEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_UseProgramStagesEXT(
		pipeline,
		stages,
		program);
}

PFNGLVALIDATEPROGRAMPIPELINEEXTPROC gl::gl_ValidateProgramPipelineEXT INIT_POINTER;
void gl::ValidateProgramPipelineEXT  (GLuint pipeline, const char* file, int line)
{
	TRACE_FUNCTION("glValidateProgramPipelineEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ValidateProgramPipelineEXT(
		pipeline);
}

// GL_EXT_shader_pixel_local_storage2
PFNGLCLEARPIXELLOCALSTORAGEUIEXTPROC gl::gl_ClearPixelLocalStorageuiEXT INIT_POINTER;
void gl::ClearPixelLocalStorageuiEXT  (GLsizei offset, GLsizei n, const GLuint *values, const char* file, int line)
{
	TRACE_FUNCTION("glClearPixelLocalStorageuiEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_ClearPixelLocalStorageuiEXT(
		offset,
		n,
		values);
}

PFNGLFRAMEBUFFERPIXELLOCALSTORAGESIZEEXTPROC gl::gl_FramebufferPixelLocalStorageSizeEXT INIT_POINTER;
void gl::FramebufferPixelLocalStorageSizeEXT  (GLuint target, GLsizei size, const char* file, int line)
{
	TRACE_FUNCTION("glFramebufferPixelLocalStorageSizeEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_FramebufferPixelLocalStorageSizeEXT(
		target,
		size);
}

PFNGLGETFRAMEBUFFERPIXELLOCALSTORAGESIZEEXTPROC gl::gl_GetFramebufferPixelLocalStorageSizeEXT INIT_POINTER;
GLsizei gl::GetFramebufferPixelLocalStorageSizeEXT  (GLuint target, const char* file, int line)
{
	TRACE_FUNCTION("glGetFramebufferPixelLocalStorageSizeEXT(...) called from " << get_path(file) << '(' << line << ')');
	return gl_GetFramebufferPixelLocalStorageSizeEXT(
		target);
}

// GL_EXT_sparse_texture
PFNGLTEXPAGECOMMITMENTEXTPROC gl::gl_TexPageCommitmentEXT INIT_POINTER;
void gl::TexPageCommitmentEXT  (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLboolean commit, const char* file, int line)
{
	TRACE_FUNCTION("glTexPageCommitmentEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_TexPageCommitmentEXT(
		target,
		level,
		xoffset,
		yoffset,
		zoffset,
		width,
		height,
		depth,
		commit);
}

// GL_EXT_tessellation_shader
PFNGLPATCHPARAMETERIEXTPROC gl::gl_PatchParameteriEXT INIT_POINTER;
void gl::PatchParameteriEXT  (GLenum pname, GLint value, const char* file, int line)
{
	TRACE_FUNCTION("glPatchParameteriEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_PatchParameteriEXT(
		pname,
		value);
}

// GL_EXT_texture_border_clamp
PFNGLGETSAMPLERPARAMETERIIVEXTPROC gl::gl_GetSamplerParameterIivEXT INIT_POINTER;
void gl::GetSamplerParameterIivEXT  (GLuint sampler, GLenum pname, GLint *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetSamplerParameterIivEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetSamplerParameterIivEXT(
		sampler,
		pname,
		params);
}

PFNGLGETSAMPLERPARAMETERIUIVEXTPROC gl::gl_GetSamplerParameterIuivEXT INIT_POINTER;
void gl::GetSamplerParameterIuivEXT  (GLuint sampler, GLenum pname, GLuint *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetSamplerParameterIuivEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetSamplerParameterIuivEXT(
		sampler,
		pname,
		params);
}

PFNGLGETTEXPARAMETERIIVEXTPROC gl::gl_GetTexParameterIivEXT INIT_POINTER;
void gl::GetTexParameterIivEXT  (GLenum target, GLenum pname, GLint *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetTexParameterIivEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetTexParameterIivEXT(
		target,
		pname,
		params);
}

PFNGLGETTEXPARAMETERIUIVEXTPROC gl::gl_GetTexParameterIuivEXT INIT_POINTER;
void gl::GetTexParameterIuivEXT  (GLenum target, GLenum pname, GLuint *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetTexParameterIuivEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetTexParameterIuivEXT(
		target,
		pname,
		params);
}

PFNGLSAMPLERPARAMETERIIVEXTPROC gl::gl_SamplerParameterIivEXT INIT_POINTER;
void gl::SamplerParameterIivEXT  (GLuint sampler, GLenum pname, const GLint *param, const char* file, int line)
{
	TRACE_FUNCTION("glSamplerParameterIivEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_SamplerParameterIivEXT(
		sampler,
		pname,
		param);
}

PFNGLSAMPLERPARAMETERIUIVEXTPROC gl::gl_SamplerParameterIuivEXT INIT_POINTER;
void gl::SamplerParameterIuivEXT  (GLuint sampler, GLenum pname, const GLuint *param, const char* file, int line)
{
	TRACE_FUNCTION("glSamplerParameterIuivEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_SamplerParameterIuivEXT(
		sampler,
		pname,
		param);
}

PFNGLTEXPARAMETERIIVEXTPROC gl::gl_TexParameterIivEXT INIT_POINTER;
void gl::TexParameterIivEXT  (GLenum target, GLenum pname, const GLint *params, const char* file, int line)
{
	TRACE_FUNCTION("glTexParameterIivEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_TexParameterIivEXT(
		target,
		pname,
		params);
}

PFNGLTEXPARAMETERIUIVEXTPROC gl::gl_TexParameterIuivEXT INIT_POINTER;
void gl::TexParameterIuivEXT  (GLenum target, GLenum pname, const GLuint *params, const char* file, int line)
{
	TRACE_FUNCTION("glTexParameterIuivEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_TexParameterIuivEXT(
		target,
		pname,
		params);
}

// GL_EXT_texture_buffer
PFNGLTEXBUFFEREXTPROC gl::gl_TexBufferEXT INIT_POINTER;
void gl::TexBufferEXT  (GLenum target, GLenum internalformat, GLuint buffer, const char* file, int line)
{
	TRACE_FUNCTION("glTexBufferEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_TexBufferEXT(
		target,
		internalformat,
		buffer);
}

PFNGLTEXBUFFERRANGEEXTPROC gl::gl_TexBufferRangeEXT INIT_POINTER;
void gl::TexBufferRangeEXT  (GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size, const char* file, int line)
{
	TRACE_FUNCTION("glTexBufferRangeEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_TexBufferRangeEXT(
		target,
		internalformat,
		buffer,
		offset,
		size);
}

// GL_EXT_texture_storage
PFNGLTEXSTORAGE1DEXTPROC gl::gl_TexStorage1DEXT INIT_POINTER;
void gl::TexStorage1DEXT  (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, const char* file, int line)
{
	TRACE_FUNCTION("glTexStorage1DEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_TexStorage1DEXT(
		target,
		levels,
		internalformat,
		width);
}

PFNGLTEXSTORAGE2DEXTPROC gl::gl_TexStorage2DEXT INIT_POINTER;
void gl::TexStorage2DEXT  (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, const char* file, int line)
{
	TRACE_FUNCTION("glTexStorage2DEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_TexStorage2DEXT(
		target,
		levels,
		internalformat,
		width,
		height);
}

PFNGLTEXSTORAGE3DEXTPROC gl::gl_TexStorage3DEXT INIT_POINTER;
void gl::TexStorage3DEXT  (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, const char* file, int line)
{
	TRACE_FUNCTION("glTexStorage3DEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_TexStorage3DEXT(
		target,
		levels,
		internalformat,
		width,
		height,
		depth);
}

PFNGLTEXTURESTORAGE1DEXTPROC gl::gl_TextureStorage1DEXT INIT_POINTER;
void gl::TextureStorage1DEXT  (GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, const char* file, int line)
{
	TRACE_FUNCTION("glTextureStorage1DEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_TextureStorage1DEXT(
		texture,
		target,
		levels,
		internalformat,
		width);
}

PFNGLTEXTURESTORAGE2DEXTPROC gl::gl_TextureStorage2DEXT INIT_POINTER;
void gl::TextureStorage2DEXT  (GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, const char* file, int line)
{
	TRACE_FUNCTION("glTextureStorage2DEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_TextureStorage2DEXT(
		texture,
		target,
		levels,
		internalformat,
		width,
		height);
}

PFNGLTEXTURESTORAGE3DEXTPROC gl::gl_TextureStorage3DEXT INIT_POINTER;
void gl::TextureStorage3DEXT  (GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, const char* file, int line)
{
	TRACE_FUNCTION("glTextureStorage3DEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_TextureStorage3DEXT(
		texture,
		target,
		levels,
		internalformat,
		width,
		height,
		depth);
}

// GL_EXT_texture_view
PFNGLTEXTUREVIEWEXTPROC gl::gl_TextureViewEXT INIT_POINTER;
void gl::TextureViewEXT  (GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers, const char* file, int line)
{
	TRACE_FUNCTION("glTextureViewEXT(...) called from " << get_path(file) << '(' << line << ')');
	gl_TextureViewEXT(
		texture,
		target,
		origtexture,
		internalformat,
		minlevel,
		numlevels,
		minlayer,
		numlayers);
}

// GL_IMG_framebuffer_downsample
PFNGLFRAMEBUFFERTEXTURE2DDOWNSAMPLEIMGPROC gl::gl_FramebufferTexture2DDownsampleIMG INIT_POINTER;
void gl::FramebufferTexture2DDownsampleIMG  (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint xscale, GLint yscale, const char* file, int line)
{
	TRACE_FUNCTION("glFramebufferTexture2DDownsampleIMG(...) called from " << get_path(file) << '(' << line << ')');
	gl_FramebufferTexture2DDownsampleIMG(
		target,
		attachment,
		textarget,
		texture,
		level,
		xscale,
		yscale);
}

PFNGLFRAMEBUFFERTEXTURELAYERDOWNSAMPLEIMGPROC gl::gl_FramebufferTextureLayerDownsampleIMG INIT_POINTER;
void gl::FramebufferTextureLayerDownsampleIMG  (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer, GLint xscale, GLint yscale, const char* file, int line)
{
	TRACE_FUNCTION("glFramebufferTextureLayerDownsampleIMG(...) called from " << get_path(file) << '(' << line << ')');
	gl_FramebufferTextureLayerDownsampleIMG(
		target,
		attachment,
		texture,
		level,
		layer,
		xscale,
		yscale);
}

// GL_IMG_multisampled_render_to_texture
PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEIMGPROC gl::gl_FramebufferTexture2DMultisampleIMG INIT_POINTER;
void gl::FramebufferTexture2DMultisampleIMG  (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples, const char* file, int line)
{
	TRACE_FUNCTION("glFramebufferTexture2DMultisampleIMG(...) called from " << get_path(file) << '(' << line << ')');
	gl_FramebufferTexture2DMultisampleIMG(
		target,
		attachment,
		textarget,
		texture,
		level,
		samples);
}

PFNGLRENDERBUFFERSTORAGEMULTISAMPLEIMGPROC gl::gl_RenderbufferStorageMultisampleIMG INIT_POINTER;
void gl::RenderbufferStorageMultisampleIMG  (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, const char* file, int line)
{
	TRACE_FUNCTION("glRenderbufferStorageMultisampleIMG(...) called from " << get_path(file) << '(' << line << ')');
	gl_RenderbufferStorageMultisampleIMG(
		target,
		samples,
		internalformat,
		width,
		height);
}

// GL_INTEL_framebuffer_CMAA
PFNGLAPPLYFRAMEBUFFERATTACHMENTCMAAINTELPROC gl::gl_ApplyFramebufferAttachmentCMAAINTEL INIT_POINTER;
void gl::ApplyFramebufferAttachmentCMAAINTEL  (const char* file, int line)
{
	TRACE_FUNCTION("glApplyFramebufferAttachmentCMAAINTEL(...) called from " << get_path(file) << '(' << line << ')');
	gl_ApplyFramebufferAttachmentCMAAINTEL(
		);
}

// GL_INTEL_performance_query
PFNGLBEGINPERFQUERYINTELPROC gl::gl_BeginPerfQueryINTEL INIT_POINTER;
void gl::BeginPerfQueryINTEL  (GLuint queryHandle, const char* file, int line)
{
	TRACE_FUNCTION("glBeginPerfQueryINTEL(...) called from " << get_path(file) << '(' << line << ')');
	gl_BeginPerfQueryINTEL(
		queryHandle);
}

PFNGLCREATEPERFQUERYINTELPROC gl::gl_CreatePerfQueryINTEL INIT_POINTER;
void gl::CreatePerfQueryINTEL  (GLuint queryId, GLuint *queryHandle, const char* file, int line)
{
	TRACE_FUNCTION("glCreatePerfQueryINTEL(...) called from " << get_path(file) << '(' << line << ')');
	gl_CreatePerfQueryINTEL(
		queryId,
		queryHandle);
}

PFNGLDELETEPERFQUERYINTELPROC gl::gl_DeletePerfQueryINTEL INIT_POINTER;
void gl::DeletePerfQueryINTEL  (GLuint queryHandle, const char* file, int line)
{
	TRACE_FUNCTION("glDeletePerfQueryINTEL(...) called from " << get_path(file) << '(' << line << ')');
	gl_DeletePerfQueryINTEL(
		queryHandle);
}

PFNGLENDPERFQUERYINTELPROC gl::gl_EndPerfQueryINTEL INIT_POINTER;
void gl::EndPerfQueryINTEL  (GLuint queryHandle, const char* file, int line)
{
	TRACE_FUNCTION("glEndPerfQueryINTEL(...) called from " << get_path(file) << '(' << line << ')');
	gl_EndPerfQueryINTEL(
		queryHandle);
}

PFNGLGETFIRSTPERFQUERYIDINTELPROC gl::gl_GetFirstPerfQueryIdINTEL INIT_POINTER;
void gl::GetFirstPerfQueryIdINTEL  (GLuint *queryId, const char* file, int line)
{
	TRACE_FUNCTION("glGetFirstPerfQueryIdINTEL(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetFirstPerfQueryIdINTEL(
		queryId);
}

PFNGLGETNEXTPERFQUERYIDINTELPROC gl::gl_GetNextPerfQueryIdINTEL INIT_POINTER;
void gl::GetNextPerfQueryIdINTEL  (GLuint queryId, GLuint *nextQueryId, const char* file, int line)
{
	TRACE_FUNCTION("glGetNextPerfQueryIdINTEL(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetNextPerfQueryIdINTEL(
		queryId,
		nextQueryId);
}

PFNGLGETPERFCOUNTERINFOINTELPROC gl::gl_GetPerfCounterInfoINTEL INIT_POINTER;
void gl::GetPerfCounterInfoINTEL  (GLuint queryId, GLuint counterId, GLuint counterNameLength, GLchar *counterName, GLuint counterDescLength, GLchar *counterDesc, GLuint *counterOffset, GLuint *counterDataSize, GLuint *counterTypeEnum, GLuint *counterDataTypeEnum, GLuint64 *rawCounterMaxValue, const char* file, int line)
{
	TRACE_FUNCTION("glGetPerfCounterInfoINTEL(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetPerfCounterInfoINTEL(
		queryId,
		counterId,
		counterNameLength,
		counterName,
		counterDescLength,
		counterDesc,
		counterOffset,
		counterDataSize,
		counterTypeEnum,
		counterDataTypeEnum,
		rawCounterMaxValue);
}

PFNGLGETPERFQUERYDATAINTELPROC gl::gl_GetPerfQueryDataINTEL INIT_POINTER;
void gl::GetPerfQueryDataINTEL  (GLuint queryHandle, GLuint flags, GLsizei dataSize, GLvoid *data, GLuint *bytesWritten, const char* file, int line)
{
	TRACE_FUNCTION("glGetPerfQueryDataINTEL(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetPerfQueryDataINTEL(
		queryHandle,
		flags,
		dataSize,
		data,
		bytesWritten);
}

PFNGLGETPERFQUERYIDBYNAMEINTELPROC gl::gl_GetPerfQueryIdByNameINTEL INIT_POINTER;
void gl::GetPerfQueryIdByNameINTEL  (GLchar *queryName, GLuint *queryId, const char* file, int line)
{
	TRACE_FUNCTION("glGetPerfQueryIdByNameINTEL(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetPerfQueryIdByNameINTEL(
		queryName,
		queryId);
}

PFNGLGETPERFQUERYINFOINTELPROC gl::gl_GetPerfQueryInfoINTEL INIT_POINTER;
void gl::GetPerfQueryInfoINTEL  (GLuint queryId, GLuint queryNameLength, GLchar *queryName, GLuint *dataSize, GLuint *noCounters, GLuint *noInstances, GLuint *capsMask, const char* file, int line)
{
	TRACE_FUNCTION("glGetPerfQueryInfoINTEL(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetPerfQueryInfoINTEL(
		queryId,
		queryNameLength,
		queryName,
		dataSize,
		noCounters,
		noInstances,
		capsMask);
}

// GL_KHR_blend_equation_advanced
PFNGLBLENDBARRIERKHRPROC gl::gl_BlendBarrierKHR INIT_POINTER;
void gl::BlendBarrierKHR  (const char* file, int line)
{
	TRACE_FUNCTION("glBlendBarrierKHR(...) called from " << get_path(file) << '(' << line << ')');
	gl_BlendBarrierKHR(
		);
}

// GL_KHR_debug
PFNGLDEBUGMESSAGECONTROLKHRPROC gl::gl_DebugMessageControlKHR INIT_POINTER;
void gl::DebugMessageControlKHR  (GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled, const char* file, int line)
{
	TRACE_FUNCTION("glDebugMessageControlKHR(...) called from " << get_path(file) << '(' << line << ')');
	gl_DebugMessageControlKHR(
		source,
		type,
		severity,
		count,
		ids,
		enabled);
}

PFNGLDEBUGMESSAGEINSERTKHRPROC gl::gl_DebugMessageInsertKHR INIT_POINTER;
void gl::DebugMessageInsertKHR  (GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf, const char* file, int line)
{
	TRACE_FUNCTION("glDebugMessageInsertKHR(...) called from " << get_path(file) << '(' << line << ')');
	gl_DebugMessageInsertKHR(
		source,
		type,
		id,
		severity,
		length,
		buf);
}

PFNGLDEBUGMESSAGECALLBACKKHRPROC gl::gl_DebugMessageCallbackKHR INIT_POINTER;
void gl::DebugMessageCallbackKHR(GLDEBUGPROCKHR callback, const void *userParam, const char* file, int line)
{
	TRACE_FUNCTION("glDebugMessageCallbackKHR(...) called from " << get_path(file) << '(' << line << ')');
	gl_DebugMessageCallbackKHR(callback, userParam);
}

PFNGLGETDEBUGMESSAGELOGKHRPROC gl::gl_GetDebugMessageLogKHR INIT_POINTER;
GLuint gl::GetDebugMessageLogKHR  (GLuint count, GLsizei bufSize, GLenum *sources, GLenum *types, GLuint *ids, GLenum *severities, GLsizei *lengths, GLchar *messageLog, const char* file, int line)
{
	TRACE_FUNCTION("glGetDebugMessageLogKHR(...) called from " << get_path(file) << '(' << line << ')');
	return gl_GetDebugMessageLogKHR(
		count,
		bufSize,
		sources,
		types,
		ids,
		severities,
		lengths,
		messageLog);
}

PFNGLGETOBJECTLABELKHRPROC gl::gl_GetObjectLabelKHR INIT_POINTER;
void gl::GetObjectLabelKHR  (GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length, GLchar *label, const char* file, int line)
{
	TRACE_FUNCTION("glGetObjectLabelKHR(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetObjectLabelKHR(
		identifier,
		name,
		bufSize,
		length,
		label);
}

PFNGLGETOBJECTPTRLABELKHRPROC gl::gl_GetObjectPtrLabelKHR INIT_POINTER;
void gl::GetObjectPtrLabelKHR  (const void *ptr, GLsizei bufSize, GLsizei *length, GLchar *label, const char* file, int line)
{
	TRACE_FUNCTION("glGetObjectPtrLabelKHR(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetObjectPtrLabelKHR(
		ptr,
		bufSize,
		length,
		label);
}

PFNGLGETPOINTERVKHRPROC gl::gl_GetPointervKHR INIT_POINTER;
void gl::GetPointervKHR  (GLenum pname, void **params, const char* file, int line)
{
	TRACE_FUNCTION("glGetPointervKHR(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetPointervKHR(
		pname,
		params);
}

PFNGLOBJECTLABELKHRPROC gl::gl_ObjectLabelKHR INIT_POINTER;
void gl::ObjectLabelKHR  (GLenum identifier, GLuint name, GLsizei length, const GLchar *label, const char* file, int line)
{
	TRACE_FUNCTION("glObjectLabelKHR(...) called from " << get_path(file) << '(' << line << ')');
	gl_ObjectLabelKHR(
		identifier,
		name,
		length,
		label);
}

PFNGLOBJECTPTRLABELKHRPROC gl::gl_ObjectPtrLabelKHR INIT_POINTER;
void gl::ObjectPtrLabelKHR  (const void *ptr, GLsizei length, const GLchar *label, const char* file, int line)
{
	TRACE_FUNCTION("glObjectPtrLabelKHR(...) called from " << get_path(file) << '(' << line << ')');
	gl_ObjectPtrLabelKHR(
		ptr,
		length,
		label);
}

PFNGLPOPDEBUGGROUPKHRPROC gl::gl_PopDebugGroupKHR INIT_POINTER;
void gl::PopDebugGroupKHR  (const char* file, int line)
{
	TRACE_FUNCTION("glPopDebugGroupKHR(...) called from " << get_path(file) << '(' << line << ')');
	gl_PopDebugGroupKHR(
		);
}

PFNGLPUSHDEBUGGROUPKHRPROC gl::gl_PushDebugGroupKHR INIT_POINTER;
void gl::PushDebugGroupKHR  (GLenum source, GLuint id, GLsizei length, const GLchar *message, const char* file, int line)
{
	TRACE_FUNCTION("glPushDebugGroupKHR(...) called from " << get_path(file) << '(' << line << ')');
	gl_PushDebugGroupKHR(
		source,
		id,
		length,
		message);
}

// GL_KHR_robustness
PFNGLGETGRAPHICSRESETSTATUSKHRPROC gl::gl_GetGraphicsResetStatusKHR INIT_POINTER;
GLenum gl::GetGraphicsResetStatusKHR  (const char* file, int line)
{
	TRACE_FUNCTION("glGetGraphicsResetStatusKHR(...) called from " << get_path(file) << '(' << line << ')');
	return gl_GetGraphicsResetStatusKHR(
		);
}

PFNGLGETNUNIFORMFVKHRPROC gl::gl_GetnUniformfvKHR INIT_POINTER;
void gl::GetnUniformfvKHR  (GLuint program, GLint location, GLsizei bufSize, GLfloat *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetnUniformfvKHR(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetnUniformfvKHR(
		program,
		location,
		bufSize,
		params);
}

PFNGLGETNUNIFORMIVKHRPROC gl::gl_GetnUniformivKHR INIT_POINTER;
void gl::GetnUniformivKHR  (GLuint program, GLint location, GLsizei bufSize, GLint *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetnUniformivKHR(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetnUniformivKHR(
		program,
		location,
		bufSize,
		params);
}

PFNGLGETNUNIFORMUIVKHRPROC gl::gl_GetnUniformuivKHR INIT_POINTER;
void gl::GetnUniformuivKHR  (GLuint program, GLint location, GLsizei bufSize, GLuint *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetnUniformuivKHR(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetnUniformuivKHR(
		program,
		location,
		bufSize,
		params);
}

PFNGLREADNPIXELSKHRPROC gl::gl_ReadnPixelsKHR INIT_POINTER;
void gl::ReadnPixelsKHR  (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void *data, const char* file, int line)
{
	TRACE_FUNCTION("glReadnPixelsKHR(...) called from " << get_path(file) << '(' << line << ')');
	gl_ReadnPixelsKHR(
		x,
		y,
		width,
		height,
		format,
		type,
		bufSize,
		data);
}

// GL_NV_bindless_texture
PFNGLGETIMAGEHANDLENVPROC gl::gl_GetImageHandleNV INIT_POINTER;
GLuint64 gl::GetImageHandleNV  (GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum format, const char* file, int line)
{
	TRACE_FUNCTION("glGetImageHandleNV(...) called from " << get_path(file) << '(' << line << ')');
	return gl_GetImageHandleNV(
		texture,
		level,
		layered,
		layer,
		format);
}

PFNGLGETTEXTUREHANDLENVPROC gl::gl_GetTextureHandleNV INIT_POINTER;
GLuint64 gl::GetTextureHandleNV  (GLuint texture, const char* file, int line)
{
	TRACE_FUNCTION("glGetTextureHandleNV(...) called from " << get_path(file) << '(' << line << ')');
	return gl_GetTextureHandleNV(
		texture);
}

PFNGLGETTEXTURESAMPLERHANDLENVPROC gl::gl_GetTextureSamplerHandleNV INIT_POINTER;
GLuint64 gl::GetTextureSamplerHandleNV  (GLuint texture, GLuint sampler, const char* file, int line)
{
	TRACE_FUNCTION("glGetTextureSamplerHandleNV(...) called from " << get_path(file) << '(' << line << ')');
	return gl_GetTextureSamplerHandleNV(
		texture,
		sampler);
}

PFNGLISIMAGEHANDLERESIDENTNVPROC gl::gl_IsImageHandleResidentNV INIT_POINTER;
GLboolean gl::IsImageHandleResidentNV  (GLuint64 handle, const char* file, int line)
{
	TRACE_FUNCTION("glIsImageHandleResidentNV(...) called from " << get_path(file) << '(' << line << ')');
	return gl_IsImageHandleResidentNV(
		handle);
}

PFNGLISTEXTUREHANDLERESIDENTNVPROC gl::gl_IsTextureHandleResidentNV INIT_POINTER;
GLboolean gl::IsTextureHandleResidentNV  (GLuint64 handle, const char* file, int line)
{
	TRACE_FUNCTION("glIsTextureHandleResidentNV(...) called from " << get_path(file) << '(' << line << ')');
	return gl_IsTextureHandleResidentNV(
		handle);
}

PFNGLMAKEIMAGEHANDLENONRESIDENTNVPROC gl::gl_MakeImageHandleNonResidentNV INIT_POINTER;
void gl::MakeImageHandleNonResidentNV  (GLuint64 handle, const char* file, int line)
{
	TRACE_FUNCTION("glMakeImageHandleNonResidentNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_MakeImageHandleNonResidentNV(
		handle);
}

PFNGLMAKEIMAGEHANDLERESIDENTNVPROC gl::gl_MakeImageHandleResidentNV INIT_POINTER;
void gl::MakeImageHandleResidentNV  (GLuint64 handle, GLenum access, const char* file, int line)
{
	TRACE_FUNCTION("glMakeImageHandleResidentNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_MakeImageHandleResidentNV(
		handle,
		access);
}

PFNGLMAKETEXTUREHANDLENONRESIDENTNVPROC gl::gl_MakeTextureHandleNonResidentNV INIT_POINTER;
void gl::MakeTextureHandleNonResidentNV  (GLuint64 handle, const char* file, int line)
{
	TRACE_FUNCTION("glMakeTextureHandleNonResidentNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_MakeTextureHandleNonResidentNV(
		handle);
}

PFNGLMAKETEXTUREHANDLERESIDENTNVPROC gl::gl_MakeTextureHandleResidentNV INIT_POINTER;
void gl::MakeTextureHandleResidentNV  (GLuint64 handle, const char* file, int line)
{
	TRACE_FUNCTION("glMakeTextureHandleResidentNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_MakeTextureHandleResidentNV(
		handle);
}

PFNGLPROGRAMUNIFORMHANDLEUI64NVPROC gl::gl_ProgramUniformHandleui64NV INIT_POINTER;
void gl::ProgramUniformHandleui64NV  (GLuint program, GLint location, GLuint64 value, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniformHandleui64NV(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniformHandleui64NV(
		program,
		location,
		value);
}

PFNGLPROGRAMUNIFORMHANDLEUI64VNVPROC gl::gl_ProgramUniformHandleui64vNV INIT_POINTER;
void gl::ProgramUniformHandleui64vNV  (GLuint program, GLint location, GLsizei count, const GLuint64 *values, const char* file, int line)
{
	TRACE_FUNCTION("glProgramUniformHandleui64vNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramUniformHandleui64vNV(
		program,
		location,
		count,
		values);
}

PFNGLUNIFORMHANDLEUI64NVPROC gl::gl_UniformHandleui64NV INIT_POINTER;
void gl::UniformHandleui64NV  (GLint location, GLuint64 value, const char* file, int line)
{
	TRACE_FUNCTION("glUniformHandleui64NV(...) called from " << get_path(file) << '(' << line << ')');
	gl_UniformHandleui64NV(
		location,
		value);
}

PFNGLUNIFORMHANDLEUI64VNVPROC gl::gl_UniformHandleui64vNV INIT_POINTER;
void gl::UniformHandleui64vNV  (GLint location, GLsizei count, const GLuint64 *value, const char* file, int line)
{
	TRACE_FUNCTION("glUniformHandleui64vNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_UniformHandleui64vNV(
		location,
		count,
		value);
}

// GL_NV_blend_equation_advanced
PFNGLBLENDBARRIERNVPROC gl::gl_BlendBarrierNV INIT_POINTER;
void gl::BlendBarrierNV  (const char* file, int line)
{
	TRACE_FUNCTION("glBlendBarrierNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_BlendBarrierNV(
		);
}

PFNGLBLENDPARAMETERINVPROC gl::gl_BlendParameteriNV INIT_POINTER;
void gl::BlendParameteriNV  (GLenum pname, GLint value, const char* file, int line)
{
	TRACE_FUNCTION("glBlendParameteriNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_BlendParameteriNV(
		pname,
		value);
}

// GL_NV_conditional_render
PFNGLBEGINCONDITIONALRENDERNVPROC gl::gl_BeginConditionalRenderNV INIT_POINTER;
void gl::BeginConditionalRenderNV  (GLuint id, GLenum mode, const char* file, int line)
{
	TRACE_FUNCTION("glBeginConditionalRenderNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_BeginConditionalRenderNV(
		id,
		mode);
}

PFNGLENDCONDITIONALRENDERNVPROC gl::gl_EndConditionalRenderNV INIT_POINTER;
void gl::EndConditionalRenderNV  (const char* file, int line)
{
	TRACE_FUNCTION("glEndConditionalRenderNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_EndConditionalRenderNV(
		);
}

// GL_NV_conservative_raster
PFNGLSUBPIXELPRECISIONBIASNVPROC gl::gl_SubpixelPrecisionBiasNV INIT_POINTER;
void gl::SubpixelPrecisionBiasNV  (GLuint xbits, GLuint ybits, const char* file, int line)
{
	TRACE_FUNCTION("glSubpixelPrecisionBiasNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_SubpixelPrecisionBiasNV(
		xbits,
		ybits);
}

// GL_NV_copy_buffer
PFNGLCOPYBUFFERSUBDATANVPROC gl::gl_CopyBufferSubDataNV INIT_POINTER;
void gl::CopyBufferSubDataNV  (GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size, const char* file, int line)
{
	TRACE_FUNCTION("glCopyBufferSubDataNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_CopyBufferSubDataNV(
		readTarget,
		writeTarget,
		readOffset,
		writeOffset,
		size);
}

// GL_NV_coverage_sample
PFNGLCOVERAGEMASKNVPROC gl::gl_CoverageMaskNV INIT_POINTER;
void gl::CoverageMaskNV  (GLboolean mask, const char* file, int line)
{
	TRACE_FUNCTION("glCoverageMaskNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_CoverageMaskNV(
		mask);
}

PFNGLCOVERAGEOPERATIONNVPROC gl::gl_CoverageOperationNV INIT_POINTER;
void gl::CoverageOperationNV  (GLenum operation, const char* file, int line)
{
	TRACE_FUNCTION("glCoverageOperationNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_CoverageOperationNV(
		operation);
}

// GL_NV_draw_buffers
PFNGLDRAWBUFFERSNVPROC gl::gl_DrawBuffersNV INIT_POINTER;
void gl::DrawBuffersNV  (GLsizei n, const GLenum *bufs, const char* file, int line)
{
	TRACE_FUNCTION("glDrawBuffersNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_DrawBuffersNV(
		n,
		bufs);
}

// GL_NV_draw_instanced
PFNGLDRAWARRAYSINSTANCEDNVPROC gl::gl_DrawArraysInstancedNV INIT_POINTER;
void gl::DrawArraysInstancedNV  (GLenum mode, GLint first, GLsizei count, GLsizei primcount, const char* file, int line)
{
	TRACE_FUNCTION("glDrawArraysInstancedNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_DrawArraysInstancedNV(
		mode,
		first,
		count,
		primcount);
}

PFNGLDRAWELEMENTSINSTANCEDNVPROC gl::gl_DrawElementsInstancedNV INIT_POINTER;
void gl::DrawElementsInstancedNV  (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei primcount, const char* file, int line)
{
	TRACE_FUNCTION("glDrawElementsInstancedNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_DrawElementsInstancedNV(
		mode,
		count,
		type,
		indices,
		primcount);
}

// GL_NV_fence
PFNGLDELETEFENCESNVPROC gl::gl_DeleteFencesNV INIT_POINTER;
void gl::DeleteFencesNV  (GLsizei n, const GLuint *fences, const char* file, int line)
{
	TRACE_FUNCTION("glDeleteFencesNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_DeleteFencesNV(
		n,
		fences);
}

PFNGLFINISHFENCENVPROC gl::gl_FinishFenceNV INIT_POINTER;
void gl::FinishFenceNV  (GLuint fence, const char* file, int line)
{
	TRACE_FUNCTION("glFinishFenceNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_FinishFenceNV(
		fence);
}

PFNGLGENFENCESNVPROC gl::gl_GenFencesNV INIT_POINTER;
void gl::GenFencesNV  (GLsizei n, GLuint *fences, const char* file, int line)
{
	TRACE_FUNCTION("glGenFencesNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_GenFencesNV(
		n,
		fences);
}

PFNGLGETFENCEIVNVPROC gl::gl_GetFenceivNV INIT_POINTER;
void gl::GetFenceivNV  (GLuint fence, GLenum pname, GLint *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetFenceivNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetFenceivNV(
		fence,
		pname,
		params);
}

PFNGLISFENCENVPROC gl::gl_IsFenceNV INIT_POINTER;
GLboolean gl::IsFenceNV  (GLuint fence, const char* file, int line)
{
	TRACE_FUNCTION("glIsFenceNV(...) called from " << get_path(file) << '(' << line << ')');
	return gl_IsFenceNV(
		fence);
}

PFNGLSETFENCENVPROC gl::gl_SetFenceNV INIT_POINTER;
void gl::SetFenceNV  (GLuint fence, GLenum condition, const char* file, int line)
{
	TRACE_FUNCTION("glSetFenceNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_SetFenceNV(
		fence,
		condition);
}

PFNGLTESTFENCENVPROC gl::gl_TestFenceNV INIT_POINTER;
GLboolean gl::TestFenceNV  (GLuint fence, const char* file, int line)
{
	TRACE_FUNCTION("glTestFenceNV(...) called from " << get_path(file) << '(' << line << ')');
	return gl_TestFenceNV(
		fence);
}

// GL_NV_fragment_coverage_to_color
PFNGLFRAGMENTCOVERAGECOLORNVPROC gl::gl_FragmentCoverageColorNV INIT_POINTER;
void gl::FragmentCoverageColorNV  (GLuint color, const char* file, int line)
{
	TRACE_FUNCTION("glFragmentCoverageColorNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_FragmentCoverageColorNV(
		color);
}

// GL_NV_framebuffer_blit
PFNGLBLITFRAMEBUFFERNVPROC gl::gl_BlitFramebufferNV INIT_POINTER;
void gl::BlitFramebufferNV  (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter, const char* file, int line)
{
	TRACE_FUNCTION("glBlitFramebufferNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_BlitFramebufferNV(
		srcX0,
		srcY0,
		srcX1,
		srcY1,
		dstX0,
		dstY0,
		dstX1,
		dstY1,
		mask,
		filter);
}

// GL_NV_framebuffer_mixed_samples
PFNGLCOVERAGEMODULATIONNVPROC gl::gl_CoverageModulationNV INIT_POINTER;
void gl::CoverageModulationNV  (GLenum components, const char* file, int line)
{
	TRACE_FUNCTION("glCoverageModulationNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_CoverageModulationNV(
		components);
}

PFNGLCOVERAGEMODULATIONTABLENVPROC gl::gl_CoverageModulationTableNV INIT_POINTER;
void gl::CoverageModulationTableNV  (GLsizei n, const GLfloat *v, const char* file, int line)
{
	TRACE_FUNCTION("glCoverageModulationTableNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_CoverageModulationTableNV(
		n,
		v);
}

PFNGLGETCOVERAGEMODULATIONTABLENVPROC gl::gl_GetCoverageModulationTableNV INIT_POINTER;
void gl::GetCoverageModulationTableNV  (GLsizei bufsize, GLfloat *v, const char* file, int line)
{
	TRACE_FUNCTION("glGetCoverageModulationTableNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetCoverageModulationTableNV(
		bufsize,
		v);
}

// GL_NV_framebuffer_multisample
PFNGLRENDERBUFFERSTORAGEMULTISAMPLENVPROC gl::gl_RenderbufferStorageMultisampleNV INIT_POINTER;
void gl::RenderbufferStorageMultisampleNV  (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, const char* file, int line)
{
	TRACE_FUNCTION("glRenderbufferStorageMultisampleNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_RenderbufferStorageMultisampleNV(
		target,
		samples,
		internalformat,
		width,
		height);
}

// GL_NV_instanced_arrays
PFNGLVERTEXATTRIBDIVISORNVPROC gl::gl_VertexAttribDivisorNV INIT_POINTER;
void gl::VertexAttribDivisorNV  (GLuint index, GLuint divisor, const char* file, int line)
{
	TRACE_FUNCTION("glVertexAttribDivisorNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_VertexAttribDivisorNV(
		index,
		divisor);
}

// GL_NV_internalformat_sample_query
PFNGLGETINTERNALFORMATSAMPLEIVNVPROC gl::gl_GetInternalformatSampleivNV INIT_POINTER;
void gl::GetInternalformatSampleivNV  (GLenum target, GLenum internalformat, GLsizei samples, GLenum pname, GLsizei bufSize, GLint *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetInternalformatSampleivNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetInternalformatSampleivNV(
		target,
		internalformat,
		samples,
		pname,
		bufSize,
		params);
}

// GL_NV_non_square_matrices
PFNGLUNIFORMMATRIX2X3FVNVPROC gl::gl_UniformMatrix2x3fvNV INIT_POINTER;
void gl::UniformMatrix2x3fvNV  (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glUniformMatrix2x3fvNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_UniformMatrix2x3fvNV(
		location,
		count,
		transpose,
		value);
}

PFNGLUNIFORMMATRIX2X4FVNVPROC gl::gl_UniformMatrix2x4fvNV INIT_POINTER;
void gl::UniformMatrix2x4fvNV  (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glUniformMatrix2x4fvNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_UniformMatrix2x4fvNV(
		location,
		count,
		transpose,
		value);
}

PFNGLUNIFORMMATRIX3X2FVNVPROC gl::gl_UniformMatrix3x2fvNV INIT_POINTER;
void gl::UniformMatrix3x2fvNV  (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glUniformMatrix3x2fvNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_UniformMatrix3x2fvNV(
		location,
		count,
		transpose,
		value);
}

PFNGLUNIFORMMATRIX3X4FVNVPROC gl::gl_UniformMatrix3x4fvNV INIT_POINTER;
void gl::UniformMatrix3x4fvNV  (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glUniformMatrix3x4fvNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_UniformMatrix3x4fvNV(
		location,
		count,
		transpose,
		value);
}

PFNGLUNIFORMMATRIX4X2FVNVPROC gl::gl_UniformMatrix4x2fvNV INIT_POINTER;
void gl::UniformMatrix4x2fvNV  (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glUniformMatrix4x2fvNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_UniformMatrix4x2fvNV(
		location,
		count,
		transpose,
		value);
}

PFNGLUNIFORMMATRIX4X3FVNVPROC gl::gl_UniformMatrix4x3fvNV INIT_POINTER;
void gl::UniformMatrix4x3fvNV  (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glUniformMatrix4x3fvNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_UniformMatrix4x3fvNV(
		location,
		count,
		transpose,
		value);
}

// GL_NV_path_rendering
PFNGLCOPYPATHNVPROC gl::gl_CopyPathNV INIT_POINTER;
void gl::CopyPathNV  (GLuint resultPath, GLuint srcPath, const char* file, int line)
{
	TRACE_FUNCTION("glCopyPathNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_CopyPathNV(
		resultPath,
		srcPath);
}

PFNGLCOVERFILLPATHINSTANCEDNVPROC gl::gl_CoverFillPathInstancedNV INIT_POINTER;
void gl::CoverFillPathInstancedNV  (GLsizei numPaths, GLenum pathNameType, const void *paths, GLuint pathBase, GLenum coverMode, GLenum transformType, const GLfloat *transformValues, const char* file, int line)
{
	TRACE_FUNCTION("glCoverFillPathInstancedNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_CoverFillPathInstancedNV(
		numPaths,
		pathNameType,
		paths,
		pathBase,
		coverMode,
		transformType,
		transformValues);
}

PFNGLCOVERFILLPATHNVPROC gl::gl_CoverFillPathNV INIT_POINTER;
void gl::CoverFillPathNV  (GLuint path, GLenum coverMode, const char* file, int line)
{
	TRACE_FUNCTION("glCoverFillPathNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_CoverFillPathNV(
		path,
		coverMode);
}

PFNGLCOVERSTROKEPATHINSTANCEDNVPROC gl::gl_CoverStrokePathInstancedNV INIT_POINTER;
void gl::CoverStrokePathInstancedNV  (GLsizei numPaths, GLenum pathNameType, const void *paths, GLuint pathBase, GLenum coverMode, GLenum transformType, const GLfloat *transformValues, const char* file, int line)
{
	TRACE_FUNCTION("glCoverStrokePathInstancedNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_CoverStrokePathInstancedNV(
		numPaths,
		pathNameType,
		paths,
		pathBase,
		coverMode,
		transformType,
		transformValues);
}

PFNGLCOVERSTROKEPATHNVPROC gl::gl_CoverStrokePathNV INIT_POINTER;
void gl::CoverStrokePathNV  (GLuint path, GLenum coverMode, const char* file, int line)
{
	TRACE_FUNCTION("glCoverStrokePathNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_CoverStrokePathNV(
		path,
		coverMode);
}

PFNGLDELETEPATHSNVPROC gl::gl_DeletePathsNV INIT_POINTER;
void gl::DeletePathsNV  (GLuint path, GLsizei range, const char* file, int line)
{
	TRACE_FUNCTION("glDeletePathsNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_DeletePathsNV(
		path,
		range);
}

PFNGLGENPATHSNVPROC gl::gl_GenPathsNV INIT_POINTER;
GLuint gl::GenPathsNV  (GLsizei range, const char* file, int line)
{
	TRACE_FUNCTION("glGenPathsNV(...) called from " << get_path(file) << '(' << line << ')');
	return gl_GenPathsNV(
		range);
}

PFNGLGETPATHCOMMANDSNVPROC gl::gl_GetPathCommandsNV INIT_POINTER;
void gl::GetPathCommandsNV  (GLuint path, GLubyte *commands, const char* file, int line)
{
	TRACE_FUNCTION("glGetPathCommandsNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetPathCommandsNV(
		path,
		commands);
}

PFNGLGETPATHCOORDSNVPROC gl::gl_GetPathCoordsNV INIT_POINTER;
void gl::GetPathCoordsNV  (GLuint path, GLfloat *coords, const char* file, int line)
{
	TRACE_FUNCTION("glGetPathCoordsNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetPathCoordsNV(
		path,
		coords);
}

PFNGLGETPATHDASHARRAYNVPROC gl::gl_GetPathDashArrayNV INIT_POINTER;
void gl::GetPathDashArrayNV  (GLuint path, GLfloat *dashArray, const char* file, int line)
{
	TRACE_FUNCTION("glGetPathDashArrayNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetPathDashArrayNV(
		path,
		dashArray);
}

PFNGLGETPATHLENGTHNVPROC gl::gl_GetPathLengthNV INIT_POINTER;
GLfloat gl::GetPathLengthNV  (GLuint path, GLsizei startSegment, GLsizei numSegments, const char* file, int line)
{
	TRACE_FUNCTION("glGetPathLengthNV(...) called from " << get_path(file) << '(' << line << ')');
	return gl_GetPathLengthNV(
		path,
		startSegment,
		numSegments);
}

PFNGLGETPATHMETRICRANGENVPROC gl::gl_GetPathMetricRangeNV INIT_POINTER;
void gl::GetPathMetricRangeNV  (GLbitfield metricQueryMask, GLuint firstPathName, GLsizei numPaths, GLsizei stride, GLfloat *metrics, const char* file, int line)
{
	TRACE_FUNCTION("glGetPathMetricRangeNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetPathMetricRangeNV(
		metricQueryMask,
		firstPathName,
		numPaths,
		stride,
		metrics);
}

PFNGLGETPATHMETRICSNVPROC gl::gl_GetPathMetricsNV INIT_POINTER;
void gl::GetPathMetricsNV  (GLbitfield metricQueryMask, GLsizei numPaths, GLenum pathNameType, const void *paths, GLuint pathBase, GLsizei stride, GLfloat *metrics, const char* file, int line)
{
	TRACE_FUNCTION("glGetPathMetricsNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetPathMetricsNV(
		metricQueryMask,
		numPaths,
		pathNameType,
		paths,
		pathBase,
		stride,
		metrics);
}

PFNGLGETPATHPARAMETERFVNVPROC gl::gl_GetPathParameterfvNV INIT_POINTER;
void gl::GetPathParameterfvNV  (GLuint path, GLenum pname, GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glGetPathParameterfvNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetPathParameterfvNV(
		path,
		pname,
		value);
}

PFNGLGETPATHPARAMETERIVNVPROC gl::gl_GetPathParameterivNV INIT_POINTER;
void gl::GetPathParameterivNV  (GLuint path, GLenum pname, GLint *value, const char* file, int line)
{
	TRACE_FUNCTION("glGetPathParameterivNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetPathParameterivNV(
		path,
		pname,
		value);
}

PFNGLGETPATHSPACINGNVPROC gl::gl_GetPathSpacingNV INIT_POINTER;
void gl::GetPathSpacingNV  (GLenum pathListMode, GLsizei numPaths, GLenum pathNameType, const void *paths, GLuint pathBase, GLfloat advanceScale, GLfloat kerningScale, GLenum transformType, GLfloat *returnedSpacing, const char* file, int line)
{
	TRACE_FUNCTION("glGetPathSpacingNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetPathSpacingNV(
		pathListMode,
		numPaths,
		pathNameType,
		paths,
		pathBase,
		advanceScale,
		kerningScale,
		transformType,
		returnedSpacing);
}

PFNGLGETPROGRAMRESOURCEFVNVPROC gl::gl_GetProgramResourcefvNV INIT_POINTER;
void gl::GetProgramResourcefvNV  (GLuint program, GLenum programInterface, GLuint index, GLsizei propCount, const GLenum *props, GLsizei bufSize, GLsizei *length, GLfloat *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetProgramResourcefvNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetProgramResourcefvNV(
		program,
		programInterface,
		index,
		propCount,
		props,
		bufSize,
		length,
		params);
}

PFNGLINTERPOLATEPATHSNVPROC gl::gl_InterpolatePathsNV INIT_POINTER;
void gl::InterpolatePathsNV  (GLuint resultPath, GLuint pathA, GLuint pathB, GLfloat weight, const char* file, int line)
{
	TRACE_FUNCTION("glInterpolatePathsNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_InterpolatePathsNV(
		resultPath,
		pathA,
		pathB,
		weight);
}

PFNGLISPATHNVPROC gl::gl_IsPathNV INIT_POINTER;
GLboolean gl::IsPathNV  (GLuint path, const char* file, int line)
{
	TRACE_FUNCTION("glIsPathNV(...) called from " << get_path(file) << '(' << line << ')');
	return gl_IsPathNV(
		path);
}

PFNGLISPOINTINFILLPATHNVPROC gl::gl_IsPointInFillPathNV INIT_POINTER;
GLboolean gl::IsPointInFillPathNV  (GLuint path, GLuint mask, GLfloat x, GLfloat y, const char* file, int line)
{
	TRACE_FUNCTION("glIsPointInFillPathNV(...) called from " << get_path(file) << '(' << line << ')');
	return gl_IsPointInFillPathNV(
		path,
		mask,
		x,
		y);
}

PFNGLISPOINTINSTROKEPATHNVPROC gl::gl_IsPointInStrokePathNV INIT_POINTER;
GLboolean gl::IsPointInStrokePathNV  (GLuint path, GLfloat x, GLfloat y, const char* file, int line)
{
	TRACE_FUNCTION("glIsPointInStrokePathNV(...) called from " << get_path(file) << '(' << line << ')');
	return gl_IsPointInStrokePathNV(
		path,
		x,
		y);
}

PFNGLMATRIXLOAD3X2FNVPROC gl::gl_MatrixLoad3x2fNV INIT_POINTER;
void gl::MatrixLoad3x2fNV  (GLenum matrixMode, const GLfloat *m, const char* file, int line)
{
	TRACE_FUNCTION("glMatrixLoad3x2fNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_MatrixLoad3x2fNV(
		matrixMode,
		m);
}

PFNGLMATRIXLOAD3X3FNVPROC gl::gl_MatrixLoad3x3fNV INIT_POINTER;
void gl::MatrixLoad3x3fNV  (GLenum matrixMode, const GLfloat *m, const char* file, int line)
{
	TRACE_FUNCTION("glMatrixLoad3x3fNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_MatrixLoad3x3fNV(
		matrixMode,
		m);
}

PFNGLMATRIXLOADTRANSPOSE3X3FNVPROC gl::gl_MatrixLoadTranspose3x3fNV INIT_POINTER;
void gl::MatrixLoadTranspose3x3fNV  (GLenum matrixMode, const GLfloat *m, const char* file, int line)
{
	TRACE_FUNCTION("glMatrixLoadTranspose3x3fNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_MatrixLoadTranspose3x3fNV(
		matrixMode,
		m);
}

PFNGLMATRIXMULT3X2FNVPROC gl::gl_MatrixMult3x2fNV INIT_POINTER;
void gl::MatrixMult3x2fNV  (GLenum matrixMode, const GLfloat *m, const char* file, int line)
{
	TRACE_FUNCTION("glMatrixMult3x2fNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_MatrixMult3x2fNV(
		matrixMode,
		m);
}

PFNGLMATRIXMULT3X3FNVPROC gl::gl_MatrixMult3x3fNV INIT_POINTER;
void gl::MatrixMult3x3fNV  (GLenum matrixMode, const GLfloat *m, const char* file, int line)
{
	TRACE_FUNCTION("glMatrixMult3x3fNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_MatrixMult3x3fNV(
		matrixMode,
		m);
}

PFNGLMATRIXMULTTRANSPOSE3X3FNVPROC gl::gl_MatrixMultTranspose3x3fNV INIT_POINTER;
void gl::MatrixMultTranspose3x3fNV  (GLenum matrixMode, const GLfloat *m, const char* file, int line)
{
	TRACE_FUNCTION("glMatrixMultTranspose3x3fNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_MatrixMultTranspose3x3fNV(
		matrixMode,
		m);
}

PFNGLPATHCOMMANDSNVPROC gl::gl_PathCommandsNV INIT_POINTER;
void gl::PathCommandsNV  (GLuint path, GLsizei numCommands, const GLubyte *commands, GLsizei numCoords, GLenum coordType, const void *coords, const char* file, int line)
{
	TRACE_FUNCTION("glPathCommandsNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_PathCommandsNV(
		path,
		numCommands,
		commands,
		numCoords,
		coordType,
		coords);
}

PFNGLPATHCOORDSNVPROC gl::gl_PathCoordsNV INIT_POINTER;
void gl::PathCoordsNV  (GLuint path, GLsizei numCoords, GLenum coordType, const void *coords, const char* file, int line)
{
	TRACE_FUNCTION("glPathCoordsNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_PathCoordsNV(
		path,
		numCoords,
		coordType,
		coords);
}

PFNGLPATHCOVERDEPTHFUNCNVPROC gl::gl_PathCoverDepthFuncNV INIT_POINTER;
void gl::PathCoverDepthFuncNV  (GLenum func, const char* file, int line)
{
	TRACE_FUNCTION("glPathCoverDepthFuncNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_PathCoverDepthFuncNV(
		func);
}

PFNGLPATHDASHARRAYNVPROC gl::gl_PathDashArrayNV INIT_POINTER;
void gl::PathDashArrayNV  (GLuint path, GLsizei dashCount, const GLfloat *dashArray, const char* file, int line)
{
	TRACE_FUNCTION("glPathDashArrayNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_PathDashArrayNV(
		path,
		dashCount,
		dashArray);
}

PFNGLPATHGLYPHINDEXARRAYNVPROC gl::gl_PathGlyphIndexArrayNV INIT_POINTER;
GLenum gl::PathGlyphIndexArrayNV  (GLuint firstPathName, GLenum fontTarget, const void *fontName, GLbitfield fontStyle, GLuint firstGlyphIndex, GLsizei numGlyphs, GLuint pathParameterTemplate, GLfloat emScale, const char* file, int line)
{
	TRACE_FUNCTION("glPathGlyphIndexArrayNV(...) called from " << get_path(file) << '(' << line << ')');
	return gl_PathGlyphIndexArrayNV(
		firstPathName,
		fontTarget,
		fontName,
		fontStyle,
		firstGlyphIndex,
		numGlyphs,
		pathParameterTemplate,
		emScale);
}

PFNGLPATHGLYPHINDEXRANGENVPROC gl::gl_PathGlyphIndexRangeNV INIT_POINTER;
GLenum gl::PathGlyphIndexRangeNV  (GLenum fontTarget, const void *fontName, GLbitfield fontStyle, GLuint pathParameterTemplate, GLfloat emScale, GLuint baseAndCount[2], const char* file, int line)
{
	TRACE_FUNCTION("glPathGlyphIndexRangeNV(...) called from " << get_path(file) << '(' << line << ')');
	return gl_PathGlyphIndexRangeNV(
		fontTarget,
		fontName,
		fontStyle,
		pathParameterTemplate,
		emScale,
		baseAndCount);
}

PFNGLPATHGLYPHRANGENVPROC gl::gl_PathGlyphRangeNV INIT_POINTER;
void gl::PathGlyphRangeNV  (GLuint firstPathName, GLenum fontTarget, const void *fontName, GLbitfield fontStyle, GLuint firstGlyph, GLsizei numGlyphs, GLenum handleMissingGlyphs, GLuint pathParameterTemplate, GLfloat emScale, const char* file, int line)
{
	TRACE_FUNCTION("glPathGlyphRangeNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_PathGlyphRangeNV(
		firstPathName,
		fontTarget,
		fontName,
		fontStyle,
		firstGlyph,
		numGlyphs,
		handleMissingGlyphs,
		pathParameterTemplate,
		emScale);
}

PFNGLPATHGLYPHSNVPROC gl::gl_PathGlyphsNV INIT_POINTER;
void gl::PathGlyphsNV  (GLuint firstPathName, GLenum fontTarget, const void *fontName, GLbitfield fontStyle, GLsizei numGlyphs, GLenum type, const void *charcodes, GLenum handleMissingGlyphs, GLuint pathParameterTemplate, GLfloat emScale, const char* file, int line)
{
	TRACE_FUNCTION("glPathGlyphsNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_PathGlyphsNV(
		firstPathName,
		fontTarget,
		fontName,
		fontStyle,
		numGlyphs,
		type,
		charcodes,
		handleMissingGlyphs,
		pathParameterTemplate,
		emScale);
}

PFNGLPATHMEMORYGLYPHINDEXARRAYNVPROC gl::gl_PathMemoryGlyphIndexArrayNV INIT_POINTER;
GLenum gl::PathMemoryGlyphIndexArrayNV  (GLuint firstPathName, GLenum fontTarget, GLsizeiptr fontSize, const void *fontData, GLsizei faceIndex, GLuint firstGlyphIndex, GLsizei numGlyphs, GLuint pathParameterTemplate, GLfloat emScale, const char* file, int line)
{
	TRACE_FUNCTION("glPathMemoryGlyphIndexArrayNV(...) called from " << get_path(file) << '(' << line << ')');
	return gl_PathMemoryGlyphIndexArrayNV(
		firstPathName,
		fontTarget,
		fontSize,
		fontData,
		faceIndex,
		firstGlyphIndex,
		numGlyphs,
		pathParameterTemplate,
		emScale);
}

PFNGLPATHPARAMETERFNVPROC gl::gl_PathParameterfNV INIT_POINTER;
void gl::PathParameterfNV  (GLuint path, GLenum pname, GLfloat value, const char* file, int line)
{
	TRACE_FUNCTION("glPathParameterfNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_PathParameterfNV(
		path,
		pname,
		value);
}

PFNGLPATHPARAMETERFVNVPROC gl::gl_PathParameterfvNV INIT_POINTER;
void gl::PathParameterfvNV  (GLuint path, GLenum pname, const GLfloat *value, const char* file, int line)
{
	TRACE_FUNCTION("glPathParameterfvNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_PathParameterfvNV(
		path,
		pname,
		value);
}

PFNGLPATHPARAMETERINVPROC gl::gl_PathParameteriNV INIT_POINTER;
void gl::PathParameteriNV  (GLuint path, GLenum pname, GLint value, const char* file, int line)
{
	TRACE_FUNCTION("glPathParameteriNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_PathParameteriNV(
		path,
		pname,
		value);
}

PFNGLPATHPARAMETERIVNVPROC gl::gl_PathParameterivNV INIT_POINTER;
void gl::PathParameterivNV  (GLuint path, GLenum pname, const GLint *value, const char* file, int line)
{
	TRACE_FUNCTION("glPathParameterivNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_PathParameterivNV(
		path,
		pname,
		value);
}

PFNGLPATHSTENCILDEPTHOFFSETNVPROC gl::gl_PathStencilDepthOffsetNV INIT_POINTER;
void gl::PathStencilDepthOffsetNV  (GLfloat factor, GLfloat units, const char* file, int line)
{
	TRACE_FUNCTION("glPathStencilDepthOffsetNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_PathStencilDepthOffsetNV(
		factor,
		units);
}

PFNGLPATHSTENCILFUNCNVPROC gl::gl_PathStencilFuncNV INIT_POINTER;
void gl::PathStencilFuncNV  (GLenum func, GLint ref, GLuint mask, const char* file, int line)
{
	TRACE_FUNCTION("glPathStencilFuncNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_PathStencilFuncNV(
		func,
		ref,
		mask);
}

PFNGLPATHSTRINGNVPROC gl::gl_PathStringNV INIT_POINTER;
void gl::PathStringNV  (GLuint path, GLenum format, GLsizei length, const void *pathString, const char* file, int line)
{
	TRACE_FUNCTION("glPathStringNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_PathStringNV(
		path,
		format,
		length,
		pathString);
}

PFNGLPATHSUBCOMMANDSNVPROC gl::gl_PathSubCommandsNV INIT_POINTER;
void gl::PathSubCommandsNV  (GLuint path, GLsizei commandStart, GLsizei commandsToDelete, GLsizei numCommands, const GLubyte *commands, GLsizei numCoords, GLenum coordType, const void *coords, const char* file, int line)
{
	TRACE_FUNCTION("glPathSubCommandsNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_PathSubCommandsNV(
		path,
		commandStart,
		commandsToDelete,
		numCommands,
		commands,
		numCoords,
		coordType,
		coords);
}

PFNGLPATHSUBCOORDSNVPROC gl::gl_PathSubCoordsNV INIT_POINTER;
void gl::PathSubCoordsNV  (GLuint path, GLsizei coordStart, GLsizei numCoords, GLenum coordType, const void *coords, const char* file, int line)
{
	TRACE_FUNCTION("glPathSubCoordsNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_PathSubCoordsNV(
		path,
		coordStart,
		numCoords,
		coordType,
		coords);
}

PFNGLPOINTALONGPATHNVPROC gl::gl_PointAlongPathNV INIT_POINTER;
GLboolean gl::PointAlongPathNV  (GLuint path, GLsizei startSegment, GLsizei numSegments, GLfloat distance, GLfloat *x, GLfloat *y, GLfloat *tangentX, GLfloat *tangentY, const char* file, int line)
{
	TRACE_FUNCTION("glPointAlongPathNV(...) called from " << get_path(file) << '(' << line << ')');
	return gl_PointAlongPathNV(
		path,
		startSegment,
		numSegments,
		distance,
		x,
		y,
		tangentX,
		tangentY);
}

PFNGLPROGRAMPATHFRAGMENTINPUTGENNVPROC gl::gl_ProgramPathFragmentInputGenNV INIT_POINTER;
void gl::ProgramPathFragmentInputGenNV  (GLuint program, GLint location, GLenum genMode, GLint components, const GLfloat *coeffs, const char* file, int line)
{
	TRACE_FUNCTION("glProgramPathFragmentInputGenNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramPathFragmentInputGenNV(
		program,
		location,
		genMode,
		components,
		coeffs);
}

PFNGLSTENCILFILLPATHINSTANCEDNVPROC gl::gl_StencilFillPathInstancedNV INIT_POINTER;
void gl::StencilFillPathInstancedNV  (GLsizei numPaths, GLenum pathNameType, const void *paths, GLuint pathBase, GLenum fillMode, GLuint mask, GLenum transformType, const GLfloat *transformValues, const char* file, int line)
{
	TRACE_FUNCTION("glStencilFillPathInstancedNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_StencilFillPathInstancedNV(
		numPaths,
		pathNameType,
		paths,
		pathBase,
		fillMode,
		mask,
		transformType,
		transformValues);
}

PFNGLSTENCILFILLPATHNVPROC gl::gl_StencilFillPathNV INIT_POINTER;
void gl::StencilFillPathNV  (GLuint path, GLenum fillMode, GLuint mask, const char* file, int line)
{
	TRACE_FUNCTION("glStencilFillPathNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_StencilFillPathNV(
		path,
		fillMode,
		mask);
}

PFNGLSTENCILSTROKEPATHINSTANCEDNVPROC gl::gl_StencilStrokePathInstancedNV INIT_POINTER;
void gl::StencilStrokePathInstancedNV  (GLsizei numPaths, GLenum pathNameType, const void *paths, GLuint pathBase, GLint reference, GLuint mask, GLenum transformType, const GLfloat *transformValues, const char* file, int line)
{
	TRACE_FUNCTION("glStencilStrokePathInstancedNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_StencilStrokePathInstancedNV(
		numPaths,
		pathNameType,
		paths,
		pathBase,
		reference,
		mask,
		transformType,
		transformValues);
}

PFNGLSTENCILSTROKEPATHNVPROC gl::gl_StencilStrokePathNV INIT_POINTER;
void gl::StencilStrokePathNV  (GLuint path, GLint reference, GLuint mask, const char* file, int line)
{
	TRACE_FUNCTION("glStencilStrokePathNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_StencilStrokePathNV(
		path,
		reference,
		mask);
}

PFNGLSTENCILTHENCOVERFILLPATHINSTANCEDNVPROC gl::gl_StencilThenCoverFillPathInstancedNV INIT_POINTER;
void gl::StencilThenCoverFillPathInstancedNV  (GLsizei numPaths, GLenum pathNameType, const void *paths, GLuint pathBase, GLenum fillMode, GLuint mask, GLenum coverMode, GLenum transformType, const GLfloat *transformValues, const char* file, int line)
{
	TRACE_FUNCTION("glStencilThenCoverFillPathInstancedNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_StencilThenCoverFillPathInstancedNV(
		numPaths,
		pathNameType,
		paths,
		pathBase,
		fillMode,
		mask,
		coverMode,
		transformType,
		transformValues);
}

PFNGLSTENCILTHENCOVERFILLPATHNVPROC gl::gl_StencilThenCoverFillPathNV INIT_POINTER;
void gl::StencilThenCoverFillPathNV  (GLuint path, GLenum fillMode, GLuint mask, GLenum coverMode, const char* file, int line)
{
	TRACE_FUNCTION("glStencilThenCoverFillPathNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_StencilThenCoverFillPathNV(
		path,
		fillMode,
		mask,
		coverMode);
}

PFNGLSTENCILTHENCOVERSTROKEPATHINSTANCEDNVPROC gl::gl_StencilThenCoverStrokePathInstancedNV INIT_POINTER;
void gl::StencilThenCoverStrokePathInstancedNV  (GLsizei numPaths, GLenum pathNameType, const void *paths, GLuint pathBase, GLint reference, GLuint mask, GLenum coverMode, GLenum transformType, const GLfloat *transformValues, const char* file, int line)
{
	TRACE_FUNCTION("glStencilThenCoverStrokePathInstancedNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_StencilThenCoverStrokePathInstancedNV(
		numPaths,
		pathNameType,
		paths,
		pathBase,
		reference,
		mask,
		coverMode,
		transformType,
		transformValues);
}

PFNGLSTENCILTHENCOVERSTROKEPATHNVPROC gl::gl_StencilThenCoverStrokePathNV INIT_POINTER;
void gl::StencilThenCoverStrokePathNV  (GLuint path, GLint reference, GLuint mask, GLenum coverMode, const char* file, int line)
{
	TRACE_FUNCTION("glStencilThenCoverStrokePathNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_StencilThenCoverStrokePathNV(
		path,
		reference,
		mask,
		coverMode);
}

PFNGLTRANSFORMPATHNVPROC gl::gl_TransformPathNV INIT_POINTER;
void gl::TransformPathNV  (GLuint resultPath, GLuint srcPath, GLenum transformType, const GLfloat *transformValues, const char* file, int line)
{
	TRACE_FUNCTION("glTransformPathNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_TransformPathNV(
		resultPath,
		srcPath,
		transformType,
		transformValues);
}

PFNGLWEIGHTPATHSNVPROC gl::gl_WeightPathsNV INIT_POINTER;
void gl::WeightPathsNV  (GLuint resultPath, GLsizei numPaths, const GLuint *paths, const GLfloat *weights, const char* file, int line)
{
	TRACE_FUNCTION("glWeightPathsNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_WeightPathsNV(
		resultPath,
		numPaths,
		paths,
		weights);
}

// GL_NV_polygon_mode
PFNGLPOLYGONMODENVPROC gl::gl_PolygonModeNV INIT_POINTER;
void gl::PolygonModeNV  (GLenum face, GLenum mode, const char* file, int line)
{
	TRACE_FUNCTION("glPolygonModeNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_PolygonModeNV(
		face,
		mode);
}

// GL_NV_read_buffer
PFNGLREADBUFFERNVPROC gl::gl_ReadBufferNV INIT_POINTER;
void gl::ReadBufferNV  (GLenum mode, const char* file, int line)
{
	TRACE_FUNCTION("glReadBufferNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_ReadBufferNV(
		mode);
}

// GL_NV_sample_locations
PFNGLFRAMEBUFFERSAMPLELOCATIONSFVNVPROC gl::gl_FramebufferSampleLocationsfvNV INIT_POINTER;
void gl::FramebufferSampleLocationsfvNV  (GLenum target, GLuint start, GLsizei count, const GLfloat *v, const char* file, int line)
{
	TRACE_FUNCTION("glFramebufferSampleLocationsfvNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_FramebufferSampleLocationsfvNV(
		target,
		start,
		count,
		v);
}

PFNGLNAMEDFRAMEBUFFERSAMPLELOCATIONSFVNVPROC gl::gl_NamedFramebufferSampleLocationsfvNV INIT_POINTER;
void gl::NamedFramebufferSampleLocationsfvNV  (GLuint framebuffer, GLuint start, GLsizei count, const GLfloat *v, const char* file, int line)
{
	TRACE_FUNCTION("glNamedFramebufferSampleLocationsfvNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_NamedFramebufferSampleLocationsfvNV(
		framebuffer,
		start,
		count,
		v);
}

PFNGLRESOLVEDEPTHVALUESNVPROC gl::gl_ResolveDepthValuesNV INIT_POINTER;
void gl::ResolveDepthValuesNV  (const char* file, int line)
{
	TRACE_FUNCTION("glResolveDepthValuesNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_ResolveDepthValuesNV(
		);
}

// GL_NV_viewport_array
PFNGLDEPTHRANGEARRAYFVNVPROC gl::gl_DepthRangeArrayfvNV INIT_POINTER;
void gl::DepthRangeArrayfvNV  (GLuint first, GLsizei count, const GLfloat *v, const char* file, int line)
{
	TRACE_FUNCTION("glDepthRangeArrayfvNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_DepthRangeArrayfvNV(
		first,
		count,
		v);
}

PFNGLDEPTHRANGEINDEXEDFNVPROC gl::gl_DepthRangeIndexedfNV INIT_POINTER;
void gl::DepthRangeIndexedfNV  (GLuint index, GLfloat n, GLfloat f, const char* file, int line)
{
	TRACE_FUNCTION("glDepthRangeIndexedfNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_DepthRangeIndexedfNV(
		index,
		n,
		f);
}

PFNGLDISABLEINVPROC gl::gl_DisableiNV INIT_POINTER;
void gl::DisableiNV  (GLenum target, GLuint index, const char* file, int line)
{
	TRACE_FUNCTION("glDisableiNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_DisableiNV(
		target,
		index);
}

PFNGLENABLEINVPROC gl::gl_EnableiNV INIT_POINTER;
void gl::EnableiNV  (GLenum target, GLuint index, const char* file, int line)
{
	TRACE_FUNCTION("glEnableiNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_EnableiNV(
		target,
		index);
}

PFNGLGETFLOATI_VNVPROC gl::gl_GetFloati_vNV INIT_POINTER;
void gl::GetFloati_vNV  (GLenum target, GLuint index, GLfloat *data, const char* file, int line)
{
	TRACE_FUNCTION("glGetFloati_vNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetFloati_vNV(
		target,
		index,
		data);
}

PFNGLISENABLEDINVPROC gl::gl_IsEnablediNV INIT_POINTER;
GLboolean gl::IsEnablediNV  (GLenum target, GLuint index, const char* file, int line)
{
	TRACE_FUNCTION("glIsEnablediNV(...) called from " << get_path(file) << '(' << line << ')');
	return gl_IsEnablediNV(
		target,
		index);
}

PFNGLSCISSORARRAYVNVPROC gl::gl_ScissorArrayvNV INIT_POINTER;
void gl::ScissorArrayvNV  (GLuint first, GLsizei count, const GLint *v, const char* file, int line)
{
	TRACE_FUNCTION("glScissorArrayvNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_ScissorArrayvNV(
		first,
		count,
		v);
}

PFNGLSCISSORINDEXEDNVPROC gl::gl_ScissorIndexedNV INIT_POINTER;
void gl::ScissorIndexedNV  (GLuint index, GLint left, GLint bottom, GLsizei width, GLsizei height, const char* file, int line)
{
	TRACE_FUNCTION("glScissorIndexedNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_ScissorIndexedNV(
		index,
		left,
		bottom,
		width,
		height);
}

PFNGLSCISSORINDEXEDVNVPROC gl::gl_ScissorIndexedvNV INIT_POINTER;
void gl::ScissorIndexedvNV  (GLuint index, const GLint *v, const char* file, int line)
{
	TRACE_FUNCTION("glScissorIndexedvNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_ScissorIndexedvNV(
		index,
		v);
}

PFNGLVIEWPORTARRAYVNVPROC gl::gl_ViewportArrayvNV INIT_POINTER;
void gl::ViewportArrayvNV  (GLuint first, GLsizei count, const GLfloat *v, const char* file, int line)
{
	TRACE_FUNCTION("glViewportArrayvNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_ViewportArrayvNV(
		first,
		count,
		v);
}

PFNGLVIEWPORTINDEXEDFNVPROC gl::gl_ViewportIndexedfNV INIT_POINTER;
void gl::ViewportIndexedfNV  (GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h, const char* file, int line)
{
	TRACE_FUNCTION("glViewportIndexedfNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_ViewportIndexedfNV(
		index,
		x,
		y,
		w,
		h);
}

PFNGLVIEWPORTINDEXEDFVNVPROC gl::gl_ViewportIndexedfvNV INIT_POINTER;
void gl::ViewportIndexedfvNV  (GLuint index, const GLfloat *v, const char* file, int line)
{
	TRACE_FUNCTION("glViewportIndexedfvNV(...) called from " << get_path(file) << '(' << line << ')');
	gl_ViewportIndexedfvNV(
		index,
		v);
}

// GL_OES_EGL_image
PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC gl::gl_EGLImageTargetRenderbufferStorageOES INIT_POINTER;
void gl::EGLImageTargetRenderbufferStorageOES  (GLenum target, GLeglImageOES image, const char* file, int line)
{
	TRACE_FUNCTION("glEGLImageTargetRenderbufferStorageOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_EGLImageTargetRenderbufferStorageOES(
		target,
		image);
}

PFNGLEGLIMAGETARGETTEXTURE2DOESPROC gl::gl_EGLImageTargetTexture2DOES INIT_POINTER;
void gl::EGLImageTargetTexture2DOES  (GLenum target, GLeglImageOES image, const char* file, int line)
{
	TRACE_FUNCTION("glEGLImageTargetTexture2DOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_EGLImageTargetTexture2DOES(
		target,
		image);
}

// GL_OES_copy_image
PFNGLCOPYIMAGESUBDATAOESPROC gl::gl_CopyImageSubDataOES INIT_POINTER;
void gl::CopyImageSubDataOES  (GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth, const char* file, int line)
{
	TRACE_FUNCTION("glCopyImageSubDataOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_CopyImageSubDataOES(
		srcName,
		srcTarget,
		srcLevel,
		srcX,
		srcY,
		srcZ,
		dstName,
		dstTarget,
		dstLevel,
		dstX,
		dstY,
		dstZ,
		srcWidth,
		srcHeight,
		srcDepth);
}

// GL_OES_draw_buffers_indexed
PFNGLBLENDEQUATIONSEPARATEIOESPROC gl::gl_BlendEquationSeparateiOES INIT_POINTER;
void gl::BlendEquationSeparateiOES  (GLuint buf, GLenum modeRGB, GLenum modeAlpha, const char* file, int line)
{
	TRACE_FUNCTION("glBlendEquationSeparateiOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_BlendEquationSeparateiOES(
		buf,
		modeRGB,
		modeAlpha);
}

PFNGLBLENDEQUATIONIOESPROC gl::gl_BlendEquationiOES INIT_POINTER;
void gl::BlendEquationiOES  (GLuint buf, GLenum mode, const char* file, int line)
{
	TRACE_FUNCTION("glBlendEquationiOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_BlendEquationiOES(
		buf,
		mode);
}

PFNGLBLENDFUNCSEPARATEIOESPROC gl::gl_BlendFuncSeparateiOES INIT_POINTER;
void gl::BlendFuncSeparateiOES  (GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha, const char* file, int line)
{
	TRACE_FUNCTION("glBlendFuncSeparateiOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_BlendFuncSeparateiOES(
		buf,
		srcRGB,
		dstRGB,
		srcAlpha,
		dstAlpha);
}

PFNGLBLENDFUNCIOESPROC gl::gl_BlendFunciOES INIT_POINTER;
void gl::BlendFunciOES  (GLuint buf, GLenum src, GLenum dst, const char* file, int line)
{
	TRACE_FUNCTION("glBlendFunciOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_BlendFunciOES(
		buf,
		src,
		dst);
}

PFNGLCOLORMASKIOESPROC gl::gl_ColorMaskiOES INIT_POINTER;
void gl::ColorMaskiOES  (GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a, const char* file, int line)
{
	TRACE_FUNCTION("glColorMaskiOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_ColorMaskiOES(
		index,
		r,
		g,
		b,
		a);
}

PFNGLDISABLEIOESPROC gl::gl_DisableiOES INIT_POINTER;
void gl::DisableiOES  (GLenum target, GLuint index, const char* file, int line)
{
	TRACE_FUNCTION("glDisableiOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_DisableiOES(
		target,
		index);
}

PFNGLENABLEIOESPROC gl::gl_EnableiOES INIT_POINTER;
void gl::EnableiOES  (GLenum target, GLuint index, const char* file, int line)
{
	TRACE_FUNCTION("glEnableiOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_EnableiOES(
		target,
		index);
}

PFNGLISENABLEDIOESPROC gl::gl_IsEnablediOES INIT_POINTER;
GLboolean gl::IsEnablediOES  (GLenum target, GLuint index, const char* file, int line)
{
	TRACE_FUNCTION("glIsEnablediOES(...) called from " << get_path(file) << '(' << line << ')');
	return gl_IsEnablediOES(
		target,
		index);
}

// GL_OES_draw_elements_base_vertex
PFNGLDRAWELEMENTSBASEVERTEXOESPROC gl::gl_DrawElementsBaseVertexOES INIT_POINTER;
void gl::DrawElementsBaseVertexOES  (GLenum mode, GLsizei count, GLenum type, const void *indices, GLint basevertex, const char* file, int line)
{
	TRACE_FUNCTION("glDrawElementsBaseVertexOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_DrawElementsBaseVertexOES(
		mode,
		count,
		type,
		indices,
		basevertex);
}

PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXOESPROC gl::gl_DrawElementsInstancedBaseVertexOES INIT_POINTER;
void gl::DrawElementsInstancedBaseVertexOES  (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex, const char* file, int line)
{
	TRACE_FUNCTION("glDrawElementsInstancedBaseVertexOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_DrawElementsInstancedBaseVertexOES(
		mode,
		count,
		type,
		indices,
		instancecount,
		basevertex);
}

PFNGLDRAWRANGEELEMENTSBASEVERTEXOESPROC gl::gl_DrawRangeElementsBaseVertexOES INIT_POINTER;
void gl::DrawRangeElementsBaseVertexOES  (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices, GLint basevertex, const char* file, int line)
{
	TRACE_FUNCTION("glDrawRangeElementsBaseVertexOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_DrawRangeElementsBaseVertexOES(
		mode,
		start,
		end,
		count,
		type,
		indices,
		basevertex);
}

PFNGLMULTIDRAWELEMENTSBASEVERTEXOESPROC gl::gl_MultiDrawElementsBaseVertexOES INIT_POINTER;
void gl::MultiDrawElementsBaseVertexOES  (GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei primcount, const GLint *basevertex, const char* file, int line)
{
	TRACE_FUNCTION("glMultiDrawElementsBaseVertexOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_MultiDrawElementsBaseVertexOES(
		mode,
		count,
		type,
		indices,
		primcount,
		basevertex);
}

// GL_OES_geometry_shader
PFNGLFRAMEBUFFERTEXTUREOESPROC gl::gl_FramebufferTextureOES INIT_POINTER;
void gl::FramebufferTextureOES  (GLenum target, GLenum attachment, GLuint texture, GLint level, const char* file, int line)
{
	TRACE_FUNCTION("glFramebufferTextureOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_FramebufferTextureOES(
		target,
		attachment,
		texture,
		level);
}

// GL_OES_get_program_binary
PFNGLGETPROGRAMBINARYOESPROC gl::gl_GetProgramBinaryOES INIT_POINTER;
void gl::GetProgramBinaryOES  (GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, void *binary, const char* file, int line)
{
	TRACE_FUNCTION("glGetProgramBinaryOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetProgramBinaryOES(
		program,
		bufSize,
		length,
		binaryFormat,
		binary);
}

PFNGLPROGRAMBINARYOESPROC gl::gl_ProgramBinaryOES INIT_POINTER;
void gl::ProgramBinaryOES  (GLuint program, GLenum binaryFormat, const void *binary, GLint length, const char* file, int line)
{
	TRACE_FUNCTION("glProgramBinaryOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_ProgramBinaryOES(
		program,
		binaryFormat,
		binary,
		length);
}

// GL_OES_mapbuffer
PFNGLGETBUFFERPOINTERVOESPROC gl::gl_GetBufferPointervOES INIT_POINTER;
void gl::GetBufferPointervOES  (GLenum target, GLenum pname, void **params, const char* file, int line)
{
	TRACE_FUNCTION("glGetBufferPointervOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetBufferPointervOES(
		target,
		pname,
		params);
}

PFNGLMAPBUFFEROESPROC gl::gl_MapBufferOES INIT_POINTER;
void gl::MapBufferOES  (GLenum target, GLenum access, const char* file, int line)
{
	TRACE_FUNCTION("glMapBufferOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_MapBufferOES(
		target,
		access);
}

PFNGLUNMAPBUFFEROESPROC gl::gl_UnmapBufferOES INIT_POINTER;
GLboolean gl::UnmapBufferOES  (GLenum target, const char* file, int line)
{
	TRACE_FUNCTION("glUnmapBufferOES(...) called from " << get_path(file) << '(' << line << ')');
	return gl_UnmapBufferOES(
		target);
}

// GL_OES_primitive_bounding_box
PFNGLPRIMITIVEBOUNDINGBOXOESPROC gl::gl_PrimitiveBoundingBoxOES INIT_POINTER;
void gl::PrimitiveBoundingBoxOES  (GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW, GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW, const char* file, int line)
{
	TRACE_FUNCTION("glPrimitiveBoundingBoxOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_PrimitiveBoundingBoxOES(
		minX,
		minY,
		minZ,
		minW,
		maxX,
		maxY,
		maxZ,
		maxW);
}

// GL_OES_sample_shading
PFNGLMINSAMPLESHADINGOESPROC gl::gl_MinSampleShadingOES INIT_POINTER;
void gl::MinSampleShadingOES  (GLfloat value, const char* file, int line)
{
	TRACE_FUNCTION("glMinSampleShadingOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_MinSampleShadingOES(
		value);
}

// GL_OES_tessellation_shader
PFNGLPATCHPARAMETERIOESPROC gl::gl_PatchParameteriOES INIT_POINTER;
void gl::PatchParameteriOES  (GLenum pname, GLint value, const char* file, int line)
{
	TRACE_FUNCTION("glPatchParameteriOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_PatchParameteriOES(
		pname,
		value);
}

// GL_OES_texture_3D
PFNGLCOMPRESSEDTEXIMAGE3DOESPROC gl::gl_CompressedTexImage3DOES INIT_POINTER;
void gl::CompressedTexImage3DOES  (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *data, const char* file, int line)
{
	TRACE_FUNCTION("glCompressedTexImage3DOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_CompressedTexImage3DOES(
		target,
		level,
		internalformat,
		width,
		height,
		depth,
		border,
		imageSize,
		data);
}

PFNGLCOMPRESSEDTEXSUBIMAGE3DOESPROC gl::gl_CompressedTexSubImage3DOES INIT_POINTER;
void gl::CompressedTexSubImage3DOES  (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data, const char* file, int line)
{
	TRACE_FUNCTION("glCompressedTexSubImage3DOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_CompressedTexSubImage3DOES(
		target,
		level,
		xoffset,
		yoffset,
		zoffset,
		width,
		height,
		depth,
		format,
		imageSize,
		data);
}

PFNGLCOPYTEXSUBIMAGE3DOESPROC gl::gl_CopyTexSubImage3DOES INIT_POINTER;
void gl::CopyTexSubImage3DOES  (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height, const char* file, int line)
{
	TRACE_FUNCTION("glCopyTexSubImage3DOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_CopyTexSubImage3DOES(
		target,
		level,
		xoffset,
		yoffset,
		zoffset,
		x,
		y,
		width,
		height);
}

PFNGLFRAMEBUFFERTEXTURE3DOESPROC gl::gl_FramebufferTexture3DOES INIT_POINTER;
void gl::FramebufferTexture3DOES  (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset, const char* file, int line)
{
	TRACE_FUNCTION("glFramebufferTexture3DOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_FramebufferTexture3DOES(
		target,
		attachment,
		textarget,
		texture,
		level,
		zoffset);
}

PFNGLTEXIMAGE3DOESPROC gl::gl_TexImage3DOES INIT_POINTER;
void gl::TexImage3DOES  (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels, const char* file, int line)
{
	TRACE_FUNCTION("glTexImage3DOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_TexImage3DOES(
		target,
		level,
		internalformat,
		width,
		height,
		depth,
		border,
		format,
		type,
		pixels);
}

PFNGLTEXSUBIMAGE3DOESPROC gl::gl_TexSubImage3DOES INIT_POINTER;
void gl::TexSubImage3DOES  (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels, const char* file, int line)
{
	TRACE_FUNCTION("glTexSubImage3DOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_TexSubImage3DOES(
		target,
		level,
		xoffset,
		yoffset,
		zoffset,
		width,
		height,
		depth,
		format,
		type,
		pixels);
}

// GL_OES_texture_border_clamp
PFNGLGETSAMPLERPARAMETERIIVOESPROC gl::gl_GetSamplerParameterIivOES INIT_POINTER;
void gl::GetSamplerParameterIivOES  (GLuint sampler, GLenum pname, GLint *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetSamplerParameterIivOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetSamplerParameterIivOES(
		sampler,
		pname,
		params);
}

PFNGLGETSAMPLERPARAMETERIUIVOESPROC gl::gl_GetSamplerParameterIuivOES INIT_POINTER;
void gl::GetSamplerParameterIuivOES  (GLuint sampler, GLenum pname, GLuint *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetSamplerParameterIuivOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetSamplerParameterIuivOES(
		sampler,
		pname,
		params);
}

PFNGLGETTEXPARAMETERIIVOESPROC gl::gl_GetTexParameterIivOES INIT_POINTER;
void gl::GetTexParameterIivOES  (GLenum target, GLenum pname, GLint *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetTexParameterIivOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetTexParameterIivOES(
		target,
		pname,
		params);
}

PFNGLGETTEXPARAMETERIUIVOESPROC gl::gl_GetTexParameterIuivOES INIT_POINTER;
void gl::GetTexParameterIuivOES  (GLenum target, GLenum pname, GLuint *params, const char* file, int line)
{
	TRACE_FUNCTION("glGetTexParameterIuivOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetTexParameterIuivOES(
		target,
		pname,
		params);
}

PFNGLSAMPLERPARAMETERIIVOESPROC gl::gl_SamplerParameterIivOES INIT_POINTER;
void gl::SamplerParameterIivOES  (GLuint sampler, GLenum pname, const GLint *param, const char* file, int line)
{
	TRACE_FUNCTION("glSamplerParameterIivOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_SamplerParameterIivOES(
		sampler,
		pname,
		param);
}

PFNGLSAMPLERPARAMETERIUIVOESPROC gl::gl_SamplerParameterIuivOES INIT_POINTER;
void gl::SamplerParameterIuivOES  (GLuint sampler, GLenum pname, const GLuint *param, const char* file, int line)
{
	TRACE_FUNCTION("glSamplerParameterIuivOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_SamplerParameterIuivOES(
		sampler,
		pname,
		param);
}

PFNGLTEXPARAMETERIIVOESPROC gl::gl_TexParameterIivOES INIT_POINTER;
void gl::TexParameterIivOES  (GLenum target, GLenum pname, const GLint *params, const char* file, int line)
{
	TRACE_FUNCTION("glTexParameterIivOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_TexParameterIivOES(
		target,
		pname,
		params);
}

PFNGLTEXPARAMETERIUIVOESPROC gl::gl_TexParameterIuivOES INIT_POINTER;
void gl::TexParameterIuivOES  (GLenum target, GLenum pname, const GLuint *params, const char* file, int line)
{
	TRACE_FUNCTION("glTexParameterIuivOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_TexParameterIuivOES(
		target,
		pname,
		params);
}

// GL_OES_texture_buffer
PFNGLTEXBUFFEROESPROC gl::gl_TexBufferOES INIT_POINTER;
void gl::TexBufferOES  (GLenum target, GLenum internalformat, GLuint buffer, const char* file, int line)
{
	TRACE_FUNCTION("glTexBufferOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_TexBufferOES(
		target,
		internalformat,
		buffer);
}

PFNGLTEXBUFFERRANGEOESPROC gl::gl_TexBufferRangeOES INIT_POINTER;
void gl::TexBufferRangeOES  (GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size, const char* file, int line)
{
	TRACE_FUNCTION("glTexBufferRangeOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_TexBufferRangeOES(
		target,
		internalformat,
		buffer,
		offset,
		size);
}

// GL_OES_texture_storage_multisample_2d_array
PFNGLTEXSTORAGE3DMULTISAMPLEOESPROC gl::gl_TexStorage3DMultisampleOES INIT_POINTER;
void gl::TexStorage3DMultisampleOES  (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations, const char* file, int line)
{
	TRACE_FUNCTION("glTexStorage3DMultisampleOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_TexStorage3DMultisampleOES(
		target,
		samples,
		internalformat,
		width,
		height,
		depth,
		fixedsamplelocations);
}

// GL_OES_texture_view
PFNGLTEXTUREVIEWOESPROC gl::gl_TextureViewOES INIT_POINTER;
void gl::TextureViewOES  (GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers, const char* file, int line)
{
	TRACE_FUNCTION("glTextureViewOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_TextureViewOES(
		texture,
		target,
		origtexture,
		internalformat,
		minlevel,
		numlevels,
		minlayer,
		numlayers);
}

// GL_OES_vertex_array_object
PFNGLBINDVERTEXARRAYOESPROC gl::gl_BindVertexArrayOES INIT_POINTER;
void gl::BindVertexArrayOES  (GLuint array, const char* file, int line)
{
	TRACE_FUNCTION("glBindVertexArrayOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_BindVertexArrayOES(
		array);
}

PFNGLDELETEVERTEXARRAYSOESPROC gl::gl_DeleteVertexArraysOES INIT_POINTER;
void gl::DeleteVertexArraysOES  (GLsizei n, const GLuint *arrays, const char* file, int line)
{
	TRACE_FUNCTION("glDeleteVertexArraysOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_DeleteVertexArraysOES(
		n,
		arrays);
}

PFNGLGENVERTEXARRAYSOESPROC gl::gl_GenVertexArraysOES INIT_POINTER;
void gl::GenVertexArraysOES  (GLsizei n, GLuint *arrays, const char* file, int line)
{
	TRACE_FUNCTION("glGenVertexArraysOES(...) called from " << get_path(file) << '(' << line << ')');
	gl_GenVertexArraysOES(
		n,
		arrays);
}

PFNGLISVERTEXARRAYOESPROC gl::gl_IsVertexArrayOES INIT_POINTER;
GLboolean gl::IsVertexArrayOES  (GLuint array, const char* file, int line)
{
	TRACE_FUNCTION("glIsVertexArrayOES(...) called from " << get_path(file) << '(' << line << ')');
	return gl_IsVertexArrayOES(
		array);
}

// GL_OVR_multiview
PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC gl::gl_FramebufferTextureMultiviewOVR INIT_POINTER;
void gl::FramebufferTextureMultiviewOVR  (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint baseViewIndex, GLsizei numViews, const char* file, int line)
{
	TRACE_FUNCTION("glFramebufferTextureMultiviewOVR(...) called from " << get_path(file) << '(' << line << ')');
	gl_FramebufferTextureMultiviewOVR(
		target,
		attachment,
		texture,
		level,
		baseViewIndex,
		numViews);
}

// GL_OVR_multiview_multisampled_render_to_texture
PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC gl::gl_FramebufferTextureMultisampleMultiviewOVR INIT_POINTER;
void gl::FramebufferTextureMultisampleMultiviewOVR  (GLenum target, GLenum attachment, GLuint texture, GLint level, GLsizei samples, GLint baseViewIndex, GLsizei numViews, const char* file, int line)
{
	TRACE_FUNCTION("glFramebufferTextureMultisampleMultiviewOVR(...) called from " << get_path(file) << '(' << line << ')');
	gl_FramebufferTextureMultisampleMultiviewOVR(
		target,
		attachment,
		texture,
		level,
		samples,
		baseViewIndex,
		numViews);
}

// GL_QCOM_alpha_test
PFNGLALPHAFUNCQCOMPROC gl::gl_AlphaFuncQCOM INIT_POINTER;
void gl::AlphaFuncQCOM  (GLenum func, GLclampf ref, const char* file, int line)
{
	TRACE_FUNCTION("glAlphaFuncQCOM(...) called from " << get_path(file) << '(' << line << ')');
	gl_AlphaFuncQCOM(
		func,
		ref);
}

// GL_QCOM_driver_control
PFNGLDISABLEDRIVERCONTROLQCOMPROC gl::gl_DisableDriverControlQCOM INIT_POINTER;
void gl::DisableDriverControlQCOM  (GLuint driverControl, const char* file, int line)
{
	TRACE_FUNCTION("glDisableDriverControlQCOM(...) called from " << get_path(file) << '(' << line << ')');
	gl_DisableDriverControlQCOM(
		driverControl);
}

PFNGLENABLEDRIVERCONTROLQCOMPROC gl::gl_EnableDriverControlQCOM INIT_POINTER;
void gl::EnableDriverControlQCOM  (GLuint driverControl, const char* file, int line)
{
	TRACE_FUNCTION("glEnableDriverControlQCOM(...) called from " << get_path(file) << '(' << line << ')');
	gl_EnableDriverControlQCOM(
		driverControl);
}

PFNGLGETDRIVERCONTROLSTRINGQCOMPROC gl::gl_GetDriverControlStringQCOM INIT_POINTER;
void gl::GetDriverControlStringQCOM  (GLuint driverControl, GLsizei bufSize, GLsizei *length, GLchar *driverControlString, const char* file, int line)
{
	TRACE_FUNCTION("glGetDriverControlStringQCOM(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetDriverControlStringQCOM(
		driverControl,
		bufSize,
		length,
		driverControlString);
}

PFNGLGETDRIVERCONTROLSQCOMPROC gl::gl_GetDriverControlsQCOM INIT_POINTER;
void gl::GetDriverControlsQCOM  (GLint *num, GLsizei size, GLuint *driverControls, const char* file, int line)
{
	TRACE_FUNCTION("glGetDriverControlsQCOM(...) called from " << get_path(file) << '(' << line << ')');
	gl_GetDriverControlsQCOM(
		num,
		size,
		driverControls);
}

// GL_QCOM_extended_get
PFNGLEXTGETBUFFERPOINTERVQCOMPROC gl::gl_ExtGetBufferPointervQCOM INIT_POINTER;
void gl::ExtGetBufferPointervQCOM  (GLenum target, void **params, const char* file, int line)
{
	TRACE_FUNCTION("glExtGetBufferPointervQCOM(...) called from " << get_path(file) << '(' << line << ')');
	gl_ExtGetBufferPointervQCOM(
		target,
		params);
}

PFNGLEXTGETBUFFERSQCOMPROC gl::gl_ExtGetBuffersQCOM INIT_POINTER;
void gl::ExtGetBuffersQCOM  (GLuint *buffers, GLint maxBuffers, GLint *numBuffers, const char* file, int line)
{
	TRACE_FUNCTION("glExtGetBuffersQCOM(...) called from " << get_path(file) << '(' << line << ')');
	gl_ExtGetBuffersQCOM(
		buffers,
		maxBuffers,
		numBuffers);
}

PFNGLEXTGETFRAMEBUFFERSQCOMPROC gl::gl_ExtGetFramebuffersQCOM INIT_POINTER;
void gl::ExtGetFramebuffersQCOM  (GLuint *framebuffers, GLint maxFramebuffers, GLint *numFramebuffers, const char* file, int line)
{
	TRACE_FUNCTION("glExtGetFramebuffersQCOM(...) called from " << get_path(file) << '(' << line << ')');
	gl_ExtGetFramebuffersQCOM(
		framebuffers,
		maxFramebuffers,
		numFramebuffers);
}

PFNGLEXTGETRENDERBUFFERSQCOMPROC gl::gl_ExtGetRenderbuffersQCOM INIT_POINTER;
void gl::ExtGetRenderbuffersQCOM  (GLuint *renderbuffers, GLint maxRenderbuffers, GLint *numRenderbuffers, const char* file, int line)
{
	TRACE_FUNCTION("glExtGetRenderbuffersQCOM(...) called from " << get_path(file) << '(' << line << ')');
	gl_ExtGetRenderbuffersQCOM(
		renderbuffers,
		maxRenderbuffers,
		numRenderbuffers);
}

PFNGLEXTGETTEXLEVELPARAMETERIVQCOMPROC gl::gl_ExtGetTexLevelParameterivQCOM INIT_POINTER;
void gl::ExtGetTexLevelParameterivQCOM  (GLuint texture, GLenum face, GLint level, GLenum pname, GLint *params, const char* file, int line)
{
	TRACE_FUNCTION("glExtGetTexLevelParameterivQCOM(...) called from " << get_path(file) << '(' << line << ')');
	gl_ExtGetTexLevelParameterivQCOM(
		texture,
		face,
		level,
		pname,
		params);
}

PFNGLEXTGETTEXSUBIMAGEQCOMPROC gl::gl_ExtGetTexSubImageQCOM INIT_POINTER;
void gl::ExtGetTexSubImageQCOM  (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, void *texels, const char* file, int line)
{
	TRACE_FUNCTION("glExtGetTexSubImageQCOM(...) called from " << get_path(file) << '(' << line << ')');
	gl_ExtGetTexSubImageQCOM(
		target,
		level,
		xoffset,
		yoffset,
		zoffset,
		width,
		height,
		depth,
		format,
		type,
		texels);
}

PFNGLEXTGETTEXTURESQCOMPROC gl::gl_ExtGetTexturesQCOM INIT_POINTER;
void gl::ExtGetTexturesQCOM  (GLuint *textures, GLint maxTextures, GLint *numTextures, const char* file, int line)
{
	TRACE_FUNCTION("glExtGetTexturesQCOM(...) called from " << get_path(file) << '(' << line << ')');
	gl_ExtGetTexturesQCOM(
		textures,
		maxTextures,
		numTextures);
}

PFNGLEXTTEXOBJECTSTATEOVERRIDEIQCOMPROC gl::gl_ExtTexObjectStateOverrideiQCOM INIT_POINTER;
void gl::ExtTexObjectStateOverrideiQCOM  (GLenum target, GLenum pname, GLint param, const char* file, int line)
{
	TRACE_FUNCTION("glExtTexObjectStateOverrideiQCOM(...) called from " << get_path(file) << '(' << line << ')');
	gl_ExtTexObjectStateOverrideiQCOM(
		target,
		pname,
		param);
}

// GL_QCOM_extended_get2
PFNGLEXTGETPROGRAMBINARYSOURCEQCOMPROC gl::gl_ExtGetProgramBinarySourceQCOM INIT_POINTER;
void gl::ExtGetProgramBinarySourceQCOM  (GLuint program, GLenum shadertype, GLchar *source, GLint *length, const char* file, int line)
{
	TRACE_FUNCTION("glExtGetProgramBinarySourceQCOM(...) called from " << get_path(file) << '(' << line << ')');
	gl_ExtGetProgramBinarySourceQCOM(
		program,
		shadertype,
		source,
		length);
}

PFNGLEXTGETPROGRAMSQCOMPROC gl::gl_ExtGetProgramsQCOM INIT_POINTER;
void gl::ExtGetProgramsQCOM  (GLuint *programs, GLint maxPrograms, GLint *numPrograms, const char* file, int line)
{
	TRACE_FUNCTION("glExtGetProgramsQCOM(...) called from " << get_path(file) << '(' << line << ')');
	gl_ExtGetProgramsQCOM(
		programs,
		maxPrograms,
		numPrograms);
}

PFNGLEXTGETSHADERSQCOMPROC gl::gl_ExtGetShadersQCOM INIT_POINTER;
void gl::ExtGetShadersQCOM  (GLuint *shaders, GLint maxShaders, GLint *numShaders, const char* file, int line)
{
	TRACE_FUNCTION("glExtGetShadersQCOM(...) called from " << get_path(file) << '(' << line << ')');
	gl_ExtGetShadersQCOM(
		shaders,
		maxShaders,
		numShaders);
}

PFNGLEXTISPROGRAMBINARYQCOMPROC gl::gl_ExtIsProgramBinaryQCOM INIT_POINTER;
GLboolean gl::ExtIsProgramBinaryQCOM  (GLuint program, const char* file, int line)
{
	TRACE_FUNCTION("glExtIsProgramBinaryQCOM(...) called from " << get_path(file) << '(' << line << ')');
	return gl_ExtIsProgramBinaryQCOM(
		program);
}

// GL_QCOM_tiled_rendering
PFNGLENDTILINGQCOMPROC gl::gl_EndTilingQCOM INIT_POINTER;
void gl::EndTilingQCOM  (GLbitfield preserveMask, const char* file, int line)
{
	TRACE_FUNCTION("glEndTilingQCOM(...) called from " << get_path(file) << '(' << line << ')');
	gl_EndTilingQCOM(
		preserveMask);
}

PFNGLSTARTTILINGQCOMPROC gl::gl_StartTilingQCOM INIT_POINTER;
void gl::StartTilingQCOM  (GLuint x, GLuint y, GLuint width, GLuint height, GLbitfield preserveMask, const char* file, int line)
{
	TRACE_FUNCTION("glStartTilingQCOM(...) called from " << get_path(file) << '(' << line << ')');
	gl_StartTilingQCOM(
		x,
		y,
		width,
		height,
		preserveMask);
}

// ---------------------------------------------------------------------
// eglext.h
// ---------------------------------------------------------------------

// EGL_ANDROID_blob_cache
PFNEGLSETBLOBCACHEFUNCSANDROIDPROC gl::egl::egl_SetBlobCacheFuncsANDROID INIT_POINTER;
void gl::egl::SetBlobCacheFuncsANDROID  (EGLDisplay dpy, EGLSetBlobFuncANDROID set, EGLGetBlobFuncANDROID get, const char* file, int line)
{
	TRACE_FUNCTION("eglSetBlobCacheFuncsANDROID(...) called from " << get_path(file) << '(' << line << ')');
	egl_SetBlobCacheFuncsANDROID(
		dpy,
		set,
		get);
}

// EGL_ANDROID_create_native_client_buffer
PFNEGLCREATENATIVECLIENTBUFFERANDROIDPROC gl::egl::egl_CreateNativeClientBufferANDROID INIT_POINTER;
EGLClientBuffer gl::egl::CreateNativeClientBufferANDROID  (const EGLint *attrib_list, const char* file, int line)
{
	TRACE_FUNCTION("eglCreateNativeClientBufferANDROID(...) called from " << get_path(file) << '(' << line << ')');
	return egl_CreateNativeClientBufferANDROID(
		attrib_list);
}

// EGL_ANDROID_native_fence_sync
PFNEGLDUPNATIVEFENCEFDANDROIDPROC gl::egl::egl_DupNativeFenceFDANDROID INIT_POINTER;
EGLint gl::egl::DupNativeFenceFDANDROID  (EGLDisplay dpy, EGLSyncKHR sync, const char* file, int line)
{
	TRACE_FUNCTION("eglDupNativeFenceFDANDROID(...) called from " << get_path(file) << '(' << line << ')');
	return egl_DupNativeFenceFDANDROID(
		dpy,
		sync);
}

// EGL_ANDROID_presentation_time
PFNEGLPRESENTATIONTIMEANDROIDPROC gl::egl::egl_PresentationTimeANDROID INIT_POINTER;
EGLBoolean gl::egl::PresentationTimeANDROID  (EGLDisplay dpy, EGLSurface surface, EGLnsecsANDROID time, const char* file, int line)
{
	TRACE_FUNCTION("eglPresentationTimeANDROID(...) called from " << get_path(file) << '(' << line << ')');
	return egl_PresentationTimeANDROID(
		dpy,
		surface,
		time);
}

// EGL_ANGLE_query_surface_pointer
PFNEGLQUERYSURFACEPOINTERANGLEPROC gl::egl::egl_QuerySurfacePointerANGLE INIT_POINTER;
EGLBoolean gl::egl::QuerySurfacePointerANGLE  (EGLDisplay dpy, EGLSurface surface, EGLint attribute, void **value, const char* file, int line)
{
	TRACE_FUNCTION("eglQuerySurfacePointerANGLE(...) called from " << get_path(file) << '(' << line << ')');
	return egl_QuerySurfacePointerANGLE(
		dpy,
		surface,
		attribute,
		value);
}

// EGL_EXT_device_base
PFNEGLQUERYDEVICEATTRIBEXTPROC gl::egl::egl_QueryDeviceAttribEXT INIT_POINTER;
EGLBoolean gl::egl::QueryDeviceAttribEXT  (EGLDeviceEXT device, EGLint attribute, EGLAttrib *value, const char* file, int line)
{
	TRACE_FUNCTION("eglQueryDeviceAttribEXT(...) called from " << get_path(file) << '(' << line << ')');
	return egl_QueryDeviceAttribEXT(
		device,
		attribute,
		value);
}

PFNEGLQUERYDEVICESTRINGEXTPROC gl::egl::egl_QueryDeviceStringEXT INIT_POINTER;
const char* gl::egl::QueryDeviceStringEXT (EGLDeviceEXT device, EGLint name, const char* file, int line)
{
	TRACE_FUNCTION("eglQueryDeviceStringEXT(...) called from " << get_path(file) << '(' << line << ')');
	return egl_QueryDeviceStringEXT(
		device,
		name);
}

PFNEGLQUERYDEVICESEXTPROC gl::egl::egl_QueryDevicesEXT INIT_POINTER;
EGLBoolean gl::egl::QueryDevicesEXT  (EGLint max_devices, EGLDeviceEXT *devices, EGLint *num_devices, const char* file, int line)
{
	TRACE_FUNCTION("eglQueryDevicesEXT(...) called from " << get_path(file) << '(' << line << ')');
	return egl_QueryDevicesEXT(
		max_devices,
		devices,
		num_devices);
}

PFNEGLQUERYDISPLAYATTRIBEXTPROC gl::egl::egl_QueryDisplayAttribEXT INIT_POINTER;
EGLBoolean gl::egl::QueryDisplayAttribEXT  (EGLDisplay dpy, EGLint attribute, EGLAttrib *value, const char* file, int line)
{
	TRACE_FUNCTION("eglQueryDisplayAttribEXT(...) called from " << get_path(file) << '(' << line << ')');
	return egl_QueryDisplayAttribEXT(
		dpy,
		attribute,
		value);
}

// EGL_EXT_output_base
PFNEGLGETOUTPUTLAYERSEXTPROC gl::egl::egl_GetOutputLayersEXT INIT_POINTER;
EGLBoolean gl::egl::GetOutputLayersEXT  (EGLDisplay dpy, const EGLAttrib *attrib_list, EGLOutputLayerEXT *layers, EGLint max_layers, EGLint *num_layers, const char* file, int line)
{
	TRACE_FUNCTION("eglGetOutputLayersEXT(...) called from " << get_path(file) << '(' << line << ')');
	return egl_GetOutputLayersEXT(
		dpy,
		attrib_list,
		layers,
		max_layers,
		num_layers);
}

PFNEGLGETOUTPUTPORTSEXTPROC gl::egl::egl_GetOutputPortsEXT INIT_POINTER;
EGLBoolean gl::egl::GetOutputPortsEXT  (EGLDisplay dpy, const EGLAttrib *attrib_list, EGLOutputPortEXT *ports, EGLint max_ports, EGLint *num_ports, const char* file, int line)
{
	TRACE_FUNCTION("eglGetOutputPortsEXT(...) called from " << get_path(file) << '(' << line << ')');
	return egl_GetOutputPortsEXT(
		dpy,
		attrib_list,
		ports,
		max_ports,
		num_ports);
}

PFNEGLOUTPUTLAYERATTRIBEXTPROC gl::egl::egl_OutputLayerAttribEXT INIT_POINTER;
EGLBoolean gl::egl::OutputLayerAttribEXT  (EGLDisplay dpy, EGLOutputLayerEXT layer, EGLint attribute, EGLAttrib value, const char* file, int line)
{
	TRACE_FUNCTION("eglOutputLayerAttribEXT(...) called from " << get_path(file) << '(' << line << ')');
	return egl_OutputLayerAttribEXT(
		dpy,
		layer,
		attribute,
		value);
}

PFNEGLOUTPUTPORTATTRIBEXTPROC gl::egl::egl_OutputPortAttribEXT INIT_POINTER;
EGLBoolean gl::egl::OutputPortAttribEXT  (EGLDisplay dpy, EGLOutputPortEXT port, EGLint attribute, EGLAttrib value, const char* file, int line)
{
	TRACE_FUNCTION("eglOutputPortAttribEXT(...) called from " << get_path(file) << '(' << line << ')');
	return egl_OutputPortAttribEXT(
		dpy,
		port,
		attribute,
		value);
}

PFNEGLQUERYOUTPUTLAYERATTRIBEXTPROC gl::egl::egl_QueryOutputLayerAttribEXT INIT_POINTER;
EGLBoolean gl::egl::QueryOutputLayerAttribEXT  (EGLDisplay dpy, EGLOutputLayerEXT layer, EGLint attribute, EGLAttrib *value, const char* file, int line)
{
	TRACE_FUNCTION("eglQueryOutputLayerAttribEXT(...) called from " << get_path(file) << '(' << line << ')');
	return egl_QueryOutputLayerAttribEXT(
		dpy,
		layer,
		attribute,
		value);
}

PFNEGLQUERYOUTPUTLAYERSTRINGEXTPROC gl::egl::egl_QueryOutputLayerStringEXT INIT_POINTER;
const char* gl::egl::QueryOutputLayerStringEXT(EGLDisplay dpy, EGLOutputLayerEXT layer, EGLint name, const char* file, int line)
{
	TRACE_FUNCTION("eglQueryOutputLayerStringEXT(...) called from " << get_path(file) << '(' << line << ')');
	return egl_QueryOutputLayerStringEXT(
		dpy,
		layer,
		name);
}

PFNEGLQUERYOUTPUTPORTATTRIBEXTPROC gl::egl::egl_QueryOutputPortAttribEXT INIT_POINTER;
EGLBoolean gl::egl::QueryOutputPortAttribEXT  (EGLDisplay dpy, EGLOutputPortEXT port, EGLint attribute, EGLAttrib *value, const char* file, int line)
{
	TRACE_FUNCTION("eglQueryOutputPortAttribEXT(...) called from " << get_path(file) << '(' << line << ')');
	return egl_QueryOutputPortAttribEXT(
		dpy,
		port,
		attribute,
		value);
}

PFNEGLQUERYOUTPUTPORTSTRINGEXTPROC gl::egl::egl_QueryOutputPortStringEXT INIT_POINTER;
const char* gl::egl::QueryOutputPortStringEXT (EGLDisplay dpy, EGLOutputPortEXT port, EGLint name, const char* file, int line)
{
	TRACE_FUNCTION("egl*EGLAPIENTRY(...) called from " << get_path(file) << '(' << line << ')');
	return egl_QueryOutputPortStringEXT(
		dpy,
		port,
		name);
}

// EGL_EXT_platform_base
PFNEGLCREATEPLATFORMPIXMAPSURFACEEXTPROC gl::egl::egl_CreatePlatformPixmapSurfaceEXT INIT_POINTER;
EGLSurface gl::egl::CreatePlatformPixmapSurfaceEXT  (EGLDisplay dpy, EGLConfig config, void *native_pixmap, const EGLint *attrib_list, const char* file, int line)
{
	TRACE_FUNCTION("eglCreatePlatformPixmapSurfaceEXT(...) called from " << get_path(file) << '(' << line << ')');
	return egl_CreatePlatformPixmapSurfaceEXT(
		dpy,
		config,
		native_pixmap,
		attrib_list);
}

PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC gl::egl::egl_CreatePlatformWindowSurfaceEXT INIT_POINTER;
EGLSurface gl::egl::CreatePlatformWindowSurfaceEXT  (EGLDisplay dpy, EGLConfig config, void *native_window, const EGLint *attrib_list, const char* file, int line)
{
	TRACE_FUNCTION("eglCreatePlatformWindowSurfaceEXT(...) called from " << get_path(file) << '(' << line << ')');
	return egl_CreatePlatformWindowSurfaceEXT(
		dpy,
		config,
		native_window,
		attrib_list);
}

PFNEGLGETPLATFORMDISPLAYEXTPROC gl::egl::egl_GetPlatformDisplayEXT INIT_POINTER;
EGLDisplay gl::egl::GetPlatformDisplayEXT  (EGLenum platform, void *native_display, const EGLint *attrib_list, const char* file, int line)
{
	TRACE_FUNCTION("eglGetPlatformDisplayEXT(...) called from " << get_path(file) << '(' << line << ')');
	return egl_GetPlatformDisplayEXT(
		platform,
		native_display,
		attrib_list);
}

// EGL_EXT_stream_consumer_egloutput
PFNEGLSTREAMCONSUMEROUTPUTEXTPROC gl::egl::egl_StreamConsumerOutputEXT INIT_POINTER;
EGLBoolean gl::egl::StreamConsumerOutputEXT  (EGLDisplay dpy, EGLStreamKHR stream, EGLOutputLayerEXT layer, const char* file, int line)
{
	TRACE_FUNCTION("eglStreamConsumerOutputEXT(...) called from " << get_path(file) << '(' << line << ')');
	return egl_StreamConsumerOutputEXT(
		dpy,
		stream,
		layer);
}

// EGL_EXT_swap_buffers_with_damage
PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC gl::egl::egl_SwapBuffersWithDamageEXT INIT_POINTER;
EGLBoolean gl::egl::SwapBuffersWithDamageEXT  (EGLDisplay dpy, EGLSurface surface, EGLint *rects, EGLint n_rects, const char* file, int line)
{
	TRACE_FUNCTION("eglSwapBuffersWithDamageEXT(...) called from " << get_path(file) << '(' << line << ')');
	return egl_SwapBuffersWithDamageEXT(
		dpy,
		surface,
		rects,
		n_rects);
}

// EGL_HI_clientpixmap
PFNEGLCREATEPIXMAPSURFACEHIPROC gl::egl::egl_CreatePixmapSurfaceHI INIT_POINTER;
EGLSurface gl::egl::CreatePixmapSurfaceHI  (EGLDisplay dpy, EGLConfig config, struct EGLClientPixmapHI *pixmap, const char* file, int line)
{
	TRACE_FUNCTION("eglCreatePixmapSurfaceHI(...) called from " << get_path(file) << '(' << line << ')');
	return egl_CreatePixmapSurfaceHI(
		dpy,
		config,
		pixmap);
}

// EGL_KHR_cl_event2
PFNEGLCREATESYNC64KHRPROC gl::egl::egl_CreateSync64KHR INIT_POINTER;
EGLSyncKHR gl::egl::CreateSync64KHR  (EGLDisplay dpy, EGLenum type, const EGLAttribKHR *attrib_list, const char* file, int line)
{
	TRACE_FUNCTION("eglCreateSync64KHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_CreateSync64KHR(
		dpy,
		type,
		attrib_list);
}

// EGL_KHR_debug
PFNEGLLABELOBJECTKHRPROC gl::egl::egl_LabelObjectKHR INIT_POINTER;
EGLint gl::egl::LabelObjectKHR  (EGLDisplay display, EGLenum objectType, EGLObjectKHR object, EGLLabelKHR label, const char* file, int line)
{
	TRACE_FUNCTION("eglLabelObjectKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_LabelObjectKHR(
		display,
		objectType,
		object,
		label);
}

PFNEGLQUERYDEBUGKHRPROC gl::egl::egl_QueryDebugKHR INIT_POINTER;
EGLBoolean gl::egl::QueryDebugKHR  (EGLint attribute, EGLAttrib *value, const char* file, int line)
{
	TRACE_FUNCTION("eglQueryDebugKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_QueryDebugKHR(
		attribute,
		value);
}

// EGL_KHR_fence_sync
PFNEGLCLIENTWAITSYNCKHRPROC gl::egl::egl_ClientWaitSyncKHR INIT_POINTER;
EGLint gl::egl::ClientWaitSyncKHR  (EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout, const char* file, int line)
{
	TRACE_FUNCTION("eglClientWaitSyncKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_ClientWaitSyncKHR(
		dpy,
		sync,
		flags,
		timeout);
}

PFNEGLCREATESYNCKHRPROC gl::egl::egl_CreateSyncKHR INIT_POINTER;
EGLSyncKHR gl::egl::CreateSyncKHR  (EGLDisplay dpy, EGLenum type, const EGLint *attrib_list, const char* file, int line)
{
	TRACE_FUNCTION("eglCreateSyncKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_CreateSyncKHR(
		dpy,
		type,
		attrib_list);
}

PFNEGLDESTROYSYNCKHRPROC gl::egl::egl_DestroySyncKHR INIT_POINTER;
EGLBoolean gl::egl::DestroySyncKHR  (EGLDisplay dpy, EGLSyncKHR sync, const char* file, int line)
{
	TRACE_FUNCTION("eglDestroySyncKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_DestroySyncKHR(
		dpy,
		sync);
}

PFNEGLGETSYNCATTRIBKHRPROC gl::egl::egl_GetSyncAttribKHR INIT_POINTER;
EGLBoolean gl::egl::GetSyncAttribKHR  (EGLDisplay dpy, EGLSyncKHR sync, EGLint attribute, EGLint *value, const char* file, int line)
{
	TRACE_FUNCTION("eglGetSyncAttribKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_GetSyncAttribKHR(
		dpy,
		sync,
		attribute,
		value);
}

// EGL_KHR_image
PFNEGLCREATEIMAGEKHRPROC gl::egl::egl_CreateImageKHR INIT_POINTER;
EGLImageKHR gl::egl::CreateImageKHR  (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list, const char* file, int line)
{
	TRACE_FUNCTION("eglCreateImageKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_CreateImageKHR(
		dpy,
		ctx,
		target,
		buffer,
		attrib_list);
}

PFNEGLDESTROYIMAGEKHRPROC gl::egl::egl_DestroyImageKHR INIT_POINTER;
EGLBoolean gl::egl::DestroyImageKHR  (EGLDisplay dpy, EGLImageKHR image, const char* file, int line)
{
	TRACE_FUNCTION("eglDestroyImageKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_DestroyImageKHR(
		dpy,
		image);
}

// EGL_KHR_lock_surface
PFNEGLLOCKSURFACEKHRPROC gl::egl::egl_LockSurfaceKHR INIT_POINTER;
EGLBoolean gl::egl::LockSurfaceKHR  (EGLDisplay dpy, EGLSurface surface, const EGLint *attrib_list, const char* file, int line)
{
	TRACE_FUNCTION("eglLockSurfaceKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_LockSurfaceKHR(
		dpy,
		surface,
		attrib_list);
}

PFNEGLUNLOCKSURFACEKHRPROC gl::egl::egl_UnlockSurfaceKHR INIT_POINTER;
EGLBoolean gl::egl::UnlockSurfaceKHR  (EGLDisplay dpy, EGLSurface surface, const char* file, int line)
{
	TRACE_FUNCTION("eglUnlockSurfaceKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_UnlockSurfaceKHR(
		dpy,
		surface);
}

// EGL_KHR_lock_surface3
PFNEGLQUERYSURFACE64KHRPROC gl::egl::egl_QuerySurface64KHR INIT_POINTER;
EGLBoolean gl::egl::QuerySurface64KHR  (EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLAttribKHR *value, const char* file, int line)
{
	TRACE_FUNCTION("eglQuerySurface64KHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_QuerySurface64KHR(
		dpy,
		surface,
		attribute,
		value);
}

// EGL_KHR_partial_update
PFNEGLSETDAMAGEREGIONKHRPROC gl::egl::egl_SetDamageRegionKHR INIT_POINTER;
EGLBoolean gl::egl::SetDamageRegionKHR  (EGLDisplay dpy, EGLSurface surface, EGLint *rects, EGLint n_rects, const char* file, int line)
{
	TRACE_FUNCTION("eglSetDamageRegionKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_SetDamageRegionKHR(
		dpy,
		surface,
		rects,
		n_rects);
}

// EGL_KHR_reusable_sync
PFNEGLSIGNALSYNCKHRPROC gl::egl::egl_SignalSyncKHR INIT_POINTER;
EGLBoolean gl::egl::SignalSyncKHR  (EGLDisplay dpy, EGLSyncKHR sync, EGLenum mode, const char* file, int line)
{
	TRACE_FUNCTION("eglSignalSyncKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_SignalSyncKHR(
		dpy,
		sync,
		mode);
}

// EGL_KHR_stream
PFNEGLCREATESTREAMKHRPROC gl::egl::egl_CreateStreamKHR INIT_POINTER;
EGLStreamKHR gl::egl::CreateStreamKHR  (EGLDisplay dpy, const EGLint *attrib_list, const char* file, int line)
{
	TRACE_FUNCTION("eglCreateStreamKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_CreateStreamKHR(
		dpy,
		attrib_list);
}

PFNEGLDESTROYSTREAMKHRPROC gl::egl::egl_DestroyStreamKHR INIT_POINTER;
EGLBoolean gl::egl::DestroyStreamKHR  (EGLDisplay dpy, EGLStreamKHR stream, const char* file, int line)
{
	TRACE_FUNCTION("eglDestroyStreamKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_DestroyStreamKHR(
		dpy,
		stream);
}

PFNEGLQUERYSTREAMKHRPROC gl::egl::egl_QueryStreamKHR INIT_POINTER;
EGLBoolean gl::egl::QueryStreamKHR  (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLint *value, const char* file, int line)
{
	TRACE_FUNCTION("eglQueryStreamKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_QueryStreamKHR(
		dpy,
		stream,
		attribute,
		value);
}

PFNEGLQUERYSTREAMU64KHRPROC gl::egl::egl_QueryStreamu64KHR INIT_POINTER;
EGLBoolean gl::egl::QueryStreamu64KHR  (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLuint64KHR *value, const char* file, int line)
{
	TRACE_FUNCTION("eglQueryStreamu64KHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_QueryStreamu64KHR(
		dpy,
		stream,
		attribute,
		value);
}

PFNEGLSTREAMATTRIBKHRPROC gl::egl::egl_StreamAttribKHR INIT_POINTER;
EGLBoolean gl::egl::StreamAttribKHR  (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLint value, const char* file, int line)
{
	TRACE_FUNCTION("eglStreamAttribKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_StreamAttribKHR(
		dpy,
		stream,
		attribute,
		value);
}

// EGL_KHR_stream_attrib
PFNEGLCREATESTREAMATTRIBKHRPROC gl::egl::egl_CreateStreamAttribKHR INIT_POINTER;
EGLStreamKHR gl::egl::CreateStreamAttribKHR  (EGLDisplay dpy, const EGLAttrib *attrib_list, const char* file, int line)
{
	TRACE_FUNCTION("eglCreateStreamAttribKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_CreateStreamAttribKHR(
		dpy,
		attrib_list);
}

PFNEGLQUERYSTREAMATTRIBKHRPROC gl::egl::egl_QueryStreamAttribKHR INIT_POINTER;
EGLBoolean gl::egl::QueryStreamAttribKHR  (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLAttrib *value, const char* file, int line)
{
	TRACE_FUNCTION("eglQueryStreamAttribKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_QueryStreamAttribKHR(
		dpy,
		stream,
		attribute,
		value);
}

PFNEGLSETSTREAMATTRIBKHRPROC gl::egl::egl_SetStreamAttribKHR INIT_POINTER;
EGLBoolean gl::egl::SetStreamAttribKHR  (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLAttrib value, const char* file, int line)
{
	TRACE_FUNCTION("eglSetStreamAttribKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_SetStreamAttribKHR(
		dpy,
		stream,
		attribute,
		value);
}

PFNEGLSTREAMCONSUMERACQUIREATTRIBKHRPROC gl::egl::egl_StreamConsumerAcquireAttribKHR INIT_POINTER;
EGLBoolean gl::egl::StreamConsumerAcquireAttribKHR  (EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib *attrib_list, const char* file, int line)
{
	TRACE_FUNCTION("eglStreamConsumerAcquireAttribKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_StreamConsumerAcquireAttribKHR(
		dpy,
		stream,
		attrib_list);
}

PFNEGLSTREAMCONSUMERRELEASEATTRIBKHRPROC gl::egl::egl_StreamConsumerReleaseAttribKHR INIT_POINTER;
EGLBoolean gl::egl::StreamConsumerReleaseAttribKHR  (EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib *attrib_list, const char* file, int line)
{
	TRACE_FUNCTION("eglStreamConsumerReleaseAttribKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_StreamConsumerReleaseAttribKHR(
		dpy,
		stream,
		attrib_list);
}

// EGL_KHR_stream_consumer_gltexture
PFNEGLSTREAMCONSUMERACQUIREKHRPROC gl::egl::egl_StreamConsumerAcquireKHR INIT_POINTER;
EGLBoolean gl::egl::StreamConsumerAcquireKHR  (EGLDisplay dpy, EGLStreamKHR stream, const char* file, int line)
{
	TRACE_FUNCTION("eglStreamConsumerAcquireKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_StreamConsumerAcquireKHR(
		dpy,
		stream);
}

PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALKHRPROC gl::egl::egl_StreamConsumerGLTextureExternalKHR INIT_POINTER;
EGLBoolean gl::egl::StreamConsumerGLTextureExternalKHR  (EGLDisplay dpy, EGLStreamKHR stream, const char* file, int line)
{
	TRACE_FUNCTION("eglStreamConsumerGLTextureExternalKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_StreamConsumerGLTextureExternalKHR(
		dpy,
		stream);
}

PFNEGLSTREAMCONSUMERRELEASEKHRPROC gl::egl::egl_StreamConsumerReleaseKHR INIT_POINTER;
EGLBoolean gl::egl::StreamConsumerReleaseKHR  (EGLDisplay dpy, EGLStreamKHR stream, const char* file, int line)
{
	TRACE_FUNCTION("eglStreamConsumerReleaseKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_StreamConsumerReleaseKHR(
		dpy,
		stream);
}

// EGL_KHR_stream_cross_process_fd
PFNEGLCREATESTREAMFROMFILEDESCRIPTORKHRPROC gl::egl::egl_CreateStreamFromFileDescriptorKHR INIT_POINTER;
EGLStreamKHR gl::egl::CreateStreamFromFileDescriptorKHR  (EGLDisplay dpy, EGLNativeFileDescriptorKHR file_descriptor, const char* file, int line)
{
	TRACE_FUNCTION("eglCreateStreamFromFileDescriptorKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_CreateStreamFromFileDescriptorKHR(
		dpy,
		file_descriptor);
}

PFNEGLGETSTREAMFILEDESCRIPTORKHRPROC gl::egl::egl_GetStreamFileDescriptorKHR INIT_POINTER;
EGLNativeFileDescriptorKHR gl::egl::GetStreamFileDescriptorKHR  (EGLDisplay dpy, EGLStreamKHR stream, const char* file, int line)
{
	TRACE_FUNCTION("eglGetStreamFileDescriptorKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_GetStreamFileDescriptorKHR(
		dpy,
		stream);
}

// EGL_KHR_stream_fifo
PFNEGLQUERYSTREAMTIMEKHRPROC gl::egl::egl_QueryStreamTimeKHR INIT_POINTER;
EGLBoolean gl::egl::QueryStreamTimeKHR  (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLTimeKHR *value, const char* file, int line)
{
	TRACE_FUNCTION("eglQueryStreamTimeKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_QueryStreamTimeKHR(
		dpy,
		stream,
		attribute,
		value);
}

// EGL_KHR_stream_producer_eglsurface
PFNEGLCREATESTREAMPRODUCERSURFACEKHRPROC gl::egl::egl_CreateStreamProducerSurfaceKHR INIT_POINTER;
EGLSurface gl::egl::CreateStreamProducerSurfaceKHR  (EGLDisplay dpy, EGLConfig config, EGLStreamKHR stream, const EGLint *attrib_list, const char* file, int line)
{
	TRACE_FUNCTION("eglCreateStreamProducerSurfaceKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_CreateStreamProducerSurfaceKHR(
		dpy,
		config,
		stream,
		attrib_list);
}

// EGL_KHR_swap_buffers_with_damage
PFNEGLSWAPBUFFERSWITHDAMAGEKHRPROC gl::egl::egl_SwapBuffersWithDamageKHR INIT_POINTER;
EGLBoolean gl::egl::SwapBuffersWithDamageKHR  (EGLDisplay dpy, EGLSurface surface, EGLint *rects, EGLint n_rects, const char* file, int line)
{
	TRACE_FUNCTION("eglSwapBuffersWithDamageKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_SwapBuffersWithDamageKHR(
		dpy,
		surface,
		rects,
		n_rects);
}

// EGL_KHR_wait_sync
PFNEGLWAITSYNCKHRPROC gl::egl::egl_WaitSyncKHR INIT_POINTER;
EGLint gl::egl::WaitSyncKHR  (EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, const char* file, int line)
{
	TRACE_FUNCTION("eglWaitSyncKHR(...) called from " << get_path(file) << '(' << line << ')');
	return egl_WaitSyncKHR(
		dpy,
		sync,
		flags);
}

// EGL_MESA_drm_image
PFNEGLCREATEDRMIMAGEMESAPROC gl::egl::egl_CreateDRMImageMESA INIT_POINTER;
EGLImageKHR gl::egl::CreateDRMImageMESA  (EGLDisplay dpy, const EGLint *attrib_list, const char* file, int line)
{
	TRACE_FUNCTION("eglCreateDRMImageMESA(...) called from " << get_path(file) << '(' << line << ')');
	return egl_CreateDRMImageMESA(
		dpy,
		attrib_list);
}

PFNEGLEXPORTDRMIMAGEMESAPROC gl::egl::egl_ExportDRMImageMESA INIT_POINTER;
EGLBoolean gl::egl::ExportDRMImageMESA  (EGLDisplay dpy, EGLImageKHR image, EGLint *name, EGLint *handle, EGLint *stride, const char* file, int line)
{
	TRACE_FUNCTION("eglExportDRMImageMESA(...) called from " << get_path(file) << '(' << line << ')');
	return egl_ExportDRMImageMESA(
		dpy,
		image,
		name,
		handle,
		stride);
}

// EGL_MESA_image_dma_buf_export
PFNEGLEXPORTDMABUFIMAGEMESAPROC gl::egl::egl_ExportDMABUFImageMESA INIT_POINTER;
EGLBoolean gl::egl::ExportDMABUFImageMESA  (EGLDisplay dpy, EGLImageKHR image, int *fds, EGLint *strides, EGLint *offsets, const char* file, int line)
{
	TRACE_FUNCTION("eglExportDMABUFImageMESA(...) called from " << get_path(file) << '(' << line << ')');
	return egl_ExportDMABUFImageMESA(
		dpy,
		image,
		fds,
		strides,
		offsets);
}

PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC gl::egl::egl_ExportDMABUFImageQueryMESA INIT_POINTER;
EGLBoolean gl::egl::ExportDMABUFImageQueryMESA  (EGLDisplay dpy, EGLImageKHR image, int *fourcc, int *num_planes, EGLuint64KHR *modifiers, const char* file, int line)
{
	TRACE_FUNCTION("eglExportDMABUFImageQueryMESA(...) called from " << get_path(file) << '(' << line << ')');
	return egl_ExportDMABUFImageQueryMESA(
		dpy,
		image,
		fourcc,
		num_planes,
		modifiers);
}

// EGL_NOK_swap_region
PFNEGLSWAPBUFFERSREGIONNOKPROC gl::egl::egl_SwapBuffersRegionNOK INIT_POINTER;
EGLBoolean gl::egl::SwapBuffersRegionNOK  (EGLDisplay dpy, EGLSurface surface, EGLint numRects, const EGLint *rects, const char* file, int line)
{
	TRACE_FUNCTION("eglSwapBuffersRegionNOK(...) called from " << get_path(file) << '(' << line << ')');
	return egl_SwapBuffersRegionNOK(
		dpy,
		surface,
		numRects,
		rects);
}

// EGL_NOK_swap_region2
PFNEGLSWAPBUFFERSREGION2NOKPROC gl::egl::egl_SwapBuffersRegion2NOK INIT_POINTER;
EGLBoolean gl::egl::SwapBuffersRegion2NOK  (EGLDisplay dpy, EGLSurface surface, EGLint numRects, const EGLint *rects, const char* file, int line)
{
	TRACE_FUNCTION("eglSwapBuffersRegion2NOK(...) called from " << get_path(file) << '(' << line << ')');
	return egl_SwapBuffersRegion2NOK(
		dpy,
		surface,
		numRects,
		rects);
}

// EGL_NV_native_query
PFNEGLQUERYNATIVEDISPLAYNVPROC gl::egl::egl_QueryNativeDisplayNV INIT_POINTER;
EGLBoolean gl::egl::QueryNativeDisplayNV  (EGLDisplay dpy, EGLNativeDisplayType *display_id, const char* file, int line)
{
	TRACE_FUNCTION("eglQueryNativeDisplayNV(...) called from " << get_path(file) << '(' << line << ')');
	return egl_QueryNativeDisplayNV(
		dpy,
		display_id);
}

PFNEGLQUERYNATIVEPIXMAPNVPROC gl::egl::egl_QueryNativePixmapNV INIT_POINTER;
EGLBoolean gl::egl::QueryNativePixmapNV  (EGLDisplay dpy, EGLSurface surf, EGLNativePixmapType *pixmap, const char* file, int line)
{
	TRACE_FUNCTION("eglQueryNativePixmapNV(...) called from " << get_path(file) << '(' << line << ')');
	return egl_QueryNativePixmapNV(
		dpy,
		surf,
		pixmap);
}

PFNEGLQUERYNATIVEWINDOWNVPROC gl::egl::egl_QueryNativeWindowNV INIT_POINTER;
EGLBoolean gl::egl::QueryNativeWindowNV  (EGLDisplay dpy, EGLSurface surf, EGLNativeWindowType *window, const char* file, int line)
{
	TRACE_FUNCTION("eglQueryNativeWindowNV(...) called from " << get_path(file) << '(' << line << ')');
	return egl_QueryNativeWindowNV(
		dpy,
		surf,
		window);
}

// EGL_NV_post_sub_buffer
PFNEGLPOSTSUBBUFFERNVPROC gl::egl::egl_PostSubBufferNV INIT_POINTER;
EGLBoolean gl::egl::PostSubBufferNV  (EGLDisplay dpy, EGLSurface surface, EGLint x, EGLint y, EGLint width, EGLint height, const char* file, int line)
{
	TRACE_FUNCTION("eglPostSubBufferNV(...) called from " << get_path(file) << '(' << line << ')');
	return egl_PostSubBufferNV(
		dpy,
		surface,
		x,
		y,
		width,
		height);
}

// EGL_NV_stream_consumer_gltexture_yuv
PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALATTRIBSNVPROC gl::egl::egl_StreamConsumerGLTextureExternalAttribsNV INIT_POINTER;
EGLBoolean gl::egl::StreamConsumerGLTextureExternalAttribsNV  (EGLDisplay dpy, EGLStreamKHR stream, EGLAttrib *attrib_list, const char* file, int line)
{
	TRACE_FUNCTION("eglStreamConsumerGLTextureExternalAttribsNV(...) called from " << get_path(file) << '(' << line << ')');
	return egl_StreamConsumerGLTextureExternalAttribsNV(
		dpy,
		stream,
		attrib_list);
}

// EGL_NV_stream_metadata
PFNEGLQUERYDISPLAYATTRIBNVPROC gl::egl::egl_QueryDisplayAttribNV INIT_POINTER;
EGLBoolean gl::egl::QueryDisplayAttribNV  (EGLDisplay dpy, EGLint attribute, EGLAttrib *value, const char* file, int line)
{
	TRACE_FUNCTION("eglQueryDisplayAttribNV(...) called from " << get_path(file) << '(' << line << ')');
	return egl_QueryDisplayAttribNV(
		dpy,
		attribute,
		value);
}

PFNEGLQUERYSTREAMMETADATANVPROC gl::egl::egl_QueryStreamMetadataNV INIT_POINTER;
EGLBoolean gl::egl::QueryStreamMetadataNV  (EGLDisplay dpy, EGLStreamKHR stream, EGLenum name, EGLint n, EGLint offset, EGLint size, void *data, const char* file, int line)
{
	TRACE_FUNCTION("eglQueryStreamMetadataNV(...) called from " << get_path(file) << '(' << line << ')');
	return egl_QueryStreamMetadataNV(
		dpy,
		stream,
		name,
		n,
		offset,
		size,
		data);
}

PFNEGLSETSTREAMMETADATANVPROC gl::egl::egl_SetStreamMetadataNV INIT_POINTER;
EGLBoolean gl::egl::SetStreamMetadataNV  (EGLDisplay dpy, EGLStreamKHR stream, EGLint n, EGLint offset, EGLint size, const void *data, const char* file, int line)
{
	TRACE_FUNCTION("eglSetStreamMetadataNV(...) called from " << get_path(file) << '(' << line << ')');
	return egl_SetStreamMetadataNV(
		dpy,
		stream,
		n,
		offset,
		size,
		data);
}

// EGL_NV_stream_sync
PFNEGLCREATESTREAMSYNCNVPROC gl::egl::egl_CreateStreamSyncNV INIT_POINTER;
EGLSyncKHR gl::egl::CreateStreamSyncNV  (EGLDisplay dpy, EGLStreamKHR stream, EGLenum type, const EGLint *attrib_list, const char* file, int line)
{
	TRACE_FUNCTION("eglCreateStreamSyncNV(...) called from " << get_path(file) << '(' << line << ')');
	return egl_CreateStreamSyncNV(
		dpy,
		stream,
		type,
		attrib_list);
}

// EGL_NV_sync
PFNEGLCLIENTWAITSYNCNVPROC gl::egl::egl_ClientWaitSyncNV INIT_POINTER;
EGLint gl::egl::ClientWaitSyncNV  (EGLSyncNV sync, EGLint flags, EGLTimeNV timeout, const char* file, int line)
{
	TRACE_FUNCTION("eglClientWaitSyncNV(...) called from " << get_path(file) << '(' << line << ')');
	return egl_ClientWaitSyncNV(
		sync,
		flags,
		timeout);
}

PFNEGLCREATEFENCESYNCNVPROC gl::egl::egl_CreateFenceSyncNV INIT_POINTER;
EGLSyncNV gl::egl::CreateFenceSyncNV  (EGLDisplay dpy, EGLenum condition, const EGLint *attrib_list, const char* file, int line)
{
	TRACE_FUNCTION("eglCreateFenceSyncNV(...) called from " << get_path(file) << '(' << line << ')');
	return egl_CreateFenceSyncNV(
		dpy,
		condition,
		attrib_list);
}

PFNEGLDESTROYSYNCNVPROC gl::egl::egl_DestroySyncNV INIT_POINTER;
EGLBoolean gl::egl::DestroySyncNV  (EGLSyncNV sync, const char* file, int line)
{
	TRACE_FUNCTION("eglDestroySyncNV(...) called from " << get_path(file) << '(' << line << ')');
	return egl_DestroySyncNV(
		sync);
}

PFNEGLFENCENVPROC gl::egl::egl_FenceNV INIT_POINTER;
EGLBoolean gl::egl::FenceNV  (EGLSyncNV sync, const char* file, int line)
{
	TRACE_FUNCTION("eglFenceNV(...) called from " << get_path(file) << '(' << line << ')');
	return egl_FenceNV(
		sync);
}

PFNEGLGETSYNCATTRIBNVPROC gl::egl::egl_GetSyncAttribNV INIT_POINTER;
EGLBoolean gl::egl::GetSyncAttribNV  (EGLSyncNV sync, EGLint attribute, EGLint *value, const char* file, int line)
{
	TRACE_FUNCTION("eglGetSyncAttribNV(...) called from " << get_path(file) << '(' << line << ')');
	return egl_GetSyncAttribNV(
		sync,
		attribute,
		value);
}

PFNEGLSIGNALSYNCNVPROC gl::egl::egl_SignalSyncNV INIT_POINTER;
EGLBoolean gl::egl::SignalSyncNV  (EGLSyncNV sync, EGLenum mode, const char* file, int line)
{
	TRACE_FUNCTION("eglSignalSyncNV(...) called from " << 
		get_path(file) << '(' << line << ')');
	return egl_SignalSyncNV(
		sync,
		mode);
}

// EGL_NV_system_time
PFNEGLGETSYSTEMTIMEFREQUENCYNVPROC gl::egl::egl_GetSystemTimeFrequencyNV INIT_POINTER;
EGLuint64NV gl::egl::GetSystemTimeFrequencyNV  (const char* file, int line)
{
	TRACE_FUNCTION("eglGetSystemTimeFrequencyNV(...) called from " << 
		get_path(file) << '(' << line << ')');
	return egl_GetSystemTimeFrequencyNV();
}

PFNEGLGETSYSTEMTIMENVPROC gl::egl::egl_GetSystemTimeNV INIT_POINTER;
EGLuint64NV gl::egl::GetSystemTimeNV (const char* file, int line)
{
	TRACE_FUNCTION("eglGetSystemTimeNV(...) called from " << 
		get_path(file) << '(' << line << ')');
	return egl_GetSystemTimeNV();
}

// ---------------------------------------------------------------------
// ---------------------------------------------------------------------

const char* gl::is_define_gl2_h(GLenum pname)
{
	switch(pname)
	{
		case GL_DEPTH_BUFFER_BIT: return "GL_DEPTH_BUFFER_BIT";
		case GL_STENCIL_BUFFER_BIT: return "GL_STENCIL_BUFFER_BIT";
		case GL_COLOR_BUFFER_BIT: return "GL_COLOR_BUFFER_BIT";

		case GL_POINTS: return "GL_POINTS";
		case GL_LINES: return "GL_LINES";
		case GL_LINE_LOOP: return "GL_LINE_LOOP";
		case GL_LINE_STRIP: return "GL_LINE_STRIP";
		case GL_TRIANGLES: return "GL_TRIANGLES";
		case GL_TRIANGLE_STRIP: return "GL_TRIANGLE_STRIP";
		case GL_TRIANGLE_FAN: return "GL_TRIANGLE_FAN";

		case GL_SRC_COLOR: return "GL_SRC_COLOR";
		case GL_ONE_MINUS_SRC_COLOR: return "GL_ONE_MINUS_SRC_COLOR";
		case GL_SRC_ALPHA: return "GL_SRC_ALPHA";
		case GL_ONE_MINUS_SRC_ALPHA: return "GL_ONE_MINUS_SRC_ALPHA";
		case GL_DST_ALPHA: return "GL_DST_ALPHA";
		case GL_ONE_MINUS_DST_ALPHA: return "GL_ONE_MINUS_DST_ALPHA";
		case GL_DST_COLOR: return "GL_DST_COLOR";
		case GL_ONE_MINUS_DST_COLOR: return "GL_ONE_MINUS_DST_COLOR";
		case GL_SRC_ALPHA_SATURATE: return "GL_SRC_ALPHA_SATURATE";
		case GL_FUNC_ADD: return "GL_FUNC_ADD";
		case GL_BLEND_EQUATION: return "GL_BLEND_EQUATION";
		//case GL_BLEND_EQUATION_RGB: return "GL_BLEND_EQUATION_RGB";
		case GL_BLEND_EQUATION_ALPHA: return "GL_BLEND_EQUATION_ALPHA";
		case GL_FUNC_SUBTRACT: return "GL_FUNC_SUBTRACT";
		case GL_FUNC_REVERSE_SUBTRACT: return "GL_FUNC_REVERSE_SUBTRACT";
		case GL_BLEND_DST_RGB: return "GL_BLEND_DST_RGB";
		case GL_BLEND_SRC_RGB: return "GL_BLEND_SRC_RGB";
		case GL_BLEND_DST_ALPHA: return "GL_BLEND_DST_ALPHA";
		case GL_BLEND_SRC_ALPHA: return "GL_BLEND_SRC_ALPHA";
		case GL_CONSTANT_COLOR: return "GL_CONSTANT_COLOR";
		case GL_ONE_MINUS_CONSTANT_COLOR: return "GL_ONE_MINUS_CONSTANT_COLOR";
		case GL_CONSTANT_ALPHA: return "GL_CONSTANT_ALPHA";
		case GL_ONE_MINUS_CONSTANT_ALPHA: return "GL_ONE_MINUS_CONSTANT_ALPHA";
		case GL_BLEND_COLOR: return "GL_BLEND_COLOR";
		case GL_ARRAY_BUFFER: return "GL_ARRAY_BUFFER";
		case GL_ELEMENT_ARRAY_BUFFER: return "GL_ELEMENT_ARRAY_BUFFER";
		case GL_ARRAY_BUFFER_BINDING: return "GL_ARRAY_BUFFER_BINDING";
		case GL_ELEMENT_ARRAY_BUFFER_BINDING: return "GL_ELEMENT_ARRAY_BUFFER_BINDING";
		case GL_STREAM_DRAW: return "GL_STREAM_DRAW";
		case GL_STATIC_DRAW: return "GL_STATIC_DRAW";
		case GL_DYNAMIC_DRAW: return "GL_DYNAMIC_DRAW";
		case GL_BUFFER_SIZE: return "GL_BUFFER_SIZE";
		case GL_BUFFER_USAGE: return "GL_BUFFER_USAGE";
		case GL_CURRENT_VERTEX_ATTRIB: return "GL_CURRENT_VERTEX_ATTRIB";
		case GL_FRONT: return "GL_FRONT";
		case GL_BACK: return "GL_BACK";
		case GL_FRONT_AND_BACK: return "GL_FRONT_AND_BACK";
		case GL_TEXTURE_2D: return "GL_TEXTURE_2D";
		case GL_CULL_FACE: return "GL_CULL_FACE";
		case GL_BLEND: return "GL_BLEND";
		case GL_DITHER: return "GL_DITHER";
		case GL_STENCIL_TEST: return "GL_STENCIL_TEST";
		case GL_DEPTH_TEST: return "GL_DEPTH_TEST";
		case GL_SCISSOR_TEST: return "GL_SCISSOR_TEST";
		case GL_POLYGON_OFFSET_FILL: return "GL_POLYGON_OFFSET_FILL";
		case GL_SAMPLE_ALPHA_TO_COVERAGE: return "GL_SAMPLE_ALPHA_TO_COVERAGE";
		case GL_SAMPLE_COVERAGE: return "GL_SAMPLE_COVERAGE";
		case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
		case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
		case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
		case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
		case GL_CW: return "GL_CW";
		case GL_CCW: return "GL_CCW";
		case GL_LINE_WIDTH: return "GL_LINE_WIDTH";
		case GL_ALIASED_POINT_SIZE_RANGE: return "GL_ALIASED_POINT_SIZE_RANGE";
		case GL_ALIASED_LINE_WIDTH_RANGE: return "GL_ALIASED_LINE_WIDTH_RANGE";
		case GL_CULL_FACE_MODE: return "GL_CULL_FACE_MODE";
		case GL_FRONT_FACE: return "GL_FRONT_FACE";
		case GL_DEPTH_RANGE: return "GL_DEPTH_RANGE";
		case GL_DEPTH_WRITEMASK: return "GL_DEPTH_WRITEMASK";
		case GL_DEPTH_CLEAR_VALUE: return "GL_DEPTH_CLEAR_VALUE";
		case GL_DEPTH_FUNC: return "GL_DEPTH_FUNC";
		case GL_STENCIL_CLEAR_VALUE: return "GL_STENCIL_CLEAR_VALUE";
		case GL_STENCIL_FUNC: return "GL_STENCIL_FUNC";
		case GL_STENCIL_FAIL: return "GL_STENCIL_FAIL";
		case GL_STENCIL_PASS_DEPTH_FAIL: return "GL_STENCIL_PASS_DEPTH_FAIL";
		case GL_STENCIL_PASS_DEPTH_PASS: return "GL_STENCIL_PASS_DEPTH_PASS";
		case GL_STENCIL_REF: return "GL_STENCIL_REF";
		case GL_STENCIL_VALUE_MASK: return "GL_STENCIL_VALUE_MASK";
		case GL_STENCIL_WRITEMASK: return "GL_STENCIL_WRITEMASK";
		case GL_STENCIL_BACK_FUNC: return "GL_STENCIL_BACK_FUNC";
		case GL_STENCIL_BACK_FAIL: return "GL_STENCIL_BACK_FAIL";
		case GL_STENCIL_BACK_PASS_DEPTH_FAIL: return "GL_STENCIL_BACK_PASS_DEPTH_FAIL";
		case GL_STENCIL_BACK_PASS_DEPTH_PASS: return "GL_STENCIL_BACK_PASS_DEPTH_PASS";
		case GL_STENCIL_BACK_REF: return "GL_STENCIL_BACK_REF";
		case GL_STENCIL_BACK_VALUE_MASK: return "GL_STENCIL_BACK_VALUE_MASK";
		case GL_STENCIL_BACK_WRITEMASK: return "GL_STENCIL_BACK_WRITEMASK";
		case GL_VIEWPORT: return "GL_VIEWPORT";
		case GL_SCISSOR_BOX: return "GL_SCISSOR_BOX";
		case GL_COLOR_CLEAR_VALUE: return "GL_COLOR_CLEAR_VALUE";
		case GL_COLOR_WRITEMASK: return "GL_COLOR_WRITEMASK";
		case GL_UNPACK_ALIGNMENT: return "GL_UNPACK_ALIGNMENT";
		case GL_PACK_ALIGNMENT: return "GL_PACK_ALIGNMENT";
		case GL_MAX_TEXTURE_SIZE: return "GL_MAX_TEXTURE_SIZE";
		case GL_MAX_VIEWPORT_DIMS: return "GL_MAX_VIEWPORT_DIMS";
		case GL_SUBPIXEL_BITS: return "GL_SUBPIXEL_BITS";
		case GL_RED_BITS: return "GL_RED_BITS";
		case GL_GREEN_BITS: return "GL_GREEN_BITS";
		case GL_BLUE_BITS: return "GL_BLUE_BITS";
		case GL_ALPHA_BITS: return "GL_ALPHA_BITS";
		case GL_DEPTH_BITS: return "GL_DEPTH_BITS";
		case GL_STENCIL_BITS: return "GL_STENCIL_BITS";
		case GL_POLYGON_OFFSET_UNITS: return "GL_POLYGON_OFFSET_UNITS";
		case GL_POLYGON_OFFSET_FACTOR: return "GL_POLYGON_OFFSET_FACTOR";
		case GL_TEXTURE_BINDING_2D: return "GL_TEXTURE_BINDING_2D";
		case GL_SAMPLE_BUFFERS: return "GL_SAMPLE_BUFFERS";
		case GL_SAMPLES: return "GL_SAMPLES";
		case GL_SAMPLE_COVERAGE_VALUE: return "GL_SAMPLE_COVERAGE_VALUE";
		case GL_SAMPLE_COVERAGE_INVERT: return "GL_SAMPLE_COVERAGE_INVERT";
		case GL_NUM_COMPRESSED_TEXTURE_FORMATS: return "GL_NUM_COMPRESSED_TEXTURE_FORMATS";
		case GL_COMPRESSED_TEXTURE_FORMATS: return "GL_COMPRESSED_TEXTURE_FORMATS";
		case GL_DONT_CARE: return "GL_DONT_CARE";
		case GL_FASTEST: return "GL_FASTEST";
		case GL_NICEST: return "GL_NICEST";
		case GL_GENERATE_MIPMAP_HINT: return "GL_GENERATE_MIPMAP_HINT";
		case GL_BYTE: return "GL_BYTE";
		case GL_UNSIGNED_BYTE: return "GL_UNSIGNED_BYTE";
		case GL_SHORT: return "GL_SHORT";
		case GL_UNSIGNED_SHORT: return "GL_UNSIGNED_SHORT";
		case GL_INT: return "GL_INT";
		case GL_UNSIGNED_INT: return "GL_UNSIGNED_INT";
		case GL_FLOAT: return "GL_FLOAT";
		case GL_FIXED: return "GL_FIXED";
		case GL_DEPTH_COMPONENT: return "GL_DEPTH_COMPONENT";
		case GL_ALPHA: return "GL_ALPHA";
		case GL_RGB: return "GL_RGB";
		case GL_RGBA: return "GL_RGBA";
		case GL_LUMINANCE: return "GL_LUMINANCE";
		case GL_LUMINANCE_ALPHA: return "GL_LUMINANCE_ALPHA";
		case GL_UNSIGNED_SHORT_4_4_4_4: return "GL_UNSIGNED_SHORT_4_4_4_4";
		case GL_UNSIGNED_SHORT_5_5_5_1: return "GL_UNSIGNED_SHORT_5_5_5_1";
		case GL_UNSIGNED_SHORT_5_6_5: return "GL_UNSIGNED_SHORT_5_6_5";
		case GL_FRAGMENT_SHADER: return "GL_FRAGMENT_SHADER";
		case GL_VERTEX_SHADER: return "GL_VERTEX_SHADER";
		case GL_MAX_VERTEX_ATTRIBS: return "GL_MAX_VERTEX_ATTRIBS";
		case GL_MAX_VERTEX_UNIFORM_VECTORS: return "GL_MAX_VERTEX_UNIFORM_VECTORS";
		case GL_MAX_VARYING_VECTORS: return "GL_MAX_VARYING_VECTORS";
		case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS: return "GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS";
		case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS: return "GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS";
		case GL_MAX_TEXTURE_IMAGE_UNITS: return "GL_MAX_TEXTURE_IMAGE_UNITS";
		case GL_MAX_FRAGMENT_UNIFORM_VECTORS: return "GL_MAX_FRAGMENT_UNIFORM_VECTORS";
		case GL_SHADER_TYPE: return "GL_SHADER_TYPE";
		case GL_DELETE_STATUS: return "GL_DELETE_STATUS";
		case GL_LINK_STATUS: return "GL_LINK_STATUS";
		case GL_VALIDATE_STATUS: return "GL_VALIDATE_STATUS";
		case GL_ATTACHED_SHADERS: return "GL_ATTACHED_SHADERS";
		case GL_ACTIVE_UNIFORMS: return "GL_ACTIVE_UNIFORMS";
		case GL_ACTIVE_UNIFORM_MAX_LENGTH: return "GL_ACTIVE_UNIFORM_MAX_LENGTH";
		case GL_ACTIVE_ATTRIBUTES: return "GL_ACTIVE_ATTRIBUTES";
		case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH: return "GL_ACTIVE_ATTRIBUTE_MAX_LENGTH";
		case GL_SHADING_LANGUAGE_VERSION: return "GL_SHADING_LANGUAGE_VERSION";
		case GL_CURRENT_PROGRAM: return "GL_CURRENT_PROGRAM";
		case GL_NEVER: return "GL_NEVER";
		case GL_LESS: return "GL_LESS";
		case GL_EQUAL: return "GL_EQUAL";
		case GL_LEQUAL: return "GL_LEQUAL";
		case GL_GREATER: return "GL_GREATER";
		case GL_NOTEQUAL: return "GL_NOTEQUAL";
		case GL_GEQUAL: return "GL_GEQUAL";
		case GL_ALWAYS: return "GL_ALWAYS";
		case GL_KEEP: return "GL_KEEP";
		case GL_REPLACE: return "GL_REPLACE";
		case GL_INCR: return "GL_INCR";
		case GL_DECR: return "GL_DECR";
		case GL_INVERT: return "GL_INVERT";
		case GL_INCR_WRAP: return "GL_INCR_WRAP";
		case GL_DECR_WRAP: return "GL_DECR_WRAP";
		case GL_VENDOR: return "GL_VENDOR";
		case GL_RENDERER: return "GL_RENDERER";
		case GL_VERSION: return "GL_VERSION";
		case GL_EXTENSIONS: return "GL_EXTENSIONS";
		case GL_NEAREST: return "GL_NEAREST";
		case GL_LINEAR: return "GL_LINEAR";
		case GL_NEAREST_MIPMAP_NEAREST: return "GL_NEAREST_MIPMAP_NEAREST";
		case GL_LINEAR_MIPMAP_NEAREST: return "GL_LINEAR_MIPMAP_NEAREST";
		case GL_NEAREST_MIPMAP_LINEAR: return "GL_NEAREST_MIPMAP_LINEAR";
		case GL_LINEAR_MIPMAP_LINEAR: return "GL_LINEAR_MIPMAP_LINEAR";
		case GL_TEXTURE_MAG_FILTER: return "GL_TEXTURE_MAG_FILTER";
		case GL_TEXTURE_MIN_FILTER: return "GL_TEXTURE_MIN_FILTER";
		case GL_TEXTURE_WRAP_S: return "GL_TEXTURE_WRAP_S";
		case GL_TEXTURE_WRAP_T: return "GL_TEXTURE_WRAP_T";
		case GL_TEXTURE: return "GL_TEXTURE";
		case GL_TEXTURE_CUBE_MAP: return "GL_TEXTURE_CUBE_MAP";
		case GL_TEXTURE_BINDING_CUBE_MAP: return "GL_TEXTURE_BINDING_CUBE_MAP";
		case GL_TEXTURE_CUBE_MAP_POSITIVE_X: return "GL_TEXTURE_CUBE_MAP_POSITIVE_X";
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_X: return "GL_TEXTURE_CUBE_MAP_NEGATIVE_X";
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Y: return "GL_TEXTURE_CUBE_MAP_POSITIVE_Y";
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y: return "GL_TEXTURE_CUBE_MAP_NEGATIVE_Y";
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Z: return "GL_TEXTURE_CUBE_MAP_POSITIVE_Z";
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z: return "GL_TEXTURE_CUBE_MAP_NEGATIVE_Z";
		case GL_MAX_CUBE_MAP_TEXTURE_SIZE: return "GL_MAX_CUBE_MAP_TEXTURE_SIZE";
		case GL_TEXTURE0: return "GL_TEXTURE0";
		case GL_TEXTURE1: return "GL_TEXTURE1";
		case GL_TEXTURE2: return "GL_TEXTURE2";
		case GL_TEXTURE3: return "GL_TEXTURE3";
		case GL_TEXTURE4: return "GL_TEXTURE4";
		case GL_TEXTURE5: return "GL_TEXTURE5";
		case GL_TEXTURE6: return "GL_TEXTURE6";
		case GL_TEXTURE7: return "GL_TEXTURE7";
		case GL_TEXTURE8: return "GL_TEXTURE8";
		case GL_TEXTURE9: return "GL_TEXTURE9";
		case GL_TEXTURE10: return "GL_TEXTURE10";
		case GL_TEXTURE11: return "GL_TEXTURE11";
		case GL_TEXTURE12: return "GL_TEXTURE12";
		case GL_TEXTURE13: return "GL_TEXTURE13";
		case GL_TEXTURE14: return "GL_TEXTURE14";
		case GL_TEXTURE15: return "GL_TEXTURE15";
		case GL_TEXTURE16: return "GL_TEXTURE16";
		case GL_TEXTURE17: return "GL_TEXTURE17";
		case GL_TEXTURE18: return "GL_TEXTURE18";
		case GL_TEXTURE19: return "GL_TEXTURE19";
		case GL_TEXTURE20: return "GL_TEXTURE20";
		case GL_TEXTURE21: return "GL_TEXTURE21";
		case GL_TEXTURE22: return "GL_TEXTURE22";
		case GL_TEXTURE23: return "GL_TEXTURE23";
		case GL_TEXTURE24: return "GL_TEXTURE24";
		case GL_TEXTURE25: return "GL_TEXTURE25";
		case GL_TEXTURE26: return "GL_TEXTURE26";
		case GL_TEXTURE27: return "GL_TEXTURE27";
		case GL_TEXTURE28: return "GL_TEXTURE28";
		case GL_TEXTURE29: return "GL_TEXTURE29";
		case GL_TEXTURE30: return "GL_TEXTURE30";
		case GL_TEXTURE31: return "GL_TEXTURE31";
		case GL_ACTIVE_TEXTURE: return "GL_ACTIVE_TEXTURE";
		case GL_REPEAT: return "GL_REPEAT";
		case GL_CLAMP_TO_EDGE: return "GL_CLAMP_TO_EDGE";
		case GL_MIRRORED_REPEAT: return "GL_MIRRORED_REPEAT";
		case GL_FLOAT_VEC2: return "GL_FLOAT_VEC2";
		case GL_FLOAT_VEC3: return "GL_FLOAT_VEC3";
		case GL_FLOAT_VEC4: return "GL_FLOAT_VEC4";
		case GL_INT_VEC2: return "GL_INT_VEC2";
		case GL_INT_VEC3: return "GL_INT_VEC3";
		case GL_INT_VEC4: return "GL_INT_VEC4";
		case GL_BOOL: return "GL_BOOL";
		case GL_BOOL_VEC2: return "GL_BOOL_VEC2";
		case GL_BOOL_VEC3: return "GL_BOOL_VEC3";
		case GL_BOOL_VEC4: return "GL_BOOL_VEC4";
		case GL_FLOAT_MAT2: return "GL_FLOAT_MAT2";
		case GL_FLOAT_MAT3: return "GL_FLOAT_MAT3";
		case GL_FLOAT_MAT4: return "GL_FLOAT_MAT4";
		case GL_SAMPLER_2D: return "GL_SAMPLER_2D";
		case GL_SAMPLER_CUBE: return "GL_SAMPLER_CUBE";
		case GL_VERTEX_ATTRIB_ARRAY_ENABLED: return "GL_VERTEX_ATTRIB_ARRAY_ENABLED";
		case GL_VERTEX_ATTRIB_ARRAY_SIZE: return "GL_VERTEX_ATTRIB_ARRAY_SIZE";
		case GL_VERTEX_ATTRIB_ARRAY_STRIDE: return "GL_VERTEX_ATTRIB_ARRAY_STRIDE";
		case GL_VERTEX_ATTRIB_ARRAY_TYPE: return "GL_VERTEX_ATTRIB_ARRAY_TYPE";
		case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED: return "GL_VERTEX_ATTRIB_ARRAY_NORMALIZED";
		case GL_VERTEX_ATTRIB_ARRAY_POINTER: return "GL_VERTEX_ATTRIB_ARRAY_POINTER";
		case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING: return "GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING";
		case GL_IMPLEMENTATION_COLOR_READ_TYPE: return "GL_IMPLEMENTATION_COLOR_READ_TYPE";
		case GL_IMPLEMENTATION_COLOR_READ_FORMAT: return "GL_IMPLEMENTATION_COLOR_READ_FORMAT";
		case GL_COMPILE_STATUS: return "GL_COMPILE_STATUS";
		case GL_INFO_LOG_LENGTH: return "GL_INFO_LOG_LENGTH";
		case GL_SHADER_SOURCE_LENGTH: return "GL_SHADER_SOURCE_LENGTH";
		case GL_SHADER_COMPILER: return "GL_SHADER_COMPILER";
		case GL_SHADER_BINARY_FORMATS: return "GL_SHADER_BINARY_FORMATS";
		case GL_NUM_SHADER_BINARY_FORMATS: return "GL_NUM_SHADER_BINARY_FORMATS";
		case GL_LOW_FLOAT: return "GL_LOW_FLOAT";
		case GL_MEDIUM_FLOAT: return "GL_MEDIUM_FLOAT";
		case GL_HIGH_FLOAT: return "GL_HIGH_FLOAT";
		case GL_LOW_INT: return "GL_LOW_INT";
		case GL_MEDIUM_INT: return "GL_MEDIUM_INT";
		case GL_HIGH_INT: return "GL_HIGH_INT";
		case GL_FRAMEBUFFER: return "GL_FRAMEBUFFER";
		case GL_RENDERBUFFER: return "GL_RENDERBUFFER";
		case GL_RGBA4: return "GL_RGBA4";
		case GL_RGB5_A1: return "GL_RGB5_A1";
		case GL_RGB565: return "GL_RGB565";
		case GL_DEPTH_COMPONENT16: return "GL_DEPTH_COMPONENT16";
		case GL_STENCIL_INDEX8: return "GL_STENCIL_INDEX8";
		case GL_RENDERBUFFER_WIDTH: return "GL_RENDERBUFFER_WIDTH";
		case GL_RENDERBUFFER_HEIGHT: return "GL_RENDERBUFFER_HEIGHT";
		case GL_RENDERBUFFER_INTERNAL_FORMAT: return "GL_RENDERBUFFER_INTERNAL_FORMAT";
		case GL_RENDERBUFFER_RED_SIZE: return "GL_RENDERBUFFER_RED_SIZE";
		case GL_RENDERBUFFER_GREEN_SIZE: return "GL_RENDERBUFFER_GREEN_SIZE";
		case GL_RENDERBUFFER_BLUE_SIZE: return "GL_RENDERBUFFER_BLUE_SIZE";
		case GL_RENDERBUFFER_ALPHA_SIZE: return "GL_RENDERBUFFER_ALPHA_SIZE";
		case GL_RENDERBUFFER_DEPTH_SIZE: return "GL_RENDERBUFFER_DEPTH_SIZE";
		case GL_RENDERBUFFER_STENCIL_SIZE: return "GL_RENDERBUFFER_STENCIL_SIZE";
		case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE: return "GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE";
		case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME: return "GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME";
		case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL: return "GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL";
		case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE: return "GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE";
		case GL_COLOR_ATTACHMENT0: return "GL_COLOR_ATTACHMENT0";
		case GL_DEPTH_ATTACHMENT: return "GL_DEPTH_ATTACHMENT";
		case GL_STENCIL_ATTACHMENT: return "GL_STENCIL_ATTACHMENT";
		case GL_FRAMEBUFFER_COMPLETE: return "GL_FRAMEBUFFER_COMPLETE";
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT: return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT: return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
		case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS: return "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS";
		case GL_FRAMEBUFFER_UNSUPPORTED: return "GL_FRAMEBUFFER_UNSUPPORTED";
		case GL_FRAMEBUFFER_BINDING: return "GL_FRAMEBUFFER_BINDING";
		case GL_RENDERBUFFER_BINDING: return "GL_RENDERBUFFER_BINDING";
		case GL_MAX_RENDERBUFFER_SIZE: return "GL_MAX_RENDERBUFFER_SIZE";
		case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
	}
	/** search in gl2ext_h */
	return is_define_gl2ext_h(pname);

} // is_define_gl2_h()


const char* gl::is_define_gl2ext_h(GLenum pname)
{
	const char* ret;

	ret = is_define_khr_blend_equation_advanced(pname); if (ret) return ret;
	ret = is_define_khr_blend_equation_advanced_coherent(pname); if (ret) return ret;
	ret = is_define_khr_context_flush_control(pname); if (ret) return ret;
	ret = is_define_khr_debug(pname); if (ret) return ret;
	ret = is_define_khr_no_error(pname); if (ret) return ret;
	ret = is_define_khr_robust_buffer_access_behavior(pname); if (ret) return ret;
	ret = is_define_khr_robustness(pname); if (ret) return ret;
	ret = is_define_khr_texture_compression_astc_hdr(pname); if (ret) return ret;
	ret = is_define_khr_texture_compression_astc_ldr(pname); if (ret) return ret;
	ret = is_define_khr_texture_compression_astc_sliced_3d(pname); if (ret) return ret;
	ret = is_define_oes_eis_define_image(pname); if (ret) return ret;
	ret = is_define_oes_eis_define_image_external(pname); if (ret) return ret;
	ret = is_define_oes_eis_define_image_external_essl3(pname); if (ret) return ret;
	ret = is_define_oes_compressed_etc1_rgb8_sub_texture(pname); if (ret) return ret;
	ret = is_define_oes_compressed_etc1_rgb8_texture(pname); if (ret) return ret;
	ret = is_define_oes_compressed_paletted_texture(pname); if (ret) return ret;
	ret = is_define_oes_copy_image(pname); if (ret) return ret;
	ret = is_define_oes_depth24(pname); if (ret) return ret;
	ret = is_define_oes_depth32(pname); if (ret) return ret;
	ret = is_define_oes_depth_texture(pname); if (ret) return ret;
	ret = is_define_oes_draw_buffers_indexed(pname); if (ret) return ret;
	ret = is_define_oes_draw_elements_base_vertex(pname); if (ret) return ret;
	ret = is_define_oes_element_index_uint(pname); if (ret) return ret;
	ret = is_define_oes_fbo_render_mipmap(pname); if (ret) return ret;
	ret = is_define_oes_fragment_precision_high(pname); if (ret) return ret;
	ret = is_define_oes_geometry_point_size(pname); if (ret) return ret;
	ret = is_define_oes_geometry_shader(pname); if (ret) return ret;
	ret = is_define_oes_get_program_binary(pname); if (ret) return ret;
	ret = is_define_oes_gpu_shader5(pname); if (ret) return ret;
	ret = is_define_oes_mapbuffer(pname); if (ret) return ret;
	ret = is_define_oes_packed_depth_stencil(pname); if (ret) return ret;
	ret = is_define_oes_primitive_bounding_box(pname); if (ret) return ret;
	ret = is_define_oes_required_internalformat(pname); if (ret) return ret;
	ret = is_define_oes_rgb8_rgba8(pname); if (ret) return ret;
	ret = is_define_oes_sample_shading(pname); if (ret) return ret;
	ret = is_define_oes_sample_variables(pname); if (ret) return ret;
	ret = is_define_oes_shader_image_atomic(pname); if (ret) return ret;
	ret = is_define_oes_shader_io_blocks(pname); if (ret) return ret;
	ret = is_define_oes_shader_multisample_interpolation(pname); if (ret) return ret;
	ret = is_define_oes_standard_derivatives(pname); if (ret) return ret;
	ret = is_define_oes_stencil1(pname); if (ret) return ret;
	ret = is_define_oes_stencil4(pname); if (ret) return ret;
	ret = is_define_oes_surfaceless_context(pname); if (ret) return ret;
	ret = is_define_oes_tessellation_point_size(pname); if (ret) return ret;
	ret = is_define_oes_tessellation_shader(pname); if (ret) return ret;
	ret = is_define_oes_texture_3d(pname); if (ret) return ret;
	ret = is_define_oes_texture_border_clamp(pname); if (ret) return ret;
	ret = is_define_oes_texture_buffer(pname); if (ret) return ret;
	ret = is_define_oes_texture_compression_astc(pname); if (ret) return ret;
	ret = is_define_oes_texture_cube_map_array(pname); if (ret) return ret;
	ret = is_define_oes_texture_float(pname); if (ret) return ret;
	ret = is_define_oes_texture_float_linear(pname); if (ret) return ret;
	ret = is_define_oes_texture_half_float(pname); if (ret) return ret;
	ret = is_define_oes_texture_half_float_linear(pname); if (ret) return ret;
	ret = is_define_oes_texture_npot(pname); if (ret) return ret;
	ret = is_define_oes_texture_stencil8(pname); if (ret) return ret;
	ret = is_define_oes_texture_storage_multisample_2d_array(pname); if (ret) return ret;
	ret = is_define_oes_texture_view(pname); if (ret) return ret;
	ret = is_define_oes_vertex_array_object(pname); if (ret) return ret;
	ret = is_define_oes_vertex_half_float(pname); if (ret) return ret;
	ret = is_define_oes_vertex_type_10_10_10_2(pname); if (ret) return ret;
	ret = is_define_amd_compressed_3dc_texture(pname); if (ret) return ret;
	ret = is_define_amd_compressed_atc_texture(pname); if (ret) return ret;
	ret = is_define_amd_performance_monitor(pname); if (ret) return ret;
	ret = is_define_amd_program_binary_z400(pname); if (ret) return ret;
	ret = is_define_android_extension_pack_es31a(pname); if (ret) return ret;
	ret = is_define_angle_depth_texture(pname); if (ret) return ret;
	ret = is_define_angle_framebuffer_blit(pname); if (ret) return ret;
	ret = is_define_angle_framebuffer_multisample(pname); if (ret) return ret;
	ret = is_define_angle_instanced_arrays(pname); if (ret) return ret;
	ret = is_define_angle_pack_reverse_row_order(pname); if (ret) return ret;
	ret = is_define_angle_program_binary(pname); if (ret) return ret;
	ret = is_define_angle_texture_compression_dxt3(pname); if (ret) return ret;
	ret = is_define_angle_texture_compression_dxt5(pname); if (ret) return ret;
	ret = is_define_angle_texture_usage(pname); if (ret) return ret;
	ret = is_define_angle_translated_shader_source(pname); if (ret) return ret;
	ret = is_define_apple_clip_distance(pname); if (ret) return ret;
	ret = is_define_apple_color_buffer_packed_float(pname); if (ret) return ret;
	ret = is_define_apple_copy_texture_levels(pname); if (ret) return ret;
	ret = is_define_apple_framebuffer_multisample(pname); if (ret) return ret;
	ret = is_define_apple_rgb_422(pname); if (ret) return ret;
	ret = is_define_apple_sync(pname); if (ret) return ret;
	ret = is_define_apple_texture_format_bgra8888(pname); if (ret) return ret;
	ret = is_define_apple_texture_max_level(pname); if (ret) return ret;
	ret = is_define_apple_texture_packed_float(pname); if (ret) return ret;
	ret = is_define_arm_mali_program_binary(pname); if (ret) return ret;
	ret = is_define_arm_mali_shader_binary(pname); if (ret) return ret;
	ret = is_define_arm_rgba8(pname); if (ret) return ret;
	ret = is_define_arm_shader_framebuffer_fetch(pname); if (ret) return ret;
	ret = is_define_arm_shader_framebuffer_fetch_depth_stencil(pname); if (ret) return ret;
	ret = is_define_dmp_program_binary(pname); if (ret) return ret;
	ret = is_define_dmp_shader_binary(pname); if (ret) return ret;
	ret = is_define_ext_yuv_target(pname); if (ret) return ret;
	ret = is_define_ext_base_instance(pname); if (ret) return ret;
	ret = is_define_ext_blend_func_extended(pname); if (ret) return ret;
	ret = is_define_ext_blend_minmax(pname); if (ret) return ret;
	ret = is_define_ext_buffer_storage(pname); if (ret) return ret;
	ret = is_define_ext_color_buffer_float(pname); if (ret) return ret;
	ret = is_define_ext_color_buffer_half_float(pname); if (ret) return ret;
	ret = is_define_ext_copy_image(pname); if (ret) return ret;
	ret = is_define_ext_debug_label(pname); if (ret) return ret;
	ret = is_define_ext_debug_marker(pname); if (ret) return ret;
	ret = is_define_ext_discard_framebuffer(pname); if (ret) return ret;
	ret = is_define_ext_disjoint_timer_query(pname); if (ret) return ret;
	ret = is_define_ext_draw_buffers(pname); if (ret) return ret;
	ret = is_define_ext_draw_buffers_indexed(pname); if (ret) return ret;
	ret = is_define_ext_draw_elements_base_vertex(pname); if (ret) return ret;
	ret = is_define_ext_draw_instanced(pname); if (ret) return ret;
	ret = is_define_ext_float_blend(pname); if (ret) return ret;
	ret = is_define_ext_geometry_point_size(pname); if (ret) return ret;
	ret = is_define_ext_geometry_shader(pname); if (ret) return ret;
	ret = is_define_ext_gpu_shader5(pname); if (ret) return ret;
	ret = is_define_ext_instanced_arrays(pname); if (ret) return ret;
	ret = is_define_ext_map_buffer_range(pname); if (ret) return ret;
	ret = is_define_ext_multi_draw_arrays(pname); if (ret) return ret;
	ret = is_define_ext_multi_draw_indirect(pname); if (ret) return ret;
	ret = is_define_ext_multisampled_compatibility(pname); if (ret) return ret;
	ret = is_define_ext_multisampled_render_to_texture(pname); if (ret) return ret;
	ret = is_define_ext_multiview_draw_buffers(pname); if (ret) return ret;
	ret = is_define_ext_occlusion_query_boolean(pname); if (ret) return ret;
	ret = is_define_ext_polygon_offset_clamp(pname); if (ret) return ret;
	ret = is_define_ext_post_depth_coverage(pname); if (ret) return ret;
	ret = is_define_ext_primitive_bounding_box(pname); if (ret) return ret;
	ret = is_define_ext_pvrtc_srgb(pname); if (ret) return ret;
	ret = is_define_ext_raster_multisample(pname); if (ret) return ret;
	ret = is_define_ext_read_format_bgra(pname); if (ret) return ret;
	ret = is_define_ext_render_snorm(pname); if (ret) return ret;
	ret = is_define_ext_robustness(pname); if (ret) return ret;
	ret = is_define_ext_srgb(pname); if (ret) return ret;
	ret = is_define_ext_srgb_write_control(pname); if (ret) return ret;
	ret = is_define_ext_separate_shader_objects(pname); if (ret) return ret;
	ret = is_define_ext_shader_framebuffer_fetch(pname); if (ret) return ret;
	ret = is_define_ext_shader_group_vote(pname); if (ret) return ret;
	ret = is_define_ext_shader_implicit_conversions(pname); if (ret) return ret;
	ret = is_define_ext_shader_integer_mix(pname); if (ret) return ret;
	ret = is_define_ext_shader_io_blocks(pname); if (ret) return ret;
	ret = is_define_ext_shader_pixel_local_storage(pname); if (ret) return ret;
	ret = is_define_ext_shader_pixel_local_storage2(pname); if (ret) return ret;
	ret = is_define_ext_shader_texture_lod(pname); if (ret) return ret;
	ret = is_define_ext_shadow_samplers(pname); if (ret) return ret;
	ret = is_define_ext_sparse_texture(pname); if (ret) return ret;
	ret = is_define_ext_tessellation_point_size(pname); if (ret) return ret;
	ret = is_define_ext_tessellation_shader(pname); if (ret) return ret;
	ret = is_define_ext_texture_border_clamp(pname); if (ret) return ret;
	ret = is_define_ext_texture_buffer(pname); if (ret) return ret;
	ret = is_define_ext_texture_compression_dxt1(pname); if (ret) return ret;
	ret = is_define_ext_texture_compression_s3tc(pname); if (ret) return ret;
	ret = is_define_ext_texture_cube_map_array(pname); if (ret) return ret;
	ret = is_define_ext_texture_filter_anisotropic(pname); if (ret) return ret;
	ret = is_define_ext_texture_filter_minmax(pname); if (ret) return ret;
	ret = is_define_ext_texture_format_bgra8888(pname); if (ret) return ret;
	ret = is_define_ext_texture_norm16(pname); if (ret) return ret;
	ret = is_define_ext_texture_rg(pname); if (ret) return ret;
	ret = is_define_ext_texture_srgb_r8(pname); if (ret) return ret;
	ret = is_define_ext_texture_srgb_rg8(pname); if (ret) return ret;
	ret = is_define_ext_texture_srgb_decode(pname); if (ret) return ret;
	ret = is_define_ext_texture_storage(pname); if (ret) return ret;
	ret = is_define_ext_texture_type_2_10_10_10_rev(pname); if (ret) return ret;
	ret = is_define_ext_texture_view(pname); if (ret) return ret;
	ret = is_define_ext_unpack_subimage(pname); if (ret) return ret;
	ret = is_define_fj_shader_binary_gccso(pname); if (ret) return ret;
	ret = is_define_img_framebuffer_downsample(pname); if (ret) return ret;
	ret = is_define_img_multisampled_render_to_texture(pname); if (ret) return ret;
	ret = is_define_img_program_binary(pname); if (ret) return ret;
	ret = is_define_img_read_format(pname); if (ret) return ret;
	ret = is_define_img_shader_binary(pname); if (ret) return ret;
	ret = is_define_img_texture_compression_pvrtc(pname); if (ret) return ret;
	ret = is_define_img_texture_compression_pvrtc2(pname); if (ret) return ret;
	ret = is_define_img_texture_filter_cubic(pname); if (ret) return ret;
	ret = is_define_intel_framebuffer_cmaa(pname); if (ret) return ret;
	ret = is_define_intel_performance_query(pname); if (ret) return ret;
	ret = is_define_nv_bindless_texture(pname); if (ret) return ret;
	ret = is_define_nv_blend_equation_advanced(pname); if (ret) return ret;
	ret = is_define_nv_blend_equation_advanced_coherent(pname); if (ret) return ret;
	ret = is_define_nv_conditional_render(pname); if (ret) return ret;
	ret = is_define_nv_conservative_raster(pname); if (ret) return ret;
	ret = is_define_nv_copy_buffer(pname); if (ret) return ret;
	ret = is_define_nv_coverage_sample(pname); if (ret) return ret;
	ret = is_define_nv_depth_nonlinear(pname); if (ret) return ret;
	ret = is_define_nv_draw_buffers(pname); if (ret) return ret;
	ret = is_define_nv_draw_instanced(pname); if (ret) return ret;
	ret = is_define_nv_explicit_attrib_location(pname); if (ret) return ret;
	ret = is_define_nv_fbo_color_attachments(pname); if (ret) return ret;
	ret = is_define_nv_fence(pname); if (ret) return ret;
	ret = is_define_nv_fill_rectangle(pname); if (ret) return ret;
	ret = is_define_nv_fragment_coverage_to_color(pname); if (ret) return ret;
	ret = is_define_nv_fragment_shader_interlock(pname); if (ret) return ret;
	ret = is_define_nv_framebuffer_blit(pname); if (ret) return ret;
	ret = is_define_nv_framebuffer_mixed_samples(pname); if (ret) return ret;
	ret = is_define_nv_framebuffer_multisample(pname); if (ret) return ret;
	ret = is_define_nv_generate_mipmap_srgb(pname); if (ret) return ret;
	ret = is_define_nv_geometry_shader_passthrough(pname); if (ret) return ret;
	ret = is_define_nv_image_formats(pname); if (ret) return ret;
	ret = is_define_nv_instanced_arrays(pname); if (ret) return ret;
	ret = is_define_nv_internalformat_sample_query(pname); if (ret) return ret;
	ret = is_define_nv_non_square_matrices(pname); if (ret) return ret;
	ret = is_define_nv_path_rendering(pname); if (ret) return ret;
	ret = is_define_nv_path_rendering_shared_edge(pname); if (ret) return ret;
	ret = is_define_nv_polygon_mode(pname); if (ret) return ret;
	ret = is_define_nv_read_buffer(pname); if (ret) return ret;
	ret = is_define_nv_read_buffer_front(pname); if (ret) return ret;
	ret = is_define_nv_read_depth(pname); if (ret) return ret;
	ret = is_define_nv_read_depth_stencil(pname); if (ret) return ret;
	ret = is_define_nv_read_stencil(pname); if (ret) return ret;
	ret = is_define_nv_srgb_formats(pname); if (ret) return ret;
	ret = is_define_nv_sample_locations(pname); if (ret) return ret;
	ret = is_define_nv_sample_mask_override_coverage(pname); if (ret) return ret;
	ret = is_define_nv_shader_noperspective_interpolation(pname); if (ret) return ret;
	ret = is_define_nv_shadow_samplers_array(pname); if (ret) return ret;
	ret = is_define_nv_shadow_samplers_cube(pname); if (ret) return ret;
	ret = is_define_nv_texture_border_clamp(pname); if (ret) return ret;
	ret = is_define_nv_texture_compression_s3tc_update(pname); if (ret) return ret;
	ret = is_define_nv_texture_npot_2d_mipmap(pname); if (ret) return ret;
	ret = is_define_nv_viewport_array(pname); if (ret) return ret;
	ret = is_define_nv_viewport_array2(pname); if (ret) return ret;
	ret = is_define_ovr_multiview(pname); if (ret) return ret;
	ret = is_define_ovr_multiview2(pname); if (ret) return ret;
	ret = is_define_ovr_multiview_multisampled_render_to_texture(pname); if (ret) return ret;
	ret = is_define_qcom_alpha_test(pname); if (ret) return ret;
	ret = is_define_qcom_binning_control(pname); if (ret) return ret;
	ret = is_define_qcom_driver_control(pname); if (ret) return ret;
	ret = is_define_qcom_extended_get(pname); if (ret) return ret;
	ret = is_define_qcom_extended_get2(pname); if (ret) return ret;
	ret = is_define_qcom_perfmon_global_mode(pname); if (ret) return ret;
	ret = is_define_qcom_tiled_rendering(pname); if (ret) return ret;
	ret = is_define_qcom_writeonly_rendering(pname); if (ret) return ret;
	ret = is_define_viv_shader_binary(pname);

	return ret;
}

/** extensions GL_KHR_blend_equation_advanced */
const char* gl::is_define_khr_blend_equation_advanced(GLenum pname)
{
	switch(pname)
	{
		case GL_MULTIPLY_KHR: return "GL_MULTIPLY_KHR";
		case GL_SCREEN_KHR: return "GL_SCREEN_KHR";
		case GL_OVERLAY_KHR: return "GL_OVERLAY_KHR";
		case GL_DARKEN_KHR: return "GL_DARKEN_KHR";
		case GL_LIGHTEN_KHR: return "GL_LIGHTEN_KHR";
		case GL_COLORDODGE_KHR: return "GL_COLORDODGE_KHR";
		case GL_COLORBURN_KHR: return "GL_COLORBURN_KHR";
		case GL_HARDLIGHT_KHR: return "GL_HARDLIGHT_KHR";
		case GL_SOFTLIGHT_KHR: return "GL_SOFTLIGHT_KHR";
		case GL_DIFFERENCE_KHR: return "GL_DIFFERENCE_KHR";
		case GL_EXCLUSION_KHR: return "GL_EXCLUSION_KHR";
		case GL_HSL_HUE_KHR: return "GL_HSL_HUE_KHR";
		case GL_HSL_SATURATION_KHR: return "GL_HSL_SATURATION_KHR";
		case GL_HSL_COLOR_KHR: return "GL_HSL_COLOR_KHR";
		case GL_HSL_LUMINOSITY_KHR: return "GL_HSL_LUMINOSITY_KHR";
	} // switch
	return nullptr;
}

/** extensions GL_KHR_blend_equation_advanced_coherent */
const char* gl::is_define_khr_blend_equation_advanced_coherent(GLenum pname)
{
	switch(pname)
	{
		case GL_BLEND_ADVANCED_COHERENT_KHR: return "GL_BLEND_ADVANCED_COHERENT_KHR";
	} // switch
	return nullptr;
}

/** extensions GL_KHR_context_flush_control */
const char* gl::is_define_khr_context_flush_control(GLenum pname)
{
	switch(pname)
	{
		case GL_CONTEXT_RELEASE_BEHAVIOR_KHR: return "GL_CONTEXT_RELEASE_BEHAVIOR_KHR";
		case GL_CONTEXT_RELEASE_BEHAVIOR_FLUSH_KHR: return "GL_CONTEXT_RELEASE_BEHAVIOR_FLUSH_KHR";
	} // switch
	return nullptr;
}

/** extensions GL_KHR_debug */
const char* gl::is_define_khr_debug(GLenum pname)
{
	switch(pname)
	{
		case GL_SAMPLER: return "GL_SAMPLER";
		case GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR: return "GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR";
		case GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH_KHR: return "GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH_KHR";
		case GL_DEBUG_CALLBACK_FUNCTION_KHR: return "GL_DEBUG_CALLBACK_FUNCTION_KHR";
		case GL_DEBUG_CALLBACK_USER_PARAM_KHR: return "GL_DEBUG_CALLBACK_USER_PARAM_KHR";
		case GL_DEBUG_SOURCE_API_KHR: return "GL_DEBUG_SOURCE_API_KHR";
		case GL_DEBUG_SOURCE_WINDOW_SYSTEM_KHR: return "GL_DEBUG_SOURCE_WINDOW_SYSTEM_KHR";
		case GL_DEBUG_SOURCE_SHADER_COMPILER_KHR: return "GL_DEBUG_SOURCE_SHADER_COMPILER_KHR";
		case GL_DEBUG_SOURCE_THIRD_PARTY_KHR: return "GL_DEBUG_SOURCE_THIRD_PARTY_KHR";
		case GL_DEBUG_SOURCE_APPLICATION_KHR: return "GL_DEBUG_SOURCE_APPLICATION_KHR";
		case GL_DEBUG_SOURCE_OTHER_KHR: return "GL_DEBUG_SOURCE_OTHER_KHR";
		case GL_DEBUG_TYPE_ERROR_KHR: return "GL_DEBUG_TYPE_ERROR_KHR";
		case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_KHR: return "GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_KHR";
		case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_KHR: return "GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_KHR";
		case GL_DEBUG_TYPE_PORTABILITY_KHR: return "GL_DEBUG_TYPE_PORTABILITY_KHR";
		case GL_DEBUG_TYPE_PERFORMANCE_KHR: return "GL_DEBUG_TYPE_PERFORMANCE_KHR";
		case GL_DEBUG_TYPE_OTHER_KHR: return "GL_DEBUG_TYPE_OTHER_KHR";
		case GL_DEBUG_TYPE_MARKER_KHR: return "GL_DEBUG_TYPE_MARKER_KHR";
		case GL_DEBUG_TYPE_PUSH_GROUP_KHR: return "GL_DEBUG_TYPE_PUSH_GROUP_KHR";
		case GL_DEBUG_TYPE_POP_GROUP_KHR: return "GL_DEBUG_TYPE_POP_GROUP_KHR";
		case GL_DEBUG_SEVERITY_NOTIFICATION_KHR: return "GL_DEBUG_SEVERITY_NOTIFICATION_KHR";
		case GL_MAX_DEBUG_GROUP_STACK_DEPTH_KHR: return "GL_MAX_DEBUG_GROUP_STACK_DEPTH_KHR";
		case GL_DEBUG_GROUP_STACK_DEPTH_KHR: return "GL_DEBUG_GROUP_STACK_DEPTH_KHR";
		case GL_BUFFER_KHR: return "GL_BUFFER_KHR";
		case GL_SHADER_KHR: return "GL_SHADER_KHR";
		case GL_PROGRAM_KHR: return "GL_PROGRAM_KHR";
		case GL_VERTEX_ARRAY_KHR: return "GL_VERTEX_ARRAY_KHR";
		case GL_QUERY_KHR: return "GL_QUERY_KHR";
		case GL_PROGRAM_PIPELINE_KHR: return "GL_PROGRAM_PIPELINE_KHR";
		//case GL_SAMPLER_KHR: return "GL_SAMPLER_KHR";
		case GL_MAX_LABEL_LENGTH_KHR: return "GL_MAX_LABEL_LENGTH_KHR";
		case GL_MAX_DEBUG_MESSAGE_LENGTH_KHR: return "GL_MAX_DEBUG_MESSAGE_LENGTH_KHR";
		case GL_MAX_DEBUG_LOGGED_MESSAGES_KHR: return "GL_MAX_DEBUG_LOGGED_MESSAGES_KHR";
		case GL_DEBUG_LOGGED_MESSAGES_KHR: return "GL_DEBUG_LOGGED_MESSAGES_KHR";
		case GL_DEBUG_SEVERITY_HIGH_KHR: return "GL_DEBUG_SEVERITY_HIGH_KHR";
		case GL_DEBUG_SEVERITY_MEDIUM_KHR: return "GL_DEBUG_SEVERITY_MEDIUM_KHR";
		case GL_DEBUG_SEVERITY_LOW_KHR: return "GL_DEBUG_SEVERITY_LOW_KHR";
		case GL_DEBUG_OUTPUT_KHR: return "GL_DEBUG_OUTPUT_KHR";
		case GL_CONTEXT_FLAG_DEBUG_BIT_KHR: return "GL_CONTEXT_FLAG_DEBUG_BIT_KHR";
		case GL_STACK_OVERFLOW_KHR: return "GL_STACK_OVERFLOW_KHR";
		case GL_STACK_UNDERFLOW_KHR: return "GL_STACK_UNDERFLOW_KHR";
	} // switch
	return nullptr;
}

/** extensions GL_KHR_no_error */
const char* gl::is_define_khr_no_error(GLenum pname)
{
	switch(pname)
	{
		case GL_CONTEXT_FLAG_NO_ERROR_BIT_KHR: return "GL_CONTEXT_FLAG_NO_ERROR_BIT_KHR";
	} // switch
	return nullptr;
}

/** extensions GL_KHR_robust_buffer_access_behavior */
const char* gl::is_define_khr_robust_buffer_access_behavior(GLenum pname)
{
	return nullptr;
}

/** extensions GL_KHR_robustness */
const char* gl::is_define_khr_robustness(GLenum pname)
{
	switch(pname)
	{
		case GL_CONTEXT_ROBUST_ACCESS_KHR: return "GL_CONTEXT_ROBUST_ACCESS_KHR";
		case GL_LOSE_CONTEXT_ON_RESET_KHR: return "GL_LOSE_CONTEXT_ON_RESET_KHR";
		case GL_GUILTY_CONTEXT_RESET_KHR: return "GL_GUILTY_CONTEXT_RESET_KHR";
		case GL_INNOCENT_CONTEXT_RESET_KHR: return "GL_INNOCENT_CONTEXT_RESET_KHR";
		case GL_UNKNOWN_CONTEXT_RESET_KHR: return "GL_UNKNOWN_CONTEXT_RESET_KHR";
		case GL_RESET_NOTIFICATION_STRATEGY_KHR: return "GL_RESET_NOTIFICATION_STRATEGY_KHR";
		case GL_NO_RESET_NOTIFICATION_KHR: return "GL_NO_RESET_NOTIFICATION_KHR";
		case GL_CONTEXT_LOST_KHR: return "GL_CONTEXT_LOST_KHR";
	} // switch
	return nullptr;
}

/** extensions GL_KHR_texture_compression_astc_hdr */
const char* gl::is_define_khr_texture_compression_astc_hdr(GLenum pname)
{
	switch(pname)
	{
		case GL_COMPRESSED_RGBA_ASTC_4x4_KHR: return "GL_COMPRESSED_RGBA_ASTC_4x4_KHR";
		case GL_COMPRESSED_RGBA_ASTC_5x4_KHR: return "GL_COMPRESSED_RGBA_ASTC_5x4_KHR";
		case GL_COMPRESSED_RGBA_ASTC_5x5_KHR: return "GL_COMPRESSED_RGBA_ASTC_5x5_KHR";
		case GL_COMPRESSED_RGBA_ASTC_6x5_KHR: return "GL_COMPRESSED_RGBA_ASTC_6x5_KHR";
		case GL_COMPRESSED_RGBA_ASTC_6x6_KHR: return "GL_COMPRESSED_RGBA_ASTC_6x6_KHR";
		case GL_COMPRESSED_RGBA_ASTC_8x5_KHR: return "GL_COMPRESSED_RGBA_ASTC_8x5_KHR";
		case GL_COMPRESSED_RGBA_ASTC_8x6_KHR: return "GL_COMPRESSED_RGBA_ASTC_8x6_KHR";
		case GL_COMPRESSED_RGBA_ASTC_8x8_KHR: return "GL_COMPRESSED_RGBA_ASTC_8x8_KHR";
		case GL_COMPRESSED_RGBA_ASTC_10x5_KHR: return "GL_COMPRESSED_RGBA_ASTC_10x5_KHR";
		case GL_COMPRESSED_RGBA_ASTC_10x6_KHR: return "GL_COMPRESSED_RGBA_ASTC_10x6_KHR";
		case GL_COMPRESSED_RGBA_ASTC_10x8_KHR: return "GL_COMPRESSED_RGBA_ASTC_10x8_KHR";
		case GL_COMPRESSED_RGBA_ASTC_10x10_KHR: return "GL_COMPRESSED_RGBA_ASTC_10x10_KHR";
		case GL_COMPRESSED_RGBA_ASTC_12x10_KHR: return "GL_COMPRESSED_RGBA_ASTC_12x10_KHR";
		case GL_COMPRESSED_RGBA_ASTC_12x12_KHR: return "GL_COMPRESSED_RGBA_ASTC_12x12_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR";
	} // switch
	return nullptr;
}

/** extensions GL_KHR_texture_compression_astc_ldr */
const char* gl::is_define_khr_texture_compression_astc_ldr(GLenum pname)
{

	return nullptr;
}

/** extensions GL_KHR_texture_compression_astc_sliced_3d */
const char* gl::is_define_khr_texture_compression_astc_sliced_3d(GLenum pname)
{

	return nullptr;
}

/** extensions GL_OES_EGL_image */
const char* gl::is_define_oes_eis_define_image(GLenum pname)
{

	return nullptr;
}

/** extensions GL_OES_EGL_image_external */
const char* gl::is_define_oes_eis_define_image_external(GLenum pname)
{
	switch(pname)
	{
		case GL_TEXTURE_EXTERNAL_OES: return "GL_TEXTURE_EXTERNAL_OES";
		case GL_TEXTURE_BINDING_EXTERNAL_OES: return "GL_TEXTURE_BINDING_EXTERNAL_OES";
		case GL_REQUIRED_TEXTURE_IMAGE_UNITS_OES: return "GL_REQUIRED_TEXTURE_IMAGE_UNITS_OES";
		case GL_SAMPLER_EXTERNAL_OES: return "GL_SAMPLER_EXTERNAL_OES";
	} // switch
	return nullptr;
}

/** extensions GL_OES_EGL_image_external_essl3 */
const char* gl::is_define_oes_eis_define_image_external_essl3(GLenum pname)
{

	return nullptr;
}

/** extensions GL_OES_compressed_ETC1_RGB8_sub_texture */
const char* gl::is_define_oes_compressed_etc1_rgb8_sub_texture(GLenum pname)
{

	return nullptr;
}

/** extensions GL_OES_compressed_ETC1_RGB8_texture */
const char* gl::is_define_oes_compressed_etc1_rgb8_texture(GLenum pname)
{
	switch(pname)
	{
		case GL_ETC1_RGB8_OES: return "GL_ETC1_RGB8_OES";
	} // switch
	return nullptr;
}

/** extensions GL_OES_compressed_paletted_texture */
const char* gl::is_define_oes_compressed_paletted_texture(GLenum pname)
{
	switch(pname)
	{
		case GL_PALETTE4_RGB8_OES: return "GL_PALETTE4_RGB8_OES";
		case GL_PALETTE4_RGBA8_OES: return "GL_PALETTE4_RGBA8_OES";
		case GL_PALETTE4_R5_G6_B5_OES: return "GL_PALETTE4_R5_G6_B5_OES";
		case GL_PALETTE4_RGBA4_OES: return "GL_PALETTE4_RGBA4_OES";
		case GL_PALETTE4_RGB5_A1_OES: return "GL_PALETTE4_RGB5_A1_OES";
		case GL_PALETTE8_RGB8_OES: return "GL_PALETTE8_RGB8_OES";
		case GL_PALETTE8_RGBA8_OES: return "GL_PALETTE8_RGBA8_OES";
		case GL_PALETTE8_R5_G6_B5_OES: return "GL_PALETTE8_R5_G6_B5_OES";
		case GL_PALETTE8_RGBA4_OES: return "GL_PALETTE8_RGBA4_OES";
		case GL_PALETTE8_RGB5_A1_OES: return "GL_PALETTE8_RGB5_A1_OES";
	} // switch
	return nullptr;
}

/** extensions GL_OES_copy_image */
const char* gl::is_define_oes_copy_image(GLenum pname)
{

	return nullptr;
}

/** extensions GL_OES_depth24 */
const char* gl::is_define_oes_depth24(GLenum pname)
{
	switch(pname)
	{
		case GL_DEPTH_COMPONENT24_OES: return "GL_DEPTH_COMPONENT24_OES";
	} // switch
	return nullptr;
}

/** extensions GL_OES_depth32 */
const char* gl::is_define_oes_depth32(GLenum pname)
{
	switch(pname)
	{
		case GL_DEPTH_COMPONENT32_OES: return "GL_DEPTH_COMPONENT32_OES";
	} // switch
	return nullptr;
}

/** extensions GL_OES_depth_texture */
const char* gl::is_define_oes_depth_texture(GLenum pname)
{

	return nullptr;
}

/** extensions GL_OES_draw_buffers_indexed */
const char* gl::is_define_oes_draw_buffers_indexed(GLenum pname)
{
	switch(pname)
	{
		case GL_MIN: return "GL_MIN";
		case GL_MAX: return "GL_MAX";
	} // switch
	return nullptr;
}

/** extensions GL_OES_draw_elements_base_vertex */
const char* gl::is_define_oes_draw_elements_base_vertex(GLenum pname)
{

	return nullptr;
}

/** extensions GL_OES_element_index_uint */
const char* gl::is_define_oes_element_index_uint(GLenum pname)
{

	return nullptr;
}

/** extensions GL_OES_fbo_render_mipmap */
const char* gl::is_define_oes_fbo_render_mipmap(GLenum pname)
{

	return nullptr;
}

/** extensions GL_OES_fragment_precision_high */
const char* gl::is_define_oes_fragment_precision_high(GLenum pname)
{

	return nullptr;
}

/** extensions GL_OES_geometry_point_size */
const char* gl::is_define_oes_geometry_point_size(GLenum pname)
{

	return nullptr;
}

/** extensions GL_OES_geometry_shader */
const char* gl::is_define_oes_geometry_shader(GLenum pname)
{
	switch(pname)
	{
		case GL_GEOMETRY_SHADER_OES: return "GL_GEOMETRY_SHADER_OES";
		case GL_GEOMETRY_SHADER_BIT_OES: return "GL_GEOMETRY_SHADER_BIT_OES";
		case GL_GEOMETRY_LINKED_VERTICES_OUT_OES: return "GL_GEOMETRY_LINKED_VERTICES_OUT_OES";
		case GL_GEOMETRY_LINKED_INPUT_TYPE_OES: return "GL_GEOMETRY_LINKED_INPUT_TYPE_OES";
		case GL_GEOMETRY_LINKED_OUTPUT_TYPE_OES: return "GL_GEOMETRY_LINKED_OUTPUT_TYPE_OES";
		case GL_GEOMETRY_SHADER_INVOCATIONS_OES: return "GL_GEOMETRY_SHADER_INVOCATIONS_OES";
		case GL_LAYER_PROVOKING_VERTEX_OES: return "GL_LAYER_PROVOKING_VERTEX_OES";
		case GL_LINES_ADJACENCY_OES: return "GL_LINES_ADJACENCY_OES";
		case GL_LINE_STRIP_ADJACENCY_OES: return "GL_LINE_STRIP_ADJACENCY_OES";
		case GL_TRIANGLES_ADJACENCY_OES: return "GL_TRIANGLES_ADJACENCY_OES";
		case GL_TRIANGLE_STRIP_ADJACENCY_OES: return "GL_TRIANGLE_STRIP_ADJACENCY_OES";
		case GL_MAX_GEOMETRY_UNIFORM_COMPONENTS_OES: return "GL_MAX_GEOMETRY_UNIFORM_COMPONENTS_OES";
		case GL_MAX_GEOMETRY_UNIFORM_BLOCKS_OES: return "GL_MAX_GEOMETRY_UNIFORM_BLOCKS_OES";
		case GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS_OES: return "GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS_OES";
		case GL_MAX_GEOMETRY_INPUT_COMPONENTS_OES: return "GL_MAX_GEOMETRY_INPUT_COMPONENTS_OES";
		case GL_MAX_GEOMETRY_OUTPUT_COMPONENTS_OES: return "GL_MAX_GEOMETRY_OUTPUT_COMPONENTS_OES";
		case GL_MAX_GEOMETRY_OUTPUT_VERTICES_OES: return "GL_MAX_GEOMETRY_OUTPUT_VERTICES_OES";
		case GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS_OES: return "GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS_OES";
		case GL_MAX_GEOMETRY_SHADER_INVOCATIONS_OES: return "GL_MAX_GEOMETRY_SHADER_INVOCATIONS_OES";
		case GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS_OES: return "GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS_OES";
		case GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS_OES: return "GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS_OES";
		case GL_MAX_GEOMETRY_ATOMIC_COUNTERS_OES: return "GL_MAX_GEOMETRY_ATOMIC_COUNTERS_OES";
		case GL_MAX_GEOMETRY_IMAGE_UNIFORMS_OES: return "GL_MAX_GEOMETRY_IMAGE_UNIFORMS_OES";
		case GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS_OES: return "GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS_OES";
		case GL_FIRST_VERTEX_CONVENTION_OES: return "GL_FIRST_VERTEX_CONVENTION_OES";
		case GL_LAST_VERTEX_CONVENTION_OES: return "GL_LAST_VERTEX_CONVENTION_OES";
		case GL_UNDEFINED_VERTEX_OES: return "GL_UNDEFINED_VERTEX_OES";
		case GL_PRIMITIVES_GENERATED_OES: return "GL_PRIMITIVES_GENERATED_OES";
		case GL_FRAMEBUFFER_DEFAULT_LAYERS_OES: return "GL_FRAMEBUFFER_DEFAULT_LAYERS_OES";
		case GL_MAX_FRAMEBUFFER_LAYERS_OES: return "GL_MAX_FRAMEBUFFER_LAYERS_OES";
		case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS_OES: return "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS_OES";
		case GL_FRAMEBUFFER_ATTACHMENT_LAYERED_OES: return "GL_FRAMEBUFFER_ATTACHMENT_LAYERED_OES";
		case GL_REFERENCED_BY_GEOMETRY_SHADER_OES: return "GL_REFERENCED_BY_GEOMETRY_SHADER_OES";
	} // switch
	return nullptr;
}

/** extensions GL_OES_get_program_binary */
const char* gl::is_define_oes_get_program_binary(GLenum pname)
{
	switch(pname)
	{
		case GL_PROGRAM_BINARY_LENGTH_OES: return "GL_PROGRAM_BINARY_LENGTH_OES";
		case GL_NUM_PROGRAM_BINARY_FORMATS_OES: return "GL_NUM_PROGRAM_BINARY_FORMATS_OES";
		case GL_PROGRAM_BINARY_FORMATS_OES: return "GL_PROGRAM_BINARY_FORMATS_OES";
	} // switch
	return nullptr;
}

/** extensions GL_OES_gpu_shader5 */
const char* gl::is_define_oes_gpu_shader5(GLenum pname)
{

	return nullptr;
}

/** extensions GL_OES_mapbuffer */
const char* gl::is_define_oes_mapbuffer(GLenum pname)
{
	switch(pname)
	{
		case GL_WRITE_ONLY_OES: return "GL_WRITE_ONLY_OES";
		case GL_BUFFER_ACCESS_OES: return "GL_BUFFER_ACCESS_OES";
		case GL_BUFFER_MAPPED_OES: return "GL_BUFFER_MAPPED_OES";
		case GL_BUFFER_MAP_POINTER_OES: return "GL_BUFFER_MAP_POINTER_OES";
	} // switch
	return nullptr;
}

/** extensions GL_OES_packed_depth_stencil */
const char* gl::is_define_oes_packed_depth_stencil(GLenum pname)
{
	switch(pname)
	{
		case GL_DEPTH_STENCIL_OES: return "GL_DEPTH_STENCIL_OES";
		case GL_UNSIGNED_INT_24_8_OES: return "GL_UNSIGNED_INT_24_8_OES";
		case GL_DEPTH24_STENCIL8_OES: return "GL_DEPTH24_STENCIL8_OES";
	} // switch
	return nullptr;
}

/** extensions GL_OES_primitive_bounding_box */
const char* gl::is_define_oes_primitive_bounding_box(GLenum pname)
{
	switch(pname)
	{
		case GL_PRIMITIVE_BOUNDING_BOX_OES: return "GL_PRIMITIVE_BOUNDING_BOX_OES";
	} // switch
	return nullptr;
}

/** extensions GL_OES_required_internalformat */
const char* gl::is_define_oes_required_internalformat(GLenum pname)
{
	switch(pname)
	{
		case GL_ALPHA8_OES: return "GL_ALPHA8_OES";
		case GL_DEPTH_COMPONENT16_OES: return "GL_DEPTH_COMPONENT16_OES";
		case GL_LUMINANCE4_ALPHA4_OES: return "GL_LUMINANCE4_ALPHA4_OES";
		case GL_LUMINANCE8_ALPHA8_OES: return "GL_LUMINANCE8_ALPHA8_OES";
		case GL_LUMINANCE8_OES: return "GL_LUMINANCE8_OES";
		case GL_RGBA4_OES: return "GL_RGBA4_OES";
		case GL_RGB5_A1_OES: return "GL_RGB5_A1_OES";
		case GL_RGB565_OES: return "GL_RGB565_OES";
		case GL_RGB8_OES: return "GL_RGB8_OES";
		case GL_RGBA8_OES: return "GL_RGBA8_OES";
		case GL_RGB10_EXT: return "GL_RGB10_EXT";
		case GL_RGB10_A2_EXT: return "GL_RGB10_A2_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_OES_rgb8_rgba8 */
const char* gl::is_define_oes_rgb8_rgba8(GLenum pname)
{

	return nullptr;
}

/** extensions GL_OES_sample_shading */
const char* gl::is_define_oes_sample_shading(GLenum pname)
{
	switch(pname)
	{
		case GL_SAMPLE_SHADING_OES: return "GL_SAMPLE_SHADING_OES";
		case GL_MIN_SAMPLE_SHADING_VALUE_OES: return "GL_MIN_SAMPLE_SHADING_VALUE_OES";
	} // switch
	return nullptr;
}

/** extensions GL_OES_sample_variables */
const char* gl::is_define_oes_sample_variables(GLenum pname)
{

	return nullptr;
}

/** extensions GL_OES_shader_image_atomic */
const char* gl::is_define_oes_shader_image_atomic(GLenum pname)
{

	return nullptr;
}

/** extensions GL_OES_shader_io_blocks */
const char* gl::is_define_oes_shader_io_blocks(GLenum pname)
{

	return nullptr;
}

/** extensions GL_OES_shader_multisample_interpolation */
const char* gl::is_define_oes_shader_multisample_interpolation(GLenum pname)
{
	switch(pname)
	{
		case GL_MIN_FRAGMENT_INTERPOLATION_OFFSET_OES: return "GL_MIN_FRAGMENT_INTERPOLATION_OFFSET_OES";
		case GL_MAX_FRAGMENT_INTERPOLATION_OFFSET_OES: return "GL_MAX_FRAGMENT_INTERPOLATION_OFFSET_OES";
		case GL_FRAGMENT_INTERPOLATION_OFFSET_BITS_OES: return "GL_FRAGMENT_INTERPOLATION_OFFSET_BITS_OES";
	} // switch
	return nullptr;
}

/** extensions GL_OES_standard_derivatives */
const char* gl::is_define_oes_standard_derivatives(GLenum pname)
{
	switch(pname)
	{
		case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES: return "GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES";
	} // switch
	return nullptr;
}

/** extensions GL_OES_stencil1 */
const char* gl::is_define_oes_stencil1(GLenum pname)
{
	switch(pname)
	{
		case GL_STENCIL_INDEX1_OES: return "GL_STENCIL_INDEX1_OES";
	} // switch
	return nullptr;
}

/** extensions GL_OES_stencil4 */
const char* gl::is_define_oes_stencil4(GLenum pname)
{
	switch(pname)
	{
		case GL_STENCIL_INDEX4_OES: return "GL_STENCIL_INDEX4_OES";
	} // switch
	return nullptr;
}

/** extensions GL_OES_surfaceless_context */
const char* gl::is_define_oes_surfaceless_context(GLenum pname)
{
	switch(pname)
	{
		case GL_FRAMEBUFFER_UNDEFINED_OES: return "GL_FRAMEBUFFER_UNDEFINED_OES";
	} // switch
	return nullptr;
}

/** extensions GL_OES_tessellation_point_size */
const char* gl::is_define_oes_tessellation_point_size(GLenum pname)
{

	return nullptr;
}

/** extensions GL_OES_tessellation_shader */
const char* gl::is_define_oes_tessellation_shader(GLenum pname)
{
	switch(pname)
	{
		case GL_PATCHES_OES: return "GL_PATCHES_OES";
		case GL_PATCH_VERTICES_OES: return "GL_PATCH_VERTICES_OES";
		case GL_TESS_CONTROL_OUTPUT_VERTICES_OES: return "GL_TESS_CONTROL_OUTPUT_VERTICES_OES";
		case GL_TESS_GEN_MODE_OES: return "GL_TESS_GEN_MODE_OES";
		case GL_TESS_GEN_SPACING_OES: return "GL_TESS_GEN_SPACING_OES";
		case GL_TESS_GEN_VERTEX_ORDER_OES: return "GL_TESS_GEN_VERTEX_ORDER_OES";
		case GL_TESS_GEN_POINT_MODE_OES: return "GL_TESS_GEN_POINT_MODE_OES";
		case GL_ISOLINES_OES: return "GL_ISOLINES_OES";
		case GL_QUADS_OES: return "GL_QUADS_OES";
		case GL_FRACTIONAL_ODD_OES: return "GL_FRACTIONAL_ODD_OES";
		case GL_FRACTIONAL_EVEN_OES: return "GL_FRACTIONAL_EVEN_OES";
		case GL_MAX_PATCH_VERTICES_OES: return "GL_MAX_PATCH_VERTICES_OES";
		case GL_MAX_TESS_GEN_LEVEL_OES: return "GL_MAX_TESS_GEN_LEVEL_OES";
		case GL_MAX_TESS_CONTROL_UNIFORM_COMPONENTS_OES: return "GL_MAX_TESS_CONTROL_UNIFORM_COMPONENTS_OES";
		case GL_MAX_TESS_EVALUATION_UNIFORM_COMPONENTS_OES: return "GL_MAX_TESS_EVALUATION_UNIFORM_COMPONENTS_OES";
		case GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS_OES: return "GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS_OES";
		case GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS_OES: return "GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS_OES";
		case GL_MAX_TESS_CONTROL_OUTPUT_COMPONENTS_OES: return "GL_MAX_TESS_CONTROL_OUTPUT_COMPONENTS_OES";
		case GL_MAX_TESS_PATCH_COMPONENTS_OES: return "GL_MAX_TESS_PATCH_COMPONENTS_OES";
		case GL_MAX_TESS_CONTROL_TOTAL_OUTPUT_COMPONENTS_OES: return "GL_MAX_TESS_CONTROL_TOTAL_OUTPUT_COMPONENTS_OES";
		case GL_MAX_TESS_EVALUATION_OUTPUT_COMPONENTS_OES: return "GL_MAX_TESS_EVALUATION_OUTPUT_COMPONENTS_OES";
		case GL_MAX_TESS_CONTROL_UNIFORM_BLOCKS_OES: return "GL_MAX_TESS_CONTROL_UNIFORM_BLOCKS_OES";
		case GL_MAX_TESS_EVALUATION_UNIFORM_BLOCKS_OES: return "GL_MAX_TESS_EVALUATION_UNIFORM_BLOCKS_OES";
		case GL_MAX_TESS_CONTROL_INPUT_COMPONENTS_OES: return "GL_MAX_TESS_CONTROL_INPUT_COMPONENTS_OES";
		case GL_MAX_TESS_EVALUATION_INPUT_COMPONENTS_OES: return "GL_MAX_TESS_EVALUATION_INPUT_COMPONENTS_OES";
		case GL_MAX_COMBINED_TESS_CONTROL_UNIFORM_COMPONENTS_OES: return "GL_MAX_COMBINED_TESS_CONTROL_UNIFORM_COMPONENTS_OES";
		case GL_MAX_COMBINED_TESS_EVALUATION_UNIFORM_COMPONENTS_OES: return "GL_MAX_COMBINED_TESS_EVALUATION_UNIFORM_COMPONENTS_OES";
		case GL_MAX_TESS_CONTROL_ATOMIC_COUNTER_BUFFERS_OES: return "GL_MAX_TESS_CONTROL_ATOMIC_COUNTER_BUFFERS_OES";
		case GL_MAX_TESS_EVALUATION_ATOMIC_COUNTER_BUFFERS_OES: return "GL_MAX_TESS_EVALUATION_ATOMIC_COUNTER_BUFFERS_OES";
		case GL_MAX_TESS_CONTROL_ATOMIC_COUNTERS_OES: return "GL_MAX_TESS_CONTROL_ATOMIC_COUNTERS_OES";
		case GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS_OES: return "GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS_OES";
		case GL_MAX_TESS_CONTROL_IMAGE_UNIFORMS_OES: return "GL_MAX_TESS_CONTROL_IMAGE_UNIFORMS_OES";
		case GL_MAX_TESS_EVALUATION_IMAGE_UNIFORMS_OES: return "GL_MAX_TESS_EVALUATION_IMAGE_UNIFORMS_OES";
		case GL_MAX_TESS_CONTROL_SHADER_STORAGE_BLOCKS_OES: return "GL_MAX_TESS_CONTROL_SHADER_STORAGE_BLOCKS_OES";
		case GL_MAX_TESS_EVALUATION_SHADER_STORAGE_BLOCKS_OES: return "GL_MAX_TESS_EVALUATION_SHADER_STORAGE_BLOCKS_OES";
		case GL_PRIMITIVE_RESTART_FOR_PATCHES_SUPPORTED_OES: return "GL_PRIMITIVE_RESTART_FOR_PATCHES_SUPPORTED_OES";
		case GL_IS_PER_PATCH_OES: return "GL_IS_PER_PATCH_OES";
		case GL_REFERENCED_BY_TESS_CONTROL_SHADER_OES: return "GL_REFERENCED_BY_TESS_CONTROL_SHADER_OES";
		case GL_REFERENCED_BY_TESS_EVALUATION_SHADER_OES: return "GL_REFERENCED_BY_TESS_EVALUATION_SHADER_OES";
		case GL_TESS_CONTROL_SHADER_OES: return "GL_TESS_CONTROL_SHADER_OES";
		case GL_TESS_EVALUATION_SHADER_OES: return "GL_TESS_EVALUATION_SHADER_OES";
		case GL_TESS_CONTROL_SHADER_BIT_OES: return "GL_TESS_CONTROL_SHADER_BIT_OES";
		case GL_TESS_EVALUATION_SHADER_BIT_OES: return "GL_TESS_EVALUATION_SHADER_BIT_OES";
	} // switch
	return nullptr;
}

/** extensions GL_OES_texture_3D */
const char* gl::is_define_oes_texture_3d(GLenum pname)
{
	switch(pname)
	{
		case GL_TEXTURE_WRAP_R_OES: return "GL_TEXTURE_WRAP_R_OES";
		case GL_TEXTURE_3D_OES: return "GL_TEXTURE_3D_OES";
		case GL_TEXTURE_BINDING_3D_OES: return "GL_TEXTURE_BINDING_3D_OES";
		case GL_MAX_3D_TEXTURE_SIZE_OES: return "GL_MAX_3D_TEXTURE_SIZE_OES";
		case GL_SAMPLER_3D_OES: return "GL_SAMPLER_3D_OES";
		case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_3D_ZOFFSET_OES: return "GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_3D_ZOFFSET_OES";
	} // switch
	return nullptr;
}

/** extensions GL_OES_texture_border_clamp */
const char* gl::is_define_oes_texture_border_clamp(GLenum pname)
{
	switch(pname)
	{
		case GL_TEXTURE_BORDER_COLOR_OES: return "GL_TEXTURE_BORDER_COLOR_OES";
		case GL_CLAMP_TO_BORDER_OES: return "GL_CLAMP_TO_BORDER_OES";
	} // switch
	return nullptr;
}

/** extensions GL_OES_texture_buffer */
const char* gl::is_define_oes_texture_buffer(GLenum pname)
{
	switch(pname)
	{
		case GL_TEXTURE_BUFFER_OES: return "GL_TEXTURE_BUFFER_OES";
		//case GL_TEXTURE_BUFFER_BINDING_OES: return "GL_TEXTURE_BUFFER_BINDING_OES";
		case GL_MAX_TEXTURE_BUFFER_SIZE_OES: return "GL_MAX_TEXTURE_BUFFER_SIZE_OES";
		case GL_TEXTURE_BINDING_BUFFER_OES: return "GL_TEXTURE_BINDING_BUFFER_OES";
		case GL_TEXTURE_BUFFER_DATA_STORE_BINDING_OES: return "GL_TEXTURE_BUFFER_DATA_STORE_BINDING_OES";
		case GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT_OES: return "GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT_OES";
		case GL_SAMPLER_BUFFER_OES: return "GL_SAMPLER_BUFFER_OES";
		case GL_INT_SAMPLER_BUFFER_OES: return "GL_INT_SAMPLER_BUFFER_OES";
		case GL_UNSIGNED_INT_SAMPLER_BUFFER_OES: return "GL_UNSIGNED_INT_SAMPLER_BUFFER_OES";
		case GL_IMAGE_BUFFER_OES: return "GL_IMAGE_BUFFER_OES";
		case GL_INT_IMAGE_BUFFER_OES: return "GL_INT_IMAGE_BUFFER_OES";
		case GL_UNSIGNED_INT_IMAGE_BUFFER_OES: return "GL_UNSIGNED_INT_IMAGE_BUFFER_OES";
		case GL_TEXTURE_BUFFER_OFFSET_OES: return "GL_TEXTURE_BUFFER_OFFSET_OES";
		case GL_TEXTURE_BUFFER_SIZE_OES: return "GL_TEXTURE_BUFFER_SIZE_OES";
	} // switch
	return nullptr;
}

/** extensions GL_OES_texture_compression_astc */
const char* gl::is_define_oes_texture_compression_astc(GLenum pname)
{
	switch(pname)
	{
		case GL_COMPRESSED_RGBA_ASTC_3x3x3_OES: return "GL_COMPRESSED_RGBA_ASTC_3x3x3_OES";
		case GL_COMPRESSED_RGBA_ASTC_4x3x3_OES: return "GL_COMPRESSED_RGBA_ASTC_4x3x3_OES";
		case GL_COMPRESSED_RGBA_ASTC_4x4x3_OES: return "GL_COMPRESSED_RGBA_ASTC_4x4x3_OES";
		case GL_COMPRESSED_RGBA_ASTC_4x4x4_OES: return "GL_COMPRESSED_RGBA_ASTC_4x4x4_OES";
		case GL_COMPRESSED_RGBA_ASTC_5x4x4_OES: return "GL_COMPRESSED_RGBA_ASTC_5x4x4_OES";
		case GL_COMPRESSED_RGBA_ASTC_5x5x4_OES: return "GL_COMPRESSED_RGBA_ASTC_5x5x4_OES";
		case GL_COMPRESSED_RGBA_ASTC_5x5x5_OES: return "GL_COMPRESSED_RGBA_ASTC_5x5x5_OES";
		case GL_COMPRESSED_RGBA_ASTC_6x5x5_OES: return "GL_COMPRESSED_RGBA_ASTC_6x5x5_OES";
		case GL_COMPRESSED_RGBA_ASTC_6x6x5_OES: return "GL_COMPRESSED_RGBA_ASTC_6x6x5_OES";
		case GL_COMPRESSED_RGBA_ASTC_6x6x6_OES: return "GL_COMPRESSED_RGBA_ASTC_6x6x6_OES";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_3x3x3_OES: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_3x3x3_OES";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x3x3_OES: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x3x3_OES";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x3_OES: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x3_OES";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x4_OES: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x4_OES";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4x4_OES: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4x4_OES";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x4_OES: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x4_OES";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x5_OES: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x5_OES";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5x5_OES: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5x5_OES";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x5_OES: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x5_OES";
		case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x6_OES: return "GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x6_OES";
	} // switch
	return nullptr;
}

/** extensions GL_OES_texture_cube_map_array */
const char* gl::is_define_oes_texture_cube_map_array(GLenum pname)
{
	switch(pname)
	{
		case GL_TEXTURE_CUBE_MAP_ARRAY_OES: return "GL_TEXTURE_CUBE_MAP_ARRAY_OES";
		case GL_TEXTURE_BINDING_CUBE_MAP_ARRAY_OES: return "GL_TEXTURE_BINDING_CUBE_MAP_ARRAY_OES";
		case GL_SAMPLER_CUBE_MAP_ARRAY_OES: return "GL_SAMPLER_CUBE_MAP_ARRAY_OES";
		case GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW_OES: return "GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW_OES";
		case GL_INT_SAMPLER_CUBE_MAP_ARRAY_OES: return "GL_INT_SAMPLER_CUBE_MAP_ARRAY_OES";
		case GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY_OES: return "GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY_OES";
		case GL_IMAGE_CUBE_MAP_ARRAY_OES: return "GL_IMAGE_CUBE_MAP_ARRAY_OES";
		case GL_INT_IMAGE_CUBE_MAP_ARRAY_OES: return "GL_INT_IMAGE_CUBE_MAP_ARRAY_OES";
		case GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY_OES: return "GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY_OES";
	} // switch
	return nullptr;
}

/** extensions GL_OES_texture_float */
const char* gl::is_define_oes_texture_float(GLenum pname)
{

	return nullptr;
}

/** extensions GL_OES_texture_float_linear */
const char* gl::is_define_oes_texture_float_linear(GLenum pname)
{

	return nullptr;
}

/** extensions GL_OES_texture_half_float */
const char* gl::is_define_oes_texture_half_float(GLenum pname)
{
	switch(pname)
	{
		case GL_HALF_FLOAT_OES: return "GL_HALF_FLOAT_OES";
	} // switch
	return nullptr;
}

/** extensions GL_OES_texture_half_float_linear */
const char* gl::is_define_oes_texture_half_float_linear(GLenum pname)
{

	return nullptr;
}

/** extensions GL_OES_texture_npot */
const char* gl::is_define_oes_texture_npot(GLenum pname)
{

	return nullptr;
}

/** extensions GL_OES_texture_stencil8 */
const char* gl::is_define_oes_texture_stencil8(GLenum pname)
{
	switch(pname)
	{
		case GL_STENCIL_INDEX_OES: return "GL_STENCIL_INDEX_OES";
		case GL_STENCIL_INDEX8_OES: return "GL_STENCIL_INDEX8_OES";
	} // switch
	return nullptr;
}

/** extensions GL_OES_texture_storage_multisample_2d_array */
const char* gl::is_define_oes_texture_storage_multisample_2d_array(GLenum pname)
{
	switch(pname)
	{
		case GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES: return "GL_TEXTURE_2D_MULTISAMPLE_ARRAY_OES";
		case GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY_OES: return "GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY_OES";
		case GL_SAMPLER_2D_MULTISAMPLE_ARRAY_OES: return "GL_SAMPLER_2D_MULTISAMPLE_ARRAY_OES";
		case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY_OES: return "GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY_OES";
		case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY_OES: return "GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY_OES";
	} // switch
	return nullptr;
}

/** extensions GL_OES_texture_view */
const char* gl::is_define_oes_texture_view(GLenum pname)
{
	switch(pname)
	{
		case GL_TEXTURE_VIEW_MIN_LEVEL_OES: return "GL_TEXTURE_VIEW_MIN_LEVEL_OES";
		case GL_TEXTURE_VIEW_NUM_LEVELS_OES: return "GL_TEXTURE_VIEW_NUM_LEVELS_OES";
		case GL_TEXTURE_VIEW_MIN_LAYER_OES: return "GL_TEXTURE_VIEW_MIN_LAYER_OES";
		case GL_TEXTURE_VIEW_NUM_LAYERS_OES: return "GL_TEXTURE_VIEW_NUM_LAYERS_OES";
		case GL_TEXTURE_IMMUTABLE_LEVELS: return "GL_TEXTURE_IMMUTABLE_LEVELS";
	} // switch
	return nullptr;
}

/** extensions GL_OES_vertex_array_object */
const char* gl::is_define_oes_vertex_array_object(GLenum pname)
{
	switch(pname)
	{
		case GL_VERTEX_ARRAY_BINDING_OES: return "GL_VERTEX_ARRAY_BINDING_OES";
	} // switch
	return nullptr;
}

/** extensions GL_OES_vertex_half_float */
const char* gl::is_define_oes_vertex_half_float(GLenum pname)
{

	return nullptr;
}

/** extensions GL_OES_vertex_type_10_10_10_2 */
const char* gl::is_define_oes_vertex_type_10_10_10_2(GLenum pname)
{
	switch(pname)
	{
		case GL_UNSIGNED_INT_10_10_10_2_OES: return "GL_UNSIGNED_INT_10_10_10_2_OES";
		case GL_INT_10_10_10_2_OES: return "GL_INT_10_10_10_2_OES";
	} // switch
	return nullptr;
}

/** extensions GL_AMD_compressed_3DC_texture */
const char* gl::is_define_amd_compressed_3dc_texture(GLenum pname)
{
	switch(pname)
	{
		case GL_3DC_X_AMD: return "GL_3DC_X_AMD";
		case GL_3DC_XY_AMD: return "GL_3DC_XY_AMD";
	} // switch
	return nullptr;
}

/** extensions GL_AMD_compressed_ATC_texture */
const char* gl::is_define_amd_compressed_atc_texture(GLenum pname)
{
	switch(pname)
	{
		case GL_ATC_RGB_AMD: return "GL_ATC_RGB_AMD";
		case GL_ATC_RGBA_EXPLICIT_ALPHA_AMD: return "GL_ATC_RGBA_EXPLICIT_ALPHA_AMD";
		case GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD: return "GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD";
	} // switch
	return nullptr;
}

/** extensions GL_AMD_performance_monitor */
const char* gl::is_define_amd_performance_monitor(GLenum pname)
{
	switch(pname)
	{
		case GL_COUNTER_TYPE_AMD: return "GL_COUNTER_TYPE_AMD";
		case GL_COUNTER_RANGE_AMD: return "GL_COUNTER_RANGE_AMD";
		case GL_UNSIGNED_INT64_AMD: return "GL_UNSIGNED_INT64_AMD";
		case GL_PERCENTAGE_AMD: return "GL_PERCENTAGE_AMD";
		case GL_PERFMON_RESULT_AVAILABLE_AMD: return "GL_PERFMON_RESULT_AVAILABLE_AMD";
		case GL_PERFMON_RESULT_SIZE_AMD: return "GL_PERFMON_RESULT_SIZE_AMD";
		case GL_PERFMON_RESULT_AMD: return "GL_PERFMON_RESULT_AMD";
	} // switch
	return nullptr;
}

/** extensions GL_AMD_program_binary_Z400 */
const char* gl::is_define_amd_program_binary_z400(GLenum pname)
{
	switch(pname)
	{
		case GL_Z400_BINARY_AMD: return "GL_Z400_BINARY_AMD";
	} // switch
	return nullptr;
}

/** extensions GL_ANDROID_extension_pack_es31a */
const char* gl::is_define_android_extension_pack_es31a(GLenum pname)
{

	return nullptr;
}

/** extensions GL_ANGLE_depth_texture */
const char* gl::is_define_angle_depth_texture(GLenum pname)
{

	return nullptr;
}

/** extensions GL_ANGLE_framebuffer_blit */
const char* gl::is_define_angle_framebuffer_blit(GLenum pname)
{
	switch(pname)
	{
		case GL_READ_FRAMEBUFFER_ANGLE: return "GL_READ_FRAMEBUFFER_ANGLE";
		case GL_DRAW_FRAMEBUFFER_ANGLE: return "GL_DRAW_FRAMEBUFFER_ANGLE";
		case GL_DRAW_FRAMEBUFFER_BINDING_ANGLE: return "GL_DRAW_FRAMEBUFFER_BINDING_ANGLE";
		case GL_READ_FRAMEBUFFER_BINDING_ANGLE: return "GL_READ_FRAMEBUFFER_BINDING_ANGLE";
	} // switch
	return nullptr;
}

/** extensions GL_ANGLE_framebuffer_multisample */
const char* gl::is_define_angle_framebuffer_multisample(GLenum pname)
{
	switch(pname)
	{
		case GL_RENDERBUFFER_SAMPLES_ANGLE: return "GL_RENDERBUFFER_SAMPLES_ANGLE";
		case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_ANGLE: return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_ANGLE";
		case GL_MAX_SAMPLES_ANGLE: return "GL_MAX_SAMPLES_ANGLE";
	} // switch
	return nullptr;
}

/** extensions GL_ANGLE_instanced_arrays */
const char* gl::is_define_angle_instanced_arrays(GLenum pname)
{
	switch(pname)
	{
		case GL_VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE: return "GL_VERTEX_ATTRIB_ARRAY_DIVISOR_ANGLE";
	} // switch
	return nullptr;
}

/** extensions GL_ANGLE_pack_reverse_row_order */
const char* gl::is_define_angle_pack_reverse_row_order(GLenum pname)
{
	switch(pname)
	{
		case GL_PACK_REVERSE_ROW_ORDER_ANGLE: return "GL_PACK_REVERSE_ROW_ORDER_ANGLE";
	} // switch
	return nullptr;
}

/** extensions GL_ANGLE_program_binary */
const char* gl::is_define_angle_program_binary(GLenum pname)
{
	switch(pname)
	{
		case GL_PROGRAM_BINARY_ANGLE: return "GL_PROGRAM_BINARY_ANGLE";
	} // switch
	return nullptr;
}

/** extensions GL_ANGLE_texture_compression_dxt3 */
const char* gl::is_define_angle_texture_compression_dxt3(GLenum pname)
{
	switch(pname)
	{
		case GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE: return "GL_COMPRESSED_RGBA_S3TC_DXT3_ANGLE";
	} // switch
	return nullptr;
}

/** extensions GL_ANGLE_texture_compression_dxt5 */
const char* gl::is_define_angle_texture_compression_dxt5(GLenum pname)
{
	switch(pname)
	{
		case GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE: return "GL_COMPRESSED_RGBA_S3TC_DXT5_ANGLE";
	} // switch
	return nullptr;
}

/** extensions GL_ANGLE_texture_usage */
const char* gl::is_define_angle_texture_usage(GLenum pname)
{
	switch(pname)
	{
		case GL_TEXTURE_USAGE_ANGLE: return "GL_TEXTURE_USAGE_ANGLE";
		case GL_FRAMEBUFFER_ATTACHMENT_ANGLE: return "GL_FRAMEBUFFER_ATTACHMENT_ANGLE";
	} // switch
	return nullptr;
}

/** extensions GL_ANGLE_translated_shader_source */
const char* gl::is_define_angle_translated_shader_source(GLenum pname)
{
	switch(pname)
	{
		case GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE: return "GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE";
	} // switch
	return nullptr;
}

/** extensions GL_APPLE_clip_distance */
const char* gl::is_define_apple_clip_distance(GLenum pname)
{
	switch(pname)
	{
		case GL_MAX_CLIP_DISTANCES_APPLE: return "GL_MAX_CLIP_DISTANCES_APPLE";
		case GL_CLIP_DISTANCE0_APPLE: return "GL_CLIP_DISTANCE0_APPLE";
		case GL_CLIP_DISTANCE1_APPLE: return "GL_CLIP_DISTANCE1_APPLE";
		case GL_CLIP_DISTANCE2_APPLE: return "GL_CLIP_DISTANCE2_APPLE";
		case GL_CLIP_DISTANCE3_APPLE: return "GL_CLIP_DISTANCE3_APPLE";
		case GL_CLIP_DISTANCE4_APPLE: return "GL_CLIP_DISTANCE4_APPLE";
		case GL_CLIP_DISTANCE5_APPLE: return "GL_CLIP_DISTANCE5_APPLE";
		case GL_CLIP_DISTANCE6_APPLE: return "GL_CLIP_DISTANCE6_APPLE";
		case GL_CLIP_DISTANCE7_APPLE: return "GL_CLIP_DISTANCE7_APPLE";
	} // switch
	return nullptr;
}

/** extensions GL_APPLE_color_buffer_packed_float */
const char* gl::is_define_apple_color_buffer_packed_float(GLenum pname)
{

	return nullptr;
}

/** extensions GL_APPLE_copy_texture_levels */
const char* gl::is_define_apple_copy_texture_levels(GLenum pname)
{

	return nullptr;
}

/** extensions GL_APPLE_framebuffer_multisample */
const char* gl::is_define_apple_framebuffer_multisample(GLenum pname)
{
	switch(pname)
	{
		case GL_RENDERBUFFER_SAMPLES_APPLE: return "GL_RENDERBUFFER_SAMPLES_APPLE";
		case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_APPLE: return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_APPLE";
		case GL_MAX_SAMPLES_APPLE: return "GL_MAX_SAMPLES_APPLE";
		case GL_READ_FRAMEBUFFER_APPLE: return "GL_READ_FRAMEBUFFER_APPLE";
		case GL_DRAW_FRAMEBUFFER_APPLE: return "GL_DRAW_FRAMEBUFFER_APPLE";
		case GL_DRAW_FRAMEBUFFER_BINDING_APPLE: return "GL_DRAW_FRAMEBUFFER_BINDING_APPLE";
		case GL_READ_FRAMEBUFFER_BINDING_APPLE: return "GL_READ_FRAMEBUFFER_BINDING_APPLE";
	} // switch
	return nullptr;
}

/** extensions GL_APPLE_rgb_422 */
const char* gl::is_define_apple_rgb_422(GLenum pname)
{
	switch(pname)
	{
		case GL_RGB_422_APPLE: return "GL_RGB_422_APPLE";
		case GL_UNSIGNED_SHORT_8_8_APPLE: return "GL_UNSIGNED_SHORT_8_8_APPLE";
		case GL_UNSIGNED_SHORT_8_8_REV_APPLE: return "GL_UNSIGNED_SHORT_8_8_REV_APPLE";
		case GL_RGB_RAW_422_APPLE: return "GL_RGB_RAW_422_APPLE";
	} // switch
	return nullptr;
}

/** extensions GL_APPLE_sync */
const char* gl::is_define_apple_sync(GLenum pname)
{
	switch(pname)
	{
		case GL_SYNC_OBJECT_APPLE: return "GL_SYNC_OBJECT_APPLE";
		case GL_MAX_SERVER_WAIT_TIMEOUT_APPLE: return "GL_MAX_SERVER_WAIT_TIMEOUT_APPLE";
		case GL_OBJECT_TYPE_APPLE: return "GL_OBJECT_TYPE_APPLE";
		case GL_SYNC_CONDITION_APPLE: return "GL_SYNC_CONDITION_APPLE";
		case GL_SYNC_STATUS_APPLE: return "GL_SYNC_STATUS_APPLE";
		case GL_SYNC_FLAGS_APPLE: return "GL_SYNC_FLAGS_APPLE";
		case GL_SYNC_FENCE_APPLE: return "GL_SYNC_FENCE_APPLE";
		case GL_SYNC_GPU_COMMANDS_COMPLETE_APPLE: return "GL_SYNC_GPU_COMMANDS_COMPLETE_APPLE";
		case GL_UNSIGNALED_APPLE: return "GL_UNSIGNALED_APPLE";
		case GL_SIGNALED_APPLE: return "GL_SIGNALED_APPLE";
		case GL_ALREADY_SIGNALED_APPLE: return "GL_ALREADY_SIGNALED_APPLE";
		case GL_TIMEOUT_EXPIRED_APPLE: return "GL_TIMEOUT_EXPIRED_APPLE";
		case GL_CONDITION_SATISFIED_APPLE: return "GL_CONDITION_SATISFIED_APPLE";
		case GL_WAIT_FAILED_APPLE: return "GL_WAIT_FAILED_APPLE";
		case GL_SYNC_FLUSH_COMMANDS_BIT_APPLE: return "GL_SYNC_FLUSH_COMMANDS_BIT_APPLE";
		//case GL_TIMEOUT_IGNORED_APPLE: return "GL_TIMEOUT_IGNORED_APPLE";
	} // switch
	return nullptr;
}

/** extensions GL_APPLE_texture_format_BGRA8888 */
const char* gl::is_define_apple_texture_format_bgra8888(GLenum pname)
{
	switch(pname)
	{
		case GL_BGRA_EXT: return "GL_BGRA_EXT";
		case GL_BGRA8_EXT: return "GL_BGRA8_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_APPLE_texture_max_level */
const char* gl::is_define_apple_texture_max_level(GLenum pname)
{
	switch(pname)
	{
		case GL_TEXTURE_MAX_LEVEL_APPLE: return "GL_TEXTURE_MAX_LEVEL_APPLE";
	} // switch
	return nullptr;
}

/** extensions GL_APPLE_texture_packed_float */
const char* gl::is_define_apple_texture_packed_float(GLenum pname)
{
	switch(pname)
	{
		case GL_UNSIGNED_INT_10F_11F_11F_REV_APPLE: return "GL_UNSIGNED_INT_10F_11F_11F_REV_APPLE";
		case GL_UNSIGNED_INT_5_9_9_9_REV_APPLE: return "GL_UNSIGNED_INT_5_9_9_9_REV_APPLE";
		case GL_R11F_G11F_B10F_APPLE: return "GL_R11F_G11F_B10F_APPLE";
		case GL_RGB9_E5_APPLE: return "GL_RGB9_E5_APPLE";
	} // switch
	return nullptr;
}

/** extensions GL_ARM_mali_program_binary */
const char* gl::is_define_arm_mali_program_binary(GLenum pname)
{
	switch(pname)
	{
		case GL_MALI_PROGRAM_BINARY_ARM: return "GL_MALI_PROGRAM_BINARY_ARM";
	} // switch
	return nullptr;
}

/** extensions GL_ARM_mali_shader_binary */
const char* gl::is_define_arm_mali_shader_binary(GLenum pname)
{
	switch(pname)
	{
		case GL_MALI_SHADER_BINARY_ARM: return "GL_MALI_SHADER_BINARY_ARM";
	} // switch
	return nullptr;
}

/** extensions GL_ARM_rgba8 */
const char* gl::is_define_arm_rgba8(GLenum pname)
{

	return nullptr;
}

/** extensions GL_ARM_shader_framebuffer_fetch */
const char* gl::is_define_arm_shader_framebuffer_fetch(GLenum pname)
{
	switch(pname)
	{
		case GL_FETCH_PER_SAMPLE_ARM: return "GL_FETCH_PER_SAMPLE_ARM";
		case GL_FRAGMENT_SHADER_FRAMEBUFFER_FETCH_MRT_ARM: return "GL_FRAGMENT_SHADER_FRAMEBUFFER_FETCH_MRT_ARM";
	} // switch
	return nullptr;
}

/** extensions GL_ARM_shader_framebuffer_fetch_depth_stencil */
const char* gl::is_define_arm_shader_framebuffer_fetch_depth_stencil(GLenum pname)
{

	return nullptr;
}

/** extensions GL_DMP_program_binary */
const char* gl::is_define_dmp_program_binary(GLenum pname)
{
	switch(pname)
	{
		case GL_SMAPHS30_PROGRAM_BINARY_DMP: return "GL_SMAPHS30_PROGRAM_BINARY_DMP";
		case GL_SMAPHS_PROGRAM_BINARY_DMP: return "GL_SMAPHS_PROGRAM_BINARY_DMP";
		case GL_DMP_PROGRAM_BINARY_DMP: return "GL_DMP_PROGRAM_BINARY_DMP";
	} // switch
	return nullptr;
}

/** extensions GL_DMP_shader_binary */
const char* gl::is_define_dmp_shader_binary(GLenum pname)
{
	switch(pname)
	{
		case GL_SHADER_BINARY_DMP: return "GL_SHADER_BINARY_DMP";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_YUV_target */
const char* gl::is_define_ext_yuv_target(GLenum pname)
{
	switch(pname)
	{
		case GL_SAMPLER_EXTERNAL_2D_Y2Y_EXT: return "GL_SAMPLER_EXTERNAL_2D_Y2Y_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_base_instance */
const char* gl::is_define_ext_base_instance(GLenum pname)
{

	return nullptr;
}

/** extensions GL_EXT_blend_func_extended */
const char* gl::is_define_ext_blend_func_extended(GLenum pname)
{
	switch(pname)
	{
		case GL_SRC1_COLOR_EXT: return "GL_SRC1_COLOR_EXT";
		case GL_SRC1_ALPHA_EXT: return "GL_SRC1_ALPHA_EXT";
		case GL_ONE_MINUS_SRC1_COLOR_EXT: return "GL_ONE_MINUS_SRC1_COLOR_EXT";
		case GL_ONE_MINUS_SRC1_ALPHA_EXT: return "GL_ONE_MINUS_SRC1_ALPHA_EXT";
		case GL_SRC_ALPHA_SATURATE_EXT: return "GL_SRC_ALPHA_SATURATE_EXT";
		case GL_LOCATION_INDEX_EXT: return "GL_LOCATION_INDEX_EXT";
		case GL_MAX_DUAL_SOURCE_DRAW_BUFFERS_EXT: return "GL_MAX_DUAL_SOURCE_DRAW_BUFFERS_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_blend_minmax */
const char* gl::is_define_ext_blend_minmax(GLenum pname)
{
	switch(pname)
	{
		case GL_MIN_EXT: return "GL_MIN_EXT";
		case GL_MAX_EXT: return "GL_MAX_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_buffer_storage */
const char* gl::is_define_ext_buffer_storage(GLenum pname)
{
	switch(pname)
	{
		case GL_MAP_READ_BIT: return "GL_MAP_READ_BIT";
		case GL_MAP_WRITE_BIT: return "GL_MAP_WRITE_BIT";
		case GL_MAP_PERSISTENT_BIT_EXT: return "GL_MAP_PERSISTENT_BIT_EXT";
		case GL_MAP_COHERENT_BIT_EXT: return "GL_MAP_COHERENT_BIT_EXT";
		case GL_DYNAMIC_STORAGE_BIT_EXT: return "GL_DYNAMIC_STORAGE_BIT_EXT";
		case GL_CLIENT_STORAGE_BIT_EXT: return "GL_CLIENT_STORAGE_BIT_EXT";
		case GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT_EXT: return "GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT_EXT";
		case GL_BUFFER_IMMUTABLE_STORAGE_EXT: return "GL_BUFFER_IMMUTABLE_STORAGE_EXT";
		case GL_BUFFER_STORAGE_FLAGS_EXT: return "GL_BUFFER_STORAGE_FLAGS_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_color_buffer_float */
const char* gl::is_define_ext_color_buffer_float(GLenum pname)
{

	return nullptr;
}

/** extensions GL_EXT_color_buffer_half_float */
const char* gl::is_define_ext_color_buffer_half_float(GLenum pname)
{
	switch(pname)
	{
		case GL_RGBA16F_EXT: return "GL_RGBA16F_EXT";
		case GL_RGB16F_EXT: return "GL_RGB16F_EXT";
		case GL_RG16F_EXT: return "GL_RG16F_EXT";
		case GL_R16F_EXT: return "GL_R16F_EXT";
		case GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE_EXT: return "GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE_EXT";
		case GL_UNSIGNED_NORMALIZED_EXT: return "GL_UNSIGNED_NORMALIZED_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_copy_image */
const char* gl::is_define_ext_copy_image(GLenum pname)
{

	return nullptr;
}

/** extensions GL_EXT_debug_label */
const char* gl::is_define_ext_debug_label(GLenum pname)
{
	switch(pname)
	{
		case GL_PROGRAM_PIPELINE_OBJECT_EXT: return "GL_PROGRAM_PIPELINE_OBJECT_EXT";
		case GL_PROGRAM_OBJECT_EXT: return "GL_PROGRAM_OBJECT_EXT";
		case GL_SHADER_OBJECT_EXT: return "GL_SHADER_OBJECT_EXT";
		case GL_BUFFER_OBJECT_EXT: return "GL_BUFFER_OBJECT_EXT";
		case GL_QUERY_OBJECT_EXT: return "GL_QUERY_OBJECT_EXT";
		case GL_VERTEX_ARRAY_OBJECT_EXT: return "GL_VERTEX_ARRAY_OBJECT_EXT";
		case GL_TRANSFORM_FEEDBACK: return "GL_TRANSFORM_FEEDBACK";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_debug_marker */
const char* gl::is_define_ext_debug_marker(GLenum pname)
{

	return nullptr;
}

/** extensions GL_EXT_discard_framebuffer */
const char* gl::is_define_ext_discard_framebuffer(GLenum pname)
{
	switch(pname)
	{
		case GL_COLOR_EXT: return "GL_COLOR_EXT";
		case GL_DEPTH_EXT: return "GL_DEPTH_EXT";
		case GL_STENCIL_EXT: return "GL_STENCIL_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_disjoint_timer_query */
const char* gl::is_define_ext_disjoint_timer_query(GLenum pname)
{
	switch(pname)
	{
		case GL_QUERY_COUNTER_BITS_EXT: return "GL_QUERY_COUNTER_BITS_EXT";
		case GL_CURRENT_QUERY_EXT: return "GL_CURRENT_QUERY_EXT";
		case GL_QUERY_RESULT_EXT: return "GL_QUERY_RESULT_EXT";
		case GL_QUERY_RESULT_AVAILABLE_EXT: return "GL_QUERY_RESULT_AVAILABLE_EXT";
		case GL_TIME_ELAPSED_EXT: return "GL_TIME_ELAPSED_EXT";
		case GL_TIMESTAMP_EXT: return "GL_TIMESTAMP_EXT";
		case GL_GPU_DISJOINT_EXT: return "GL_GPU_DISJOINT_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_draw_buffers */
const char* gl::is_define_ext_draw_buffers(GLenum pname)
{
	switch(pname)
	{
		case GL_MAX_COLOR_ATTACHMENTS_EXT: return "GL_MAX_COLOR_ATTACHMENTS_EXT";
		case GL_MAX_DRAW_BUFFERS_EXT: return "GL_MAX_DRAW_BUFFERS_EXT";
		case GL_DRAW_BUFFER0_EXT: return "GL_DRAW_BUFFER0_EXT";
		case GL_DRAW_BUFFER1_EXT: return "GL_DRAW_BUFFER1_EXT";
		case GL_DRAW_BUFFER2_EXT: return "GL_DRAW_BUFFER2_EXT";
		case GL_DRAW_BUFFER3_EXT: return "GL_DRAW_BUFFER3_EXT";
		case GL_DRAW_BUFFER4_EXT: return "GL_DRAW_BUFFER4_EXT";
		case GL_DRAW_BUFFER5_EXT: return "GL_DRAW_BUFFER5_EXT";
		case GL_DRAW_BUFFER6_EXT: return "GL_DRAW_BUFFER6_EXT";
		case GL_DRAW_BUFFER7_EXT: return "GL_DRAW_BUFFER7_EXT";
		case GL_DRAW_BUFFER8_EXT: return "GL_DRAW_BUFFER8_EXT";
		case GL_DRAW_BUFFER9_EXT: return "GL_DRAW_BUFFER9_EXT";
		case GL_DRAW_BUFFER10_EXT: return "GL_DRAW_BUFFER10_EXT";
		case GL_DRAW_BUFFER11_EXT: return "GL_DRAW_BUFFER11_EXT";
		case GL_DRAW_BUFFER12_EXT: return "GL_DRAW_BUFFER12_EXT";
		case GL_DRAW_BUFFER13_EXT: return "GL_DRAW_BUFFER13_EXT";
		case GL_DRAW_BUFFER14_EXT: return "GL_DRAW_BUFFER14_EXT";
		case GL_DRAW_BUFFER15_EXT: return "GL_DRAW_BUFFER15_EXT";
		case GL_COLOR_ATTACHMENT0_EXT: return "GL_COLOR_ATTACHMENT0_EXT";
		case GL_COLOR_ATTACHMENT1_EXT: return "GL_COLOR_ATTACHMENT1_EXT";
		case GL_COLOR_ATTACHMENT2_EXT: return "GL_COLOR_ATTACHMENT2_EXT";
		case GL_COLOR_ATTACHMENT3_EXT: return "GL_COLOR_ATTACHMENT3_EXT";
		case GL_COLOR_ATTACHMENT4_EXT: return "GL_COLOR_ATTACHMENT4_EXT";
		case GL_COLOR_ATTACHMENT5_EXT: return "GL_COLOR_ATTACHMENT5_EXT";
		case GL_COLOR_ATTACHMENT6_EXT: return "GL_COLOR_ATTACHMENT6_EXT";
		case GL_COLOR_ATTACHMENT7_EXT: return "GL_COLOR_ATTACHMENT7_EXT";
		case GL_COLOR_ATTACHMENT8_EXT: return "GL_COLOR_ATTACHMENT8_EXT";
		case GL_COLOR_ATTACHMENT9_EXT: return "GL_COLOR_ATTACHMENT9_EXT";
		case GL_COLOR_ATTACHMENT10_EXT: return "GL_COLOR_ATTACHMENT10_EXT";
		case GL_COLOR_ATTACHMENT11_EXT: return "GL_COLOR_ATTACHMENT11_EXT";
		case GL_COLOR_ATTACHMENT12_EXT: return "GL_COLOR_ATTACHMENT12_EXT";
		case GL_COLOR_ATTACHMENT13_EXT: return "GL_COLOR_ATTACHMENT13_EXT";
		case GL_COLOR_ATTACHMENT14_EXT: return "GL_COLOR_ATTACHMENT14_EXT";
		case GL_COLOR_ATTACHMENT15_EXT: return "GL_COLOR_ATTACHMENT15_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_draw_buffers_indexed */
const char* gl::is_define_ext_draw_buffers_indexed(GLenum pname)
{

	return nullptr;
}

/** extensions GL_EXT_draw_elements_base_vertex */
const char* gl::is_define_ext_draw_elements_base_vertex(GLenum pname)
{

	return nullptr;
}

/** extensions GL_EXT_draw_instanced */
const char* gl::is_define_ext_draw_instanced(GLenum pname)
{

	return nullptr;
}

/** extensions GL_EXT_float_blend */
const char* gl::is_define_ext_float_blend(GLenum pname)
{

	return nullptr;
}

/** extensions GL_EXT_geometry_point_size */
const char* gl::is_define_ext_geometry_point_size(GLenum pname)
{

	return nullptr;
}

/** extensions GL_EXT_geometry_shader */
const char* gl::is_define_ext_geometry_shader(GLenum pname)
{
	switch(pname)
	{
		case GL_GEOMETRY_SHADER_EXT: return "GL_GEOMETRY_SHADER_EXT";
		case GL_GEOMETRY_SHADER_BIT_EXT: return "GL_GEOMETRY_SHADER_BIT_EXT";
		case GL_GEOMETRY_LINKED_VERTICES_OUT_EXT: return "GL_GEOMETRY_LINKED_VERTICES_OUT_EXT";
		case GL_GEOMETRY_LINKED_INPUT_TYPE_EXT: return "GL_GEOMETRY_LINKED_INPUT_TYPE_EXT";
		case GL_GEOMETRY_LINKED_OUTPUT_TYPE_EXT: return "GL_GEOMETRY_LINKED_OUTPUT_TYPE_EXT";
		case GL_GEOMETRY_SHADER_INVOCATIONS_EXT: return "GL_GEOMETRY_SHADER_INVOCATIONS_EXT";
		case GL_LAYER_PROVOKING_VERTEX_EXT: return "GL_LAYER_PROVOKING_VERTEX_EXT";
		case GL_LINES_ADJACENCY_EXT: return "GL_LINES_ADJACENCY_EXT";
		case GL_LINE_STRIP_ADJACENCY_EXT: return "GL_LINE_STRIP_ADJACENCY_EXT";
		case GL_TRIANGLES_ADJACENCY_EXT: return "GL_TRIANGLES_ADJACENCY_EXT";
		case GL_TRIANGLE_STRIP_ADJACENCY_EXT: return "GL_TRIANGLE_STRIP_ADJACENCY_EXT";
		case GL_MAX_GEOMETRY_UNIFORM_COMPONENTS_EXT: return "GL_MAX_GEOMETRY_UNIFORM_COMPONENTS_EXT";
		case GL_MAX_GEOMETRY_UNIFORM_BLOCKS_EXT: return "GL_MAX_GEOMETRY_UNIFORM_BLOCKS_EXT";
		case GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS_EXT: return "GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS_EXT";
		case GL_MAX_GEOMETRY_INPUT_COMPONENTS_EXT: return "GL_MAX_GEOMETRY_INPUT_COMPONENTS_EXT";
		case GL_MAX_GEOMETRY_OUTPUT_COMPONENTS_EXT: return "GL_MAX_GEOMETRY_OUTPUT_COMPONENTS_EXT";
		case GL_MAX_GEOMETRY_OUTPUT_VERTICES_EXT: return "GL_MAX_GEOMETRY_OUTPUT_VERTICES_EXT";
		case GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS_EXT: return "GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS_EXT";
		case GL_MAX_GEOMETRY_SHADER_INVOCATIONS_EXT: return "GL_MAX_GEOMETRY_SHADER_INVOCATIONS_EXT";
		case GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS_EXT: return "GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS_EXT";
		case GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS_EXT: return "GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS_EXT";
		case GL_MAX_GEOMETRY_ATOMIC_COUNTERS_EXT: return "GL_MAX_GEOMETRY_ATOMIC_COUNTERS_EXT";
		case GL_MAX_GEOMETRY_IMAGE_UNIFORMS_EXT: return "GL_MAX_GEOMETRY_IMAGE_UNIFORMS_EXT";
		case GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS_EXT: return "GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS_EXT";
		case GL_FIRST_VERTEX_CONVENTION_EXT: return "GL_FIRST_VERTEX_CONVENTION_EXT";
		case GL_LAST_VERTEX_CONVENTION_EXT: return "GL_LAST_VERTEX_CONVENTION_EXT";
		case GL_UNDEFINED_VERTEX_EXT: return "GL_UNDEFINED_VERTEX_EXT";
		case GL_PRIMITIVES_GENERATED_EXT: return "GL_PRIMITIVES_GENERATED_EXT";
		case GL_FRAMEBUFFER_DEFAULT_LAYERS_EXT: return "GL_FRAMEBUFFER_DEFAULT_LAYERS_EXT";
		case GL_MAX_FRAMEBUFFER_LAYERS_EXT: return "GL_MAX_FRAMEBUFFER_LAYERS_EXT";
		case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS_EXT: return "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS_EXT";
		case GL_FRAMEBUFFER_ATTACHMENT_LAYERED_EXT: return "GL_FRAMEBUFFER_ATTACHMENT_LAYERED_EXT";
		case GL_REFERENCED_BY_GEOMETRY_SHADER_EXT: return "GL_REFERENCED_BY_GEOMETRY_SHADER_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_gpu_shader5 */
const char* gl::is_define_ext_gpu_shader5(GLenum pname)
{

	return nullptr;
}

/** extensions GL_EXT_instanced_arrays */
const char* gl::is_define_ext_instanced_arrays(GLenum pname)
{
	switch(pname)
	{
		case GL_VERTEX_ATTRIB_ARRAY_DIVISOR_EXT: return "GL_VERTEX_ATTRIB_ARRAY_DIVISOR_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_map_buffer_range */
const char* gl::is_define_ext_map_buffer_range(GLenum pname)
{
	switch(pname)
	{
		case GL_MAP_READ_BIT_EXT: return "GL_MAP_READ_BIT_EXT";
		case GL_MAP_WRITE_BIT_EXT: return "GL_MAP_WRITE_BIT_EXT";
		case GL_MAP_INVALIDATE_RANGE_BIT_EXT: return "GL_MAP_INVALIDATE_RANGE_BIT_EXT";
		case GL_MAP_INVALIDATE_BUFFER_BIT_EXT: return "GL_MAP_INVALIDATE_BUFFER_BIT_EXT";
		case GL_MAP_FLUSH_EXPLICIT_BIT_EXT: return "GL_MAP_FLUSH_EXPLICIT_BIT_EXT";
		case GL_MAP_UNSYNCHRONIZED_BIT_EXT: return "GL_MAP_UNSYNCHRONIZED_BIT_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_multi_draw_arrays */
const char* gl::is_define_ext_multi_draw_arrays(GLenum pname)
{

	return nullptr;
}

/** extensions GL_EXT_multi_draw_indirect */
const char* gl::is_define_ext_multi_draw_indirect(GLenum pname)
{

	return nullptr;
}

/** extensions GL_EXT_multisampled_compatibility */
const char* gl::is_define_ext_multisampled_compatibility(GLenum pname)
{
	switch(pname)
	{
		case GL_MULTISAMPLE_EXT: return "GL_MULTISAMPLE_EXT";
		case GL_SAMPLE_ALPHA_TO_ONE_EXT: return "GL_SAMPLE_ALPHA_TO_ONE_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_multisampled_render_to_texture */
const char* gl::is_define_ext_multisampled_render_to_texture(GLenum pname)
{
	switch(pname)
	{
		case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SAMPLES_EXT: return "GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SAMPLES_EXT";
		case GL_RENDERBUFFER_SAMPLES_EXT: return "GL_RENDERBUFFER_SAMPLES_EXT";
		case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT: return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT";
		case GL_MAX_SAMPLES_EXT: return "GL_MAX_SAMPLES_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_multiview_draw_buffers */
const char* gl::is_define_ext_multiview_draw_buffers(GLenum pname)
{
	switch(pname)
	{
		case GL_COLOR_ATTACHMENT_EXT: return "GL_COLOR_ATTACHMENT_EXT";
		case GL_MULTIVIEW_EXT: return "GL_MULTIVIEW_EXT";
		case GL_DRAW_BUFFER_EXT: return "GL_DRAW_BUFFER_EXT";
		case GL_READ_BUFFER_EXT: return "GL_READ_BUFFER_EXT";
		case GL_MAX_MULTIVIEW_BUFFERS_EXT: return "GL_MAX_MULTIVIEW_BUFFERS_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_occlusion_query_boolean */
const char* gl::is_define_ext_occlusion_query_boolean(GLenum pname)
{
	switch(pname)
	{
		case GL_ANY_SAMPLES_PASSED_EXT: return "GL_ANY_SAMPLES_PASSED_EXT";
		case GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT: return "GL_ANY_SAMPLES_PASSED_CONSERVATIVE_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_polygon_offset_clamp */
const char* gl::is_define_ext_polygon_offset_clamp(GLenum pname)
{
	switch(pname)
	{
		case GL_POLYGON_OFFSET_CLAMP_EXT: return "GL_POLYGON_OFFSET_CLAMP_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_post_depth_coverage */
const char* gl::is_define_ext_post_depth_coverage(GLenum pname)
{

	return nullptr;
}

/** extensions GL_EXT_primitive_bounding_box */
const char* gl::is_define_ext_primitive_bounding_box(GLenum pname)
{
	switch(pname)
	{
		case GL_PRIMITIVE_BOUNDING_BOX_EXT: return "GL_PRIMITIVE_BOUNDING_BOX_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_pvrtc_sRGB */
const char* gl::is_define_ext_pvrtc_srgb(GLenum pname)
{
	switch(pname)
	{
		case GL_COMPRESSED_SRGB_PVRTC_2BPPV1_EXT: return "GL_COMPRESSED_SRGB_PVRTC_2BPPV1_EXT";
		case GL_COMPRESSED_SRGB_PVRTC_4BPPV1_EXT: return "GL_COMPRESSED_SRGB_PVRTC_4BPPV1_EXT";
		case GL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV1_EXT: return "GL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV1_EXT";
		case GL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV1_EXT: return "GL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV1_EXT";
		case GL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV2_IMG: return "GL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV2_IMG";
		case GL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV2_IMG: return "GL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV2_IMG";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_raster_multisample */
const char* gl::is_define_ext_raster_multisample(GLenum pname)
{
	switch(pname)
	{
		case GL_RASTER_MULTISAMPLE_EXT: return "GL_RASTER_MULTISAMPLE_EXT";
		case GL_RASTER_SAMPLES_EXT: return "GL_RASTER_SAMPLES_EXT";
		case GL_MAX_RASTER_SAMPLES_EXT: return "GL_MAX_RASTER_SAMPLES_EXT";
		case GL_RASTER_FIXED_SAMPLE_LOCATIONS_EXT: return "GL_RASTER_FIXED_SAMPLE_LOCATIONS_EXT";
		case GL_MULTISAMPLE_RASTERIZATION_ALLOWED_EXT: return "GL_MULTISAMPLE_RASTERIZATION_ALLOWED_EXT";
		case GL_EFFECTIVE_RASTER_SAMPLES_EXT: return "GL_EFFECTIVE_RASTER_SAMPLES_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_read_format_bgra */
const char* gl::is_define_ext_read_format_bgra(GLenum pname)
{
	switch(pname)
	{
		case GL_UNSIGNED_SHORT_4_4_4_4_REV_EXT: return "GL_UNSIGNED_SHORT_4_4_4_4_REV_EXT";
		case GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT: return "GL_UNSIGNED_SHORT_1_5_5_5_REV_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_render_snorm */
const char* gl::is_define_ext_render_snorm(GLenum pname)
{
	switch(pname)
	{
		case GL_R8_SNORM: return "GL_R8_SNORM";
		case GL_RG8_SNORM: return "GL_RG8_SNORM";
		case GL_RGBA8_SNORM: return "GL_RGBA8_SNORM";
		case GL_R16_SNORM_EXT: return "GL_R16_SNORM_EXT";
		case GL_RG16_SNORM_EXT: return "GL_RG16_SNORM_EXT";
		case GL_RGBA16_SNORM_EXT: return "GL_RGBA16_SNORM_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_robustness */
const char* gl::is_define_ext_robustness(GLenum pname)
{
	switch(pname)
	{
		case GL_GUILTY_CONTEXT_RESET_EXT: return "GL_GUILTY_CONTEXT_RESET_EXT";
		case GL_INNOCENT_CONTEXT_RESET_EXT: return "GL_INNOCENT_CONTEXT_RESET_EXT";
		case GL_UNKNOWN_CONTEXT_RESET_EXT: return "GL_UNKNOWN_CONTEXT_RESET_EXT";
		case GL_CONTEXT_ROBUST_ACCESS_EXT: return "GL_CONTEXT_ROBUST_ACCESS_EXT";
		case GL_RESET_NOTIFICATION_STRATEGY_EXT: return "GL_RESET_NOTIFICATION_STRATEGY_EXT";
		case GL_LOSE_CONTEXT_ON_RESET_EXT: return "GL_LOSE_CONTEXT_ON_RESET_EXT";
		case GL_NO_RESET_NOTIFICATION_EXT: return "GL_NO_RESET_NOTIFICATION_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_sRGB */
const char* gl::is_define_ext_srgb(GLenum pname)
{
	switch(pname)
	{
		case GL_SRGB_EXT: return "GL_SRGB_EXT";
		case GL_SRGB_ALPHA_EXT: return "GL_SRGB_ALPHA_EXT";
		case GL_SRGB8_ALPHA8_EXT: return "GL_SRGB8_ALPHA8_EXT";
		case GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT: return "GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_sRGB_write_control */
const char* gl::is_define_ext_srgb_write_control(GLenum pname)
{
	switch(pname)
	{
		case GL_FRAMEBUFFER_SRGB_EXT: return "GL_FRAMEBUFFER_SRGB_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_separate_shader_objects */
const char* gl::is_define_ext_separate_shader_objects(GLenum pname)
{
	switch(pname)
	{
		case GL_ACTIVE_PROGRAM_EXT: return "GL_ACTIVE_PROGRAM_EXT";
		case GL_VERTEX_SHADER_BIT_EXT: return "GL_VERTEX_SHADER_BIT_EXT";
		case GL_FRAGMENT_SHADER_BIT_EXT: return "GL_FRAGMENT_SHADER_BIT_EXT";
		case GL_ALL_SHADER_BITS_EXT: return "GL_ALL_SHADER_BITS_EXT";
		case GL_PROGRAM_SEPARABLE_EXT: return "GL_PROGRAM_SEPARABLE_EXT";
		case GL_PROGRAM_PIPELINE_BINDING_EXT: return "GL_PROGRAM_PIPELINE_BINDING_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_shader_framebuffer_fetch */
const char* gl::is_define_ext_shader_framebuffer_fetch(GLenum pname)
{
	switch(pname)
	{
		case GL_FRAGMENT_SHADER_DISCARDS_SAMPLES_EXT: return "GL_FRAGMENT_SHADER_DISCARDS_SAMPLES_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_shader_group_vote */
const char* gl::is_define_ext_shader_group_vote(GLenum pname)
{

	return nullptr;
}

/** extensions GL_EXT_shader_implicit_conversions */
const char* gl::is_define_ext_shader_implicit_conversions(GLenum pname)
{

	return nullptr;
}

/** extensions GL_EXT_shader_integer_mix */
const char* gl::is_define_ext_shader_integer_mix(GLenum pname)
{

	return nullptr;
}

/** extensions GL_EXT_shader_io_blocks */
const char* gl::is_define_ext_shader_io_blocks(GLenum pname)
{

	return nullptr;
}

/** extensions GL_EXT_shader_pixel_local_storage */
const char* gl::is_define_ext_shader_pixel_local_storage(GLenum pname)
{
	switch(pname)
	{
		case GL_MAX_SHADER_PIXEL_LOCAL_STORAGE_FAST_SIZE_EXT: return "GL_MAX_SHADER_PIXEL_LOCAL_STORAGE_FAST_SIZE_EXT";
		case GL_MAX_SHADER_PIXEL_LOCAL_STORAGE_SIZE_EXT: return "GL_MAX_SHADER_PIXEL_LOCAL_STORAGE_SIZE_EXT";
		case GL_SHADER_PIXEL_LOCAL_STORAGE_EXT: return "GL_SHADER_PIXEL_LOCAL_STORAGE_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_shader_pixel_local_storage2 */
const char* gl::is_define_ext_shader_pixel_local_storage2(GLenum pname)
{
	switch(pname)
	{
		case GL_MAX_SHADER_COMBINED_LOCAL_STORAGE_FAST_SIZE_EXT: return "GL_MAX_SHADER_COMBINED_LOCAL_STORAGE_FAST_SIZE_EXT";
		case GL_MAX_SHADER_COMBINED_LOCAL_STORAGE_SIZE_EXT: return "GL_MAX_SHADER_COMBINED_LOCAL_STORAGE_SIZE_EXT";
		case GL_FRAMEBUFFER_INCOMPLETE_INSUFFICIENT_SHADER_COMBINED_LOCAL_STORAGE_EXT: return "GL_FRAMEBUFFER_INCOMPLETE_INSUFFICIENT_SHADER_COMBINED_LOCAL_STORAGE_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_shader_texture_lod */
const char* gl::is_define_ext_shader_texture_lod(GLenum pname)
{

	return nullptr;
}

/** extensions GL_EXT_shadow_samplers */
const char* gl::is_define_ext_shadow_samplers(GLenum pname)
{
	switch(pname)
	{
		case GL_TEXTURE_COMPARE_MODE_EXT: return "GL_TEXTURE_COMPARE_MODE_EXT";
		case GL_TEXTURE_COMPARE_FUNC_EXT: return "GL_TEXTURE_COMPARE_FUNC_EXT";
		case GL_COMPARE_REF_TO_TEXTURE_EXT: return "GL_COMPARE_REF_TO_TEXTURE_EXT";
		case GL_SAMPLER_2D_SHADOW_EXT: return "GL_SAMPLER_2D_SHADOW_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_sparse_texture */
const char* gl::is_define_ext_sparse_texture(GLenum pname)
{
	switch(pname)
	{
		case GL_TEXTURE_SPARSE_EXT: return "GL_TEXTURE_SPARSE_EXT";
		case GL_VIRTUAL_PAGE_SIZE_INDEX_EXT: return "GL_VIRTUAL_PAGE_SIZE_INDEX_EXT";
		case GL_NUM_SPARSE_LEVELS_EXT: return "GL_NUM_SPARSE_LEVELS_EXT";
		case GL_NUM_VIRTUAL_PAGE_SIZES_EXT: return "GL_NUM_VIRTUAL_PAGE_SIZES_EXT";
		case GL_VIRTUAL_PAGE_SIZE_X_EXT: return "GL_VIRTUAL_PAGE_SIZE_X_EXT";
		case GL_VIRTUAL_PAGE_SIZE_Y_EXT: return "GL_VIRTUAL_PAGE_SIZE_Y_EXT";
		case GL_VIRTUAL_PAGE_SIZE_Z_EXT: return "GL_VIRTUAL_PAGE_SIZE_Z_EXT";
		case GL_TEXTURE_2D_ARRAY: return "GL_TEXTURE_2D_ARRAY";
		case GL_TEXTURE_3D: return "GL_TEXTURE_3D";
		case GL_MAX_SPARSE_TEXTURE_SIZE_EXT: return "GL_MAX_SPARSE_TEXTURE_SIZE_EXT";
		case GL_MAX_SPARSE_3D_TEXTURE_SIZE_EXT: return "GL_MAX_SPARSE_3D_TEXTURE_SIZE_EXT";
		case GL_MAX_SPARSE_ARRAY_TEXTURE_LAYERS_EXT: return "GL_MAX_SPARSE_ARRAY_TEXTURE_LAYERS_EXT";
		case GL_SPARSE_TEXTURE_FULL_ARRAY_CUBE_MIPMAPS_EXT: return "GL_SPARSE_TEXTURE_FULL_ARRAY_CUBE_MIPMAPS_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_tessellation_point_size */
const char* gl::is_define_ext_tessellation_point_size(GLenum pname)
{

	return nullptr;
}

/** extensions GL_EXT_tessellation_shader */
const char* gl::is_define_ext_tessellation_shader(GLenum pname)
{
	switch(pname)
	{
		case GL_PATCHES_EXT: return "GL_PATCHES_EXT";
		case GL_PATCH_VERTICES_EXT: return "GL_PATCH_VERTICES_EXT";
		case GL_TESS_CONTROL_OUTPUT_VERTICES_EXT: return "GL_TESS_CONTROL_OUTPUT_VERTICES_EXT";
		case GL_TESS_GEN_MODE_EXT: return "GL_TESS_GEN_MODE_EXT";
		case GL_TESS_GEN_SPACING_EXT: return "GL_TESS_GEN_SPACING_EXT";
		case GL_TESS_GEN_VERTEX_ORDER_EXT: return "GL_TESS_GEN_VERTEX_ORDER_EXT";
		case GL_TESS_GEN_POINT_MODE_EXT: return "GL_TESS_GEN_POINT_MODE_EXT";
		case GL_ISOLINES_EXT: return "GL_ISOLINES_EXT";
		case GL_QUADS_EXT: return "GL_QUADS_EXT";
		case GL_FRACTIONAL_ODD_EXT: return "GL_FRACTIONAL_ODD_EXT";
		case GL_FRACTIONAL_EVEN_EXT: return "GL_FRACTIONAL_EVEN_EXT";
		case GL_MAX_PATCH_VERTICES_EXT: return "GL_MAX_PATCH_VERTICES_EXT";
		case GL_MAX_TESS_GEN_LEVEL_EXT: return "GL_MAX_TESS_GEN_LEVEL_EXT";
		case GL_MAX_TESS_CONTROL_UNIFORM_COMPONENTS_EXT: return "GL_MAX_TESS_CONTROL_UNIFORM_COMPONENTS_EXT";
		case GL_MAX_TESS_EVALUATION_UNIFORM_COMPONENTS_EXT: return "GL_MAX_TESS_EVALUATION_UNIFORM_COMPONENTS_EXT";
		case GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS_EXT: return "GL_MAX_TESS_CONTROL_TEXTURE_IMAGE_UNITS_EXT";
		case GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS_EXT: return "GL_MAX_TESS_EVALUATION_TEXTURE_IMAGE_UNITS_EXT";
		case GL_MAX_TESS_CONTROL_OUTPUT_COMPONENTS_EXT: return "GL_MAX_TESS_CONTROL_OUTPUT_COMPONENTS_EXT";
		case GL_MAX_TESS_PATCH_COMPONENTS_EXT: return "GL_MAX_TESS_PATCH_COMPONENTS_EXT";
		case GL_MAX_TESS_CONTROL_TOTAL_OUTPUT_COMPONENTS_EXT: return "GL_MAX_TESS_CONTROL_TOTAL_OUTPUT_COMPONENTS_EXT";
		case GL_MAX_TESS_EVALUATION_OUTPUT_COMPONENTS_EXT: return "GL_MAX_TESS_EVALUATION_OUTPUT_COMPONENTS_EXT";
		case GL_MAX_TESS_CONTROL_UNIFORM_BLOCKS_EXT: return "GL_MAX_TESS_CONTROL_UNIFORM_BLOCKS_EXT";
		case GL_MAX_TESS_EVALUATION_UNIFORM_BLOCKS_EXT: return "GL_MAX_TESS_EVALUATION_UNIFORM_BLOCKS_EXT";
		case GL_MAX_TESS_CONTROL_INPUT_COMPONENTS_EXT: return "GL_MAX_TESS_CONTROL_INPUT_COMPONENTS_EXT";
		case GL_MAX_TESS_EVALUATION_INPUT_COMPONENTS_EXT: return "GL_MAX_TESS_EVALUATION_INPUT_COMPONENTS_EXT";
		case GL_MAX_COMBINED_TESS_CONTROL_UNIFORM_COMPONENTS_EXT: return "GL_MAX_COMBINED_TESS_CONTROL_UNIFORM_COMPONENTS_EXT";
		case GL_MAX_COMBINED_TESS_EVALUATION_UNIFORM_COMPONENTS_EXT: return "GL_MAX_COMBINED_TESS_EVALUATION_UNIFORM_COMPONENTS_EXT";
		case GL_MAX_TESS_CONTROL_ATOMIC_COUNTER_BUFFERS_EXT: return "GL_MAX_TESS_CONTROL_ATOMIC_COUNTER_BUFFERS_EXT";
		case GL_MAX_TESS_EVALUATION_ATOMIC_COUNTER_BUFFERS_EXT: return "GL_MAX_TESS_EVALUATION_ATOMIC_COUNTER_BUFFERS_EXT";
		case GL_MAX_TESS_CONTROL_ATOMIC_COUNTERS_EXT: return "GL_MAX_TESS_CONTROL_ATOMIC_COUNTERS_EXT";
		case GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS_EXT: return "GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS_EXT";
		case GL_MAX_TESS_CONTROL_IMAGE_UNIFORMS_EXT: return "GL_MAX_TESS_CONTROL_IMAGE_UNIFORMS_EXT";
		case GL_MAX_TESS_EVALUATION_IMAGE_UNIFORMS_EXT: return "GL_MAX_TESS_EVALUATION_IMAGE_UNIFORMS_EXT";
		case GL_MAX_TESS_CONTROL_SHADER_STORAGE_BLOCKS_EXT: return "GL_MAX_TESS_CONTROL_SHADER_STORAGE_BLOCKS_EXT";
		case GL_MAX_TESS_EVALUATION_SHADER_STORAGE_BLOCKS_EXT: return "GL_MAX_TESS_EVALUATION_SHADER_STORAGE_BLOCKS_EXT";
		case GL_PRIMITIVE_RESTART_FOR_PATCHES_SUPPORTED: return "GL_PRIMITIVE_RESTART_FOR_PATCHES_SUPPORTED";
		case GL_IS_PER_PATCH_EXT: return "GL_IS_PER_PATCH_EXT";
		case GL_REFERENCED_BY_TESS_CONTROL_SHADER_EXT: return "GL_REFERENCED_BY_TESS_CONTROL_SHADER_EXT";
		case GL_REFERENCED_BY_TESS_EVALUATION_SHADER_EXT: return "GL_REFERENCED_BY_TESS_EVALUATION_SHADER_EXT";
		case GL_TESS_CONTROL_SHADER_EXT: return "GL_TESS_CONTROL_SHADER_EXT";
		case GL_TESS_EVALUATION_SHADER_EXT: return "GL_TESS_EVALUATION_SHADER_EXT";
		case GL_TESS_CONTROL_SHADER_BIT_EXT: return "GL_TESS_CONTROL_SHADER_BIT_EXT";
		case GL_TESS_EVALUATION_SHADER_BIT_EXT: return "GL_TESS_EVALUATION_SHADER_BIT_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_texture_border_clamp */
const char* gl::is_define_ext_texture_border_clamp(GLenum pname)
{
	switch(pname)
	{
		case GL_TEXTURE_BORDER_COLOR_EXT: return "GL_TEXTURE_BORDER_COLOR_EXT";
		case GL_CLAMP_TO_BORDER_EXT: return "GL_CLAMP_TO_BORDER_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_texture_buffer */
const char* gl::is_define_ext_texture_buffer(GLenum pname)
{
	switch(pname)
	{
		case GL_TEXTURE_BUFFER_EXT: return "GL_TEXTURE_BUFFER_EXT";
		//case GL_TEXTURE_BUFFER_BINDING_EXT: return "GL_TEXTURE_BUFFER_BINDING_EXT";
		case GL_MAX_TEXTURE_BUFFER_SIZE_EXT: return "GL_MAX_TEXTURE_BUFFER_SIZE_EXT";
		case GL_TEXTURE_BINDING_BUFFER_EXT: return "GL_TEXTURE_BINDING_BUFFER_EXT";
		case GL_TEXTURE_BUFFER_DATA_STORE_BINDING_EXT: return "GL_TEXTURE_BUFFER_DATA_STORE_BINDING_EXT";
		case GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT_EXT: return "GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT_EXT";
		case GL_SAMPLER_BUFFER_EXT: return "GL_SAMPLER_BUFFER_EXT";
		case GL_INT_SAMPLER_BUFFER_EXT: return "GL_INT_SAMPLER_BUFFER_EXT";
		case GL_UNSIGNED_INT_SAMPLER_BUFFER_EXT: return "GL_UNSIGNED_INT_SAMPLER_BUFFER_EXT";
		case GL_IMAGE_BUFFER_EXT: return "GL_IMAGE_BUFFER_EXT";
		case GL_INT_IMAGE_BUFFER_EXT: return "GL_INT_IMAGE_BUFFER_EXT";
		case GL_UNSIGNED_INT_IMAGE_BUFFER_EXT: return "GL_UNSIGNED_INT_IMAGE_BUFFER_EXT";
		case GL_TEXTURE_BUFFER_OFFSET_EXT: return "GL_TEXTURE_BUFFER_OFFSET_EXT";
		case GL_TEXTURE_BUFFER_SIZE_EXT: return "GL_TEXTURE_BUFFER_SIZE_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_texture_compression_dxt1 */
const char* gl::is_define_ext_texture_compression_dxt1(GLenum pname)
{
	switch(pname)
	{
		case GL_COMPRESSED_RGB_S3TC_DXT1_EXT: return "GL_COMPRESSED_RGB_S3TC_DXT1_EXT";
		case GL_COMPRESSED_RGBA_S3TC_DXT1_EXT: return "GL_COMPRESSED_RGBA_S3TC_DXT1_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_texture_compression_s3tc */
const char* gl::is_define_ext_texture_compression_s3tc(GLenum pname)
{
	switch(pname)
	{
		case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT: return "GL_COMPRESSED_RGBA_S3TC_DXT3_EXT";
		case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT: return "GL_COMPRESSED_RGBA_S3TC_DXT5_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_texture_cube_map_array */
const char* gl::is_define_ext_texture_cube_map_array(GLenum pname)
{
	switch(pname)
	{
		case GL_TEXTURE_CUBE_MAP_ARRAY_EXT: return "GL_TEXTURE_CUBE_MAP_ARRAY_EXT";
		case GL_TEXTURE_BINDING_CUBE_MAP_ARRAY_EXT: return "GL_TEXTURE_BINDING_CUBE_MAP_ARRAY_EXT";
		case GL_SAMPLER_CUBE_MAP_ARRAY_EXT: return "GL_SAMPLER_CUBE_MAP_ARRAY_EXT";
		case GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW_EXT: return "GL_SAMPLER_CUBE_MAP_ARRAY_SHADOW_EXT";
		case GL_INT_SAMPLER_CUBE_MAP_ARRAY_EXT: return "GL_INT_SAMPLER_CUBE_MAP_ARRAY_EXT";
		case GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY_EXT: return "GL_UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY_EXT";
		case GL_IMAGE_CUBE_MAP_ARRAY_EXT: return "GL_IMAGE_CUBE_MAP_ARRAY_EXT";
		case GL_INT_IMAGE_CUBE_MAP_ARRAY_EXT: return "GL_INT_IMAGE_CUBE_MAP_ARRAY_EXT";
		case GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY_EXT: return "GL_UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_texture_filter_anisotropic */
const char* gl::is_define_ext_texture_filter_anisotropic(GLenum pname)
{
	switch( pname )
	{
		case GL_TEXTURE_MAX_ANISOTROPY_EXT:
		return "GL_TEXTURE_MAX_ANISOTROPY_EXT";

		case GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT:
		return "GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT";
	} // switch

	return nullptr;
}

/** extensions GL_EXT_texture_filter_minmax */
const char* gl::is_define_ext_texture_filter_minmax(GLenum pname)
{

	return nullptr;
}

/** extensions GL_EXT_texture_format_BGRA8888 */
const char* gl::is_define_ext_texture_format_bgra8888(GLenum pname)
{

	return nullptr;
}

/** extensions GL_EXT_texture_norm16 */
const char* gl::is_define_ext_texture_norm16(GLenum pname)
{
	switch(pname)
	{
		case GL_R16_EXT: return "GL_R16_EXT";
		case GL_RG16_EXT: return "GL_RG16_EXT";
		case GL_RGBA16_EXT: return "GL_RGBA16_EXT";
		case GL_RGB16_EXT: return "GL_RGB16_EXT";
		case GL_RGB16_SNORM_EXT: return "GL_RGB16_SNORM_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_texture_rg */
const char* gl::is_define_ext_texture_rg(GLenum pname)
{
	switch(pname)
	{
		case GL_RED_EXT: return "GL_RED_EXT";
		case GL_RG_EXT: return "GL_RG_EXT";
		case GL_R8_EXT: return "GL_R8_EXT";
		case GL_RG8_EXT: return "GL_RG8_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_texture_sRGB_R8 */
const char* gl::is_define_ext_texture_srgb_r8(GLenum pname)
{
	switch(pname)
	{
		case GL_SR8_EXT: return "GL_SR8_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_texture_sRGB_RG8 */
const char* gl::is_define_ext_texture_srgb_rg8(GLenum pname)
{
	switch(pname)
	{
		case GL_SRG8_EXT: return "GL_SRG8_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_texture_sRGB_decode */
const char* gl::is_define_ext_texture_srgb_decode(GLenum pname)
{
	switch(pname)
	{
		case GL_TEXTURE_SRGB_DECODE_EXT: return "GL_TEXTURE_SRGB_DECODE_EXT";
		case GL_DECODE_EXT: return "GL_DECODE_EXT";
		case GL_SKIP_DECODE_EXT: return "GL_SKIP_DECODE_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_texture_storage */
const char* gl::is_define_ext_texture_storage(GLenum pname)
{
	switch(pname)
	{
		case GL_TEXTURE_IMMUTABLE_FORMAT_EXT: return "GL_TEXTURE_IMMUTABLE_FORMAT_EXT";
		case GL_ALPHA8_EXT: return "GL_ALPHA8_EXT";
		case GL_LUMINANCE8_EXT: return "GL_LUMINANCE8_EXT";
		case GL_LUMINANCE8_ALPHA8_EXT: return "GL_LUMINANCE8_ALPHA8_EXT";
		case GL_RGBA32F_EXT: return "GL_RGBA32F_EXT";
		case GL_RGB32F_EXT: return "GL_RGB32F_EXT";
		case GL_ALPHA32F_EXT: return "GL_ALPHA32F_EXT";
		case GL_LUMINANCE32F_EXT: return "GL_LUMINANCE32F_EXT";
		case GL_LUMINANCE_ALPHA32F_EXT: return "GL_LUMINANCE_ALPHA32F_EXT";
		case GL_ALPHA16F_EXT: return "GL_ALPHA16F_EXT";
		case GL_LUMINANCE16F_EXT: return "GL_LUMINANCE16F_EXT";
		case GL_LUMINANCE_ALPHA16F_EXT: return "GL_LUMINANCE_ALPHA16F_EXT";
		case GL_R32F_EXT: return "GL_R32F_EXT";
		case GL_RG32F_EXT: return "GL_RG32F_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_texture_type_2_10_10_10_REV */
const char* gl::is_define_ext_texture_type_2_10_10_10_rev(GLenum pname)
{
	switch(pname)
	{
		case GL_UNSIGNED_INT_2_10_10_10_REV_EXT: return "GL_UNSIGNED_INT_2_10_10_10_REV_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_texture_view */
const char* gl::is_define_ext_texture_view(GLenum pname)
{
	switch(pname)
	{
		case GL_TEXTURE_VIEW_MIN_LEVEL_EXT: return "GL_TEXTURE_VIEW_MIN_LEVEL_EXT";
		case GL_TEXTURE_VIEW_NUM_LEVELS_EXT: return "GL_TEXTURE_VIEW_NUM_LEVELS_EXT";
		case GL_TEXTURE_VIEW_MIN_LAYER_EXT: return "GL_TEXTURE_VIEW_MIN_LAYER_EXT";
		case GL_TEXTURE_VIEW_NUM_LAYERS_EXT: return "GL_TEXTURE_VIEW_NUM_LAYERS_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_EXT_unpack_subimage */
const char* gl::is_define_ext_unpack_subimage(GLenum pname)
{
	switch(pname)
	{
		case GL_UNPACK_ROW_LENGTH_EXT: return "GL_UNPACK_ROW_LENGTH_EXT";
		case GL_UNPACK_SKIP_ROWS_EXT: return "GL_UNPACK_SKIP_ROWS_EXT";
		case GL_UNPACK_SKIP_PIXELS_EXT: return "GL_UNPACK_SKIP_PIXELS_EXT";
	} // switch
	return nullptr;
}

/** extensions GL_FJ_shader_binary_GCCSO */
const char* gl::is_define_fj_shader_binary_gccso(GLenum pname)
{
	switch(pname)
	{
		case GL_GCCSO_SHADER_BINARY_FJ: return "GL_GCCSO_SHADER_BINARY_FJ";
	} // switch
	return nullptr;
}

/** extensions GL_IMG_framebuffer_downsample */
const char* gl::is_define_img_framebuffer_downsample(GLenum pname)
{
	switch(pname)
	{
		case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_AND_DOWNSAMPLE_IMG: return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_AND_DOWNSAMPLE_IMG";
		case GL_NUM_DOWNSAMPLE_SCALES_IMG: return "GL_NUM_DOWNSAMPLE_SCALES_IMG";
		case GL_DOWNSAMPLE_SCALES_IMG: return "GL_DOWNSAMPLE_SCALES_IMG";
		case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SCALE_IMG: return "GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_SCALE_IMG";
	} // switch
	return nullptr;
}

/** extensions GL_IMG_multisampled_render_to_texture */
const char* gl::is_define_img_multisampled_render_to_texture(GLenum pname)
{
	switch(pname)
	{
		case GL_RENDERBUFFER_SAMPLES_IMG: return "GL_RENDERBUFFER_SAMPLES_IMG";
		case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_IMG: return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_IMG";
		case GL_MAX_SAMPLES_IMG: return "GL_MAX_SAMPLES_IMG";
		case GL_TEXTURE_SAMPLES_IMG: return "GL_TEXTURE_SAMPLES_IMG";
	} // switch
	return nullptr;
}

/** extensions GL_IMG_program_binary */
const char* gl::is_define_img_program_binary(GLenum pname)
{
	switch(pname)
	{
		case GL_SGX_PROGRAM_BINARY_IMG: return "GL_SGX_PROGRAM_BINARY_IMG";
	} // switch
	return nullptr;
}

/** extensions GL_IMG_read_format */
const char* gl::is_define_img_read_format(GLenum pname)
{
	switch(pname)
	{
		case GL_BGRA_IMG: return "GL_BGRA_IMG";
		case GL_UNSIGNED_SHORT_4_4_4_4_REV_IMG: return "GL_UNSIGNED_SHORT_4_4_4_4_REV_IMG";
	} // switch
	return nullptr;
}

/** extensions GL_IMG_shader_binary */
const char* gl::is_define_img_shader_binary(GLenum pname)
{
	switch(pname)
	{
		case GL_SGX_BINARY_IMG: return "GL_SGX_BINARY_IMG";
	} // switch
	return nullptr;
}

/** extensions GL_IMG_texture_compression_pvrtc */
const char* gl::is_define_img_texture_compression_pvrtc(GLenum pname)
{
	switch(pname)
	{
		case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG: return "GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG";
		case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG: return "GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG";
		case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG: return "GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG";
		case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG: return "GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG";
	} // switch
	return nullptr;
}

/** extensions GL_IMG_texture_compression_pvrtc2 */
const char* gl::is_define_img_texture_compression_pvrtc2(GLenum pname)
{
	switch(pname)
	{
		case GL_COMPRESSED_RGBA_PVRTC_2BPPV2_IMG: return "GL_COMPRESSED_RGBA_PVRTC_2BPPV2_IMG";
		case GL_COMPRESSED_RGBA_PVRTC_4BPPV2_IMG: return "GL_COMPRESSED_RGBA_PVRTC_4BPPV2_IMG";
	} // switch
	return nullptr;
}

/** extensions GL_IMG_texture_filter_cubic */
const char* gl::is_define_img_texture_filter_cubic(GLenum pname)
{
	switch(pname)
	{
		case GL_CUBIC_IMG: return "GL_CUBIC_IMG";
		case GL_CUBIC_MIPMAP_NEAREST_IMG: return "GL_CUBIC_MIPMAP_NEAREST_IMG";
		case GL_CUBIC_MIPMAP_LINEAR_IMG: return "GL_CUBIC_MIPMAP_LINEAR_IMG";
	} // switch
	return nullptr;
}

/** extensions GL_INTEL_framebuffer_CMAA */
const char* gl::is_define_intel_framebuffer_cmaa(GLenum pname)
{

	return nullptr;
}

/** extensions GL_INTEL_performance_query */
const char* gl::is_define_intel_performance_query(GLenum pname)
{
	switch(pname)
	{
		case GL_PERFQUERY_SINGLE_CONTEXT_INTEL: return "GL_PERFQUERY_SINGLE_CONTEXT_INTEL";
		case GL_PERFQUERY_GLOBAL_CONTEXT_INTEL: return "GL_PERFQUERY_GLOBAL_CONTEXT_INTEL";
		case GL_PERFQUERY_WAIT_INTEL: return "GL_PERFQUERY_WAIT_INTEL";
		case GL_PERFQUERY_FLUSH_INTEL: return "GL_PERFQUERY_FLUSH_INTEL";
		case GL_PERFQUERY_DONOT_FLUSH_INTEL: return "GL_PERFQUERY_DONOT_FLUSH_INTEL";
		case GL_PERFQUERY_COUNTER_EVENT_INTEL: return "GL_PERFQUERY_COUNTER_EVENT_INTEL";
		case GL_PERFQUERY_COUNTER_DURATION_NORM_INTEL: return "GL_PERFQUERY_COUNTER_DURATION_NORM_INTEL";
		case GL_PERFQUERY_COUNTER_DURATION_RAW_INTEL: return "GL_PERFQUERY_COUNTER_DURATION_RAW_INTEL";
		case GL_PERFQUERY_COUNTER_THROUGHPUT_INTEL: return "GL_PERFQUERY_COUNTER_THROUGHPUT_INTEL";
		case GL_PERFQUERY_COUNTER_RAW_INTEL: return "GL_PERFQUERY_COUNTER_RAW_INTEL";
		case GL_PERFQUERY_COUNTER_TIMESTAMP_INTEL: return "GL_PERFQUERY_COUNTER_TIMESTAMP_INTEL";
		case GL_PERFQUERY_COUNTER_DATA_UINT32_INTEL: return "GL_PERFQUERY_COUNTER_DATA_UINT32_INTEL";
		case GL_PERFQUERY_COUNTER_DATA_UINT64_INTEL: return "GL_PERFQUERY_COUNTER_DATA_UINT64_INTEL";
		case GL_PERFQUERY_COUNTER_DATA_FLOAT_INTEL: return "GL_PERFQUERY_COUNTER_DATA_FLOAT_INTEL";
		case GL_PERFQUERY_COUNTER_DATA_DOUBLE_INTEL: return "GL_PERFQUERY_COUNTER_DATA_DOUBLE_INTEL";
		case GL_PERFQUERY_COUNTER_DATA_BOOL32_INTEL: return "GL_PERFQUERY_COUNTER_DATA_BOOL32_INTEL";
		case GL_PERFQUERY_QUERY_NAME_LENGTH_MAX_INTEL: return "GL_PERFQUERY_QUERY_NAME_LENGTH_MAX_INTEL";
		case GL_PERFQUERY_COUNTER_NAME_LENGTH_MAX_INTEL: return "GL_PERFQUERY_COUNTER_NAME_LENGTH_MAX_INTEL";
		case GL_PERFQUERY_COUNTER_DESC_LENGTH_MAX_INTEL: return "GL_PERFQUERY_COUNTER_DESC_LENGTH_MAX_INTEL";
		case GL_PERFQUERY_GPA_EXTENDED_COUNTERS_INTEL: return "GL_PERFQUERY_GPA_EXTENDED_COUNTERS_INTEL";
	} // switch
	return nullptr;
}

/** extensions GL_NV_bindless_texture */
const char* gl::is_define_nv_bindless_texture(GLenum pname)
{

	return nullptr;
}

/** extensions GL_NV_blend_equation_advanced */
const char* gl::is_define_nv_blend_equation_advanced(GLenum pname)
{
	switch(pname)
	{
		case GL_BLEND_OVERLAP_NV: return "GL_BLEND_OVERLAP_NV";
		case GL_BLEND_PREMULTIPLIED_SRC_NV: return "GL_BLEND_PREMULTIPLIED_SRC_NV";
		case GL_BLUE_NV: return "GL_BLUE_NV";
		case GL_COLORBURN_NV: return "GL_COLORBURN_NV";
		case GL_COLORDODGE_NV: return "GL_COLORDODGE_NV";
		case GL_CONJOINT_NV: return "GL_CONJOINT_NV";
		case GL_CONTRAST_NV: return "GL_CONTRAST_NV";
		case GL_DARKEN_NV: return "GL_DARKEN_NV";
		case GL_DIFFERENCE_NV: return "GL_DIFFERENCE_NV";
		case GL_DISJOINT_NV: return "GL_DISJOINT_NV";
		case GL_DST_ATOP_NV: return "GL_DST_ATOP_NV";
		case GL_DST_IN_NV: return "GL_DST_IN_NV";
		case GL_DST_NV: return "GL_DST_NV";
		case GL_DST_OUT_NV: return "GL_DST_OUT_NV";
		case GL_DST_OVER_NV: return "GL_DST_OVER_NV";
		case GL_EXCLUSION_NV: return "GL_EXCLUSION_NV";
		case GL_GREEN_NV: return "GL_GREEN_NV";
		case GL_HARDLIGHT_NV: return "GL_HARDLIGHT_NV";
		case GL_HARDMIX_NV: return "GL_HARDMIX_NV";
		case GL_HSL_COLOR_NV: return "GL_HSL_COLOR_NV";
		case GL_HSL_HUE_NV: return "GL_HSL_HUE_NV";
		case GL_HSL_LUMINOSITY_NV: return "GL_HSL_LUMINOSITY_NV";
		case GL_HSL_SATURATION_NV: return "GL_HSL_SATURATION_NV";
		case GL_INVERT_OVG_NV: return "GL_INVERT_OVG_NV";
		case GL_INVERT_RGB_NV: return "GL_INVERT_RGB_NV";
		case GL_LIGHTEN_NV: return "GL_LIGHTEN_NV";
		case GL_LINEARBURN_NV: return "GL_LINEARBURN_NV";
		case GL_LINEARDODGE_NV: return "GL_LINEARDODGE_NV";
		case GL_LINEARLIGHT_NV: return "GL_LINEARLIGHT_NV";
		case GL_MINUS_CLAMPED_NV: return "GL_MINUS_CLAMPED_NV";
		case GL_MINUS_NV: return "GL_MINUS_NV";
		case GL_MULTIPLY_NV: return "GL_MULTIPLY_NV";
		case GL_OVERLAY_NV: return "GL_OVERLAY_NV";
		case GL_PINLIGHT_NV: return "GL_PINLIGHT_NV";
		case GL_PLUS_CLAMPED_ALPHA_NV: return "GL_PLUS_CLAMPED_ALPHA_NV";
		case GL_PLUS_CLAMPED_NV: return "GL_PLUS_CLAMPED_NV";
		case GL_PLUS_DARKER_NV: return "GL_PLUS_DARKER_NV";
		case GL_PLUS_NV: return "GL_PLUS_NV";
		case GL_RED_NV: return "GL_RED_NV";
		case GL_SCREEN_NV: return "GL_SCREEN_NV";
		case GL_SOFTLIGHT_NV: return "GL_SOFTLIGHT_NV";
		case GL_SRC_ATOP_NV: return "GL_SRC_ATOP_NV";
		case GL_SRC_IN_NV: return "GL_SRC_IN_NV";
		case GL_SRC_NV: return "GL_SRC_NV";
		case GL_SRC_OUT_NV: return "GL_SRC_OUT_NV";
		case GL_SRC_OVER_NV: return "GL_SRC_OVER_NV";
		case GL_UNCORRELATED_NV: return "GL_UNCORRELATED_NV";
		case GL_VIVIDLIGHT_NV: return "GL_VIVIDLIGHT_NV";
		case GL_XOR_NV: return "GL_XOR_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_blend_equation_advanced_coherent */
const char* gl::is_define_nv_blend_equation_advanced_coherent(GLenum pname)
{
	switch(pname)
	{
		case GL_BLEND_ADVANCED_COHERENT_NV: return "GL_BLEND_ADVANCED_COHERENT_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_conditional_render */
const char* gl::is_define_nv_conditional_render(GLenum pname)
{
	switch(pname)
	{
		case GL_QUERY_WAIT_NV: return "GL_QUERY_WAIT_NV";
		case GL_QUERY_NO_WAIT_NV: return "GL_QUERY_NO_WAIT_NV";
		case GL_QUERY_BY_REGION_WAIT_NV: return "GL_QUERY_BY_REGION_WAIT_NV";
		case GL_QUERY_BY_REGION_NO_WAIT_NV: return "GL_QUERY_BY_REGION_NO_WAIT_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_conservative_raster */
const char* gl::is_define_nv_conservative_raster(GLenum pname)
{
	switch(pname)
	{
		case GL_CONSERVATIVE_RASTERIZATION_NV: return "GL_CONSERVATIVE_RASTERIZATION_NV";
		case GL_SUBPIXEL_PRECISION_BIAS_X_BITS_NV: return "GL_SUBPIXEL_PRECISION_BIAS_X_BITS_NV";
		case GL_SUBPIXEL_PRECISION_BIAS_Y_BITS_NV: return "GL_SUBPIXEL_PRECISION_BIAS_Y_BITS_NV";
		case GL_MAX_SUBPIXEL_PRECISION_BIAS_BITS_NV: return "GL_MAX_SUBPIXEL_PRECISION_BIAS_BITS_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_copy_buffer */
const char* gl::is_define_nv_copy_buffer(GLenum pname)
{
	switch(pname)
	{
		case GL_COPY_READ_BUFFER_NV: return "GL_COPY_READ_BUFFER_NV";
		case GL_COPY_WRITE_BUFFER_NV: return "GL_COPY_WRITE_BUFFER_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_coverage_sample */
const char* gl::is_define_nv_coverage_sample(GLenum pname)
{
	switch(pname)
	{
		case GL_COVERAGE_COMPONENT_NV: return "GL_COVERAGE_COMPONENT_NV";
		case GL_COVERAGE_COMPONENT4_NV: return "GL_COVERAGE_COMPONENT4_NV";
		case GL_COVERAGE_ATTACHMENT_NV: return "GL_COVERAGE_ATTACHMENT_NV";
		case GL_COVERAGE_BUFFERS_NV: return "GL_COVERAGE_BUFFERS_NV";
		case GL_COVERAGE_SAMPLES_NV: return "GL_COVERAGE_SAMPLES_NV";
		case GL_COVERAGE_ALL_FRAGMENTS_NV: return "GL_COVERAGE_ALL_FRAGMENTS_NV";
		case GL_COVERAGE_EDGE_FRAGMENTS_NV: return "GL_COVERAGE_EDGE_FRAGMENTS_NV";
		case GL_COVERAGE_AUTOMATIC_NV: return "GL_COVERAGE_AUTOMATIC_NV";
		case GL_COVERAGE_BUFFER_BIT_NV: return "GL_COVERAGE_BUFFER_BIT_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_depth_nonlinear */
const char* gl::is_define_nv_depth_nonlinear(GLenum pname)
{
	switch(pname)
	{
		case GL_DEPTH_COMPONENT16_NONLINEAR_NV: return "GL_DEPTH_COMPONENT16_NONLINEAR_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_draw_buffers */
const char* gl::is_define_nv_draw_buffers(GLenum pname)
{
	switch(pname)
	{
		case GL_MAX_DRAW_BUFFERS_NV: return "GL_MAX_DRAW_BUFFERS_NV";
		case GL_DRAW_BUFFER0_NV: return "GL_DRAW_BUFFER0_NV";
		case GL_DRAW_BUFFER1_NV: return "GL_DRAW_BUFFER1_NV";
		case GL_DRAW_BUFFER2_NV: return "GL_DRAW_BUFFER2_NV";
		case GL_DRAW_BUFFER3_NV: return "GL_DRAW_BUFFER3_NV";
		case GL_DRAW_BUFFER4_NV: return "GL_DRAW_BUFFER4_NV";
		case GL_DRAW_BUFFER5_NV: return "GL_DRAW_BUFFER5_NV";
		case GL_DRAW_BUFFER6_NV: return "GL_DRAW_BUFFER6_NV";
		case GL_DRAW_BUFFER7_NV: return "GL_DRAW_BUFFER7_NV";
		case GL_DRAW_BUFFER8_NV: return "GL_DRAW_BUFFER8_NV";
		case GL_DRAW_BUFFER9_NV: return "GL_DRAW_BUFFER9_NV";
		case GL_DRAW_BUFFER10_NV: return "GL_DRAW_BUFFER10_NV";
		case GL_DRAW_BUFFER11_NV: return "GL_DRAW_BUFFER11_NV";
		case GL_DRAW_BUFFER12_NV: return "GL_DRAW_BUFFER12_NV";
		case GL_DRAW_BUFFER13_NV: return "GL_DRAW_BUFFER13_NV";
		case GL_DRAW_BUFFER14_NV: return "GL_DRAW_BUFFER14_NV";
		case GL_DRAW_BUFFER15_NV: return "GL_DRAW_BUFFER15_NV";
		case GL_COLOR_ATTACHMENT0_NV: return "GL_COLOR_ATTACHMENT0_NV";
		case GL_COLOR_ATTACHMENT1_NV: return "GL_COLOR_ATTACHMENT1_NV";
		case GL_COLOR_ATTACHMENT2_NV: return "GL_COLOR_ATTACHMENT2_NV";
		case GL_COLOR_ATTACHMENT3_NV: return "GL_COLOR_ATTACHMENT3_NV";
		case GL_COLOR_ATTACHMENT4_NV: return "GL_COLOR_ATTACHMENT4_NV";
		case GL_COLOR_ATTACHMENT5_NV: return "GL_COLOR_ATTACHMENT5_NV";
		case GL_COLOR_ATTACHMENT6_NV: return "GL_COLOR_ATTACHMENT6_NV";
		case GL_COLOR_ATTACHMENT7_NV: return "GL_COLOR_ATTACHMENT7_NV";
		case GL_COLOR_ATTACHMENT8_NV: return "GL_COLOR_ATTACHMENT8_NV";
		case GL_COLOR_ATTACHMENT9_NV: return "GL_COLOR_ATTACHMENT9_NV";
		case GL_COLOR_ATTACHMENT10_NV: return "GL_COLOR_ATTACHMENT10_NV";
		case GL_COLOR_ATTACHMENT11_NV: return "GL_COLOR_ATTACHMENT11_NV";
		case GL_COLOR_ATTACHMENT12_NV: return "GL_COLOR_ATTACHMENT12_NV";
		case GL_COLOR_ATTACHMENT13_NV: return "GL_COLOR_ATTACHMENT13_NV";
		case GL_COLOR_ATTACHMENT14_NV: return "GL_COLOR_ATTACHMENT14_NV";
		case GL_COLOR_ATTACHMENT15_NV: return "GL_COLOR_ATTACHMENT15_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_draw_instanced */
const char* gl::is_define_nv_draw_instanced(GLenum pname)
{

	return nullptr;
}

/** extensions GL_NV_explicit_attrib_location */
const char* gl::is_define_nv_explicit_attrib_location(GLenum pname)
{

	return nullptr;
}

/** extensions GL_NV_fbo_color_attachments */
const char* gl::is_define_nv_fbo_color_attachments(GLenum pname)
{
	switch(pname)
	{
		case GL_MAX_COLOR_ATTACHMENTS_NV: return "GL_MAX_COLOR_ATTACHMENTS_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_fence */
const char* gl::is_define_nv_fence(GLenum pname)
{
	switch(pname)
	{
		case GL_ALL_COMPLETED_NV: return "GL_ALL_COMPLETED_NV";
		case GL_FENCE_STATUS_NV: return "GL_FENCE_STATUS_NV";
		case GL_FENCE_CONDITION_NV: return "GL_FENCE_CONDITION_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_fill_rectangle */
const char* gl::is_define_nv_fill_rectangle(GLenum pname)
{
	switch(pname)
	{
		case GL_FILL_RECTANGLE_NV: return "GL_FILL_RECTANGLE_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_fragment_coverage_to_color */
const char* gl::is_define_nv_fragment_coverage_to_color(GLenum pname)
{
	switch(pname)
	{
		case GL_FRAGMENT_COVERAGE_TO_COLOR_NV: return "GL_FRAGMENT_COVERAGE_TO_COLOR_NV";
		case GL_FRAGMENT_COVERAGE_COLOR_NV: return "GL_FRAGMENT_COVERAGE_COLOR_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_fragment_shader_interlock */
const char* gl::is_define_nv_fragment_shader_interlock(GLenum pname)
{

	return nullptr;
}

/** extensions GL_NV_framebuffer_blit */
const char* gl::is_define_nv_framebuffer_blit(GLenum pname)
{
	switch(pname)
	{
		case GL_READ_FRAMEBUFFER_NV: return "GL_READ_FRAMEBUFFER_NV";
		case GL_DRAW_FRAMEBUFFER_NV: return "GL_DRAW_FRAMEBUFFER_NV";
		case GL_DRAW_FRAMEBUFFER_BINDING_NV: return "GL_DRAW_FRAMEBUFFER_BINDING_NV";
		case GL_READ_FRAMEBUFFER_BINDING_NV: return "GL_READ_FRAMEBUFFER_BINDING_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_framebuffer_mixed_samples */
const char* gl::is_define_nv_framebuffer_mixed_samples(GLenum pname)
{
	switch(pname)
	{
		case GL_COVERAGE_MODULATION_TABLE_NV: return "GL_COVERAGE_MODULATION_TABLE_NV";
		case GL_COLOR_SAMPLES_NV: return "GL_COLOR_SAMPLES_NV";
		case GL_DEPTH_SAMPLES_NV: return "GL_DEPTH_SAMPLES_NV";
		case GL_STENCIL_SAMPLES_NV: return "GL_STENCIL_SAMPLES_NV";
		case GL_MIXED_DEPTH_SAMPLES_SUPPORTED_NV: return "GL_MIXED_DEPTH_SAMPLES_SUPPORTED_NV";
		case GL_MIXED_STENCIL_SAMPLES_SUPPORTED_NV: return "GL_MIXED_STENCIL_SAMPLES_SUPPORTED_NV";
		case GL_COVERAGE_MODULATION_NV: return "GL_COVERAGE_MODULATION_NV";
		case GL_COVERAGE_MODULATION_TABLE_SIZE_NV: return "GL_COVERAGE_MODULATION_TABLE_SIZE_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_framebuffer_multisample */
const char* gl::is_define_nv_framebuffer_multisample(GLenum pname)
{
	switch(pname)
	{
		case GL_RENDERBUFFER_SAMPLES_NV: return "GL_RENDERBUFFER_SAMPLES_NV";
		case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_NV: return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_NV";
		case GL_MAX_SAMPLES_NV: return "GL_MAX_SAMPLES_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_generate_mipmap_sRGB */
const char* gl::is_define_nv_generate_mipmap_srgb(GLenum pname)
{

	return nullptr;
}

/** extensions GL_NV_geometry_shader_passthrough */
const char* gl::is_define_nv_geometry_shader_passthrough(GLenum pname)
{

	return nullptr;
}

/** extensions GL_NV_image_formats */
const char* gl::is_define_nv_image_formats(GLenum pname)
{

	return nullptr;
}

/** extensions GL_NV_instanced_arrays */
const char* gl::is_define_nv_instanced_arrays(GLenum pname)
{
	switch(pname)
	{
		case GL_VERTEX_ATTRIB_ARRAY_DIVISOR_NV: return "GL_VERTEX_ATTRIB_ARRAY_DIVISOR_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_internalformat_sample_query */
const char* gl::is_define_nv_internalformat_sample_query(GLenum pname)
{
	switch(pname)
	{
		case GL_TEXTURE_2D_MULTISAMPLE: return "GL_TEXTURE_2D_MULTISAMPLE";
		case GL_TEXTURE_2D_MULTISAMPLE_ARRAY: return "GL_TEXTURE_2D_MULTISAMPLE_ARRAY";
		case GL_MULTISAMPLES_NV: return "GL_MULTISAMPLES_NV";
		case GL_SUPERSAMPLE_SCALE_X_NV: return "GL_SUPERSAMPLE_SCALE_X_NV";
		case GL_SUPERSAMPLE_SCALE_Y_NV: return "GL_SUPERSAMPLE_SCALE_Y_NV";
		case GL_CONFORMANT_NV: return "GL_CONFORMANT_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_non_square_matrices */
const char* gl::is_define_nv_non_square_matrices(GLenum pname)
{
	switch(pname)
	{
		case GL_FLOAT_MAT2x3_NV: return "GL_FLOAT_MAT2x3_NV";
		case GL_FLOAT_MAT2x4_NV: return "GL_FLOAT_MAT2x4_NV";
		case GL_FLOAT_MAT3x2_NV: return "GL_FLOAT_MAT3x2_NV";
		case GL_FLOAT_MAT3x4_NV: return "GL_FLOAT_MAT3x4_NV";
		case GL_FLOAT_MAT4x2_NV: return "GL_FLOAT_MAT4x2_NV";
		case GL_FLOAT_MAT4x3_NV: return "GL_FLOAT_MAT4x3_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_path_rendering */
const char* gl::is_define_nv_path_rendering(GLenum pname)
{
	switch(pname)
	{
		case GL_PATH_FORMAT_SVG_NV: return "GL_PATH_FORMAT_SVG_NV";
		case GL_PATH_FORMAT_PS_NV: return "GL_PATH_FORMAT_PS_NV";
		case GL_STANDARD_FONT_NAME_NV: return "GL_STANDARD_FONT_NAME_NV";
		case GL_SYSTEM_FONT_NAME_NV: return "GL_SYSTEM_FONT_NAME_NV";
		case GL_FILE_NAME_NV: return "GL_FILE_NAME_NV";
		case GL_PATH_STROKE_WIDTH_NV: return "GL_PATH_STROKE_WIDTH_NV";
		case GL_PATH_END_CAPS_NV: return "GL_PATH_END_CAPS_NV";
		case GL_PATH_INITIAL_END_CAP_NV: return "GL_PATH_INITIAL_END_CAP_NV";
		case GL_PATH_TERMINAL_END_CAP_NV: return "GL_PATH_TERMINAL_END_CAP_NV";
		case GL_PATH_JOIN_STYLE_NV: return "GL_PATH_JOIN_STYLE_NV";
		case GL_PATH_MITER_LIMIT_NV: return "GL_PATH_MITER_LIMIT_NV";
		case GL_PATH_DASH_CAPS_NV: return "GL_PATH_DASH_CAPS_NV";
		case GL_PATH_INITIAL_DASH_CAP_NV: return "GL_PATH_INITIAL_DASH_CAP_NV";
		case GL_PATH_TERMINAL_DASH_CAP_NV: return "GL_PATH_TERMINAL_DASH_CAP_NV";
		case GL_PATH_DASH_OFFSET_NV: return "GL_PATH_DASH_OFFSET_NV";
		case GL_PATH_CLIENT_LENGTH_NV: return "GL_PATH_CLIENT_LENGTH_NV";
		case GL_PATH_FILL_MODE_NV: return "GL_PATH_FILL_MODE_NV";
		case GL_PATH_FILL_MASK_NV: return "GL_PATH_FILL_MASK_NV";
		case GL_PATH_FILL_COVER_MODE_NV: return "GL_PATH_FILL_COVER_MODE_NV";
		case GL_PATH_STROKE_COVER_MODE_NV: return "GL_PATH_STROKE_COVER_MODE_NV";
		case GL_PATH_STROKE_MASK_NV: return "GL_PATH_STROKE_MASK_NV";
		case GL_COUNT_UP_NV: return "GL_COUNT_UP_NV";
		case GL_COUNT_DOWN_NV: return "GL_COUNT_DOWN_NV";
		case GL_PATH_OBJECT_BOUNDING_BOX_NV: return "GL_PATH_OBJECT_BOUNDING_BOX_NV";
		case GL_CONVEX_HULL_NV: return "GL_CONVEX_HULL_NV";
		case GL_BOUNDING_BOX_NV: return "GL_BOUNDING_BOX_NV";
		case GL_TRANSLATE_X_NV: return "GL_TRANSLATE_X_NV";
		case GL_TRANSLATE_Y_NV: return "GL_TRANSLATE_Y_NV";
		case GL_TRANSLATE_2D_NV: return "GL_TRANSLATE_2D_NV";
		case GL_TRANSLATE_3D_NV: return "GL_TRANSLATE_3D_NV";
		case GL_AFFINE_2D_NV: return "GL_AFFINE_2D_NV";
		case GL_AFFINE_3D_NV: return "GL_AFFINE_3D_NV";
		case GL_TRANSPOSE_AFFINE_2D_NV: return "GL_TRANSPOSE_AFFINE_2D_NV";
		case GL_TRANSPOSE_AFFINE_3D_NV: return "GL_TRANSPOSE_AFFINE_3D_NV";
		case GL_UTF8_NV: return "GL_UTF8_NV";
		case GL_UTF16_NV: return "GL_UTF16_NV";
		case GL_BOUNDING_BOX_OF_BOUNDING_BOXES_NV: return "GL_BOUNDING_BOX_OF_BOUNDING_BOXES_NV";
		case GL_PATH_COMMAND_COUNT_NV: return "GL_PATH_COMMAND_COUNT_NV";
		case GL_PATH_COORD_COUNT_NV: return "GL_PATH_COORD_COUNT_NV";
		case GL_PATH_DASH_ARRAY_COUNT_NV: return "GL_PATH_DASH_ARRAY_COUNT_NV";
		case GL_PATH_COMPUTED_LENGTH_NV: return "GL_PATH_COMPUTED_LENGTH_NV";
		case GL_PATH_FILL_BOUNDING_BOX_NV: return "GL_PATH_FILL_BOUNDING_BOX_NV";
		case GL_PATH_STROKE_BOUNDING_BOX_NV: return "GL_PATH_STROKE_BOUNDING_BOX_NV";
		case GL_SQUARE_NV: return "GL_SQUARE_NV";
		case GL_ROUND_NV: return "GL_ROUND_NV";
		case GL_TRIANGULAR_NV: return "GL_TRIANGULAR_NV";
		case GL_BEVEL_NV: return "GL_BEVEL_NV";
		case GL_MITER_REVERT_NV: return "GL_MITER_REVERT_NV";
		case GL_MITER_TRUNCATE_NV: return "GL_MITER_TRUNCATE_NV";
		case GL_SKIP_MISSING_GLYPH_NV: return "GL_SKIP_MISSING_GLYPH_NV";
		case GL_USE_MISSING_GLYPH_NV: return "GL_USE_MISSING_GLYPH_NV";
		case GL_PATH_ERROR_POSITION_NV: return "GL_PATH_ERROR_POSITION_NV";
		case GL_ACCUM_ADJACENT_PAIRS_NV: return "GL_ACCUM_ADJACENT_PAIRS_NV";
		case GL_ADJACENT_PAIRS_NV: return "GL_ADJACENT_PAIRS_NV";
		case GL_FIRST_TO_REST_NV: return "GL_FIRST_TO_REST_NV";
		case GL_PATH_GEN_MODE_NV: return "GL_PATH_GEN_MODE_NV";
		case GL_PATH_GEN_COEFF_NV: return "GL_PATH_GEN_COEFF_NV";
		case GL_PATH_GEN_COMPONENTS_NV: return "GL_PATH_GEN_COMPONENTS_NV";
		case GL_PATH_STENCIL_FUNC_NV: return "GL_PATH_STENCIL_FUNC_NV";
		case GL_PATH_STENCIL_REF_NV: return "GL_PATH_STENCIL_REF_NV";
		case GL_PATH_STENCIL_VALUE_MASK_NV: return "GL_PATH_STENCIL_VALUE_MASK_NV";
		case GL_PATH_STENCIL_DEPTH_OFFSET_FACTOR_NV: return "GL_PATH_STENCIL_DEPTH_OFFSET_FACTOR_NV";
		case GL_PATH_STENCIL_DEPTH_OFFSET_UNITS_NV: return "GL_PATH_STENCIL_DEPTH_OFFSET_UNITS_NV";
		case GL_PATH_COVER_DEPTH_FUNC_NV: return "GL_PATH_COVER_DEPTH_FUNC_NV";
		case GL_PATH_DASH_OFFSET_RESET_NV: return "GL_PATH_DASH_OFFSET_RESET_NV";
		case GL_MOVE_TO_RESETS_NV: return "GL_MOVE_TO_RESETS_NV";
		case GL_MOVE_TO_CONTINUES_NV: return "GL_MOVE_TO_CONTINUES_NV";
		case GL_CLOSE_PATH_NV: return "GL_CLOSE_PATH_NV";
		case GL_MOVE_TO_NV: return "GL_MOVE_TO_NV";
		case GL_RELATIVE_MOVE_TO_NV: return "GL_RELATIVE_MOVE_TO_NV";
		case GL_LINE_TO_NV: return "GL_LINE_TO_NV";
		case GL_RELATIVE_LINE_TO_NV: return "GL_RELATIVE_LINE_TO_NV";
		case GL_HORIZONTAL_LINE_TO_NV: return "GL_HORIZONTAL_LINE_TO_NV";
		case GL_RELATIVE_HORIZONTAL_LINE_TO_NV: return "GL_RELATIVE_HORIZONTAL_LINE_TO_NV";
		case GL_VERTICAL_LINE_TO_NV: return "GL_VERTICAL_LINE_TO_NV";
		case GL_RELATIVE_VERTICAL_LINE_TO_NV: return "GL_RELATIVE_VERTICAL_LINE_TO_NV";
		case GL_QUADRATIC_CURVE_TO_NV: return "GL_QUADRATIC_CURVE_TO_NV";
		case GL_RELATIVE_QUADRATIC_CURVE_TO_NV: return "GL_RELATIVE_QUADRATIC_CURVE_TO_NV";
		case GL_CUBIC_CURVE_TO_NV: return "GL_CUBIC_CURVE_TO_NV";
		case GL_RELATIVE_CUBIC_CURVE_TO_NV: return "GL_RELATIVE_CUBIC_CURVE_TO_NV";
		case GL_SMOOTH_QUADRATIC_CURVE_TO_NV: return "GL_SMOOTH_QUADRATIC_CURVE_TO_NV";
		case GL_RELATIVE_SMOOTH_QUADRATIC_CURVE_TO_NV: return "GL_RELATIVE_SMOOTH_QUADRATIC_CURVE_TO_NV";
		case GL_SMOOTH_CUBIC_CURVE_TO_NV: return "GL_SMOOTH_CUBIC_CURVE_TO_NV";
		case GL_RELATIVE_SMOOTH_CUBIC_CURVE_TO_NV: return "GL_RELATIVE_SMOOTH_CUBIC_CURVE_TO_NV";
		case GL_SMALL_CCW_ARC_TO_NV: return "GL_SMALL_CCW_ARC_TO_NV";
		case GL_RELATIVE_SMALL_CCW_ARC_TO_NV: return "GL_RELATIVE_SMALL_CCW_ARC_TO_NV";
		case GL_SMALL_CW_ARC_TO_NV: return "GL_SMALL_CW_ARC_TO_NV";
		case GL_RELATIVE_SMALL_CW_ARC_TO_NV: return "GL_RELATIVE_SMALL_CW_ARC_TO_NV";
		case GL_LARGE_CCW_ARC_TO_NV: return "GL_LARGE_CCW_ARC_TO_NV";
		case GL_RELATIVE_LARGE_CCW_ARC_TO_NV: return "GL_RELATIVE_LARGE_CCW_ARC_TO_NV";
		case GL_LARGE_CW_ARC_TO_NV: return "GL_LARGE_CW_ARC_TO_NV";
		case GL_RELATIVE_LARGE_CW_ARC_TO_NV: return "GL_RELATIVE_LARGE_CW_ARC_TO_NV";
		case GL_RESTART_PATH_NV: return "GL_RESTART_PATH_NV";
		case GL_DUP_FIRST_CUBIC_CURVE_TO_NV: return "GL_DUP_FIRST_CUBIC_CURVE_TO_NV";
		case GL_DUP_LAST_CUBIC_CURVE_TO_NV: return "GL_DUP_LAST_CUBIC_CURVE_TO_NV";
		case GL_RECT_NV: return "GL_RECT_NV";
		case GL_CIRCULAR_CCW_ARC_TO_NV: return "GL_CIRCULAR_CCW_ARC_TO_NV";
		case GL_CIRCULAR_CW_ARC_TO_NV: return "GL_CIRCULAR_CW_ARC_TO_NV";
		case GL_CIRCULAR_TANGENT_ARC_TO_NV: return "GL_CIRCULAR_TANGENT_ARC_TO_NV";
		case GL_ARC_TO_NV: return "GL_ARC_TO_NV";
		case GL_RELATIVE_ARC_TO_NV: return "GL_RELATIVE_ARC_TO_NV";
		//case GL_BOLD_BIT_NV: return "GL_BOLD_BIT_NV";
		//case GL_ITALIC_BIT_NV: return "GL_ITALIC_BIT_NV";
		//case GL_GLYPH_WIDTH_BIT_NV: return "GL_GLYPH_WIDTH_BIT_NV";
		//case GL_GLYPH_HEIGHT_BIT_NV: return "GL_GLYPH_HEIGHT_BIT_NV";
		//case GL_GLYPH_HORIZONTAL_BEARING_X_BIT_NV: return "GL_GLYPH_HORIZONTAL_BEARING_X_BIT_NV";
		//case GL_GLYPH_HORIZONTAL_BEARING_Y_BIT_NV: return "GL_GLYPH_HORIZONTAL_BEARING_Y_BIT_NV";
		//case GL_GLYPH_HORIZONTAL_BEARING_ADVANCE_BIT_NV: return "GL_GLYPH_HORIZONTAL_BEARING_ADVANCE_BIT_NV";
		//case GL_GLYPH_VERTICAL_BEARING_X_BIT_NV: return "GL_GLYPH_VERTICAL_BEARING_X_BIT_NV";
		//case GL_GLYPH_VERTICAL_BEARING_Y_BIT_NV: return "GL_GLYPH_VERTICAL_BEARING_Y_BIT_NV";
		//case GL_GLYPH_VERTICAL_BEARING_ADVANCE_BIT_NV: return "GL_GLYPH_VERTICAL_BEARING_ADVANCE_BIT_NV";
		case GL_GLYPH_HAS_KERNING_BIT_NV: return "GL_GLYPH_HAS_KERNING_BIT_NV";
		case GL_FONT_X_MIN_BOUNDS_BIT_NV: return "GL_FONT_X_MIN_BOUNDS_BIT_NV";
		case GL_FONT_Y_MIN_BOUNDS_BIT_NV: return "GL_FONT_Y_MIN_BOUNDS_BIT_NV";
		case GL_FONT_X_MAX_BOUNDS_BIT_NV: return "GL_FONT_X_MAX_BOUNDS_BIT_NV";
		case GL_FONT_Y_MAX_BOUNDS_BIT_NV: return "GL_FONT_Y_MAX_BOUNDS_BIT_NV";
		case GL_FONT_UNITS_PER_EM_BIT_NV: return "GL_FONT_UNITS_PER_EM_BIT_NV";
		case GL_FONT_ASCENDER_BIT_NV: return "GL_FONT_ASCENDER_BIT_NV";
		case GL_FONT_DESCENDER_BIT_NV: return "GL_FONT_DESCENDER_BIT_NV";
		case GL_FONT_HEIGHT_BIT_NV: return "GL_FONT_HEIGHT_BIT_NV";
		case GL_FONT_MAX_ADVANCE_WIDTH_BIT_NV: return "GL_FONT_MAX_ADVANCE_WIDTH_BIT_NV";
		case GL_FONT_MAX_ADVANCE_HEIGHT_BIT_NV: return "GL_FONT_MAX_ADVANCE_HEIGHT_BIT_NV";
		case GL_FONT_UNDERLINE_POSITION_BIT_NV: return "GL_FONT_UNDERLINE_POSITION_BIT_NV";
		case GL_FONT_UNDERLINE_THICKNESS_BIT_NV: return "GL_FONT_UNDERLINE_THICKNESS_BIT_NV";
		case GL_FONT_HAS_KERNING_BIT_NV: return "GL_FONT_HAS_KERNING_BIT_NV";
		case GL_ROUNDED_RECT_NV: return "GL_ROUNDED_RECT_NV";
		case GL_RELATIVE_ROUNDED_RECT_NV: return "GL_RELATIVE_ROUNDED_RECT_NV";
		case GL_ROUNDED_RECT2_NV: return "GL_ROUNDED_RECT2_NV";
		case GL_RELATIVE_ROUNDED_RECT2_NV: return "GL_RELATIVE_ROUNDED_RECT2_NV";
		case GL_ROUNDED_RECT4_NV: return "GL_ROUNDED_RECT4_NV";
		case GL_RELATIVE_ROUNDED_RECT4_NV: return "GL_RELATIVE_ROUNDED_RECT4_NV";
		case GL_ROUNDED_RECT8_NV: return "GL_ROUNDED_RECT8_NV";
		case GL_RELATIVE_ROUNDED_RECT8_NV: return "GL_RELATIVE_ROUNDED_RECT8_NV";
		case GL_RELATIVE_RECT_NV: return "GL_RELATIVE_RECT_NV";
		case GL_FONT_GLYPHS_AVAILABLE_NV: return "GL_FONT_GLYPHS_AVAILABLE_NV";
		case GL_FONT_TARGET_UNAVAILABLE_NV: return "GL_FONT_TARGET_UNAVAILABLE_NV";
		case GL_FONT_UNAVAILABLE_NV: return "GL_FONT_UNAVAILABLE_NV";
		case GL_FONT_UNINTELLIGIBLE_NV: return "GL_FONT_UNINTELLIGIBLE_NV";
		case GL_CONIC_CURVE_TO_NV: return "GL_CONIC_CURVE_TO_NV";
		case GL_RELATIVE_CONIC_CURVE_TO_NV: return "GL_RELATIVE_CONIC_CURVE_TO_NV";
		case GL_FONT_NUM_GLYPH_INDICES_BIT_NV: return "GL_FONT_NUM_GLYPH_INDICES_BIT_NV";
		case GL_STANDARD_FONT_FORMAT_NV: return "GL_STANDARD_FONT_FORMAT_NV";
		case GL_PATH_PROJECTION_NV: return "GL_PATH_PROJECTION_NV";
		case GL_PATH_MODELVIEW_NV: return "GL_PATH_MODELVIEW_NV";
		case GL_PATH_MODELVIEW_STACK_DEPTH_NV: return "GL_PATH_MODELVIEW_STACK_DEPTH_NV";
		case GL_PATH_MODELVIEW_MATRIX_NV: return "GL_PATH_MODELVIEW_MATRIX_NV";
		case GL_PATH_MAX_MODELVIEW_STACK_DEPTH_NV: return "GL_PATH_MAX_MODELVIEW_STACK_DEPTH_NV";
		case GL_PATH_TRANSPOSE_MODELVIEW_MATRIX_NV: return "GL_PATH_TRANSPOSE_MODELVIEW_MATRIX_NV";
		case GL_PATH_PROJECTION_STACK_DEPTH_NV: return "GL_PATH_PROJECTION_STACK_DEPTH_NV";
		case GL_PATH_PROJECTION_MATRIX_NV: return "GL_PATH_PROJECTION_MATRIX_NV";
		case GL_PATH_MAX_PROJECTION_STACK_DEPTH_NV: return "GL_PATH_MAX_PROJECTION_STACK_DEPTH_NV";
		case GL_PATH_TRANSPOSE_PROJECTION_MATRIX_NV: return "GL_PATH_TRANSPOSE_PROJECTION_MATRIX_NV";
		case GL_FRAGMENT_INPUT_NV: return "GL_FRAGMENT_INPUT_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_path_rendering_shared_edge */
const char* gl::is_define_nv_path_rendering_shared_edge(GLenum pname)
{
	switch(pname)
	{
		case GL_SHARED_EDGE_NV: return "GL_SHARED_EDGE_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_polygon_mode */
const char* gl::is_define_nv_polygon_mode(GLenum pname)
{
	switch(pname)
	{
		case GL_POLYGON_MODE_NV: return "GL_POLYGON_MODE_NV";
		case GL_POLYGON_OFFSET_POINT_NV: return "GL_POLYGON_OFFSET_POINT_NV";
		case GL_POLYGON_OFFSET_LINE_NV: return "GL_POLYGON_OFFSET_LINE_NV";
		case GL_POINT_NV: return "GL_POINT_NV";
		case GL_LINE_NV: return "GL_LINE_NV";
		case GL_FILL_NV: return "GL_FILL_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_read_buffer */
const char* gl::is_define_nv_read_buffer(GLenum pname)
{
	switch(pname)
	{
		case GL_READ_BUFFER_NV: return "GL_READ_BUFFER_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_read_buffer_front */
const char* gl::is_define_nv_read_buffer_front(GLenum pname)
{

	return nullptr;
}

/** extensions GL_NV_read_depth */
const char* gl::is_define_nv_read_depth(GLenum pname)
{

	return nullptr;
}

/** extensions GL_NV_read_depth_stencil */
const char* gl::is_define_nv_read_depth_stencil(GLenum pname)
{

	return nullptr;
}

/** extensions GL_NV_read_stencil */
const char* gl::is_define_nv_read_stencil(GLenum pname)
{

	return nullptr;
}

/** extensions GL_NV_sRGB_formats */
const char* gl::is_define_nv_srgb_formats(GLenum pname)
{
	switch(pname)
	{
		case GL_SLUMINANCE_NV: return "GL_SLUMINANCE_NV";
		case GL_SLUMINANCE_ALPHA_NV: return "GL_SLUMINANCE_ALPHA_NV";
		case GL_SRGB8_NV: return "GL_SRGB8_NV";
		case GL_SLUMINANCE8_NV: return "GL_SLUMINANCE8_NV";
		case GL_SLUMINANCE8_ALPHA8_NV: return "GL_SLUMINANCE8_ALPHA8_NV";
		case GL_COMPRESSED_SRGB_S3TC_DXT1_NV: return "GL_COMPRESSED_SRGB_S3TC_DXT1_NV";
		case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_NV: return "GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_NV";
		case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_NV: return "GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_NV";
		case GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_NV: return "GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_NV";
		case GL_ETC1_SRGB8_NV: return "GL_ETC1_SRGB8_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_sample_locations */
const char* gl::is_define_nv_sample_locations(GLenum pname)
{
	switch(pname)
	{
		case GL_SAMPLE_LOCATION_SUBPIXEL_BITS_NV: return "GL_SAMPLE_LOCATION_SUBPIXEL_BITS_NV";
		case GL_SAMPLE_LOCATION_PIXEL_GRID_WIDTH_NV: return "GL_SAMPLE_LOCATION_PIXEL_GRID_WIDTH_NV";
		case GL_SAMPLE_LOCATION_PIXEL_GRID_HEIGHT_NV: return "GL_SAMPLE_LOCATION_PIXEL_GRID_HEIGHT_NV";
		case GL_PROGRAMMABLE_SAMPLE_LOCATION_TABLE_SIZE_NV: return "GL_PROGRAMMABLE_SAMPLE_LOCATION_TABLE_SIZE_NV";
		case GL_SAMPLE_LOCATION_NV: return "GL_SAMPLE_LOCATION_NV";
		case GL_PROGRAMMABLE_SAMPLE_LOCATION_NV: return "GL_PROGRAMMABLE_SAMPLE_LOCATION_NV";
		case GL_FRAMEBUFFER_PROGRAMMABLE_SAMPLE_LOCATIONS_NV: return "GL_FRAMEBUFFER_PROGRAMMABLE_SAMPLE_LOCATIONS_NV";
		case GL_FRAMEBUFFER_SAMPLE_LOCATION_PIXEL_GRID_NV: return "GL_FRAMEBUFFER_SAMPLE_LOCATION_PIXEL_GRID_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_sample_mask_override_coverage */
const char* gl::is_define_nv_sample_mask_override_coverage(GLenum pname)
{

	return nullptr;
}

/** extensions GL_NV_shader_noperspective_interpolation */
const char* gl::is_define_nv_shader_noperspective_interpolation(GLenum pname)
{

	return nullptr;
}

/** extensions GL_NV_shadow_samplers_array */
const char* gl::is_define_nv_shadow_samplers_array(GLenum pname)
{
	switch(pname)
	{
		case GL_SAMPLER_2D_ARRAY_SHADOW_NV: return "GL_SAMPLER_2D_ARRAY_SHADOW_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_shadow_samplers_cube */
const char* gl::is_define_nv_shadow_samplers_cube(GLenum pname)
{
	switch(pname)
	{
		case GL_SAMPLER_CUBE_SHADOW_NV: return "GL_SAMPLER_CUBE_SHADOW_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_texture_border_clamp */
const char* gl::is_define_nv_texture_border_clamp(GLenum pname)
{
	switch(pname)
	{
		case GL_TEXTURE_BORDER_COLOR_NV: return "GL_TEXTURE_BORDER_COLOR_NV";
		case GL_CLAMP_TO_BORDER_NV: return "GL_CLAMP_TO_BORDER_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_texture_compression_s3tc_update */
const char* gl::is_define_nv_texture_compression_s3tc_update(GLenum pname)
{

	return nullptr;
}

/** extensions GL_NV_texture_npot_2D_mipmap */
const char* gl::is_define_nv_texture_npot_2d_mipmap(GLenum pname)
{

	return nullptr;
}

/** extensions GL_NV_viewport_array */
const char* gl::is_define_nv_viewport_array(GLenum pname)
{
	switch(pname)
	{
		case GL_MAX_VIEWPORTS_NV: return "GL_MAX_VIEWPORTS_NV";
		case GL_VIEWPORT_SUBPIXEL_BITS_NV: return "GL_VIEWPORT_SUBPIXEL_BITS_NV";
		case GL_VIEWPORT_BOUNDS_RANGE_NV: return "GL_VIEWPORT_BOUNDS_RANGE_NV";
		case GL_VIEWPORT_INDEX_PROVOKING_VERTEX_NV: return "GL_VIEWPORT_INDEX_PROVOKING_VERTEX_NV";
	} // switch
	return nullptr;
}

/** extensions GL_NV_viewport_array2 */
const char* gl::is_define_nv_viewport_array2(GLenum pname)
{

	return nullptr;
}

/** extensions GL_OVR_multiview */
const char* gl::is_define_ovr_multiview(GLenum pname)
{
	switch(pname)
	{
		case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR: return "GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR";
		case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR: return "GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR";
		case GL_MAX_VIEWS_OVR: return "GL_MAX_VIEWS_OVR";
	} // switch
	return nullptr;
}

/** extensions GL_OVR_multiview2 */
const char* gl::is_define_ovr_multiview2(GLenum pname)
{

	return nullptr;
}

/** extensions GL_OVR_multiview_multisampled_render_to_texture */
const char* gl::is_define_ovr_multiview_multisampled_render_to_texture(GLenum pname)
{

	return nullptr;
}

/** extensions GL_QCOM_alpha_test */
const char* gl::is_define_qcom_alpha_test(GLenum pname)
{
	switch(pname)
	{
		case GL_ALPHA_TEST_QCOM: return "GL_ALPHA_TEST_QCOM";
		case GL_ALPHA_TEST_FUNC_QCOM: return "GL_ALPHA_TEST_FUNC_QCOM";
		case GL_ALPHA_TEST_REF_QCOM: return "GL_ALPHA_TEST_REF_QCOM";
	} // switch
	return nullptr;
}

/** extensions GL_QCOM_binning_control */
const char* gl::is_define_qcom_binning_control(GLenum pname)
{
	switch(pname)
	{
		case GL_BINNING_CONTROL_HINT_QCOM: return "GL_BINNING_CONTROL_HINT_QCOM";
		case GL_CPU_OPTIMIZED_QCOM: return "GL_CPU_OPTIMIZED_QCOM";
		case GL_GPU_OPTIMIZED_QCOM: return "GL_GPU_OPTIMIZED_QCOM";
		case GL_RENDER_DIRECT_TO_FRAMEBUFFER_QCOM: return "GL_RENDER_DIRECT_TO_FRAMEBUFFER_QCOM";
	} // switch
	return nullptr;
}

/** extensions GL_QCOM_driver_control */
const char* gl::is_define_qcom_driver_control(GLenum pname)
{

	return nullptr;
}

/** extensions GL_QCOM_extended_get */
const char* gl::is_define_qcom_extended_get(GLenum pname)
{
	switch(pname)
	{
		case GL_TEXTURE_WIDTH_QCOM: return "GL_TEXTURE_WIDTH_QCOM";
		case GL_TEXTURE_HEIGHT_QCOM: return "GL_TEXTURE_HEIGHT_QCOM";
		case GL_TEXTURE_DEPTH_QCOM: return "GL_TEXTURE_DEPTH_QCOM";
		case GL_TEXTURE_INTERNAL_FORMAT_QCOM: return "GL_TEXTURE_INTERNAL_FORMAT_QCOM";
		case GL_TEXTURE_FORMAT_QCOM: return "GL_TEXTURE_FORMAT_QCOM";
		case GL_TEXTURE_TYPE_QCOM: return "GL_TEXTURE_TYPE_QCOM";
		case GL_TEXTURE_IMAGE_VALID_QCOM: return "GL_TEXTURE_IMAGE_VALID_QCOM";
		case GL_TEXTURE_NUM_LEVELS_QCOM: return "GL_TEXTURE_NUM_LEVELS_QCOM";
		case GL_TEXTURE_TARGET_QCOM: return "GL_TEXTURE_TARGET_QCOM";
		case GL_TEXTURE_OBJECT_VALID_QCOM: return "GL_TEXTURE_OBJECT_VALID_QCOM";
		case GL_STATE_RESTORE: return "GL_STATE_RESTORE";
	} // switch
	return nullptr;
}

/** extensions GL_QCOM_extended_get2 */
const char* gl::is_define_qcom_extended_get2(GLenum pname)
{

	return nullptr;
}

/** extensions GL_QCOM_perfmon_global_mode */
const char* gl::is_define_qcom_perfmon_global_mode(GLenum pname)
{
	switch(pname)
	{
		case GL_PERFMON_GLOBAL_MODE_QCOM: return "GL_PERFMON_GLOBAL_MODE_QCOM";
	} // switch
	return nullptr;
}

/** extensions GL_QCOM_tiled_rendering */
const char* gl::is_define_qcom_tiled_rendering(GLenum pname)
{
	switch(pname)
	{
		case GL_COLOR_BUFFER_BIT0_QCOM: return "GL_COLOR_BUFFER_BIT0_QCOM";
		case GL_COLOR_BUFFER_BIT1_QCOM: return "GL_COLOR_BUFFER_BIT1_QCOM";
		case GL_COLOR_BUFFER_BIT2_QCOM: return "GL_COLOR_BUFFER_BIT2_QCOM";
		case GL_COLOR_BUFFER_BIT3_QCOM: return "GL_COLOR_BUFFER_BIT3_QCOM";
		case GL_COLOR_BUFFER_BIT4_QCOM: return "GL_COLOR_BUFFER_BIT4_QCOM";
		case GL_COLOR_BUFFER_BIT5_QCOM: return "GL_COLOR_BUFFER_BIT5_QCOM";
		case GL_COLOR_BUFFER_BIT6_QCOM: return "GL_COLOR_BUFFER_BIT6_QCOM";
		case GL_COLOR_BUFFER_BIT7_QCOM: return "GL_COLOR_BUFFER_BIT7_QCOM";
		case GL_DEPTH_BUFFER_BIT0_QCOM: return "GL_DEPTH_BUFFER_BIT0_QCOM";
		case GL_DEPTH_BUFFER_BIT1_QCOM: return "GL_DEPTH_BUFFER_BIT1_QCOM";
		case GL_DEPTH_BUFFER_BIT2_QCOM: return "GL_DEPTH_BUFFER_BIT2_QCOM";
		case GL_DEPTH_BUFFER_BIT3_QCOM: return "GL_DEPTH_BUFFER_BIT3_QCOM";
		case GL_DEPTH_BUFFER_BIT4_QCOM: return "GL_DEPTH_BUFFER_BIT4_QCOM";
		case GL_DEPTH_BUFFER_BIT5_QCOM: return "GL_DEPTH_BUFFER_BIT5_QCOM";
		case GL_DEPTH_BUFFER_BIT6_QCOM: return "GL_DEPTH_BUFFER_BIT6_QCOM";
		case GL_DEPTH_BUFFER_BIT7_QCOM: return "GL_DEPTH_BUFFER_BIT7_QCOM";
		case GL_STENCIL_BUFFER_BIT0_QCOM: return "GL_STENCIL_BUFFER_BIT0_QCOM";
		case GL_STENCIL_BUFFER_BIT1_QCOM: return "GL_STENCIL_BUFFER_BIT1_QCOM";
		case GL_STENCIL_BUFFER_BIT2_QCOM: return "GL_STENCIL_BUFFER_BIT2_QCOM";
		case GL_STENCIL_BUFFER_BIT3_QCOM: return "GL_STENCIL_BUFFER_BIT3_QCOM";
		case GL_STENCIL_BUFFER_BIT4_QCOM: return "GL_STENCIL_BUFFER_BIT4_QCOM";
		case GL_STENCIL_BUFFER_BIT5_QCOM: return "GL_STENCIL_BUFFER_BIT5_QCOM";
		case GL_STENCIL_BUFFER_BIT6_QCOM: return "GL_STENCIL_BUFFER_BIT6_QCOM";
		case GL_STENCIL_BUFFER_BIT7_QCOM: return "GL_STENCIL_BUFFER_BIT7_QCOM";
		case GL_MULTISAMPLE_BUFFER_BIT0_QCOM: return "GL_MULTISAMPLE_BUFFER_BIT0_QCOM";
		case GL_MULTISAMPLE_BUFFER_BIT1_QCOM: return "GL_MULTISAMPLE_BUFFER_BIT1_QCOM";
		case GL_MULTISAMPLE_BUFFER_BIT2_QCOM: return "GL_MULTISAMPLE_BUFFER_BIT2_QCOM";
		case GL_MULTISAMPLE_BUFFER_BIT3_QCOM: return "GL_MULTISAMPLE_BUFFER_BIT3_QCOM";
		case GL_MULTISAMPLE_BUFFER_BIT4_QCOM: return "GL_MULTISAMPLE_BUFFER_BIT4_QCOM";
		case GL_MULTISAMPLE_BUFFER_BIT5_QCOM: return "GL_MULTISAMPLE_BUFFER_BIT5_QCOM";
		case GL_MULTISAMPLE_BUFFER_BIT6_QCOM: return "GL_MULTISAMPLE_BUFFER_BIT6_QCOM";
		case GL_MULTISAMPLE_BUFFER_BIT7_QCOM: return "GL_MULTISAMPLE_BUFFER_BIT7_QCOM";
	} // switch
	return nullptr;
}

/** extensions GL_QCOM_writeonly_rendering */
const char* gl::is_define_qcom_writeonly_rendering(GLenum pname)
{
	switch(pname)
	{
		case GL_WRITEONLY_RENDERING_QCOM: return "GL_WRITEONLY_RENDERING_QCOM";
	} // switch
	return nullptr;
}

/** extensions GL_VIV_shader_binary */
const char* gl::is_define_viv_shader_binary(GLenum pname)
{
	switch(pname)
	{
		case GL_SHADER_BINARY_VIV: return "GL_SHADER_BINARY_VIV";
	} // switch
	return nullptr;
}

// ---------------------------------------------------------------------

const char* gl::egl::is_define_egl_h(GLenum pname)
{
	const char* ret;

	ret = is_define_version_1_0(pname); if (ret) return ret;
	ret = is_define_egl_version_1_1(pname); if (ret) return ret;
	ret = is_define_egl_version_1_2(pname); if (ret) return ret;
	ret = is_define_egl_version_1_3(pname); if (ret) return ret;
	ret = is_define_egl_version_1_4(pname); if (ret) return ret;
	ret = is_define_egl_version_1_5(pname); if (ret) return ret;
	ret = is_define_eglext_h(pname);

	return ret;
}

/** extensions EGL_VERSION_1_0 */
const char* gl::egl::is_define_version_1_0(GLenum pname)
{
	switch(pname)
	{
		case EGL_ALPHA_SIZE: return "EGL_ALPHA_SIZE";
		case EGL_BAD_ACCESS: return "EGL_BAD_ACCESS";
		case EGL_BAD_ALLOC: return "EGL_BAD_ALLOC";
		case EGL_BAD_ATTRIBUTE: return "EGL_BAD_ATTRIBUTE";
		case EGL_BAD_CONFIG: return "EGL_BAD_CONFIG";
		case EGL_BAD_CONTEXT: return "EGL_BAD_CONTEXT";
		case EGL_BAD_CURRENT_SURFACE: return "EGL_BAD_CURRENT_SURFACE";
		case EGL_BAD_DISPLAY: return "EGL_BAD_DISPLAY";
		case EGL_BAD_MATCH: return "EGL_BAD_MATCH";
		case EGL_BAD_NATIVE_PIXMAP: return "EGL_BAD_NATIVE_PIXMAP";
		case EGL_BAD_NATIVE_WINDOW: return "EGL_BAD_NATIVE_WINDOW";
		case EGL_BAD_PARAMETER: return "EGL_BAD_PARAMETER";
		case EGL_BAD_SURFACE: return "EGL_BAD_SURFACE";
		case EGL_BLUE_SIZE: return "EGL_BLUE_SIZE";
		case EGL_BUFFER_SIZE: return "EGL_BUFFER_SIZE";
		case EGL_CONFIG_CAVEAT: return "EGL_CONFIG_CAVEAT";
		case EGL_CONFIG_ID: return "EGL_CONFIG_ID";
		case EGL_CORE_NATIVE_ENGINE: return "EGL_CORE_NATIVE_ENGINE";
		case EGL_DEPTH_SIZE: return "EGL_DEPTH_SIZE";
		case EGL_DRAW: return "EGL_DRAW";
		case EGL_EXTENSIONS: return "EGL_EXTENSIONS";
		case EGL_GREEN_SIZE: return "EGL_GREEN_SIZE";
		case EGL_HEIGHT: return "EGL_HEIGHT";
		case EGL_LARGEST_PBUFFER: return "EGL_LARGEST_PBUFFER";
		case EGL_LEVEL: return "EGL_LEVEL";
		case EGL_MAX_PBUFFER_HEIGHT: return "EGL_MAX_PBUFFER_HEIGHT";
		case EGL_MAX_PBUFFER_PIXELS: return "EGL_MAX_PBUFFER_PIXELS";
		case EGL_MAX_PBUFFER_WIDTH: return "EGL_MAX_PBUFFER_WIDTH";
		case EGL_NATIVE_RENDERABLE: return "EGL_NATIVE_RENDERABLE";
		case EGL_NATIVE_VISUAL_ID: return "EGL_NATIVE_VISUAL_ID";
		case EGL_NATIVE_VISUAL_TYPE: return "EGL_NATIVE_VISUAL_TYPE";
		case EGL_NONE: return "EGL_NONE";
		case EGL_NON_CONFORMANT_CONFIG: return "EGL_NON_CONFORMANT_CONFIG";
		case EGL_NOT_INITIALIZED: return "EGL_NOT_INITIALIZED";
		case EGL_PBUFFER_BIT: return "EGL_PBUFFER_BIT";
		case EGL_PIXMAP_BIT: return "EGL_PIXMAP_BIT";
		case EGL_READ: return "EGL_READ";
		case EGL_RED_SIZE: return "EGL_RED_SIZE";
		case EGL_SAMPLES: return "EGL_SAMPLES";
		case EGL_SAMPLE_BUFFERS: return "EGL_SAMPLE_BUFFERS";
		case EGL_SLOW_CONFIG: return "EGL_SLOW_CONFIG";
		case EGL_STENCIL_SIZE: return "EGL_STENCIL_SIZE";
		case EGL_SUCCESS: return "EGL_SUCCESS";
		case EGL_SURFACE_TYPE: return "EGL_SURFACE_TYPE";
		case EGL_TRANSPARENT_BLUE_VALUE: return "EGL_TRANSPARENT_BLUE_VALUE";
		case EGL_TRANSPARENT_GREEN_VALUE: return "EGL_TRANSPARENT_GREEN_VALUE";
		case EGL_TRANSPARENT_RED_VALUE: return "EGL_TRANSPARENT_RED_VALUE";
		case EGL_TRANSPARENT_RGB: return "EGL_TRANSPARENT_RGB";
		case EGL_TRANSPARENT_TYPE: return "EGL_TRANSPARENT_TYPE";

		case EGL_VENDOR: return "EGL_VENDOR";
		case EGL_VERSION: return "EGL_VERSION";
		case EGL_WIDTH: return "EGL_WIDTH";
		case EGL_WINDOW_BIT: return "EGL_WINDOW_BIT";
	} // switch
	return nullptr;
}

/** EGL_VERSION_1_1 */
const char* gl::egl::is_define_egl_version_1_1(GLenum pname)
{
	switch(pname)
	{
		case EGL_BACK_BUFFER: return "EGL_BACK_BUFFER";
		case EGL_BIND_TO_TEXTURE_RGB: return "EGL_BIND_TO_TEXTURE_RGB";
		case EGL_BIND_TO_TEXTURE_RGBA: return "EGL_BIND_TO_TEXTURE_RGBA";
		case EGL_CONTEXT_LOST: return "EGL_CONTEXT_LOST";
		case EGL_MIN_SWAP_INTERVAL: return "EGL_MIN_SWAP_INTERVAL";
		case EGL_MAX_SWAP_INTERVAL: return "EGL_MAX_SWAP_INTERVAL";
		case EGL_MIPMAP_TEXTURE: return "EGL_MIPMAP_TEXTURE";
		case EGL_MIPMAP_LEVEL: return "EGL_MIPMAP_LEVEL";
		case EGL_NO_TEXTURE: return "EGL_NO_TEXTURE";
		case EGL_TEXTURE_2D: return "EGL_TEXTURE_2D";
		case EGL_TEXTURE_FORMAT: return "EGL_TEXTURE_FORMAT";
		case EGL_TEXTURE_RGB: return "EGL_TEXTURE_RGB";
		case EGL_TEXTURE_RGBA: return "EGL_TEXTURE_RGBA";
		case EGL_TEXTURE_TARGET: return "EGL_TEXTURE_TARGET";
	} // switch
	return nullptr;
}

/** EGL_VERSION_1_2 */
const char* gl::egl::is_define_egl_version_1_2(GLenum pname)
{
	switch(pname)
	{
		case EGL_ALPHA_FORMAT: return "EGL_ALPHA_FORMAT";
		case EGL_ALPHA_FORMAT_NONPRE: return "EGL_ALPHA_FORMAT_NONPRE";
		case EGL_ALPHA_FORMAT_PRE: return "EGL_ALPHA_FORMAT_PRE";
		case EGL_ALPHA_MASK_SIZE: return "EGL_ALPHA_MASK_SIZE";
		case EGL_BUFFER_PRESERVED: return "EGL_BUFFER_PRESERVED";
		case EGL_BUFFER_DESTROYED: return "EGL_BUFFER_DESTROYED";
		case EGL_CLIENT_APIS: return "EGL_CLIENT_APIS";
		case EGL_COLORSPACE: return "EGL_COLORSPACE";
		case EGL_COLORSPACE_sRGB: return "EGL_COLORSPACE_sRGB";
		case EGL_COLORSPACE_LINEAR: return "EGL_COLORSPACE_LINEAR";
		case EGL_COLOR_BUFFER_TYPE: return "EGL_COLOR_BUFFER_TYPE";
		case EGL_CONTEXT_CLIENT_TYPE: return "EGL_CONTEXT_CLIENT_TYPE";
		case EGL_HORIZONTAL_RESOLUTION: return "EGL_HORIZONTAL_RESOLUTION";
		case EGL_LUMINANCE_BUFFER: return "EGL_LUMINANCE_BUFFER";
		case EGL_LUMINANCE_SIZE: return "EGL_LUMINANCE_SIZE";
		case EGL_OPENGL_ES_BIT: return "EGL_OPENGL_ES_BIT";
		case EGL_OPENVG_BIT: return "EGL_OPENVG_BIT";
		case EGL_OPENGL_ES_API: return "EGL_OPENGL_ES_API";
		case EGL_OPENVG_API: return "EGL_OPENVG_API";
		case EGL_OPENVG_IMAGE: return "EGL_OPENVG_IMAGE";
		case EGL_PIXEL_ASPECT_RATIO: return "EGL_PIXEL_ASPECT_RATIO";
		case EGL_RENDERABLE_TYPE: return "EGL_RENDERABLE_TYPE";
		case EGL_RENDER_BUFFER: return "EGL_RENDER_BUFFER";
		case EGL_RGB_BUFFER: return "EGL_RGB_BUFFER";
		case EGL_SINGLE_BUFFER: return "EGL_SINGLE_BUFFER";
		case EGL_SWAP_BEHAVIOR: return "EGL_SWAP_BEHAVIOR";
		case EGL_VERTICAL_RESOLUTION: return "EGL_VERTICAL_RESOLUTION";
	} // switch
	return nullptr;
}

/** EGL_VERSION_1_3 */
const char* gl::egl::is_define_egl_version_1_3(GLenum pname)
{
	switch(pname)
	{
		case EGL_CONFORMANT: return "EGL_CONFORMANT";
		case EGL_CONTEXT_CLIENT_VERSION: return "EGL_CONTEXT_CLIENT_VERSION";
		case EGL_MATCH_NATIVE_PIXMAP: return "EGL_MATCH_NATIVE_PIXMAP";
		case EGL_OPENGL_ES2_BIT: return "EGL_OPENGL_ES2_BIT";
		case EGL_VG_ALPHA_FORMAT: return "EGL_VG_ALPHA_FORMAT";
		case EGL_VG_ALPHA_FORMAT_NONPRE: return "EGL_VG_ALPHA_FORMAT_NONPRE";
		case EGL_VG_ALPHA_FORMAT_PRE: return "EGL_VG_ALPHA_FORMAT_PRE";
		case EGL_VG_ALPHA_FORMAT_PRE_BIT: return "EGL_VG_ALPHA_FORMAT_PRE_BIT";
		case EGL_VG_COLORSPACE: return "EGL_VG_COLORSPACE";
		case EGL_VG_COLORSPACE_sRGB: return "EGL_VG_COLORSPACE_sRGB";
		case EGL_VG_COLORSPACE_LINEAR: return "EGL_VG_COLORSPACE_LINEAR";
		case EGL_VG_COLORSPACE_LINEAR_BIT: return "EGL_VG_COLORSPACE_LINEAR_BIT";
	} // switch
	return nullptr;
}

/** EGL_VERSION_1_4 */
const char* gl::egl::is_define_egl_version_1_4(GLenum pname)
{
	switch(pname)
	{
		case EGL_MULTISAMPLE_RESOLVE_BOX_BIT: return "EGL_MULTISAMPLE_RESOLVE_BOX_BIT";
		case EGL_MULTISAMPLE_RESOLVE: return "EGL_MULTISAMPLE_RESOLVE";
		case EGL_MULTISAMPLE_RESOLVE_DEFAULT: return "EGL_MULTISAMPLE_RESOLVE_DEFAULT";
		case EGL_MULTISAMPLE_RESOLVE_BOX: return "EGL_MULTISAMPLE_RESOLVE_BOX";
		case EGL_OPENGL_API: return "EGL_OPENGL_API";
		case EGL_OPENGL_BIT: return "EGL_OPENGL_BIT";
		case EGL_SWAP_BEHAVIOR_PRESERVED_BIT: return "EGL_SWAP_BEHAVIOR_PRESERVED_BIT";
	} // switch
	return nullptr;
}

/** EGL_VERSION_1_5 */
const char* gl::egl::is_define_egl_version_1_5(GLenum pname)
{
	switch(pname)
	{
		case EGL_CONTEXT_MAJOR_VERSION: return "EGL_CONTEXT_MAJOR_VERSION";
		case EGL_CONTEXT_MINOR_VERSION: return "EGL_CONTEXT_MINOR_VERSION";
		case EGL_CONTEXT_OPENGL_PROFILE_MASK: return "EGL_CONTEXT_OPENGL_PROFILE_MASK";
		case EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY: return "EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY";
		case EGL_NO_RESET_NOTIFICATION: return "EGL_NO_RESET_NOTIFICATION";
		case EGL_LOSE_CONTEXT_ON_RESET: return "EGL_LOSE_CONTEXT_ON_RESET";
		case EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT: return "EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT";
		case EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT: return "EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT";
		case EGL_CONTEXT_OPENGL_DEBUG: return "EGL_CONTEXT_OPENGL_DEBUG";
		case EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE: return "EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE";
		case EGL_CONTEXT_OPENGL_ROBUST_ACCESS: return "EGL_CONTEXT_OPENGL_ROBUST_ACCESS";
		case EGL_OPENGL_ES3_BIT: return "EGL_OPENGL_ES3_BIT";
		case EGL_CL_EVENT_HANDLE: return "EGL_CL_EVENT_HANDLE";
		case EGL_SYNC_CL_EVENT: return "EGL_SYNC_CL_EVENT";
		case EGL_SYNC_CL_EVENT_COMPLETE: return "EGL_SYNC_CL_EVENT_COMPLETE";
		case EGL_SYNC_PRIOR_COMMANDS_COMPLETE: return "EGL_SYNC_PRIOR_COMMANDS_COMPLETE";
		case EGL_SYNC_TYPE: return "EGL_SYNC_TYPE";
		case EGL_SYNC_STATUS: return "EGL_SYNC_STATUS";
		case EGL_SYNC_CONDITION: return "EGL_SYNC_CONDITION";
		case EGL_SIGNALED: return "EGL_SIGNALED";
		case EGL_UNSIGNALED: return "EGL_UNSIGNALED";
		//case EGL_SYNC_FLUSH_COMMANDS_BIT: return "EGL_SYNC_FLUSH_COMMANDS_BIT";
		//case EGL_FOREVER: return "EGL_FOREVER";
		case EGL_TIMEOUT_EXPIRED: return "EGL_TIMEOUT_EXPIRED";
		case EGL_CONDITION_SATISFIED: return "EGL_CONDITION_SATISFIED";
		case EGL_SYNC_FENCE: return "EGL_SYNC_FENCE";
		case EGL_GL_COLORSPACE: return "EGL_GL_COLORSPACE";
		case EGL_GL_COLORSPACE_SRGB: return "EGL_GL_COLORSPACE_SRGB";
		case EGL_GL_COLORSPACE_LINEAR: return "EGL_GL_COLORSPACE_LINEAR";
		case EGL_GL_RENDERBUFFER: return "EGL_GL_RENDERBUFFER";
		case EGL_GL_TEXTURE_2D: return "EGL_GL_TEXTURE_2D";
		case EGL_GL_TEXTURE_LEVEL: return "EGL_GL_TEXTURE_LEVEL";
		case EGL_GL_TEXTURE_3D: return "EGL_GL_TEXTURE_3D";
		case EGL_GL_TEXTURE_ZOFFSET: return "EGL_GL_TEXTURE_ZOFFSET";
		case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X: return "EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X";
		case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X: return "EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X";
		case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y: return "EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y";
		case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y: return "EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y";
		case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z: return "EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z";
		case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z: return "EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z";
		case EGL_IMAGE_PRESERVED: return "EGL_IMAGE_PRESERVED";
	} // switch
	return nullptr;
}

const char* gl::egl::is_define_eglext_h(GLenum pname)
{
	const char* ret;

	ret = is_define_khr_cl_event(pname); if(ret) return ret;
	//ret = is_define_khr_cl_event2(pname); if(ret) return ret;
	//ret = is_define_khr_client_get_all_proc_addresses(pname) if(ret) return ret;
	ret = is_define_khr_config_attribs(pname); if(ret) return ret;
	ret = is_define_khr_context_flush_control(pname); if(ret) return ret;
	ret = is_define_khr_create_context(pname); if(ret) return ret;
	ret = is_define_khr_create_context_no_error(pname); if(ret) return ret;
	ret = is_define_khr_debug(pname); if(ret) return ret;
	ret = is_define_khr_fence_sync(pname); if(ret) return ret;
	//ret = is_define_khr_get_all_proc_addresses(pname); if(ret) return ret;
	ret = is_define_khr_gl_colorspace(pname); if(ret) return ret;
	ret = is_define_khr_gl_renderbuffer_image(pname); if(ret) return ret;
	ret = is_define_khr_gl_texture_2d_image(pname); if(ret) return ret;
	ret = is_define_khr_gl_texture_3d_image(pname); if(ret) return ret;
	ret = is_define_khr_gl_texture_cubemap_image(pname); if(ret) return ret;
	ret = is_define_khr_image(pname); if(ret) return ret;
	ret = is_define_khr_image_base(pname); if(ret) return ret;
	//ret = is_define_khr_image_pixmap(pname); if(ret) return ret;
	ret = is_define_khr_lock_surface(pname); if(ret) return ret;
	ret = is_define_khr_lock_surface2(pname); if(ret) return ret;
	//ret = is_define_khr_lock_surface3(pname); if(ret) return ret;
	ret = is_define_khr_mutable_render_buffer(pname); if(ret) return ret;
	//ret = is_define_khr_no_config_context(pname); if(ret) return ret;
	ret = is_define_khr_partial_update(pname); if(ret) return ret;
	ret = is_define_khr_platform_android(pname); if(ret) return ret;
	ret = is_define_khr_platform_gbm(pname); if(ret) return ret;
	ret = is_define_khr_platform_wayland(pname); if(ret) return ret;
	ret = is_define_khr_platform_x11(pname); if(ret) return ret;
	ret = is_define_khr_reusable_sync(pname); if(ret) return ret;
	ret = is_define_khr_stream(pname); if(ret) return ret;
	//ret = is_define_khr_stream_attrib(pname); if(ret) return ret;
	ret = is_define_khr_stream_consumer_gltexture(pname); if(ret) return ret;
	//ret = is_define_khr_stream_cross_process_fd(pname); if(ret) return ret;
	ret = is_define_khr_stream_fifo(pname); if(ret) return ret;
	//ret = is_define_khr_stream_producer_aldatalocator(pname); if(ret) return ret;
	ret = is_define_khr_stream_producer_eglsurface(pname); if(ret) return ret;
	//ret = is_define_khr_surfaceless_context(pname); if(ret) return ret;
	//ret = is_define_khr_swap_buffers_with_damage(pname); if(ret) return ret;
	ret = is_define_khr_vg_parent_image(pname); if(ret) return ret;
	//ret = is_define_khr_wait_sync(pname); if(ret) return ret;
	//ret = is_define_android_blob_cache(pname); if(ret) return ret;
	ret = is_define_android_create_native_client_buffer(pname); if(ret) return ret;
	ret = is_define_android_framebuffer_target(pname); if(ret) return ret;
	ret = is_define_android_front_buffer_auto_refresh(pname); if(ret) return ret;
	ret = is_define_android_image_native_buffer(pname); if(ret) return ret;
	ret = is_define_android_native_fence_sync(pname); if(ret) return ret;
	//ret = is_define_android_presentation_time(pname); if(ret) return ret;
	ret = is_define_android_recordable(pname); if(ret) return ret;
	ret = is_define_angle_d3d_share_handle_client_buffer(pname); if(ret) return ret;
	ret = is_define_angle_device_d3d(pname); if(ret) return ret;
	//ret = is_define_angle_query_surface_pointer(pname); if(ret) return ret;{ return nullptr; }
	//ret = is_define_angle_surface_d3d_texture_2d_share_handle(pname); if(ret) return ret;{ return nullptr; }
	ret = is_define_angle_window_fixed_size(pname); if(ret) return ret;
	ret = is_define_arm_implicit_external_sync(pname); if(ret) return ret;
	ret = is_define_arm_pixmap_multisample_discard(pname); if(ret) return ret;
	ret = is_define_ext_buffer_age(pname); if(ret) return ret;
	//ret = is_define_ext_client_extensions(pname); if(ret) return ret;{ return nullptr; }
	ret = is_define_ext_create_context_robustness(pname); if(ret) return ret;
	ret = is_define_ext_device_base(pname); if(ret) return ret;
	ret = is_define_ext_device_drm(pname); if(ret) return ret;
	//ret = is_define_ext_device_enumeration(pname); if(ret) return ret;{ return nullptr; }
	ret = is_define_ext_device_openwf(pname); if(ret) return ret;
	//ret = is_define_ext_device_query(pname); if(ret) return ret;{ return nullptr; }
	ret = is_define_ext_image_dma_buf_import(pname); if(ret) return ret;
	ret = is_define_ext_multiview_window(pname); if(ret) return ret;
	ret = is_define_ext_output_base(pname); if(ret) return ret;
	ret = is_define_ext_output_drm(pname); if(ret) return ret;
	ret = is_define_ext_output_openwf(pname); if(ret) return ret;
	//ret = is_define_ext_platform_base(pname); if(ret) return ret;{ return nullptr; }
	ret = is_define_ext_platform_device(pname); if(ret) return ret;
	ret = is_define_ext_platform_wayland(pname); if(ret) return ret;
	ret = is_define_ext_platform_x11(pname); if(ret) return ret;
	ret = is_define_ext_protected_content(pname); if(ret) return ret;
	//ret = is_define_ext_protected_surface(pname); if(ret) return ret;{ return nullptr; }
	//ret = is_define_ext_stream_consumer_egloutput(pname); if(ret) return ret;{ return nullptr; }
	//ret = is_define_ext_swap_buffers_with_damage(pname); if(ret) return ret;{ return nullptr; }
	ret = is_define_ext_yuv_surface(pname); if(ret) return ret;
	ret = is_define_hi_clientpixmap(pname); if(ret) return ret;
	ret = is_define_hi_colorformats(pname); if(ret) return ret;
	ret = is_define_img_context_priority(pname); if(ret) return ret;
	ret = is_define_img_image_plane_attribs(pname); if(ret) return ret;
	ret = is_define_mesa_drm_image(pname); if(ret) return ret;
	//ret = is_define_mesa_image_dma_buf_export(pname); if(ret) return ret;{ return nullptr; }
	ret = is_define_mesa_platform_gbm(pname); if(ret) return ret;
	ret = is_define_mesa_platform_surfaceless(pname); if(ret) return ret;
	//ret = is_define_nok_swap_region(pname); if(ret) return ret;{ return nullptr; }
	//ret = is_define_nok_swap_region2(pname); if(ret) return ret;{ return nullptr; }
	ret = is_define_nok_texture_from_pixmap(pname); if(ret) return ret;
	ret = is_define_nv_3dvision_surface(pname); if(ret) return ret;
	ret = is_define_nv_coverage_sample(pname); if(ret) return ret;
	ret = is_define_nv_coverage_sample_resolve(pname); if(ret) return ret;
	ret = is_define_nv_cuda_event(pname); if(ret) return ret;
	ret = is_define_nv_depth_nonlinear(pname); if(ret) return ret;
	ret = is_define_nv_device_cuda(pname); if(ret) return ret;
	//ret = is_define_nv_native_query(pname); if(ret) return ret;
	//ret = is_define_nv_post_convert_rounding(pname); if(ret) return ret;
	ret = is_define_nv_post_sub_buffer(pname); if(ret) return ret;
	ret = is_define_nv_robustness_video_memory_purge(pname); if(ret) return ret;
	ret = is_define_nv_stream_consumer_gltexture_yuv(pname); if(ret) return ret;
	ret = is_define_nv_stream_metadata(pname); if(ret) return ret;
	ret = is_define_nv_stream_sync(pname); if(ret) return ret;
	ret = is_define_nv_sync(pname); if(ret) return ret;
	//ret = is_define_nv_system_time(pname); if(ret) return ret;
	ret = is_define_tizen_image_native_buffer(pname); if(ret) return ret;
	ret = is_define_tizen_image_native_surface(pname);

	return ret;
} // gl::is_define_eglext_h

/** extensions EGL_KHR_cl_event */
const char* gl::egl::is_define_khr_cl_event(GLenum pname)
{
	switch(pname)
	{
		case EGL_CL_EVENT_HANDLE_KHR: return "EGL_CL_EVENT_HANDLE_KHR";
		case EGL_SYNC_CL_EVENT_KHR: return "EGL_SYNC_CL_EVENT_KHR";
		case EGL_SYNC_CL_EVENT_COMPLETE_KHR: return "EGL_SYNC_CL_EVENT_COMPLETE_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_config_attribs */
const char* gl::egl::is_define_khr_config_attribs(GLenum pname)
{
	switch(pname)
	{
		case EGL_CONFORMANT_KHR: return "EGL_CONFORMANT_KHR";
		case EGL_VG_COLORSPACE_LINEAR_BIT_KHR: return "EGL_VG_COLORSPACE_LINEAR_BIT_KHR";
		case EGL_VG_ALPHA_FORMAT_PRE_BIT_KHR: return "EGL_VG_ALPHA_FORMAT_PRE_BIT_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_context_flush_control */
const char* gl::egl::is_define_khr_context_flush_control(GLenum pname)
{
	switch(pname)
	{
		case EGL_CONTEXT_RELEASE_BEHAVIOR_KHR: return "EGL_CONTEXT_RELEASE_BEHAVIOR_KHR";
		case EGL_CONTEXT_RELEASE_BEHAVIOR_FLUSH_KHR: return "EGL_CONTEXT_RELEASE_BEHAVIOR_FLUSH_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_create_context */
const char* gl::egl::is_define_khr_create_context(GLenum pname)
{
	switch(pname)
	{
		case EGL_CONTEXT_MAJOR_VERSION_KHR: return "EGL_CONTEXT_MAJOR_VERSION_KHR";
		case EGL_CONTEXT_MINOR_VERSION_KHR: return "EGL_CONTEXT_MINOR_VERSION_KHR";
		case EGL_CONTEXT_FLAGS_KHR: return "EGL_CONTEXT_FLAGS_KHR";
		case EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR: return "EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR";
		case EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR: return "EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_KHR";
		case EGL_NO_RESET_NOTIFICATION_KHR: return "EGL_NO_RESET_NOTIFICATION_KHR";
		case EGL_LOSE_CONTEXT_ON_RESET_KHR: return "EGL_LOSE_CONTEXT_ON_RESET_KHR";
		//case EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR: return "EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR";
		//case EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR: return "EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR";
		//case EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR: return "EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR";
		//case EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR: return "EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR";
		//case EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR: return "EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR";
		//case EGL_OPENGL_ES3_BIT_KHR: return "EGL_OPENGL_ES3_BIT_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_create_context_no_error */
const char* gl::egl::is_define_khr_create_context_no_error(GLenum pname)
{
	switch(pname)
	{
		case EGL_CONTEXT_OPENGL_NO_ERROR_KHR: return "EGL_CONTEXT_OPENGL_NO_ERROR_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_debug */
const char* gl::egl::is_define_khr_debug(GLenum pname)
{
	switch(pname)
	{
		case EGL_OBJECT_THREAD_KHR: return "EGL_OBJECT_THREAD_KHR";
		case EGL_OBJECT_DISPLAY_KHR: return "EGL_OBJECT_DISPLAY_KHR";
		case EGL_OBJECT_CONTEXT_KHR: return "EGL_OBJECT_CONTEXT_KHR";
		case EGL_OBJECT_SURFACE_KHR: return "EGL_OBJECT_SURFACE_KHR";
		case EGL_OBJECT_IMAGE_KHR: return "EGL_OBJECT_IMAGE_KHR";
		case EGL_OBJECT_SYNC_KHR: return "EGL_OBJECT_SYNC_KHR";
		case EGL_OBJECT_STREAM_KHR: return "EGL_OBJECT_STREAM_KHR";
		case EGL_DEBUG_MSG_CRITICAL_KHR: return "EGL_DEBUG_MSG_CRITICAL_KHR";
		case EGL_DEBUG_MSG_ERROR_KHR: return "EGL_DEBUG_MSG_ERROR_KHR";
		case EGL_DEBUG_MSG_WARN_KHR: return "EGL_DEBUG_MSG_WARN_KHR";
		case EGL_DEBUG_MSG_INFO_KHR: return "EGL_DEBUG_MSG_INFO_KHR";
		case EGL_DEBUG_CALLBACK_KHR: return "EGL_DEBUG_CALLBACK_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_fence_sync */
const char* gl::egl::is_define_khr_fence_sync(GLenum pname)
{
	switch(pname)
	{
		case EGL_SYNC_PRIOR_COMMANDS_COMPLETE_KHR: return "EGL_SYNC_PRIOR_COMMANDS_COMPLETE_KHR";
		case EGL_SYNC_CONDITION_KHR: return "EGL_SYNC_CONDITION_KHR";
		case EGL_SYNC_FENCE_KHR: return "EGL_SYNC_FENCE_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_gl_colorspace */
const char* gl::egl::is_define_khr_gl_colorspace(GLenum pname)
{
	switch(pname)
	{
		case EGL_GL_COLORSPACE_KHR: return "EGL_GL_COLORSPACE_KHR";
		case EGL_GL_COLORSPACE_SRGB_KHR: return "EGL_GL_COLORSPACE_SRGB_KHR";
		case EGL_GL_COLORSPACE_LINEAR_KHR: return "EGL_GL_COLORSPACE_LINEAR_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_gl_renderbuffer_image */
const char* gl::egl::is_define_khr_gl_renderbuffer_image(GLenum pname)
{
	switch(pname)
	{
		case EGL_GL_RENDERBUFFER_KHR: return "EGL_GL_RENDERBUFFER_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_gl_texture_2D_image */
const char* gl::egl::is_define_khr_gl_texture_2d_image(GLenum pname)
{
	switch(pname)
	{
		case EGL_GL_TEXTURE_2D_KHR: return "EGL_GL_TEXTURE_2D_KHR";
		case EGL_GL_TEXTURE_LEVEL_KHR: return "EGL_GL_TEXTURE_LEVEL_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_gl_texture_3D_image */
const char* gl::egl::is_define_khr_gl_texture_3d_image(GLenum pname)
{
	switch(pname)
	{
		case EGL_GL_TEXTURE_3D_KHR: return "EGL_GL_TEXTURE_3D_KHR";
		case EGL_GL_TEXTURE_ZOFFSET_KHR: return "EGL_GL_TEXTURE_ZOFFSET_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_gl_texture_cubemap_image */
const char* gl::egl::is_define_khr_gl_texture_cubemap_image(GLenum pname)
{
	switch(pname)
	{
		case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR: return "EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR";
		case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR: return "EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR";
		case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR: return "EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR";
		case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR: return "EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR";
		case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR: return "EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR";
		case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR: return "EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_image */
const char* gl::egl::is_define_khr_image(GLenum pname)
{
	switch(pname)
	{
		case EGL_NATIVE_PIXMAP_KHR: return "EGL_NATIVE_PIXMAP_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_image_base */
const char* gl::egl::is_define_khr_image_base(GLenum pname)
{
	switch(pname)
	{
		case EGL_IMAGE_PRESERVED_KHR: return "EGL_IMAGE_PRESERVED_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_lock_surface */
const char* gl::egl::is_define_khr_lock_surface(GLenum pname)
{
	switch(pname)
	{
		case EGL_READ_SURFACE_BIT_KHR: return "EGL_READ_SURFACE_BIT_KHR";
		case EGL_WRITE_SURFACE_BIT_KHR: return "EGL_WRITE_SURFACE_BIT_KHR";
		case EGL_LOCK_SURFACE_BIT_KHR: return "EGL_LOCK_SURFACE_BIT_KHR";
		case EGL_OPTIMAL_FORMAT_BIT_KHR: return "EGL_OPTIMAL_FORMAT_BIT_KHR";
		case EGL_MATCH_FORMAT_KHR: return "EGL_MATCH_FORMAT_KHR";
		case EGL_FORMAT_RGB_565_EXACT_KHR: return "EGL_FORMAT_RGB_565_EXACT_KHR";
		case EGL_FORMAT_RGB_565_KHR: return "EGL_FORMAT_RGB_565_KHR";
		case EGL_FORMAT_RGBA_8888_EXACT_KHR: return "EGL_FORMAT_RGBA_8888_EXACT_KHR";
		case EGL_FORMAT_RGBA_8888_KHR: return "EGL_FORMAT_RGBA_8888_KHR";
		case EGL_MAP_PRESERVE_PIXELS_KHR: return "EGL_MAP_PRESERVE_PIXELS_KHR";
		case EGL_LOCK_USAGE_HINT_KHR: return "EGL_LOCK_USAGE_HINT_KHR";
		case EGL_BITMAP_POINTER_KHR: return "EGL_BITMAP_POINTER_KHR";
		case EGL_BITMAP_PITCH_KHR: return "EGL_BITMAP_PITCH_KHR";
		case EGL_BITMAP_ORIGIN_KHR: return "EGL_BITMAP_ORIGIN_KHR";
		case EGL_BITMAP_PIXEL_RED_OFFSET_KHR: return "EGL_BITMAP_PIXEL_RED_OFFSET_KHR";
		case EGL_BITMAP_PIXEL_GREEN_OFFSET_KHR: return "EGL_BITMAP_PIXEL_GREEN_OFFSET_KHR";
		case EGL_BITMAP_PIXEL_BLUE_OFFSET_KHR: return "EGL_BITMAP_PIXEL_BLUE_OFFSET_KHR";
		case EGL_BITMAP_PIXEL_ALPHA_OFFSET_KHR: return "EGL_BITMAP_PIXEL_ALPHA_OFFSET_KHR";
		case EGL_BITMAP_PIXEL_LUMINANCE_OFFSET_KHR: return "EGL_BITMAP_PIXEL_LUMINANCE_OFFSET_KHR";
		case EGL_LOWER_LEFT_KHR: return "EGL_LOWER_LEFT_KHR";
		case EGL_UPPER_LEFT_KHR: return "EGL_UPPER_LEFT_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_lock_surface2 */
const char* gl::egl::is_define_khr_lock_surface2(GLenum pname)
{
	switch(pname)
	{
		case EGL_BITMAP_PIXEL_SIZE_KHR: return "EGL_BITMAP_PIXEL_SIZE_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_mutable_render_buffer */
const char* gl::egl::is_define_khr_mutable_render_buffer(GLenum pname)
{
	switch(pname)
	{
		case EGL_MUTABLE_RENDER_BUFFER_BIT_KHR: return "EGL_MUTABLE_RENDER_BUFFER_BIT_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_partial_update */
const char* gl::egl::is_define_khr_partial_update(GLenum pname)
{
	switch(pname)
	{
		case EGL_BUFFER_AGE_KHR: return "EGL_BUFFER_AGE_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_platform_android */
const char* gl::egl::is_define_khr_platform_android(GLenum pname)
{
	switch(pname)
	{
		case EGL_PLATFORM_ANDROID_KHR: return "EGL_PLATFORM_ANDROID_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_platform_gbm */
const char* gl::egl::is_define_khr_platform_gbm(GLenum pname)
{
	switch(pname)
	{
		case EGL_PLATFORM_GBM_KHR: return "EGL_PLATFORM_GBM_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_platform_wayland */
const char* gl::egl::is_define_khr_platform_wayland(GLenum pname)
{
	switch(pname)
	{
		case EGL_PLATFORM_WAYLAND_KHR: return "EGL_PLATFORM_WAYLAND_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_platform_x11 */
const char* gl::egl::is_define_khr_platform_x11(GLenum pname)
{
	switch(pname)
	{
		case EGL_PLATFORM_X11_KHR: return "EGL_PLATFORM_X11_KHR";
		case EGL_PLATFORM_X11_SCREEN_KHR: return "EGL_PLATFORM_X11_SCREEN_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_reusable_sync */
const char* gl::egl::is_define_khr_reusable_sync(GLenum pname)
{
	switch(pname)
	{
		case EGL_SYNC_STATUS_KHR: return "EGL_SYNC_STATUS_KHR";
		case EGL_SIGNALED_KHR: return "EGL_SIGNALED_KHR";
		case EGL_UNSIGNALED_KHR: return "EGL_UNSIGNALED_KHR";
		case EGL_TIMEOUT_EXPIRED_KHR: return "EGL_TIMEOUT_EXPIRED_KHR";
		case EGL_CONDITION_SATISFIED_KHR: return "EGL_CONDITION_SATISFIED_KHR";
		case EGL_SYNC_TYPE_KHR: return "EGL_SYNC_TYPE_KHR";
		case EGL_SYNC_REUSABLE_KHR: return "EGL_SYNC_REUSABLE_KHR";
		case EGL_SYNC_FLUSH_COMMANDS_BIT_KHR: return "EGL_SYNC_FLUSH_COMMANDS_BIT_KHR";
		//case EGL_FOREVER_KHR: return "EGL_FOREVER_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_stream */
const char* gl::egl::is_define_khr_stream(GLenum pname)
{
	switch(pname)
	{
		case EGL_CONSUMER_LATENCY_USEC_KHR: return "EGL_CONSUMER_LATENCY_USEC_KHR";
		case EGL_PRODUCER_FRAME_KHR: return "EGL_PRODUCER_FRAME_KHR";
		case EGL_CONSUMER_FRAME_KHR: return "EGL_CONSUMER_FRAME_KHR";
		case EGL_STREAM_STATE_KHR: return "EGL_STREAM_STATE_KHR";
		case EGL_STREAM_STATE_CREATED_KHR: return "EGL_STREAM_STATE_CREATED_KHR";
		case EGL_STREAM_STATE_CONNECTING_KHR: return "EGL_STREAM_STATE_CONNECTING_KHR";
		case EGL_STREAM_STATE_EMPTY_KHR: return "EGL_STREAM_STATE_EMPTY_KHR";
		case EGL_STREAM_STATE_NEW_FRAME_AVAILABLE_KHR: return "EGL_STREAM_STATE_NEW_FRAME_AVAILABLE_KHR";
		case EGL_STREAM_STATE_OLD_FRAME_AVAILABLE_KHR: return "EGL_STREAM_STATE_OLD_FRAME_AVAILABLE_KHR";
		case EGL_STREAM_STATE_DISCONNECTED_KHR: return "EGL_STREAM_STATE_DISCONNECTED_KHR";
		case EGL_BAD_STREAM_KHR: return "EGL_BAD_STREAM_KHR";
		case EGL_BAD_STATE_KHR: return "EGL_BAD_STATE_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_stream_consumer_gltexture */
const char* gl::egl::is_define_khr_stream_consumer_gltexture(GLenum pname)
{
	switch(pname)
	{
		case EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR: return "EGL_CONSUMER_ACQUIRE_TIMEOUT_USEC_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_stream_fifo */
const char* gl::egl::is_define_khr_stream_fifo(GLenum pname)
{
	switch(pname)
	{
		case EGL_STREAM_FIFO_LENGTH_KHR: return "EGL_STREAM_FIFO_LENGTH_KHR";
		case EGL_STREAM_TIME_NOW_KHR: return "EGL_STREAM_TIME_NOW_KHR";
		case EGL_STREAM_TIME_CONSUMER_KHR: return "EGL_STREAM_TIME_CONSUMER_KHR";
		case EGL_STREAM_TIME_PRODUCER_KHR: return "EGL_STREAM_TIME_PRODUCER_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_stream_producer_eglsurface */
const char* gl::egl::is_define_khr_stream_producer_eglsurface(GLenum pname)
{
	switch(pname)
	{
		case EGL_STREAM_BIT_KHR: return "EGL_STREAM_BIT_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_KHR_vg_parent_image */
const char* gl::egl::is_define_khr_vg_parent_image(GLenum pname)
{
	switch(pname)
	{
		case EGL_VG_PARENT_IMAGE_KHR: return "EGL_VG_PARENT_IMAGE_KHR";
	} // switch
	return nullptr;
}

/** extensions EGL_ANDROID_create_native_client_buffer */
const char* gl::egl::is_define_android_create_native_client_buffer(GLenum pname)
{
	switch(pname)
	{
		case EGL_NATIVE_BUFFER_USAGE_ANDROID: return "EGL_NATIVE_BUFFER_USAGE_ANDROID";
		case EGL_NATIVE_BUFFER_USAGE_PROTECTED_BIT_ANDROID: return "EGL_NATIVE_BUFFER_USAGE_PROTECTED_BIT_ANDROID";
		case EGL_NATIVE_BUFFER_USAGE_RENDERBUFFER_BIT_ANDROID: return "EGL_NATIVE_BUFFER_USAGE_RENDERBUFFER_BIT_ANDROID";
		case EGL_NATIVE_BUFFER_USAGE_TEXTURE_BIT_ANDROID: return "EGL_NATIVE_BUFFER_USAGE_TEXTURE_BIT_ANDROID";
	} // switch
	return nullptr;
}

/** extensions EGL_ANDROID_framebuffer_target */
const char* gl::egl::is_define_android_framebuffer_target(GLenum pname)
{
	switch(pname)
	{
		case EGL_FRAMEBUFFER_TARGET_ANDROID: return "EGL_FRAMEBUFFER_TARGET_ANDROID";
	} // switch
	return nullptr;
}

/** extensions EGL_ANDROID_front_buffer_auto_refresh */
const char* gl::egl::is_define_android_front_buffer_auto_refresh(GLenum pname)
{
	switch(pname)
	{
		case EGL_FRONT_BUFFER_AUTO_REFRESH_ANDROID: return "EGL_FRONT_BUFFER_AUTO_REFRESH_ANDROID";
	} // switch
	return nullptr;
}

/** extensions EGL_ANDROID_image_native_buffer */
const char* gl::egl::is_define_android_image_native_buffer(GLenum pname)
{
	switch(pname)
	{
		case EGL_NATIVE_BUFFER_ANDROID: return "EGL_NATIVE_BUFFER_ANDROID";
	} // switch
	return nullptr;
}

/** extensions EGL_ANDROID_native_fence_sync */
const char* gl::egl::is_define_android_native_fence_sync(GLenum pname)
{
	switch(pname)
	{
		case EGL_SYNC_NATIVE_FENCE_ANDROID: return "EGL_SYNC_NATIVE_FENCE_ANDROID";
		case EGL_SYNC_NATIVE_FENCE_FD_ANDROID: return "EGL_SYNC_NATIVE_FENCE_FD_ANDROID";
		case EGL_SYNC_NATIVE_FENCE_SIGNALED_ANDROID: return "EGL_SYNC_NATIVE_FENCE_SIGNALED_ANDROID";
	} // switch
	return nullptr;
}

/** extensions EGL_ANDROID_recordable */
const char* gl::egl::is_define_android_recordable(GLenum pname)
{
	switch(pname)
	{
		case EGL_RECORDABLE_ANDROID: return "EGL_RECORDABLE_ANDROID";
	} // switch
	return nullptr;
}

/** extensions EGL_ANGLE_d3d_share_handle_client_buffer */
const char* gl::egl::is_define_angle_d3d_share_handle_client_buffer(GLenum pname)
{
	switch(pname)
	{
		case EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE: return "EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE";
	} // switch
	return nullptr;
}

/** extensions EGL_ANGLE_device_d3d */
const char* gl::egl::is_define_angle_device_d3d(GLenum pname)
{
	switch(pname)
	{
		case EGL_D3D9_DEVICE_ANGLE: return "EGL_D3D9_DEVICE_ANGLE";
		case EGL_D3D11_DEVICE_ANGLE: return "EGL_D3D11_DEVICE_ANGLE";
	} // switch
	return nullptr;
}

/** extensions EGL_ANGLE_window_fixed_size */
const char* gl::egl::is_define_angle_window_fixed_size(GLenum pname)
{
	switch(pname)
	{
		case EGL_FIXED_SIZE_ANGLE: return "EGL_FIXED_SIZE_ANGLE";
	} // switch
	return nullptr;
}

/** extensions EGL_ARM_implicit_external_sync */
const char* gl::egl::is_define_arm_implicit_external_sync(GLenum pname)
{
	switch(pname)
	{
		case EGL_SYNC_PRIOR_COMMANDS_IMPLICIT_EXTERNAL_ARM: return "EGL_SYNC_PRIOR_COMMANDS_IMPLICIT_EXTERNAL_ARM";
	} // switch
	return nullptr;
}

/** extensions EGL_ARM_pixmap_multisample_discard */
const char* gl::egl::is_define_arm_pixmap_multisample_discard(GLenum pname)
{
	switch(pname)
	{
		case EGL_DISCARD_SAMPLES_ARM: return "EGL_DISCARD_SAMPLES_ARM";
	} // switch
	return nullptr;
}

/** extensions EGL_EXT_buffer_age */
const char* gl::egl::is_define_ext_buffer_age(GLenum pname)
{
	switch(pname)
	{
		case EGL_BUFFER_AGE_EXT: return "EGL_BUFFER_AGE_EXT";
	} // switch
	return nullptr;
}

/** extensions EGL_EXT_create_context_robustness */
const char* gl::egl::is_define_ext_create_context_robustness(GLenum pname)
{
	switch(pname)
	{
		case EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT: return "EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT";
		case EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT: return "EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT";
		case EGL_NO_RESET_NOTIFICATION_EXT: return "EGL_NO_RESET_NOTIFICATION_EXT";
		case EGL_LOSE_CONTEXT_ON_RESET_EXT: return "EGL_LOSE_CONTEXT_ON_RESET_EXT";
	} // switch
	return nullptr;
}

/** extensions EGL_EXT_device_base */
const char* gl::egl::is_define_ext_device_base(GLenum pname)
{
	switch(pname)
	{
		case EGL_BAD_DEVICE_EXT: return "EGL_BAD_DEVICE_EXT";
		case EGL_DEVICE_EXT: return "EGL_DEVICE_EXT";
	} // switch
	return nullptr;
}

/** extensions EGL_EXT_device_drm */
const char* gl::egl::is_define_ext_device_drm(GLenum pname)
{
	switch(pname)
	{
		case EGL_DRM_DEVICE_FILE_EXT: return "EGL_DRM_DEVICE_FILE_EXT";
	} // switch
	return nullptr;
}

/** extensions EGL_EXT_device_openwf */
const char* gl::egl::is_define_ext_device_openwf(GLenum pname)
{
	switch(pname)
	{
		case EGL_OPENWF_DEVICE_ID_EXT: return "EGL_OPENWF_DEVICE_ID_EXT";
	} // switch
	return nullptr;
}

/** extensions EGL_EXT_image_dma_buf_import */
const char* gl::egl::is_define_ext_image_dma_buf_import(GLenum pname)
{
	switch(pname)
	{
		case EGL_LINUX_DMA_BUF_EXT: return "EGL_LINUX_DMA_BUF_EXT";
		case EGL_LINUX_DRM_FOURCC_EXT: return "EGL_LINUX_DRM_FOURCC_EXT";
		case EGL_DMA_BUF_PLANE0_FD_EXT: return "EGL_DMA_BUF_PLANE0_FD_EXT";
		case EGL_DMA_BUF_PLANE0_OFFSET_EXT: return "EGL_DMA_BUF_PLANE0_OFFSET_EXT";
		case EGL_DMA_BUF_PLANE0_PITCH_EXT: return "EGL_DMA_BUF_PLANE0_PITCH_EXT";
		case EGL_DMA_BUF_PLANE1_FD_EXT: return "EGL_DMA_BUF_PLANE1_FD_EXT";
		case EGL_DMA_BUF_PLANE1_OFFSET_EXT: return "EGL_DMA_BUF_PLANE1_OFFSET_EXT";
		case EGL_DMA_BUF_PLANE1_PITCH_EXT: return "EGL_DMA_BUF_PLANE1_PITCH_EXT";
		case EGL_DMA_BUF_PLANE2_FD_EXT: return "EGL_DMA_BUF_PLANE2_FD_EXT";
		case EGL_DMA_BUF_PLANE2_OFFSET_EXT: return "EGL_DMA_BUF_PLANE2_OFFSET_EXT";
		case EGL_DMA_BUF_PLANE2_PITCH_EXT: return "EGL_DMA_BUF_PLANE2_PITCH_EXT";
		case EGL_YUV_COLOR_SPACE_HINT_EXT: return "EGL_YUV_COLOR_SPACE_HINT_EXT";
		case EGL_SAMPLE_RANGE_HINT_EXT: return "EGL_SAMPLE_RANGE_HINT_EXT";
		case EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT: return "EGL_YUV_CHROMA_HORIZONTAL_SITING_HINT_EXT";
		case EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT: return "EGL_YUV_CHROMA_VERTICAL_SITING_HINT_EXT";
		case EGL_ITU_REC601_EXT: return "EGL_ITU_REC601_EXT";
		case EGL_ITU_REC709_EXT: return "EGL_ITU_REC709_EXT";
		case EGL_ITU_REC2020_EXT: return "EGL_ITU_REC2020_EXT";
		case EGL_YUV_FULL_RANGE_EXT: return "EGL_YUV_FULL_RANGE_EXT";
		case EGL_YUV_NARROW_RANGE_EXT: return "EGL_YUV_NARROW_RANGE_EXT";
		case EGL_YUV_CHROMA_SITING_0_EXT: return "EGL_YUV_CHROMA_SITING_0_EXT";
		case EGL_YUV_CHROMA_SITING_0_5_EXT: return "EGL_YUV_CHROMA_SITING_0_5_EXT";
	} // switch
	return nullptr;
}

/** extensions EGL_EXT_multiview_window */
const char* gl::egl::is_define_ext_multiview_window(GLenum pname)
{
	switch(pname)
	{
		case EGL_MULTIVIEW_VIEW_COUNT_EXT: return "EGL_MULTIVIEW_VIEW_COUNT_EXT";
	} // switch
	return nullptr;
}

/** extensions EGL_EXT_output_base */
const char* gl::egl::is_define_ext_output_base(GLenum pname)
{
	switch(pname)
	{
		case EGL_BAD_OUTPUT_LAYER_EXT: return "EGL_BAD_OUTPUT_LAYER_EXT";
		case EGL_BAD_OUTPUT_PORT_EXT: return "EGL_BAD_OUTPUT_PORT_EXT";
		case EGL_SWAP_INTERVAL_EXT: return "EGL_SWAP_INTERVAL_EXT";
	} // switch
	return nullptr;
}

/** extensions EGL_EXT_output_drm */
const char* gl::egl::is_define_ext_output_drm(GLenum pname)
{
	switch(pname)
	{
		case EGL_DRM_CRTC_EXT: return "EGL_DRM_CRTC_EXT";
		case EGL_DRM_PLANE_EXT: return "EGL_DRM_PLANE_EXT";
		case EGL_DRM_CONNECTOR_EXT: return "EGL_DRM_CONNECTOR_EXT";
	} // switch
	return nullptr;
}

/** extensions EGL_EXT_output_openwf */
const char* gl::egl::is_define_ext_output_openwf(GLenum pname)
{
	switch(pname)
	{
		case EGL_OPENWF_PIPELINE_ID_EXT: return "EGL_OPENWF_PIPELINE_ID_EXT";
		case EGL_OPENWF_PORT_ID_EXT: return "EGL_OPENWF_PORT_ID_EXT";
	} // switch
	return nullptr;
}

/** extensions EGL_EXT_platform_device */
const char* gl::egl::is_define_ext_platform_device(GLenum pname)
{
	switch(pname)
	{
		case EGL_PLATFORM_DEVICE_EXT: return "EGL_PLATFORM_DEVICE_EXT";
	} // switch
	return nullptr;
}

/** extensions EGL_EXT_platform_wayland */
const char* gl::egl::is_define_ext_platform_wayland(GLenum pname)
{
	switch(pname)
	{
		case EGL_PLATFORM_WAYLAND_EXT: return "EGL_PLATFORM_WAYLAND_EXT";
	} // switch
	return nullptr;
}

/** extensions EGL_EXT_platform_x11 */
const char* gl::egl::is_define_ext_platform_x11(GLenum pname)
{
	switch(pname)
	{
		case EGL_PLATFORM_X11_EXT: return "EGL_PLATFORM_X11_EXT";
		case EGL_PLATFORM_X11_SCREEN_EXT: return "EGL_PLATFORM_X11_SCREEN_EXT";
	} // switch
	return nullptr;
}

/** extensions EGL_EXT_protected_content */
const char* gl::egl::is_define_ext_protected_content(GLenum pname)
{
	switch(pname)
	{
		case EGL_PROTECTED_CONTENT_EXT: return "EGL_PROTECTED_CONTENT_EXT";
	} // switch
	return nullptr;
}

/** extensions EGL_EXT_yuv_surface */
const char* gl::egl::is_define_ext_yuv_surface(GLenum pname)
{
	switch(pname)
	{
		case EGL_YUV_ORDER_EXT: return "EGL_YUV_ORDER_EXT";
		case EGL_YUV_NUMBER_OF_PLANES_EXT: return "EGL_YUV_NUMBER_OF_PLANES_EXT";
		case EGL_YUV_SUBSAMPLE_EXT: return "EGL_YUV_SUBSAMPLE_EXT";
		case EGL_YUV_DEPTH_RANGE_EXT: return "EGL_YUV_DEPTH_RANGE_EXT";
		case EGL_YUV_CSC_STANDARD_EXT: return "EGL_YUV_CSC_STANDARD_EXT";
		case EGL_YUV_PLANE_BPP_EXT: return "EGL_YUV_PLANE_BPP_EXT";
		case EGL_YUV_BUFFER_EXT: return "EGL_YUV_BUFFER_EXT";
		case EGL_YUV_ORDER_YUV_EXT: return "EGL_YUV_ORDER_YUV_EXT";
		case EGL_YUV_ORDER_YVU_EXT: return "EGL_YUV_ORDER_YVU_EXT";
		case EGL_YUV_ORDER_YUYV_EXT: return "EGL_YUV_ORDER_YUYV_EXT";
		case EGL_YUV_ORDER_UYVY_EXT: return "EGL_YUV_ORDER_UYVY_EXT";
		case EGL_YUV_ORDER_YVYU_EXT: return "EGL_YUV_ORDER_YVYU_EXT";
		case EGL_YUV_ORDER_VYUY_EXT: return "EGL_YUV_ORDER_VYUY_EXT";
		case EGL_YUV_ORDER_AYUV_EXT: return "EGL_YUV_ORDER_AYUV_EXT";
		case EGL_YUV_SUBSAMPLE_4_2_0_EXT: return "EGL_YUV_SUBSAMPLE_4_2_0_EXT";
		case EGL_YUV_SUBSAMPLE_4_2_2_EXT: return "EGL_YUV_SUBSAMPLE_4_2_2_EXT";
		case EGL_YUV_SUBSAMPLE_4_4_4_EXT: return "EGL_YUV_SUBSAMPLE_4_4_4_EXT";
		case EGL_YUV_DEPTH_RANGE_LIMITED_EXT: return "EGL_YUV_DEPTH_RANGE_LIMITED_EXT";
		case EGL_YUV_DEPTH_RANGE_FULL_EXT: return "EGL_YUV_DEPTH_RANGE_FULL_EXT";
		case EGL_YUV_CSC_STANDARD_601_EXT: return "EGL_YUV_CSC_STANDARD_601_EXT";
		case EGL_YUV_CSC_STANDARD_709_EXT: return "EGL_YUV_CSC_STANDARD_709_EXT";
		case EGL_YUV_CSC_STANDARD_2020_EXT: return "EGL_YUV_CSC_STANDARD_2020_EXT";
		case EGL_YUV_PLANE_BPP_0_EXT: return "EGL_YUV_PLANE_BPP_0_EXT";
		case EGL_YUV_PLANE_BPP_8_EXT: return "EGL_YUV_PLANE_BPP_8_EXT";
		case EGL_YUV_PLANE_BPP_10_EXT: return "EGL_YUV_PLANE_BPP_10_EXT";
	} // switch
	return nullptr;
}

/** extensions EGL_HI_clientpixmap */
const char* gl::egl::is_define_hi_clientpixmap(GLenum pname)
{
	switch(pname)
	{
		case EGL_CLIENT_PIXMAP_POINTER_HI: return "EGL_CLIENT_PIXMAP_POINTER_HI";
	} // switch
	return nullptr;
}

/** extensions EGL_HI_colorformats */
const char* gl::egl::is_define_hi_colorformats(GLenum pname)
{
	switch(pname)
	{
		case EGL_COLOR_FORMAT_HI: return "EGL_COLOR_FORMAT_HI";
		case EGL_COLOR_RGB_HI: return "EGL_COLOR_RGB_HI";
		case EGL_COLOR_RGBA_HI: return "EGL_COLOR_RGBA_HI";
		case EGL_COLOR_ARGB_HI: return "EGL_COLOR_ARGB_HI";
	} // switch
	return nullptr;
}

/** extensions EGL_IMG_context_priority */
const char* gl::egl::is_define_img_context_priority(GLenum pname)
{
	switch(pname)
	{
		case EGL_CONTEXT_PRIORITY_LEVEL_IMG: return "EGL_CONTEXT_PRIORITY_LEVEL_IMG";
		case EGL_CONTEXT_PRIORITY_HIGH_IMG: return "EGL_CONTEXT_PRIORITY_HIGH_IMG";
		case EGL_CONTEXT_PRIORITY_MEDIUM_IMG: return "EGL_CONTEXT_PRIORITY_MEDIUM_IMG";
		case EGL_CONTEXT_PRIORITY_LOW_IMG: return "EGL_CONTEXT_PRIORITY_LOW_IMG";
	} // switch
	return nullptr;
}

/** extensions EGL_IMG_image_plane_attribs */
const char* gl::egl::is_define_img_image_plane_attribs(GLenum pname)
{
	switch(pname)
	{
		case EGL_NATIVE_BUFFER_MULTIPLANE_SEPARATE_IMG: return "EGL_NATIVE_BUFFER_MULTIPLANE_SEPARATE_IMG";
		case EGL_NATIVE_BUFFER_PLANE_OFFSET_IMG: return "EGL_NATIVE_BUFFER_PLANE_OFFSET_IMG";
	} // switch
	return nullptr;
}

/** extensions EGL_MESA_drm_image */
const char* gl::egl::is_define_mesa_drm_image(GLenum pname)
{
	switch(pname)
	{
		case EGL_DRM_BUFFER_FORMAT_MESA: return "EGL_DRM_BUFFER_FORMAT_MESA";
		case EGL_DRM_BUFFER_USE_MESA: return "EGL_DRM_BUFFER_USE_MESA";
		case EGL_DRM_BUFFER_FORMAT_ARGB32_MESA: return "EGL_DRM_BUFFER_FORMAT_ARGB32_MESA";
		case EGL_DRM_BUFFER_MESA: return "EGL_DRM_BUFFER_MESA";
		case EGL_DRM_BUFFER_STRIDE_MESA: return "EGL_DRM_BUFFER_STRIDE_MESA";
		case EGL_DRM_BUFFER_USE_SCANOUT_MESA: return "EGL_DRM_BUFFER_USE_SCANOUT_MESA";
		case EGL_DRM_BUFFER_USE_SHARE_MESA: return "EGL_DRM_BUFFER_USE_SHARE_MESA";
	} // switch
	return nullptr;
}

/** extensions EGL_MESA_platform_gbm */
const char* gl::egl::is_define_mesa_platform_gbm(GLenum pname)
{
	switch(pname)
	{
		case EGL_PLATFORM_GBM_MESA: return "EGL_PLATFORM_GBM_MESA";
	} // switch
	return nullptr;
}

/** extensions EGL_MESA_platform_surfaceless */
const char* gl::egl::is_define_mesa_platform_surfaceless(GLenum pname)
{
	switch(pname)
	{
		case EGL_PLATFORM_SURFACELESS_MESA: return "EGL_PLATFORM_SURFACELESS_MESA";
	} // switch
	return nullptr;
}

/** extensions EGL_NOK_texture_from_pixmap */
const char* gl::egl::is_define_nok_texture_from_pixmap(GLenum pname)
{
	switch(pname)
	{
		case EGL_Y_INVERTED_NOK: return "EGL_Y_INVERTED_NOK";
	} // switch
	return nullptr;
}

/** extensions EGL_NV_3dvision_surface */
const char* gl::egl::is_define_nv_3dvision_surface(GLenum pname)
{
	switch(pname)
	{
		case EGL_AUTO_STEREO_NV: return "EGL_AUTO_STEREO_NV";
	} // switch
	return nullptr;
}

/** extensions EGL_NV_coverage_sample */
const char* gl::egl::is_define_nv_coverage_sample(GLenum pname)
{
	switch(pname)
	{
		case EGL_COVERAGE_BUFFERS_NV: return "EGL_COVERAGE_BUFFERS_NV";
		case EGL_COVERAGE_SAMPLES_NV: return "EGL_COVERAGE_SAMPLES_NV";
	} // switch
	return nullptr;
}

/** extensions EGL_NV_coverage_sample_resolve */
const char* gl::egl::is_define_nv_coverage_sample_resolve(GLenum pname)
{
	switch(pname)
	{
		case EGL_COVERAGE_SAMPLE_RESOLVE_NV: return "EGL_COVERAGE_SAMPLE_RESOLVE_NV";
		case EGL_COVERAGE_SAMPLE_RESOLVE_DEFAULT_NV: return "EGL_COVERAGE_SAMPLE_RESOLVE_DEFAULT_NV";
		case EGL_COVERAGE_SAMPLE_RESOLVE_NONE_NV: return "EGL_COVERAGE_SAMPLE_RESOLVE_NONE_NV";
	} // switch
	return nullptr;
}

/** extensions EGL_NV_cuda_event */
const char* gl::egl::is_define_nv_cuda_event(GLenum pname)
{
	switch(pname)
	{
		case EGL_CUDA_EVENT_HANDLE_NV: return "EGL_CUDA_EVENT_HANDLE_NV";
		case EGL_SYNC_CUDA_EVENT_NV: return "EGL_SYNC_CUDA_EVENT_NV";
		case EGL_SYNC_CUDA_EVENT_COMPLETE_NV: return "EGL_SYNC_CUDA_EVENT_COMPLETE_NV";
	} // switch
	return nullptr;
}

/** extensions EGL_NV_depth_nonlinear */
const char* gl::egl::is_define_nv_depth_nonlinear(GLenum pname)
{
	switch(pname)
	{
		case EGL_DEPTH_ENCODING_NV: return "EGL_DEPTH_ENCODING_NV";
		case EGL_DEPTH_ENCODING_NONLINEAR_NV: return "EGL_DEPTH_ENCODING_NONLINEAR_NV";
	} // switch
	return nullptr;
}

/** extensions EGL_NV_device_cuda */
const char* gl::egl::is_define_nv_device_cuda(GLenum pname)
{
	switch(pname)
	{
		case EGL_CUDA_DEVICE_NV: return "EGL_CUDA_DEVICE_NV";
	} // switch
	return nullptr;
}

/** extensions EGL_NV_post_sub_buffer */
const char* gl::egl::is_define_nv_post_sub_buffer(GLenum pname)
{
	switch(pname)
	{
		case EGL_POST_SUB_BUFFER_SUPPORTED_NV: return "EGL_POST_SUB_BUFFER_SUPPORTED_NV";
	} // switch
	return nullptr;
}

/** extensions EGL_NV_robustness_video_memory_purge */
const char* gl::egl::is_define_nv_robustness_video_memory_purge(GLenum pname)
{
	switch(pname)
	{
		case EGL_GENERATE_RESET_ON_VIDEO_MEMORY_PURGE_NV: return "EGL_GENERATE_RESET_ON_VIDEO_MEMORY_PURGE_NV";
	} // switch
	return nullptr;
}

/** extensions EGL_NV_stream_consumer_gltexture_yuv */
const char* gl::egl::is_define_nv_stream_consumer_gltexture_yuv(GLenum pname)
{
	switch(pname)
	{
		case EGL_YUV_PLANE0_TEXTURE_UNIT_NV: return "EGL_YUV_PLANE0_TEXTURE_UNIT_NV";
		case EGL_YUV_PLANE1_TEXTURE_UNIT_NV: return "EGL_YUV_PLANE1_TEXTURE_UNIT_NV";
		case EGL_YUV_PLANE2_TEXTURE_UNIT_NV: return "EGL_YUV_PLANE2_TEXTURE_UNIT_NV";
	} // switch
	return nullptr;
}

/** extensions EGL_NV_stream_metadata */
const char* gl::egl::is_define_nv_stream_metadata(GLenum pname)
{
	switch(pname)
	{
		case EGL_MAX_STREAM_METADATA_BLOCKS_NV: return "EGL_MAX_STREAM_METADATA_BLOCKS_NV";
		case EGL_MAX_STREAM_METADATA_BLOCK_SIZE_NV: return "EGL_MAX_STREAM_METADATA_BLOCK_SIZE_NV";
		case EGL_MAX_STREAM_METADATA_TOTAL_SIZE_NV: return "EGL_MAX_STREAM_METADATA_TOTAL_SIZE_NV";
		case EGL_PRODUCER_METADATA_NV: return "EGL_PRODUCER_METADATA_NV";
		case EGL_CONSUMER_METADATA_NV: return "EGL_CONSUMER_METADATA_NV";
		case EGL_PENDING_METADATA_NV: return "EGL_PENDING_METADATA_NV";
		case EGL_METADATA0_SIZE_NV: return "EGL_METADATA0_SIZE_NV";
		case EGL_METADATA1_SIZE_NV: return "EGL_METADATA1_SIZE_NV";
		case EGL_METADATA2_SIZE_NV: return "EGL_METADATA2_SIZE_NV";
		case EGL_METADATA3_SIZE_NV: return "EGL_METADATA3_SIZE_NV";
		case EGL_METADATA0_TYPE_NV: return "EGL_METADATA0_TYPE_NV";
		case EGL_METADATA1_TYPE_NV: return "EGL_METADATA1_TYPE_NV";
		case EGL_METADATA2_TYPE_NV: return "EGL_METADATA2_TYPE_NV";
		case EGL_METADATA3_TYPE_NV: return "EGL_METADATA3_TYPE_NV";
	} // switch
	return nullptr;
}

/** extensions EGL_NV_stream_sync */
const char* gl::egl::is_define_nv_stream_sync(GLenum pname)
{
	switch(pname)
	{
		case EGL_SYNC_NEW_FRAME_NV: return "EGL_SYNC_NEW_FRAME_NV";
	} // switch
	return nullptr;
}

/** extensions EGL_NV_sync */
const char* gl::egl::is_define_nv_sync(GLenum pname)
{
	switch(pname)
	{
		case EGL_SYNC_PRIOR_COMMANDS_COMPLETE_NV: return "EGL_SYNC_PRIOR_COMMANDS_COMPLETE_NV";
		case EGL_SYNC_STATUS_NV: return "EGL_SYNC_STATUS_NV";
		case EGL_SIGNALED_NV: return "EGL_SIGNALED_NV";
		case EGL_UNSIGNALED_NV: return "EGL_UNSIGNALED_NV";
		case EGL_SYNC_FLUSH_COMMANDS_BIT_NV: return "EGL_SYNC_FLUSH_COMMANDS_BIT_NV";
		//case EGL_FOREVER_NV: return "EGL_FOREVER_NV";
		case EGL_ALREADY_SIGNALED_NV: return "EGL_ALREADY_SIGNALED_NV";
		case EGL_TIMEOUT_EXPIRED_NV: return "EGL_TIMEOUT_EXPIRED_NV";
		case EGL_CONDITION_SATISFIED_NV: return "EGL_CONDITION_SATISFIED_NV";
		case EGL_SYNC_TYPE_NV: return "EGL_SYNC_TYPE_NV";
		case EGL_SYNC_CONDITION_NV: return "EGL_SYNC_CONDITION_NV";
		case EGL_SYNC_FENCE_NV: return "EGL_SYNC_FENCE_NV";
	} // switch
	return nullptr;
}

/** extensions EGL_TIZEN_image_native_buffer */
const char* gl::egl::is_define_tizen_image_native_buffer(GLenum pname)
{
	switch(pname)
	{
		case EGL_NATIVE_BUFFER_TIZEN: return "EGL_NATIVE_BUFFER_TIZEN";
	} // switch
	return nullptr;
}

/** extensions EGL_TIZEN_image_native_surface */
const char* gl::egl::is_define_tizen_image_native_surface(GLenum pname)
{
	switch(pname)
	{
		case EGL_NATIVE_SURFACE_TIZEN: return "EGL_NATIVE_SURFACE_TIZEN";
	} // switch
	return nullptr;
}

// ---------------------------------------------------------------------

void* gl::getProc(const char* function_name)
{
	const hash_t hash = Core::hash( function_name );

	Entry_v::Iterator iter;

	foreach(_registered)
	{
		if ( iter->hash == hash )
			return iter->fnc;
	}
	return nullptr;
}

void* gl::getProcAddr(const char* function_name)
{
	return eglGetProcAddress(function_name);
}

// ---------------------------------------------------------------------

#define GET_PROC_ADDRESS(a,b) \
	gl_##b = (a)getProcAddr("gl"#b); \
	_registered.push_back( Entry_t(Core::hash( "gl"#b ), gl_##b) );

#define EGL_GET_PROC_ADDRESS(a,b) \
	egl::egl_##b = (a)getProcAddr("egl"#b); \
	_registered.push_back( Entry_t(Core::hash( "egl"#b ), egl::egl_##b) );

/** special case for opengl es function */
#define DLL_GET_PROC_ADDRESS(a,b) \
	egl::egl_##b = (a)getProcAddr("egl"#b); \
	_registered.push_back( Entry_t(Core::hash( "egl"#b ), egl::egl_##b) );

static uint _biggest_value = 0;
static String _biggest_name;

bool gl::init()
{
	static int init_ = 0;

	ne_assert(!init_);

	if (init_) return true;

	init_ = 1;

	_call_history.resize( RESERVED_SIZE );

	/** accept only allowed capability from the docs :
	 * https://www.khronos.org/opengles/sdk/docs/man/ */

	#define ADD(v_) _allowed_enable.push_back(v_)

	ADD(GL_BLEND);
	ADD(GL_CULL_FACE);
	ADD(GL_DEPTH_TEST);
	ADD(GL_DITHER);
	ADD(GL_POLYGON_OFFSET_FILL);
	ADD(GL_SAMPLE_ALPHA_TO_COVERAGE);
	ADD(GL_SAMPLE_COVERAGE);
	ADD(GL_SCISSOR_TEST);
	ADD(GL_STENCIL_TEST);

#if defined( GL_KHR_debug )
	// allow debug extension (OpenGL 1.1 required)
	ADD(GL_DEBUG_OUTPUT_KHR);
	ADD(GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR);
#endif // 

	#undef ADD

	/** the only state who is enabled by default */
	_states[GL_DITHER] = static_cast<uchar>(1);
	_states[GL_BLEND] = static_cast<uchar>(0);
	_states[GL_CULL_FACE] = static_cast<uchar>(0);
	_states[GL_DEPTH_TEST] = static_cast<uchar>(0);
	_states[GL_POLYGON_OFFSET_FILL] = static_cast<uchar>(0);
	_states[GL_SAMPLE_ALPHA_TO_COVERAGE] = static_cast<uchar>(0);
	_states[GL_SAMPLE_COVERAGE] = static_cast<uchar>(0);
	_states[GL_SCISSOR_TEST] = static_cast<uchar>(0);
	_states[GL_STENCIL_TEST] = static_cast<uchar>(0);
#if defined( GL_KHR_debug )
	_states[GL_DEBUG_OUTPUT_KHR] = static_cast<uchar>(0);
	_states[GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR] = static_cast<uchar>(0);
#endif // GL_KHR_debug

	// -----------------------------------------------------------------
	// get all the proc from opengl es library
	// -----------------------------------------------------------------

	// -----------------------------------------------------------------
	// eglext.h
	// -----------------------------------------------------------------

	// EGL_ANDROID_blob_cache
	EGL_GET_PROC_ADDRESS(PFNEGLSETBLOBCACHEFUNCSANDROIDPROC , SetBlobCacheFuncsANDROID);
	// EGL_ANDROID_create_native_client_buffer
	EGL_GET_PROC_ADDRESS(PFNEGLCREATENATIVECLIENTBUFFERANDROIDPROC , CreateNativeClientBufferANDROID);
	// EGL_ANDROID_native_fence_sync
	EGL_GET_PROC_ADDRESS(PFNEGLDUPNATIVEFENCEFDANDROIDPROC , DupNativeFenceFDANDROID);
	// EGL_ANDROID_presentation_time
	EGL_GET_PROC_ADDRESS(PFNEGLPRESENTATIONTIMEANDROIDPROC , PresentationTimeANDROID);
	// EGL_ANGLE_query_surface_pointer
	EGL_GET_PROC_ADDRESS(PFNEGLQUERYSURFACEPOINTERANGLEPROC , QuerySurfacePointerANGLE);
	// EGL_EXT_device_base
	EGL_GET_PROC_ADDRESS(PFNEGLQUERYDEVICEATTRIBEXTPROC , QueryDeviceAttribEXT);
	EGL_GET_PROC_ADDRESS(PFNEGLQUERYDEVICESTRINGEXTPROC , QueryDeviceStringEXT);
	EGL_GET_PROC_ADDRESS(PFNEGLQUERYDEVICESEXTPROC , QueryDevicesEXT);
	EGL_GET_PROC_ADDRESS(PFNEGLQUERYDISPLAYATTRIBEXTPROC , QueryDisplayAttribEXT);
	// EGL_EXT_output_base
	EGL_GET_PROC_ADDRESS(PFNEGLGETOUTPUTLAYERSEXTPROC , GetOutputLayersEXT);
	EGL_GET_PROC_ADDRESS(PFNEGLGETOUTPUTPORTSEXTPROC , GetOutputPortsEXT);
	EGL_GET_PROC_ADDRESS(PFNEGLOUTPUTLAYERATTRIBEXTPROC , OutputLayerAttribEXT);
	EGL_GET_PROC_ADDRESS(PFNEGLOUTPUTPORTATTRIBEXTPROC , OutputPortAttribEXT);
	EGL_GET_PROC_ADDRESS(PFNEGLQUERYOUTPUTLAYERATTRIBEXTPROC , QueryOutputLayerAttribEXT);
	EGL_GET_PROC_ADDRESS(PFNEGLQUERYOUTPUTLAYERSTRINGEXTPROC , QueryOutputLayerStringEXT);
	EGL_GET_PROC_ADDRESS(PFNEGLQUERYOUTPUTPORTATTRIBEXTPROC , QueryOutputPortAttribEXT);
	EGL_GET_PROC_ADDRESS(PFNEGLQUERYOUTPUTPORTSTRINGEXTPROC , QueryOutputPortStringEXT);
	// EGL_EXT_platform_base
	EGL_GET_PROC_ADDRESS(PFNEGLCREATEPLATFORMPIXMAPSURFACEEXTPROC , CreatePlatformPixmapSurfaceEXT);
	EGL_GET_PROC_ADDRESS(PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC , CreatePlatformWindowSurfaceEXT);
	EGL_GET_PROC_ADDRESS(PFNEGLGETPLATFORMDISPLAYEXTPROC , GetPlatformDisplayEXT);
	// EGL_EXT_stream_consumer_egloutput
	EGL_GET_PROC_ADDRESS(PFNEGLSTREAMCONSUMEROUTPUTEXTPROC , StreamConsumerOutputEXT);
	// EGL_EXT_swap_buffers_with_damage
	EGL_GET_PROC_ADDRESS(PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC , SwapBuffersWithDamageEXT);
	// EGL_HI_clientpixmap
	EGL_GET_PROC_ADDRESS(PFNEGLCREATEPIXMAPSURFACEHIPROC , CreatePixmapSurfaceHI);
	// EGL_KHR_cl_event2
	EGL_GET_PROC_ADDRESS(PFNEGLCREATESYNC64KHRPROC , CreateSync64KHR);
	// EGL_KHR_debug
	EGL_GET_PROC_ADDRESS(PFNEGLLABELOBJECTKHRPROC , LabelObjectKHR);
	EGL_GET_PROC_ADDRESS(PFNEGLQUERYDEBUGKHRPROC , QueryDebugKHR);
	// EGL_KHR_fence_sync
	EGL_GET_PROC_ADDRESS(PFNEGLCLIENTWAITSYNCKHRPROC , ClientWaitSyncKHR);
	EGL_GET_PROC_ADDRESS(PFNEGLCREATESYNCKHRPROC , CreateSyncKHR);
	EGL_GET_PROC_ADDRESS(PFNEGLDESTROYSYNCKHRPROC , DestroySyncKHR);
	EGL_GET_PROC_ADDRESS(PFNEGLGETSYNCATTRIBKHRPROC , GetSyncAttribKHR);
	// EGL_KHR_image
	EGL_GET_PROC_ADDRESS(PFNEGLCREATEIMAGEKHRPROC , CreateImageKHR);
	EGL_GET_PROC_ADDRESS(PFNEGLDESTROYIMAGEKHRPROC , DestroyImageKHR);
	// EGL_KHR_lock_surface
	EGL_GET_PROC_ADDRESS(PFNEGLLOCKSURFACEKHRPROC , LockSurfaceKHR);
	EGL_GET_PROC_ADDRESS(PFNEGLUNLOCKSURFACEKHRPROC , UnlockSurfaceKHR);
	// EGL_KHR_lock_surface3
	EGL_GET_PROC_ADDRESS(PFNEGLQUERYSURFACE64KHRPROC , QuerySurface64KHR);
	// EGL_KHR_partial_update
	EGL_GET_PROC_ADDRESS(PFNEGLSETDAMAGEREGIONKHRPROC , SetDamageRegionKHR);
	// EGL_KHR_reusable_sync
	EGL_GET_PROC_ADDRESS(PFNEGLSIGNALSYNCKHRPROC , SignalSyncKHR);
	// EGL_KHR_stream
	EGL_GET_PROC_ADDRESS(PFNEGLCREATESTREAMKHRPROC , CreateStreamKHR);
	EGL_GET_PROC_ADDRESS(PFNEGLDESTROYSTREAMKHRPROC , DestroyStreamKHR);
	EGL_GET_PROC_ADDRESS(PFNEGLQUERYSTREAMKHRPROC , QueryStreamKHR);
	EGL_GET_PROC_ADDRESS(PFNEGLQUERYSTREAMU64KHRPROC , QueryStreamu64KHR);
	EGL_GET_PROC_ADDRESS(PFNEGLSTREAMATTRIBKHRPROC , StreamAttribKHR);
	// EGL_KHR_stream_attrib
	EGL_GET_PROC_ADDRESS(PFNEGLCREATESTREAMATTRIBKHRPROC , CreateStreamAttribKHR);
	EGL_GET_PROC_ADDRESS(PFNEGLQUERYSTREAMATTRIBKHRPROC , QueryStreamAttribKHR);
	EGL_GET_PROC_ADDRESS(PFNEGLSETSTREAMATTRIBKHRPROC , SetStreamAttribKHR);
	EGL_GET_PROC_ADDRESS(PFNEGLSTREAMCONSUMERACQUIREATTRIBKHRPROC , StreamConsumerAcquireAttribKHR);
	EGL_GET_PROC_ADDRESS(PFNEGLSTREAMCONSUMERRELEASEATTRIBKHRPROC , StreamConsumerReleaseAttribKHR);
	// EGL_KHR_stream_consumer_gltexture
	EGL_GET_PROC_ADDRESS(PFNEGLSTREAMCONSUMERACQUIREKHRPROC , StreamConsumerAcquireKHR);
	EGL_GET_PROC_ADDRESS(PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALKHRPROC , StreamConsumerGLTextureExternalKHR);
	EGL_GET_PROC_ADDRESS(PFNEGLSTREAMCONSUMERRELEASEKHRPROC , StreamConsumerReleaseKHR);
	// EGL_KHR_stream_cross_process_fd
	EGL_GET_PROC_ADDRESS(PFNEGLCREATESTREAMFROMFILEDESCRIPTORKHRPROC , CreateStreamFromFileDescriptorKHR);
	EGL_GET_PROC_ADDRESS(PFNEGLGETSTREAMFILEDESCRIPTORKHRPROC , GetStreamFileDescriptorKHR);
	// EGL_KHR_stream_fifo
	EGL_GET_PROC_ADDRESS(PFNEGLQUERYSTREAMTIMEKHRPROC , QueryStreamTimeKHR);
	// EGL_KHR_stream_producer_eglsurface
	EGL_GET_PROC_ADDRESS(PFNEGLCREATESTREAMPRODUCERSURFACEKHRPROC , CreateStreamProducerSurfaceKHR);
	// EGL_KHR_swap_buffers_with_damage
	EGL_GET_PROC_ADDRESS(PFNEGLSWAPBUFFERSWITHDAMAGEKHRPROC , SwapBuffersWithDamageKHR);
	// EGL_KHR_wait_sync
	EGL_GET_PROC_ADDRESS(PFNEGLWAITSYNCKHRPROC , WaitSyncKHR);
	// EGL_MESA_drm_image
	EGL_GET_PROC_ADDRESS(PFNEGLCREATEDRMIMAGEMESAPROC , CreateDRMImageMESA);
	EGL_GET_PROC_ADDRESS(PFNEGLEXPORTDRMIMAGEMESAPROC , ExportDRMImageMESA);
	// EGL_MESA_image_dma_buf_export
	EGL_GET_PROC_ADDRESS(PFNEGLEXPORTDMABUFIMAGEMESAPROC , ExportDMABUFImageMESA);
	EGL_GET_PROC_ADDRESS(PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC , ExportDMABUFImageQueryMESA);
	// EGL_NOK_swap_region
	EGL_GET_PROC_ADDRESS(PFNEGLSWAPBUFFERSREGIONNOKPROC , SwapBuffersRegionNOK);
	// EGL_NOK_swap_region2
	EGL_GET_PROC_ADDRESS(PFNEGLSWAPBUFFERSREGION2NOKPROC , SwapBuffersRegion2NOK);
	// EGL_NV_native_query
	EGL_GET_PROC_ADDRESS(PFNEGLQUERYNATIVEDISPLAYNVPROC , QueryNativeDisplayNV);
	EGL_GET_PROC_ADDRESS(PFNEGLQUERYNATIVEPIXMAPNVPROC , QueryNativePixmapNV);
	EGL_GET_PROC_ADDRESS(PFNEGLQUERYNATIVEWINDOWNVPROC , QueryNativeWindowNV);
	// EGL_NV_post_sub_buffer
	EGL_GET_PROC_ADDRESS(PFNEGLPOSTSUBBUFFERNVPROC , PostSubBufferNV);
	// EGL_NV_stream_consumer_gltexture_yuv
	EGL_GET_PROC_ADDRESS(PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALATTRIBSNVPROC , StreamConsumerGLTextureExternalAttribsNV);
	// EGL_NV_stream_metadata
	EGL_GET_PROC_ADDRESS(PFNEGLQUERYDISPLAYATTRIBNVPROC , QueryDisplayAttribNV);
	EGL_GET_PROC_ADDRESS(PFNEGLQUERYSTREAMMETADATANVPROC , QueryStreamMetadataNV);
	EGL_GET_PROC_ADDRESS(PFNEGLSETSTREAMMETADATANVPROC , SetStreamMetadataNV);
	// EGL_NV_stream_sync
	EGL_GET_PROC_ADDRESS(PFNEGLCREATESTREAMSYNCNVPROC , CreateStreamSyncNV);
	// EGL_NV_sync
	EGL_GET_PROC_ADDRESS(PFNEGLCLIENTWAITSYNCNVPROC , ClientWaitSyncNV);
	EGL_GET_PROC_ADDRESS(PFNEGLCREATEFENCESYNCNVPROC , CreateFenceSyncNV);
	EGL_GET_PROC_ADDRESS(PFNEGLDESTROYSYNCNVPROC , DestroySyncNV);
	EGL_GET_PROC_ADDRESS(PFNEGLFENCENVPROC , FenceNV);
	EGL_GET_PROC_ADDRESS(PFNEGLGETSYNCATTRIBNVPROC , GetSyncAttribNV);
	EGL_GET_PROC_ADDRESS(PFNEGLSIGNALSYNCNVPROC , SignalSyncNV);
	// EGL_NV_system_time
	EGL_GET_PROC_ADDRESS(PFNEGLGETSYSTEMTIMEFREQUENCYNVPROC , GetSystemTimeFrequencyNV);
	EGL_GET_PROC_ADDRESS(PFNEGLGETSYSTEMTIMENVPROC , GetSystemTimeNV);

	// -----------------------------------------------------------------
	// gl2.h
	// -----------------------------------------------------------------

	GET_PROC_ADDRESS(PFNGLACTIVETEXTUREPROC , ActiveTexture);
	GET_PROC_ADDRESS(PFNGLATTACHSHADERPROC , AttachShader);
	GET_PROC_ADDRESS(PFNGLBINDATTRIBLOCATIONPROC , BindAttribLocation);
	GET_PROC_ADDRESS(PFNGLBINDBUFFERPROC , BindBuffer);
	GET_PROC_ADDRESS(PFNGLBINDFRAMEBUFFERPROC , BindFramebuffer);
	GET_PROC_ADDRESS(PFNGLBINDRENDERBUFFERPROC , BindRenderbuffer);
	GET_PROC_ADDRESS(PFNGLBINDTEXTUREPROC , BindTexture);
	GET_PROC_ADDRESS(PFNGLBLENDCOLORPROC , BlendColor);
	GET_PROC_ADDRESS(PFNGLBLENDEQUATIONPROC , BlendEquation);
	GET_PROC_ADDRESS(PFNGLBLENDEQUATIONSEPARATEPROC , BlendEquationSeparate);
	GET_PROC_ADDRESS(PFNGLBLENDFUNCPROC , BlendFunc);
	GET_PROC_ADDRESS(PFNGLBLENDFUNCSEPARATEPROC , BlendFuncSeparate);
	GET_PROC_ADDRESS(PFNGLBUFFERDATAPROC , BufferData);
	GET_PROC_ADDRESS(PFNGLBUFFERSUBDATAPROC , BufferSubData);
	GET_PROC_ADDRESS(PFNGLCHECKFRAMEBUFFERSTATUSPROC , CheckFramebufferStatus);
	GET_PROC_ADDRESS(PFNGLCLEARPROC , Clear);
	GET_PROC_ADDRESS(PFNGLCLEARCOLORPROC , ClearColor);
	GET_PROC_ADDRESS(PFNGLCLEARDEPTHFPROC , ClearDepthf);
	GET_PROC_ADDRESS(PFNGLCLEARSTENCILPROC , ClearStencil);
	GET_PROC_ADDRESS(PFNGLCOLORMASKPROC , ColorMask);
	GET_PROC_ADDRESS(PFNGLCOMPILESHADERPROC , CompileShader);
	GET_PROC_ADDRESS(PFNGLCOMPRESSEDTEXIMAGE2DPROC , CompressedTexImage2D);
	GET_PROC_ADDRESS(PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC , CompressedTexSubImage2D);
	GET_PROC_ADDRESS(PFNGLCOPYTEXIMAGE2DPROC , CopyTexImage2D);
	GET_PROC_ADDRESS(PFNGLCOPYTEXSUBIMAGE2DPROC , CopyTexSubImage2D);
	GET_PROC_ADDRESS(PFNGLCREATEPROGRAMPROC , CreateProgram);
	GET_PROC_ADDRESS(PFNGLCREATESHADERPROC , CreateShader);
	GET_PROC_ADDRESS(PFNGLCULLFACEPROC , CullFace);
	GET_PROC_ADDRESS(PFNGLDELETEBUFFERSPROC , DeleteBuffers);
	GET_PROC_ADDRESS(PFNGLDELETEFRAMEBUFFERSPROC , DeleteFramebuffers);
	GET_PROC_ADDRESS(PFNGLDELETEPROGRAMPROC , DeleteProgram);
	GET_PROC_ADDRESS(PFNGLDELETERENDERBUFFERSPROC , DeleteRenderbuffers);
	GET_PROC_ADDRESS(PFNGLDELETESHADERPROC , DeleteShader);
	GET_PROC_ADDRESS(PFNGLDELETETEXTURESPROC , DeleteTextures);
	GET_PROC_ADDRESS(PFNGLDEPTHFUNCPROC , DepthFunc);
	GET_PROC_ADDRESS(PFNGLDEPTHMASKPROC , DepthMask);
	GET_PROC_ADDRESS(PFNGLDEPTHRANGEFPROC , DepthRangef);
	GET_PROC_ADDRESS(PFNGLDETACHSHADERPROC , DetachShader);
	GET_PROC_ADDRESS(PFNGLDISABLEPROC , Disable);
	GET_PROC_ADDRESS(PFNGLDISABLEVERTEXATTRIBARRAYPROC , DisableVertexAttribArray);
	GET_PROC_ADDRESS(PFNGLDRAWARRAYSPROC , DrawArrays);
	GET_PROC_ADDRESS(PFNGLDRAWELEMENTSPROC , DrawElements);
	GET_PROC_ADDRESS(PFNGLENABLEPROC , Enable);
	GET_PROC_ADDRESS(PFNGLENABLEVERTEXATTRIBARRAYPROC , EnableVertexAttribArray);
	GET_PROC_ADDRESS(PFNGLFINISHPROC , Finish);
	GET_PROC_ADDRESS(PFNGLFLUSHPROC , Flush);
	GET_PROC_ADDRESS(PFNGLFRAMEBUFFERRENDERBUFFERPROC , FramebufferRenderbuffer);
	GET_PROC_ADDRESS(PFNGLFRAMEBUFFERTEXTURE2DPROC , FramebufferTexture2D);
	GET_PROC_ADDRESS(PFNGLFRONTFACEPROC , FrontFace);
	GET_PROC_ADDRESS(PFNGLGENBUFFERSPROC , GenBuffers);
	GET_PROC_ADDRESS(PFNGLGENFRAMEBUFFERSPROC , GenFramebuffers);
	GET_PROC_ADDRESS(PFNGLGENRENDERBUFFERSPROC , GenRenderbuffers);
	GET_PROC_ADDRESS(PFNGLGENTEXTURESPROC , GenTextures);
	GET_PROC_ADDRESS(PFNGLGENERATEMIPMAPPROC , GenerateMipmap);
	GET_PROC_ADDRESS(PFNGLGETACTIVEATTRIBPROC , GetActiveAttrib);
	GET_PROC_ADDRESS(PFNGLGETACTIVEUNIFORMPROC , GetActiveUniform);
	GET_PROC_ADDRESS(PFNGLGETATTACHEDSHADERSPROC , GetAttachedShaders);
	GET_PROC_ADDRESS(PFNGLGETATTRIBLOCATIONPROC , GetAttribLocation);
	GET_PROC_ADDRESS(PFNGLGETBOOLEANVPROC , GetBooleanv);
	GET_PROC_ADDRESS(PFNGLGETBUFFERPARAMETERIVPROC , GetBufferParameteriv);
	GET_PROC_ADDRESS(PFNGLGETERRORPROC , GetError);
	GET_PROC_ADDRESS(PFNGLGETFLOATVPROC , GetFloatv);
	GET_PROC_ADDRESS(PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC , GetFramebufferAttachmentParameteriv);
	GET_PROC_ADDRESS(PFNGLGETINTEGERVPROC , GetIntegerv);
	GET_PROC_ADDRESS(PFNGLGETPROGRAMINFOLOGPROC , GetProgramInfoLog);
	GET_PROC_ADDRESS(PFNGLGETPROGRAMIVPROC , GetProgramiv);
	GET_PROC_ADDRESS(PFNGLGETRENDERBUFFERPARAMETERIVPROC , GetRenderbufferParameteriv);
	GET_PROC_ADDRESS(PFNGLGETSHADERINFOLOGPROC , GetShaderInfoLog);
	GET_PROC_ADDRESS(PFNGLGETSHADERPRECISIONFORMATPROC , GetShaderPrecisionFormat);
	GET_PROC_ADDRESS(PFNGLGETSHADERSOURCEPROC , GetShaderSource);
	GET_PROC_ADDRESS(PFNGLGETSHADERIVPROC , GetShaderiv);
	GET_PROC_ADDRESS(PFNGLGETSTRINGPROC , GetString);
	GET_PROC_ADDRESS(PFNGLGETTEXPARAMETERFVPROC , GetTexParameterfv);
	GET_PROC_ADDRESS(PFNGLGETTEXPARAMETERIVPROC , GetTexParameteriv);
	GET_PROC_ADDRESS(PFNGLGETUNIFORMLOCATIONPROC , GetUniformLocation);
	GET_PROC_ADDRESS(PFNGLGETUNIFORMFVPROC , GetUniformfv);
	GET_PROC_ADDRESS(PFNGLGETUNIFORMIVPROC , GetUniformiv);
	GET_PROC_ADDRESS(PFNGLGETVERTEXATTRIBPOINTERVPROC , GetVertexAttribPointerv);
	GET_PROC_ADDRESS(PFNGLGETVERTEXATTRIBFVPROC , GetVertexAttribfv);
	GET_PROC_ADDRESS(PFNGLGETVERTEXATTRIBIVPROC , GetVertexAttribiv);
	GET_PROC_ADDRESS(PFNGLHINTPROC , Hint);
	GET_PROC_ADDRESS(PFNGLISBUFFERPROC , IsBuffer);
	GET_PROC_ADDRESS(PFNGLISENABLEDPROC , IsEnabled);
	GET_PROC_ADDRESS(PFNGLISFRAMEBUFFERPROC , IsFramebuffer);
	GET_PROC_ADDRESS(PFNGLISPROGRAMPROC , IsProgram);
	GET_PROC_ADDRESS(PFNGLISRENDERBUFFERPROC , IsRenderbuffer);
	GET_PROC_ADDRESS(PFNGLISSHADERPROC , IsShader);
	GET_PROC_ADDRESS(PFNGLISTEXTUREPROC , IsTexture);
	GET_PROC_ADDRESS(PFNGLLINEWIDTHPROC , LineWidth);
	GET_PROC_ADDRESS(PFNGLLINKPROGRAMPROC , LinkProgram);
	GET_PROC_ADDRESS(PFNGLPIXELSTOREIPROC , PixelStorei);
	GET_PROC_ADDRESS(PFNGLPOLYGONOFFSETPROC , PolygonOffset);
	GET_PROC_ADDRESS(PFNGLREADPIXELSPROC , ReadPixels);
	GET_PROC_ADDRESS(PFNGLRELEASESHADERCOMPILERPROC , ReleaseShaderCompiler);
	GET_PROC_ADDRESS(PFNGLRENDERBUFFERSTORAGEPROC , RenderbufferStorage);
	GET_PROC_ADDRESS(PFNGLSAMPLECOVERAGEPROC , SampleCoverage);
	GET_PROC_ADDRESS(PFNGLSCISSORPROC , Scissor);
	GET_PROC_ADDRESS(PFNGLSHADERBINARYPROC , ShaderBinary);
	GET_PROC_ADDRESS(PFNGLSHADERSOURCEPROC , ShaderSource);
	GET_PROC_ADDRESS(PFNGLSTENCILFUNCPROC , StencilFunc);
	GET_PROC_ADDRESS(PFNGLSTENCILFUNCSEPARATEPROC , StencilFuncSeparate);
	GET_PROC_ADDRESS(PFNGLSTENCILMASKPROC , StencilMask);
	GET_PROC_ADDRESS(PFNGLSTENCILMASKSEPARATEPROC , StencilMaskSeparate);
	GET_PROC_ADDRESS(PFNGLSTENCILOPPROC , StencilOp);
	GET_PROC_ADDRESS(PFNGLSTENCILOPSEPARATEPROC , StencilOpSeparate);
	GET_PROC_ADDRESS(PFNGLTEXIMAGE2DPROC , TexImage2D);
	GET_PROC_ADDRESS(PFNGLTEXPARAMETERFPROC , TexParameterf);
	GET_PROC_ADDRESS(PFNGLTEXPARAMETERFVPROC , TexParameterfv);
	GET_PROC_ADDRESS(PFNGLTEXPARAMETERIPROC , TexParameteri);
	GET_PROC_ADDRESS(PFNGLTEXPARAMETERIVPROC , TexParameteriv);
	GET_PROC_ADDRESS(PFNGLTEXSUBIMAGE2DPROC , TexSubImage2D);
	GET_PROC_ADDRESS(PFNGLUNIFORM1FPROC , Uniform1f);
	GET_PROC_ADDRESS(PFNGLUNIFORM1FVPROC , Uniform1fv);
	GET_PROC_ADDRESS(PFNGLUNIFORM1IPROC , Uniform1i);
	GET_PROC_ADDRESS(PFNGLUNIFORM1IVPROC , Uniform1iv);
	GET_PROC_ADDRESS(PFNGLUNIFORM2FPROC , Uniform2f);
	GET_PROC_ADDRESS(PFNGLUNIFORM2FVPROC , Uniform2fv);
	GET_PROC_ADDRESS(PFNGLUNIFORM2IPROC , Uniform2i);
	GET_PROC_ADDRESS(PFNGLUNIFORM2IVPROC , Uniform2iv);
	GET_PROC_ADDRESS(PFNGLUNIFORM3FPROC , Uniform3f);
	GET_PROC_ADDRESS(PFNGLUNIFORM3FVPROC , Uniform3fv);
	GET_PROC_ADDRESS(PFNGLUNIFORM3IPROC , Uniform3i);
	GET_PROC_ADDRESS(PFNGLUNIFORM3IVPROC , Uniform3iv);
	GET_PROC_ADDRESS(PFNGLUNIFORM4FPROC , Uniform4f);
	GET_PROC_ADDRESS(PFNGLUNIFORM4FVPROC , Uniform4fv);
	GET_PROC_ADDRESS(PFNGLUNIFORM4IPROC , Uniform4i);
	GET_PROC_ADDRESS(PFNGLUNIFORM4IVPROC , Uniform4iv);
	GET_PROC_ADDRESS(PFNGLUNIFORMMATRIX2FVPROC , UniformMatrix2fv);
	GET_PROC_ADDRESS(PFNGLUNIFORMMATRIX3FVPROC , UniformMatrix3fv);
	GET_PROC_ADDRESS(PFNGLUNIFORMMATRIX4FVPROC , UniformMatrix4fv);
	GET_PROC_ADDRESS(PFNGLUSEPROGRAMPROC , UseProgram);
	GET_PROC_ADDRESS(PFNGLVALIDATEPROGRAMPROC , ValidateProgram);
	GET_PROC_ADDRESS(PFNGLVERTEXATTRIB1FPROC , VertexAttrib1f);
	GET_PROC_ADDRESS(PFNGLVERTEXATTRIB1FVPROC , VertexAttrib1fv);
	GET_PROC_ADDRESS(PFNGLVERTEXATTRIB2FPROC , VertexAttrib2f);
	GET_PROC_ADDRESS(PFNGLVERTEXATTRIB2FVPROC , VertexAttrib2fv);
	GET_PROC_ADDRESS(PFNGLVERTEXATTRIB3FPROC , VertexAttrib3f);
	GET_PROC_ADDRESS(PFNGLVERTEXATTRIB3FVPROC , VertexAttrib3fv);
	GET_PROC_ADDRESS(PFNGLVERTEXATTRIB4FPROC , VertexAttrib4f);
	GET_PROC_ADDRESS(PFNGLVERTEXATTRIB4FVPROC , VertexAttrib4fv);
	GET_PROC_ADDRESS(PFNGLVERTEXATTRIBPOINTERPROC , VertexAttribPointer);
	GET_PROC_ADDRESS(PFNGLVIEWPORTPROC , Viewport);

	// -----------------------------------------------------------------
	// gl2ext.h"
	// -----------------------------------------------------------------

	// GL_AMD_performance_monitor
	GET_PROC_ADDRESS(PFNGLBEGINPERFMONITORAMDPROC , BeginPerfMonitorAMD);
	GET_PROC_ADDRESS(PFNGLDELETEPERFMONITORSAMDPROC , DeletePerfMonitorsAMD);
	GET_PROC_ADDRESS(PFNGLENDPERFMONITORAMDPROC , EndPerfMonitorAMD);
	GET_PROC_ADDRESS(PFNGLGENPERFMONITORSAMDPROC , GenPerfMonitorsAMD);
	GET_PROC_ADDRESS(PFNGLGETPERFMONITORCOUNTERDATAAMDPROC , GetPerfMonitorCounterDataAMD);
	GET_PROC_ADDRESS(PFNGLGETPERFMONITORCOUNTERINFOAMDPROC , GetPerfMonitorCounterInfoAMD);
	GET_PROC_ADDRESS(PFNGLGETPERFMONITORCOUNTERSTRINGAMDPROC , GetPerfMonitorCounterStringAMD);
	GET_PROC_ADDRESS(PFNGLGETPERFMONITORCOUNTERSAMDPROC , GetPerfMonitorCountersAMD);
	GET_PROC_ADDRESS(PFNGLGETPERFMONITORGROUPSTRINGAMDPROC , GetPerfMonitorGroupStringAMD);
	GET_PROC_ADDRESS(PFNGLGETPERFMONITORGROUPSAMDPROC , GetPerfMonitorGroupsAMD);
	GET_PROC_ADDRESS(PFNGLSELECTPERFMONITORCOUNTERSAMDPROC , SelectPerfMonitorCountersAMD);
	// GL_ANGLE_framebuffer_blit
	GET_PROC_ADDRESS(PFNGLBLITFRAMEBUFFERANGLEPROC , BlitFramebufferANGLE);
	// GL_ANGLE_framebuffer_multisample
	GET_PROC_ADDRESS(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEANGLEPROC , RenderbufferStorageMultisampleANGLE);
	// GL_ANGLE_instanced_arrays
	GET_PROC_ADDRESS(PFNGLDRAWARRAYSINSTANCEDANGLEPROC , DrawArraysInstancedANGLE);
	GET_PROC_ADDRESS(PFNGLDRAWELEMENTSINSTANCEDANGLEPROC , DrawElementsInstancedANGLE);
	GET_PROC_ADDRESS(PFNGLVERTEXATTRIBDIVISORANGLEPROC , VertexAttribDivisorANGLE);
	// GL_ANGLE_translated_shader_source
	GET_PROC_ADDRESS(PFNGLGETTRANSLATEDSHADERSOURCEANGLEPROC , GetTranslatedShaderSourceANGLE);
	// GL_APPLE_copy_texture_levels
	GET_PROC_ADDRESS(PFNGLCOPYTEXTURELEVELSAPPLEPROC , CopyTextureLevelsAPPLE);
	// GL_APPLE_framebuffer_multisample
	GET_PROC_ADDRESS(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEAPPLEPROC , RenderbufferStorageMultisampleAPPLE);
	GET_PROC_ADDRESS(PFNGLRESOLVEMULTISAMPLEFRAMEBUFFERAPPLEPROC , ResolveMultisampleFramebufferAPPLE);
	// GL_APPLE_sync
	GET_PROC_ADDRESS(PFNGLCLIENTWAITSYNCAPPLEPROC , ClientWaitSyncAPPLE);
	GET_PROC_ADDRESS(PFNGLDELETESYNCAPPLEPROC , DeleteSyncAPPLE);
	GET_PROC_ADDRESS(PFNGLFENCESYNCAPPLEPROC , FenceSyncAPPLE);
	GET_PROC_ADDRESS(PFNGLGETINTEGER64VAPPLEPROC , GetInteger64vAPPLE);
	GET_PROC_ADDRESS(PFNGLGETSYNCIVAPPLEPROC , GetSyncivAPPLE);
	GET_PROC_ADDRESS(PFNGLISSYNCAPPLEPROC , IsSyncAPPLE);
	GET_PROC_ADDRESS(PFNGLWAITSYNCAPPLEPROC , WaitSyncAPPLE);
	// GL_EXT_base_instance
	GET_PROC_ADDRESS(PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEEXTPROC , DrawArraysInstancedBaseInstanceEXT);
	GET_PROC_ADDRESS(PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEEXTPROC , DrawElementsInstancedBaseInstanceEXT);
	GET_PROC_ADDRESS(PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEEXTPROC , DrawElementsInstancedBaseVertexBaseInstanceEXT);
	// GL_EXT_blend_func_extended
	GET_PROC_ADDRESS(PFNGLBINDFRAGDATALOCATIONEXTPROC , BindFragDataLocationEXT);
	GET_PROC_ADDRESS(PFNGLBINDFRAGDATALOCATIONINDEXEDEXTPROC , BindFragDataLocationIndexedEXT);
	GET_PROC_ADDRESS(PFNGLGETFRAGDATAINDEXEXTPROC , GetFragDataIndexEXT);
	GET_PROC_ADDRESS(PFNGLGETPROGRAMRESOURCELOCATIONINDEXEXTPROC , GetProgramResourceLocationIndexEXT);
	// GL_EXT_buffer_storage
	GET_PROC_ADDRESS(PFNGLBUFFERSTORAGEEXTPROC , BufferStorageEXT);
	// GL_EXT_copy_image
	GET_PROC_ADDRESS(PFNGLCOPYIMAGESUBDATAEXTPROC , CopyImageSubDataEXT);
	// GL_EXT_debug_label
	GET_PROC_ADDRESS(PFNGLGETOBJECTLABELEXTPROC , GetObjectLabelEXT);
	GET_PROC_ADDRESS(PFNGLLABELOBJECTEXTPROC , LabelObjectEXT);
	// GL_EXT_debug_marker
	GET_PROC_ADDRESS(PFNGLINSERTEVENTMARKEREXTPROC , InsertEventMarkerEXT);
	GET_PROC_ADDRESS(PFNGLPOPGROUPMARKEREXTPROC , PopGroupMarkerEXT);
	GET_PROC_ADDRESS(PFNGLPUSHGROUPMARKEREXTPROC , PushGroupMarkerEXT);
	// GL_EXT_discard_framebuffer
	GET_PROC_ADDRESS(PFNGLDISCARDFRAMEBUFFEREXTPROC , DiscardFramebufferEXT);
	// GL_EXT_disjoint_timer_query
	GET_PROC_ADDRESS(PFNGLBEGINQUERYEXTPROC , BeginQueryEXT);
	GET_PROC_ADDRESS(PFNGLDELETEQUERIESEXTPROC , DeleteQueriesEXT);
	GET_PROC_ADDRESS(PFNGLENDQUERYEXTPROC , EndQueryEXT);
	GET_PROC_ADDRESS(PFNGLGENQUERIESEXTPROC , GenQueriesEXT);
	GET_PROC_ADDRESS(PFNGLGETQUERYOBJECTI64VEXTPROC , GetQueryObjecti64vEXT);
	GET_PROC_ADDRESS(PFNGLGETQUERYOBJECTIVEXTPROC , GetQueryObjectivEXT);
	GET_PROC_ADDRESS(PFNGLGETQUERYOBJECTUI64VEXTPROC , GetQueryObjectui64vEXT);
	GET_PROC_ADDRESS(PFNGLGETQUERYOBJECTUIVEXTPROC , GetQueryObjectuivEXT);
	GET_PROC_ADDRESS(PFNGLGETQUERYIVEXTPROC , GetQueryivEXT);
	GET_PROC_ADDRESS(PFNGLISQUERYEXTPROC , IsQueryEXT);
	GET_PROC_ADDRESS(PFNGLQUERYCOUNTEREXTPROC , QueryCounterEXT);
	// GL_EXT_draw_buffers
	GET_PROC_ADDRESS(PFNGLDRAWBUFFERSEXTPROC , DrawBuffersEXT);
	// GL_EXT_draw_buffers_indexed
	GET_PROC_ADDRESS(PFNGLBLENDEQUATIONSEPARATEIEXTPROC , BlendEquationSeparateiEXT);
	GET_PROC_ADDRESS(PFNGLBLENDEQUATIONIEXTPROC , BlendEquationiEXT);
	GET_PROC_ADDRESS(PFNGLBLENDFUNCSEPARATEIEXTPROC , BlendFuncSeparateiEXT);
	GET_PROC_ADDRESS(PFNGLBLENDFUNCIEXTPROC , BlendFunciEXT);
	GET_PROC_ADDRESS(PFNGLCOLORMASKIEXTPROC , ColorMaskiEXT);
	GET_PROC_ADDRESS(PFNGLDISABLEIEXTPROC , DisableiEXT);
	GET_PROC_ADDRESS(PFNGLENABLEIEXTPROC , EnableiEXT);
	GET_PROC_ADDRESS(PFNGLISENABLEDIEXTPROC , IsEnablediEXT);
	// GL_EXT_draw_elements_base_vertex
	GET_PROC_ADDRESS(PFNGLDRAWELEMENTSBASEVERTEXEXTPROC , DrawElementsBaseVertexEXT);
	GET_PROC_ADDRESS(PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXEXTPROC , DrawElementsInstancedBaseVertexEXT);
	GET_PROC_ADDRESS(PFNGLDRAWRANGEELEMENTSBASEVERTEXEXTPROC , DrawRangeElementsBaseVertexEXT);
	GET_PROC_ADDRESS(PFNGLMULTIDRAWELEMENTSBASEVERTEXEXTPROC , MultiDrawElementsBaseVertexEXT);
	// GL_EXT_draw_instanced
	GET_PROC_ADDRESS(PFNGLDRAWARRAYSINSTANCEDEXTPROC , DrawArraysInstancedEXT);
	GET_PROC_ADDRESS(PFNGLDRAWELEMENTSINSTANCEDEXTPROC , DrawElementsInstancedEXT);
	// GL_EXT_geometry_shader
	GET_PROC_ADDRESS(PFNGLFRAMEBUFFERTEXTUREEXTPROC , FramebufferTextureEXT);
	// GL_EXT_instanced_arrays
	GET_PROC_ADDRESS(PFNGLVERTEXATTRIBDIVISOREXTPROC , VertexAttribDivisorEXT);
	// GL_EXT_map_buffer_range
	GET_PROC_ADDRESS(PFNGLFLUSHMAPPEDBUFFERRANGEEXTPROC , FlushMappedBufferRangeEXT);
	GET_PROC_ADDRESS(PFNGLMAPBUFFERRANGEEXTPROC , MapBufferRangeEXT);
	// GL_EXT_multi_draw_arrays
	GET_PROC_ADDRESS(PFNGLMULTIDRAWARRAYSEXTPROC , MultiDrawArraysEXT);
	GET_PROC_ADDRESS(PFNGLMULTIDRAWELEMENTSEXTPROC , MultiDrawElementsEXT);
	// GL_EXT_multi_draw_indirect
	GET_PROC_ADDRESS(PFNGLMULTIDRAWARRAYSINDIRECTEXTPROC , MultiDrawArraysIndirectEXT);
	GET_PROC_ADDRESS(PFNGLMULTIDRAWELEMENTSINDIRECTEXTPROC , MultiDrawElementsIndirectEXT);
	// GL_EXT_multisampled_render_to_texture
	GET_PROC_ADDRESS(PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC , FramebufferTexture2DMultisampleEXT);
	GET_PROC_ADDRESS(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC , RenderbufferStorageMultisampleEXT);
	// GL_EXT_multiview_draw_buffers
	GET_PROC_ADDRESS(PFNGLDRAWBUFFERSINDEXEDEXTPROC , DrawBuffersIndexedEXT);
	GET_PROC_ADDRESS(PFNGLGETINTEGERI_VEXTPROC , GetIntegeri_vEXT);
	GET_PROC_ADDRESS(PFNGLREADBUFFERINDEXEDEXTPROC , ReadBufferIndexedEXT);
	// GL_EXT_polygon_offset_clamp
	GET_PROC_ADDRESS(PFNGLPOLYGONOFFSETCLAMPEXTPROC , PolygonOffsetClampEXT);
	// GL_EXT_primitive_bounding_box
	GET_PROC_ADDRESS(PFNGLPRIMITIVEBOUNDINGBOXEXTPROC , PrimitiveBoundingBoxEXT);
	// GL_EXT_raster_multisample
	GET_PROC_ADDRESS(PFNGLRASTERSAMPLESEXTPROC , RasterSamplesEXT);
	// GL_EXT_robustness
	GET_PROC_ADDRESS(PFNGLGETGRAPHICSRESETSTATUSEXTPROC , GetGraphicsResetStatusEXT);
	GET_PROC_ADDRESS(PFNGLGETNUNIFORMFVEXTPROC , GetnUniformfvEXT);
	GET_PROC_ADDRESS(PFNGLGETNUNIFORMIVEXTPROC , GetnUniformivEXT);
	GET_PROC_ADDRESS(PFNGLREADNPIXELSEXTPROC , ReadnPixelsEXT);
	// GL_EXT_separate_shader_objects
	GET_PROC_ADDRESS(PFNGLACTIVESHADERPROGRAMEXTPROC , ActiveShaderProgramEXT);
	GET_PROC_ADDRESS(PFNGLBINDPROGRAMPIPELINEEXTPROC , BindProgramPipelineEXT);
	GET_PROC_ADDRESS(PFNGLCREATESHADERPROGRAMVEXTPROC , CreateShaderProgramvEXT);
	GET_PROC_ADDRESS(PFNGLDELETEPROGRAMPIPELINESEXTPROC , DeleteProgramPipelinesEXT);
	GET_PROC_ADDRESS(PFNGLGENPROGRAMPIPELINESEXTPROC , GenProgramPipelinesEXT);
	GET_PROC_ADDRESS(PFNGLGETPROGRAMPIPELINEINFOLOGEXTPROC , GetProgramPipelineInfoLogEXT);
	GET_PROC_ADDRESS(PFNGLGETPROGRAMPIPELINEIVEXTPROC , GetProgramPipelineivEXT);
	GET_PROC_ADDRESS(PFNGLISPROGRAMPIPELINEEXTPROC , IsProgramPipelineEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMPARAMETERIEXTPROC , ProgramParameteriEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORM1FEXTPROC , ProgramUniform1fEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORM1FVEXTPROC , ProgramUniform1fvEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORM1IEXTPROC , ProgramUniform1iEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORM1IVEXTPROC , ProgramUniform1ivEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORM1UIEXTPROC , ProgramUniform1uiEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORM1UIVEXTPROC , ProgramUniform1uivEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORM2FEXTPROC , ProgramUniform2fEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORM2FVEXTPROC , ProgramUniform2fvEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORM2IEXTPROC , ProgramUniform2iEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORM2IVEXTPROC , ProgramUniform2ivEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORM2UIEXTPROC , ProgramUniform2uiEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORM2UIVEXTPROC , ProgramUniform2uivEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORM3FEXTPROC , ProgramUniform3fEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORM3FVEXTPROC , ProgramUniform3fvEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORM3IEXTPROC , ProgramUniform3iEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORM3IVEXTPROC , ProgramUniform3ivEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORM3UIEXTPROC , ProgramUniform3uiEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORM3UIVEXTPROC , ProgramUniform3uivEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORM4FEXTPROC , ProgramUniform4fEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORM4FVEXTPROC , ProgramUniform4fvEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORM4IEXTPROC , ProgramUniform4iEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORM4IVEXTPROC , ProgramUniform4ivEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORM4UIEXTPROC , ProgramUniform4uiEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORM4UIVEXTPROC , ProgramUniform4uivEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORMMATRIX2FVEXTPROC , ProgramUniformMatrix2fvEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORMMATRIX2X3FVEXTPROC , ProgramUniformMatrix2x3fvEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORMMATRIX2X4FVEXTPROC , ProgramUniformMatrix2x4fvEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORMMATRIX3FVEXTPROC , ProgramUniformMatrix3fvEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORMMATRIX3X2FVEXTPROC , ProgramUniformMatrix3x2fvEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORMMATRIX3X4FVEXTPROC , ProgramUniformMatrix3x4fvEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORMMATRIX4FVEXTPROC , ProgramUniformMatrix4fvEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORMMATRIX4X2FVEXTPROC , ProgramUniformMatrix4x2fvEXT);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORMMATRIX4X3FVEXTPROC , ProgramUniformMatrix4x3fvEXT);
	GET_PROC_ADDRESS(PFNGLUSEPROGRAMSTAGESEXTPROC , UseProgramStagesEXT);
	GET_PROC_ADDRESS(PFNGLVALIDATEPROGRAMPIPELINEEXTPROC , ValidateProgramPipelineEXT);
	// GL_EXT_shader_pixel_local_storage2
	GET_PROC_ADDRESS(PFNGLCLEARPIXELLOCALSTORAGEUIEXTPROC , ClearPixelLocalStorageuiEXT);
	GET_PROC_ADDRESS(PFNGLFRAMEBUFFERPIXELLOCALSTORAGESIZEEXTPROC , FramebufferPixelLocalStorageSizeEXT);
	GET_PROC_ADDRESS(PFNGLGETFRAMEBUFFERPIXELLOCALSTORAGESIZEEXTPROC , GetFramebufferPixelLocalStorageSizeEXT);
	// GL_EXT_sparse_texture
	GET_PROC_ADDRESS(PFNGLTEXPAGECOMMITMENTEXTPROC , TexPageCommitmentEXT);
	// GL_EXT_tessellation_shader
	GET_PROC_ADDRESS(PFNGLPATCHPARAMETERIEXTPROC , PatchParameteriEXT);
	// GL_EXT_texture_border_clamp
	GET_PROC_ADDRESS(PFNGLGETSAMPLERPARAMETERIIVEXTPROC , GetSamplerParameterIivEXT);
	GET_PROC_ADDRESS(PFNGLGETSAMPLERPARAMETERIUIVEXTPROC , GetSamplerParameterIuivEXT);
	GET_PROC_ADDRESS(PFNGLGETTEXPARAMETERIIVEXTPROC , GetTexParameterIivEXT);
	GET_PROC_ADDRESS(PFNGLGETTEXPARAMETERIUIVEXTPROC , GetTexParameterIuivEXT);
	GET_PROC_ADDRESS(PFNGLSAMPLERPARAMETERIIVEXTPROC , SamplerParameterIivEXT);
	GET_PROC_ADDRESS(PFNGLSAMPLERPARAMETERIUIVEXTPROC , SamplerParameterIuivEXT);
	GET_PROC_ADDRESS(PFNGLTEXPARAMETERIIVEXTPROC , TexParameterIivEXT);
	GET_PROC_ADDRESS(PFNGLTEXPARAMETERIUIVEXTPROC , TexParameterIuivEXT);
	// GL_EXT_texture_buffer
	GET_PROC_ADDRESS(PFNGLTEXBUFFEREXTPROC , TexBufferEXT);
	GET_PROC_ADDRESS(PFNGLTEXBUFFERRANGEEXTPROC , TexBufferRangeEXT);
	// GL_EXT_texture_storage
	GET_PROC_ADDRESS(PFNGLTEXSTORAGE1DEXTPROC , TexStorage1DEXT);
	GET_PROC_ADDRESS(PFNGLTEXSTORAGE2DEXTPROC , TexStorage2DEXT);
	GET_PROC_ADDRESS(PFNGLTEXSTORAGE3DEXTPROC , TexStorage3DEXT);
	GET_PROC_ADDRESS(PFNGLTEXTURESTORAGE1DEXTPROC , TextureStorage1DEXT);
	GET_PROC_ADDRESS(PFNGLTEXTURESTORAGE2DEXTPROC , TextureStorage2DEXT);
	GET_PROC_ADDRESS(PFNGLTEXTURESTORAGE3DEXTPROC , TextureStorage3DEXT);
	// GL_EXT_texture_view
	GET_PROC_ADDRESS(PFNGLTEXTUREVIEWEXTPROC , TextureViewEXT);
	// GL_IMG_framebuffer_downsample
	GET_PROC_ADDRESS(PFNGLFRAMEBUFFERTEXTURE2DDOWNSAMPLEIMGPROC , FramebufferTexture2DDownsampleIMG);
	GET_PROC_ADDRESS(PFNGLFRAMEBUFFERTEXTURELAYERDOWNSAMPLEIMGPROC , FramebufferTextureLayerDownsampleIMG);
	// GL_IMG_multisampled_render_to_texture
	GET_PROC_ADDRESS(PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEIMGPROC , FramebufferTexture2DMultisampleIMG);
	GET_PROC_ADDRESS(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEIMGPROC , RenderbufferStorageMultisampleIMG);
	// GL_INTEL_framebuffer_CMAA
	GET_PROC_ADDRESS(PFNGLAPPLYFRAMEBUFFERATTACHMENTCMAAINTELPROC , ApplyFramebufferAttachmentCMAAINTEL);
	// GL_INTEL_performance_query
	GET_PROC_ADDRESS(PFNGLBEGINPERFQUERYINTELPROC , BeginPerfQueryINTEL);
	GET_PROC_ADDRESS(PFNGLCREATEPERFQUERYINTELPROC , CreatePerfQueryINTEL);
	GET_PROC_ADDRESS(PFNGLDELETEPERFQUERYINTELPROC , DeletePerfQueryINTEL);
	GET_PROC_ADDRESS(PFNGLENDPERFQUERYINTELPROC , EndPerfQueryINTEL);
	GET_PROC_ADDRESS(PFNGLGETFIRSTPERFQUERYIDINTELPROC , GetFirstPerfQueryIdINTEL);
	GET_PROC_ADDRESS(PFNGLGETNEXTPERFQUERYIDINTELPROC , GetNextPerfQueryIdINTEL);
	GET_PROC_ADDRESS(PFNGLGETPERFCOUNTERINFOINTELPROC , GetPerfCounterInfoINTEL);
	GET_PROC_ADDRESS(PFNGLGETPERFQUERYDATAINTELPROC , GetPerfQueryDataINTEL);
	GET_PROC_ADDRESS(PFNGLGETPERFQUERYIDBYNAMEINTELPROC , GetPerfQueryIdByNameINTEL);
	GET_PROC_ADDRESS(PFNGLGETPERFQUERYINFOINTELPROC , GetPerfQueryInfoINTEL);
	// GL_KHR_blend_equation_advanced
	GET_PROC_ADDRESS(PFNGLBLENDBARRIERKHRPROC , BlendBarrierKHR);
	// GL_KHR_debug
	GET_PROC_ADDRESS(PFNGLDEBUGMESSAGECONTROLKHRPROC , DebugMessageControlKHR);
	GET_PROC_ADDRESS(PFNGLDEBUGMESSAGEINSERTKHRPROC , DebugMessageInsertKHR);
	GET_PROC_ADDRESS(PFNGLGETDEBUGMESSAGELOGKHRPROC, GetDebugMessageLogKHR);
	GET_PROC_ADDRESS(PFNGLDEBUGMESSAGECALLBACKKHRPROC, DebugMessageCallbackKHR);

	GET_PROC_ADDRESS(PFNGLGETOBJECTLABELKHRPROC , GetObjectLabelKHR);
	GET_PROC_ADDRESS(PFNGLGETOBJECTPTRLABELKHRPROC , GetObjectPtrLabelKHR);
	GET_PROC_ADDRESS(PFNGLGETPOINTERVKHRPROC , GetPointervKHR);
	GET_PROC_ADDRESS(PFNGLOBJECTLABELKHRPROC , ObjectLabelKHR);
	GET_PROC_ADDRESS(PFNGLOBJECTPTRLABELKHRPROC , ObjectPtrLabelKHR);
	GET_PROC_ADDRESS(PFNGLPOPDEBUGGROUPKHRPROC , PopDebugGroupKHR);
	GET_PROC_ADDRESS(PFNGLPUSHDEBUGGROUPKHRPROC , PushDebugGroupKHR);
	// GL_KHR_robustness
	GET_PROC_ADDRESS(PFNGLGETGRAPHICSRESETSTATUSKHRPROC , GetGraphicsResetStatusKHR);
	GET_PROC_ADDRESS(PFNGLGETNUNIFORMFVKHRPROC , GetnUniformfvKHR);
	GET_PROC_ADDRESS(PFNGLGETNUNIFORMIVKHRPROC , GetnUniformivKHR);
	GET_PROC_ADDRESS(PFNGLGETNUNIFORMUIVKHRPROC , GetnUniformuivKHR);
	GET_PROC_ADDRESS(PFNGLREADNPIXELSKHRPROC , ReadnPixelsKHR);
	// GL_NV_bindless_texture
	GET_PROC_ADDRESS(PFNGLGETIMAGEHANDLENVPROC , GetImageHandleNV);
	GET_PROC_ADDRESS(PFNGLGETTEXTUREHANDLENVPROC , GetTextureHandleNV);
	GET_PROC_ADDRESS(PFNGLGETTEXTURESAMPLERHANDLENVPROC , GetTextureSamplerHandleNV);
	GET_PROC_ADDRESS(PFNGLISIMAGEHANDLERESIDENTNVPROC , IsImageHandleResidentNV);
	GET_PROC_ADDRESS(PFNGLISTEXTUREHANDLERESIDENTNVPROC , IsTextureHandleResidentNV);
	GET_PROC_ADDRESS(PFNGLMAKEIMAGEHANDLENONRESIDENTNVPROC , MakeImageHandleNonResidentNV);
	GET_PROC_ADDRESS(PFNGLMAKEIMAGEHANDLERESIDENTNVPROC , MakeImageHandleResidentNV);
	GET_PROC_ADDRESS(PFNGLMAKETEXTUREHANDLENONRESIDENTNVPROC , MakeTextureHandleNonResidentNV);
	GET_PROC_ADDRESS(PFNGLMAKETEXTUREHANDLERESIDENTNVPROC , MakeTextureHandleResidentNV);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORMHANDLEUI64NVPROC , ProgramUniformHandleui64NV);
	GET_PROC_ADDRESS(PFNGLPROGRAMUNIFORMHANDLEUI64VNVPROC , ProgramUniformHandleui64vNV);
	GET_PROC_ADDRESS(PFNGLUNIFORMHANDLEUI64NVPROC , UniformHandleui64NV);
	GET_PROC_ADDRESS(PFNGLUNIFORMHANDLEUI64VNVPROC , UniformHandleui64vNV);
	// GL_NV_blend_equation_advanced
	GET_PROC_ADDRESS(PFNGLBLENDBARRIERNVPROC , BlendBarrierNV);
	GET_PROC_ADDRESS(PFNGLBLENDPARAMETERINVPROC , BlendParameteriNV);
	// GL_NV_conditional_render
	GET_PROC_ADDRESS(PFNGLBEGINCONDITIONALRENDERNVPROC , BeginConditionalRenderNV);
	GET_PROC_ADDRESS(PFNGLENDCONDITIONALRENDERNVPROC , EndConditionalRenderNV);
	// GL_NV_conservative_raster
	GET_PROC_ADDRESS(PFNGLSUBPIXELPRECISIONBIASNVPROC , SubpixelPrecisionBiasNV);
	// GL_NV_copy_buffer
	GET_PROC_ADDRESS(PFNGLCOPYBUFFERSUBDATANVPROC , CopyBufferSubDataNV);
	// GL_NV_coverage_sample
	GET_PROC_ADDRESS(PFNGLCOVERAGEMASKNVPROC , CoverageMaskNV);
	GET_PROC_ADDRESS(PFNGLCOVERAGEOPERATIONNVPROC , CoverageOperationNV);
	// GL_NV_draw_buffers
	GET_PROC_ADDRESS(PFNGLDRAWBUFFERSNVPROC , DrawBuffersNV);
	// GL_NV_draw_instanced
	GET_PROC_ADDRESS(PFNGLDRAWARRAYSINSTANCEDNVPROC , DrawArraysInstancedNV);
	GET_PROC_ADDRESS(PFNGLDRAWELEMENTSINSTANCEDNVPROC , DrawElementsInstancedNV);
	// GL_NV_fence
	GET_PROC_ADDRESS(PFNGLDELETEFENCESNVPROC , DeleteFencesNV);
	GET_PROC_ADDRESS(PFNGLFINISHFENCENVPROC , FinishFenceNV);
	GET_PROC_ADDRESS(PFNGLGENFENCESNVPROC , GenFencesNV);
	GET_PROC_ADDRESS(PFNGLGETFENCEIVNVPROC , GetFenceivNV);
	GET_PROC_ADDRESS(PFNGLISFENCENVPROC , IsFenceNV);
	GET_PROC_ADDRESS(PFNGLSETFENCENVPROC , SetFenceNV);
	GET_PROC_ADDRESS(PFNGLTESTFENCENVPROC , TestFenceNV);
	// GL_NV_fragment_coverage_to_color
	GET_PROC_ADDRESS(PFNGLFRAGMENTCOVERAGECOLORNVPROC , FragmentCoverageColorNV);
	// GL_NV_framebuffer_blit
	GET_PROC_ADDRESS(PFNGLBLITFRAMEBUFFERNVPROC , BlitFramebufferNV);
	// GL_NV_framebuffer_mixed_samples
	GET_PROC_ADDRESS(PFNGLCOVERAGEMODULATIONNVPROC , CoverageModulationNV);
	GET_PROC_ADDRESS(PFNGLCOVERAGEMODULATIONTABLENVPROC , CoverageModulationTableNV);
	GET_PROC_ADDRESS(PFNGLGETCOVERAGEMODULATIONTABLENVPROC , GetCoverageModulationTableNV);
	// GL_NV_framebuffer_multisample
	GET_PROC_ADDRESS(PFNGLRENDERBUFFERSTORAGEMULTISAMPLENVPROC , RenderbufferStorageMultisampleNV);
	// GL_NV_instanced_arrays
	GET_PROC_ADDRESS(PFNGLVERTEXATTRIBDIVISORNVPROC , VertexAttribDivisorNV);
	// GL_NV_internalformat_sample_query
	GET_PROC_ADDRESS(PFNGLGETINTERNALFORMATSAMPLEIVNVPROC , GetInternalformatSampleivNV);
	// GL_NV_non_square_matrices
	GET_PROC_ADDRESS(PFNGLUNIFORMMATRIX2X3FVNVPROC , UniformMatrix2x3fvNV);
	GET_PROC_ADDRESS(PFNGLUNIFORMMATRIX2X4FVNVPROC , UniformMatrix2x4fvNV);
	GET_PROC_ADDRESS(PFNGLUNIFORMMATRIX3X2FVNVPROC , UniformMatrix3x2fvNV);
	GET_PROC_ADDRESS(PFNGLUNIFORMMATRIX3X4FVNVPROC , UniformMatrix3x4fvNV);
	GET_PROC_ADDRESS(PFNGLUNIFORMMATRIX4X2FVNVPROC , UniformMatrix4x2fvNV);
	GET_PROC_ADDRESS(PFNGLUNIFORMMATRIX4X3FVNVPROC , UniformMatrix4x3fvNV);
	// GL_NV_path_rendering
	GET_PROC_ADDRESS(PFNGLCOPYPATHNVPROC , CopyPathNV);
	GET_PROC_ADDRESS(PFNGLCOVERFILLPATHINSTANCEDNVPROC , CoverFillPathInstancedNV);
	GET_PROC_ADDRESS(PFNGLCOVERFILLPATHNVPROC , CoverFillPathNV);
	GET_PROC_ADDRESS(PFNGLCOVERSTROKEPATHINSTANCEDNVPROC , CoverStrokePathInstancedNV);
	GET_PROC_ADDRESS(PFNGLCOVERSTROKEPATHNVPROC , CoverStrokePathNV);
	GET_PROC_ADDRESS(PFNGLDELETEPATHSNVPROC , DeletePathsNV);
	GET_PROC_ADDRESS(PFNGLGENPATHSNVPROC , GenPathsNV);
	GET_PROC_ADDRESS(PFNGLGETPATHCOMMANDSNVPROC , GetPathCommandsNV);
	GET_PROC_ADDRESS(PFNGLGETPATHCOORDSNVPROC , GetPathCoordsNV);
	GET_PROC_ADDRESS(PFNGLGETPATHDASHARRAYNVPROC , GetPathDashArrayNV);
	GET_PROC_ADDRESS(PFNGLGETPATHLENGTHNVPROC , GetPathLengthNV);
	GET_PROC_ADDRESS(PFNGLGETPATHMETRICRANGENVPROC , GetPathMetricRangeNV);
	GET_PROC_ADDRESS(PFNGLGETPATHMETRICSNVPROC , GetPathMetricsNV);
	GET_PROC_ADDRESS(PFNGLGETPATHPARAMETERFVNVPROC , GetPathParameterfvNV);
	GET_PROC_ADDRESS(PFNGLGETPATHPARAMETERIVNVPROC , GetPathParameterivNV);
	GET_PROC_ADDRESS(PFNGLGETPATHSPACINGNVPROC , GetPathSpacingNV);
	GET_PROC_ADDRESS(PFNGLGETPROGRAMRESOURCEFVNVPROC , GetProgramResourcefvNV);
	GET_PROC_ADDRESS(PFNGLINTERPOLATEPATHSNVPROC , InterpolatePathsNV);
	GET_PROC_ADDRESS(PFNGLISPATHNVPROC , IsPathNV);
	GET_PROC_ADDRESS(PFNGLISPOINTINFILLPATHNVPROC , IsPointInFillPathNV);
	GET_PROC_ADDRESS(PFNGLISPOINTINSTROKEPATHNVPROC , IsPointInStrokePathNV);
	GET_PROC_ADDRESS(PFNGLMATRIXLOAD3X2FNVPROC , MatrixLoad3x2fNV);
	GET_PROC_ADDRESS(PFNGLMATRIXLOAD3X3FNVPROC , MatrixLoad3x3fNV);
	GET_PROC_ADDRESS(PFNGLMATRIXLOADTRANSPOSE3X3FNVPROC , MatrixLoadTranspose3x3fNV);
	GET_PROC_ADDRESS(PFNGLMATRIXMULT3X2FNVPROC , MatrixMult3x2fNV);
	GET_PROC_ADDRESS(PFNGLMATRIXMULT3X3FNVPROC , MatrixMult3x3fNV);
	GET_PROC_ADDRESS(PFNGLMATRIXMULTTRANSPOSE3X3FNVPROC , MatrixMultTranspose3x3fNV);
	GET_PROC_ADDRESS(PFNGLPATHCOMMANDSNVPROC , PathCommandsNV);
	GET_PROC_ADDRESS(PFNGLPATHCOORDSNVPROC , PathCoordsNV);
	GET_PROC_ADDRESS(PFNGLPATHCOVERDEPTHFUNCNVPROC , PathCoverDepthFuncNV);
	GET_PROC_ADDRESS(PFNGLPATHDASHARRAYNVPROC , PathDashArrayNV);
	GET_PROC_ADDRESS(PFNGLPATHGLYPHINDEXARRAYNVPROC , PathGlyphIndexArrayNV);
	GET_PROC_ADDRESS(PFNGLPATHGLYPHINDEXRANGENVPROC , PathGlyphIndexRangeNV);
	GET_PROC_ADDRESS(PFNGLPATHGLYPHRANGENVPROC , PathGlyphRangeNV);
	GET_PROC_ADDRESS(PFNGLPATHGLYPHSNVPROC , PathGlyphsNV);
	GET_PROC_ADDRESS(PFNGLPATHMEMORYGLYPHINDEXARRAYNVPROC , PathMemoryGlyphIndexArrayNV);
	GET_PROC_ADDRESS(PFNGLPATHPARAMETERFNVPROC , PathParameterfNV);
	GET_PROC_ADDRESS(PFNGLPATHPARAMETERFVNVPROC , PathParameterfvNV);
	GET_PROC_ADDRESS(PFNGLPATHPARAMETERINVPROC , PathParameteriNV);
	GET_PROC_ADDRESS(PFNGLPATHPARAMETERIVNVPROC , PathParameterivNV);
	GET_PROC_ADDRESS(PFNGLPATHSTENCILDEPTHOFFSETNVPROC , PathStencilDepthOffsetNV);
	GET_PROC_ADDRESS(PFNGLPATHSTENCILFUNCNVPROC , PathStencilFuncNV);
	GET_PROC_ADDRESS(PFNGLPATHSTRINGNVPROC , PathStringNV);
	GET_PROC_ADDRESS(PFNGLPATHSUBCOMMANDSNVPROC , PathSubCommandsNV);
	GET_PROC_ADDRESS(PFNGLPATHSUBCOORDSNVPROC , PathSubCoordsNV);
	GET_PROC_ADDRESS(PFNGLPOINTALONGPATHNVPROC , PointAlongPathNV);
	GET_PROC_ADDRESS(PFNGLPROGRAMPATHFRAGMENTINPUTGENNVPROC , ProgramPathFragmentInputGenNV);
	GET_PROC_ADDRESS(PFNGLSTENCILFILLPATHINSTANCEDNVPROC , StencilFillPathInstancedNV);
	GET_PROC_ADDRESS(PFNGLSTENCILFILLPATHNVPROC , StencilFillPathNV);
	GET_PROC_ADDRESS(PFNGLSTENCILSTROKEPATHINSTANCEDNVPROC , StencilStrokePathInstancedNV);
	GET_PROC_ADDRESS(PFNGLSTENCILSTROKEPATHNVPROC , StencilStrokePathNV);
	GET_PROC_ADDRESS(PFNGLSTENCILTHENCOVERFILLPATHINSTANCEDNVPROC , StencilThenCoverFillPathInstancedNV);
	GET_PROC_ADDRESS(PFNGLSTENCILTHENCOVERFILLPATHNVPROC , StencilThenCoverFillPathNV);
	GET_PROC_ADDRESS(PFNGLSTENCILTHENCOVERSTROKEPATHINSTANCEDNVPROC , StencilThenCoverStrokePathInstancedNV);
	GET_PROC_ADDRESS(PFNGLSTENCILTHENCOVERSTROKEPATHNVPROC , StencilThenCoverStrokePathNV);
	GET_PROC_ADDRESS(PFNGLTRANSFORMPATHNVPROC , TransformPathNV);
	GET_PROC_ADDRESS(PFNGLWEIGHTPATHSNVPROC , WeightPathsNV);
	// GL_NV_polygon_mode
	GET_PROC_ADDRESS(PFNGLPOLYGONMODENVPROC , PolygonModeNV);
	// GL_NV_read_buffer
	GET_PROC_ADDRESS(PFNGLREADBUFFERNVPROC , ReadBufferNV);
	// GL_NV_sample_locations
	GET_PROC_ADDRESS(PFNGLFRAMEBUFFERSAMPLELOCATIONSFVNVPROC , FramebufferSampleLocationsfvNV);
	GET_PROC_ADDRESS(PFNGLNAMEDFRAMEBUFFERSAMPLELOCATIONSFVNVPROC , NamedFramebufferSampleLocationsfvNV);
	GET_PROC_ADDRESS(PFNGLRESOLVEDEPTHVALUESNVPROC , ResolveDepthValuesNV);
	// GL_NV_viewport_array
	GET_PROC_ADDRESS(PFNGLDEPTHRANGEARRAYFVNVPROC , DepthRangeArrayfvNV);
	GET_PROC_ADDRESS(PFNGLDEPTHRANGEINDEXEDFNVPROC , DepthRangeIndexedfNV);
	GET_PROC_ADDRESS(PFNGLDISABLEINVPROC , DisableiNV);
	GET_PROC_ADDRESS(PFNGLENABLEINVPROC , EnableiNV);
	GET_PROC_ADDRESS(PFNGLGETFLOATI_VNVPROC , GetFloati_vNV);
	GET_PROC_ADDRESS(PFNGLISENABLEDINVPROC , IsEnablediNV);
	GET_PROC_ADDRESS(PFNGLSCISSORARRAYVNVPROC , ScissorArrayvNV);
	GET_PROC_ADDRESS(PFNGLSCISSORINDEXEDNVPROC , ScissorIndexedNV);
	GET_PROC_ADDRESS(PFNGLSCISSORINDEXEDVNVPROC , ScissorIndexedvNV);
	GET_PROC_ADDRESS(PFNGLVIEWPORTARRAYVNVPROC , ViewportArrayvNV);
	GET_PROC_ADDRESS(PFNGLVIEWPORTINDEXEDFNVPROC , ViewportIndexedfNV);
	GET_PROC_ADDRESS(PFNGLVIEWPORTINDEXEDFVNVPROC , ViewportIndexedfvNV);
	// GL_OES_EGL_image
	GET_PROC_ADDRESS(PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC , EGLImageTargetRenderbufferStorageOES);
	GET_PROC_ADDRESS(PFNGLEGLIMAGETARGETTEXTURE2DOESPROC , EGLImageTargetTexture2DOES);
	// GL_OES_copy_image
	GET_PROC_ADDRESS(PFNGLCOPYIMAGESUBDATAOESPROC , CopyImageSubDataOES);
	// GL_OES_draw_buffers_indexed
	GET_PROC_ADDRESS(PFNGLBLENDEQUATIONSEPARATEIOESPROC , BlendEquationSeparateiOES);
	GET_PROC_ADDRESS(PFNGLBLENDEQUATIONIOESPROC , BlendEquationiOES);
	GET_PROC_ADDRESS(PFNGLBLENDFUNCSEPARATEIOESPROC , BlendFuncSeparateiOES);
	GET_PROC_ADDRESS(PFNGLBLENDFUNCIOESPROC , BlendFunciOES);
	GET_PROC_ADDRESS(PFNGLCOLORMASKIOESPROC , ColorMaskiOES);
	GET_PROC_ADDRESS(PFNGLDISABLEIOESPROC , DisableiOES);
	GET_PROC_ADDRESS(PFNGLENABLEIOESPROC , EnableiOES);
	GET_PROC_ADDRESS(PFNGLISENABLEDIOESPROC , IsEnablediOES);
	// GL_OES_draw_elements_base_vertex
	GET_PROC_ADDRESS(PFNGLDRAWELEMENTSBASEVERTEXOESPROC , DrawElementsBaseVertexOES);
	GET_PROC_ADDRESS(PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXOESPROC , DrawElementsInstancedBaseVertexOES);
	GET_PROC_ADDRESS(PFNGLDRAWRANGEELEMENTSBASEVERTEXOESPROC , DrawRangeElementsBaseVertexOES);
	GET_PROC_ADDRESS(PFNGLMULTIDRAWELEMENTSBASEVERTEXOESPROC , MultiDrawElementsBaseVertexOES);
	// GL_OES_geometry_shader
	GET_PROC_ADDRESS(PFNGLFRAMEBUFFERTEXTUREOESPROC , FramebufferTextureOES);
	// GL_OES_get_program_binary
	GET_PROC_ADDRESS(PFNGLGETPROGRAMBINARYOESPROC , GetProgramBinaryOES);
	GET_PROC_ADDRESS(PFNGLPROGRAMBINARYOESPROC , ProgramBinaryOES);
	// GL_OES_mapbuffer
	GET_PROC_ADDRESS(PFNGLGETBUFFERPOINTERVOESPROC , GetBufferPointervOES);
	GET_PROC_ADDRESS(PFNGLMAPBUFFEROESPROC , MapBufferOES);
	GET_PROC_ADDRESS(PFNGLUNMAPBUFFEROESPROC , UnmapBufferOES);
	// GL_OES_primitive_bounding_box
	GET_PROC_ADDRESS(PFNGLPRIMITIVEBOUNDINGBOXOESPROC , PrimitiveBoundingBoxOES);
	// GL_OES_sample_shading
	GET_PROC_ADDRESS(PFNGLMINSAMPLESHADINGOESPROC , MinSampleShadingOES);
	// GL_OES_tessellation_shader
	GET_PROC_ADDRESS(PFNGLPATCHPARAMETERIOESPROC , PatchParameteriOES);
	// GL_OES_texture_3D
	GET_PROC_ADDRESS(PFNGLCOMPRESSEDTEXIMAGE3DOESPROC , CompressedTexImage3DOES);
	GET_PROC_ADDRESS(PFNGLCOMPRESSEDTEXSUBIMAGE3DOESPROC , CompressedTexSubImage3DOES);
	GET_PROC_ADDRESS(PFNGLCOPYTEXSUBIMAGE3DOESPROC , CopyTexSubImage3DOES);
	GET_PROC_ADDRESS(PFNGLFRAMEBUFFERTEXTURE3DOESPROC , FramebufferTexture3DOES);
	GET_PROC_ADDRESS(PFNGLTEXIMAGE3DOESPROC , TexImage3DOES);
	GET_PROC_ADDRESS(PFNGLTEXSUBIMAGE3DOESPROC , TexSubImage3DOES);
	// GL_OES_texture_border_clamp
	GET_PROC_ADDRESS(PFNGLGETSAMPLERPARAMETERIIVOESPROC , GetSamplerParameterIivOES);
	GET_PROC_ADDRESS(PFNGLGETSAMPLERPARAMETERIUIVOESPROC , GetSamplerParameterIuivOES);
	GET_PROC_ADDRESS(PFNGLGETTEXPARAMETERIIVOESPROC , GetTexParameterIivOES);
	GET_PROC_ADDRESS(PFNGLGETTEXPARAMETERIUIVOESPROC , GetTexParameterIuivOES);
	GET_PROC_ADDRESS(PFNGLSAMPLERPARAMETERIIVOESPROC , SamplerParameterIivOES);
	GET_PROC_ADDRESS(PFNGLSAMPLERPARAMETERIUIVOESPROC , SamplerParameterIuivOES);
	GET_PROC_ADDRESS(PFNGLTEXPARAMETERIIVOESPROC , TexParameterIivOES);
	GET_PROC_ADDRESS(PFNGLTEXPARAMETERIUIVOESPROC , TexParameterIuivOES);
	// GL_OES_texture_buffer
	GET_PROC_ADDRESS(PFNGLTEXBUFFEROESPROC , TexBufferOES);
	GET_PROC_ADDRESS(PFNGLTEXBUFFERRANGEOESPROC , TexBufferRangeOES);
	// GL_OES_texture_storage_multisample_2d_array
	GET_PROC_ADDRESS(PFNGLTEXSTORAGE3DMULTISAMPLEOESPROC , TexStorage3DMultisampleOES);
	// GL_OES_texture_view
	GET_PROC_ADDRESS(PFNGLTEXTUREVIEWOESPROC , TextureViewOES);
	// GL_OES_vertex_array_object
	GET_PROC_ADDRESS(PFNGLBINDVERTEXARRAYOESPROC , BindVertexArrayOES);
	GET_PROC_ADDRESS(PFNGLDELETEVERTEXARRAYSOESPROC , DeleteVertexArraysOES);
	GET_PROC_ADDRESS(PFNGLGENVERTEXARRAYSOESPROC , GenVertexArraysOES);
	GET_PROC_ADDRESS(PFNGLISVERTEXARRAYOESPROC , IsVertexArrayOES);
	// GL_OVR_multiview
	GET_PROC_ADDRESS(PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC , FramebufferTextureMultiviewOVR);
	// GL_OVR_multiview_multisampled_render_to_texture
	GET_PROC_ADDRESS(PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC , FramebufferTextureMultisampleMultiviewOVR);
	// GL_QCOM_alpha_test
	GET_PROC_ADDRESS(PFNGLALPHAFUNCQCOMPROC , AlphaFuncQCOM);
	// GL_QCOM_driver_control
	GET_PROC_ADDRESS(PFNGLDISABLEDRIVERCONTROLQCOMPROC , DisableDriverControlQCOM);
	GET_PROC_ADDRESS(PFNGLENABLEDRIVERCONTROLQCOMPROC , EnableDriverControlQCOM);
	GET_PROC_ADDRESS(PFNGLGETDRIVERCONTROLSTRINGQCOMPROC , GetDriverControlStringQCOM);
	GET_PROC_ADDRESS(PFNGLGETDRIVERCONTROLSQCOMPROC , GetDriverControlsQCOM);
	// GL_QCOM_extended_get
	GET_PROC_ADDRESS(PFNGLEXTGETBUFFERPOINTERVQCOMPROC , ExtGetBufferPointervQCOM);
	GET_PROC_ADDRESS(PFNGLEXTGETBUFFERSQCOMPROC , ExtGetBuffersQCOM);
	GET_PROC_ADDRESS(PFNGLEXTGETFRAMEBUFFERSQCOMPROC , ExtGetFramebuffersQCOM);
	GET_PROC_ADDRESS(PFNGLEXTGETRENDERBUFFERSQCOMPROC , ExtGetRenderbuffersQCOM);
	GET_PROC_ADDRESS(PFNGLEXTGETTEXLEVELPARAMETERIVQCOMPROC , ExtGetTexLevelParameterivQCOM);
	GET_PROC_ADDRESS(PFNGLEXTGETTEXSUBIMAGEQCOMPROC , ExtGetTexSubImageQCOM);
	GET_PROC_ADDRESS(PFNGLEXTGETTEXTURESQCOMPROC , ExtGetTexturesQCOM);
	GET_PROC_ADDRESS(PFNGLEXTTEXOBJECTSTATEOVERRIDEIQCOMPROC , ExtTexObjectStateOverrideiQCOM);
	// GL_QCOM_extended_get2
	GET_PROC_ADDRESS(PFNGLEXTGETPROGRAMBINARYSOURCEQCOMPROC , ExtGetProgramBinarySourceQCOM);
	GET_PROC_ADDRESS(PFNGLEXTGETPROGRAMSQCOMPROC , ExtGetProgramsQCOM);
	GET_PROC_ADDRESS(PFNGLEXTGETSHADERSQCOMPROC , ExtGetShadersQCOM);
	GET_PROC_ADDRESS(PFNGLEXTISPROGRAMBINARYQCOMPROC , ExtIsProgramBinaryQCOM);
	// GL_QCOM_tiled_rendering
	GET_PROC_ADDRESS(PFNGLENDTILINGQCOMPROC , EndTilingQCOM);
	GET_PROC_ADDRESS(PFNGLSTARTTILINGQCOMPROC , StartTilingQCOM);

	return true;
}

}; // end of namespace Debugger


#endif // __GLES2__

