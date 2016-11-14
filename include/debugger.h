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
#ifndef __GLES2_DEBUGGER_INCLUDE_H__
#define __GLES2_DEBUGGER_INCLUDE_H__

#if defined(USE_DEBUGGER)

#include "circularBuffer.h"

namespace Debugger {

/**
 * Simple OpenGL ES 2.0 debugger
 */
class gl
{
public:
	/*!
	 * init
	 */
	 //! init function must be called before using opengl
	static bool init();

	/*!
	 * reset function clear the debug data
	 * use it to clear cache before doing a new frame
	 * in most case, call it in your loop procedure, at the beginning.
	 */
	static void reset();

	/** set state : break on error */
	static inline void setBreakOnError(const bool& state)
	{
		_break_on_error = state;
	}

	/** set state : break on warning */
	static inline void setBreakOnWarning(const bool& state)
	{
		_break_on_warning = state;
	}

	/** set state : append to log all calls function */
	static inline void setAppendToLogFunctionCalls(const bool& state)
	{
		_append_to_log_calls = state;
	}

	/** dump to log data passed to opengl */
	static inline void setDumpData(const bool& state)
	{
		_dump_data = state;
	}

	/** get the buffer message log */
	static inline const String getBufferMessage()
	{
		return _output_buffer;
	}

	typedef void(*fnc_console_cb)(const String& text);

	static void setConsoleCallback(fnc_console_cb cb)
	{
		_console_cb = cb;
	}

protected:
	// enum
	enum NewEntity_e {
		NEW_BUFFER = (1 << 1),
		NEW_TEXTURE = (1 << 2),
		NEW_PROGRAM = (1 << 3),
		NEW_SHADER = (1 << 4),
		NEW_NONE = (1 << 0),
	};

	// -----------------------------------------------------------------
	// structures
	// -----------------------------------------------------------------
	struct Type
	{
		Type() : bytes(0),bytesShift(0),specialInterpretation(false){}

		GLuint bytes;
		GLuint bytesShift;	// Bit shift by this value to effectively 
							// divide/multiply by "bytes" in a more
							// optimal way
		bool specialInterpretation;
	};
	struct Entry_t
	{
		Entry_t() : hash(0), fnc(nullptr) {}
		Entry_t(hash_t hash,void* data) : hash(hash), fnc(data) {}
		Entry_t(const Entry_t& e) : hash(e.hash), fnc(e.fnc) {}
		hash_t hash;
		void* fnc;
	}; typedef Vector<Entry_t> Entry_v;

	/** opengl define */
	struct Define_t
	{
		Define_t() : value(0), name(), hash(0) { }
		Define_t(GLenum value,const String& name, hash_t hash) :
			value(value), name(name), hash(hash) { }
		Define_t(const Define_t& e) : 
			value(e.value), name(e.name), hash(e.hash) { }
		GLenum value;
		String name;
		hash_t hash;
	}; typedef Vector<Define_t> Define_v;

	struct History_t {
		History_t() : id(-1), result(), call(), file(), line(-1) {}
		History_t(uint id, const char* result, const char* call,
			const char* file, int line) : id(id), result(result?result:""), call(call?call:""),
			file(file), line(line) { }
		History_t(const History_t& h) : id(h.id), result(h.result),
					call(h.call), file(h.file), line(h.line) { }

		int id;
		String result;
		String call;
		String file;
		int line;
	}; typedef CircularBuffer<History_t> History_cb;
	
	struct Program_t {
		Program_t() : id(0), valid(0), flags(NEW_PROGRAM) {}
		Program_t(const Program_t& b) : id(b.id), valid(b.valid),
			flags(b.flags) {}

		bool operator==(const Program_t& rhs) const {
			return memcmp((void*)this, (void*)&rhs, sizeof(*this)) == 0;
		};
		bool operator!=(const Program_t& rhs) const {
			return !(*this == rhs);
		};
		bool operator<(const Program_t& rhs) const {
			return memcmp((void*)this, (void*)&rhs, sizeof(*this))>0;
		};

		uint id;
		uchar valid;
		uint flags;
		// todo: other stuff to check
	}; typedef Vector<Program_t> Program_v;
	
	struct Shader_t {
		Shader_t() : id(0), valid(0), flags(NEW_SHADER) {}
		Shader_t(const Shader_t& b) : id(b.id), valid(b.valid),
			flags(b.flags){}

		bool operator==(const Shader_t& rhs) const {
			return memcmp((void*)this, (void*)&rhs, sizeof(*this)) == 0;
		};
		bool operator!=(const Shader_t& rhs) const {
			return !(*this == rhs);
		};
		bool operator<(const Shader_t& rhs) const {
			return memcmp((void*)this, (void*)&rhs, sizeof(*this))>0;
		};

		uint id;
		uchar valid;
		uint flags;
		// todo: other stuff to check
	}; typedef Vector<Shader_t> Shader_v;

	struct Texture_t {
		Texture_t() : id(0), valid(0),flags(NEW_TEXTURE) {}
		Texture_t(const Texture_t& b) : id(b.id), valid(b.valid),
			flags(b.flags){}

		bool operator==(const Texture_t& rhs) const {
			return memcmp((void*)this, (void*)&rhs, sizeof(*this)) == 0;
		};
		bool operator!=(const Texture_t& rhs) const {
			return !(*this == rhs);
		};
		bool operator<(const Texture_t& rhs) const {
			return memcmp((void*)this, (void*)&rhs, sizeof(*this))>0;
		};

		uint id;
		uchar valid;
		uint flags;
		// todo: other stuff to check
	}; typedef Vector<Texture_t> Texture_v;
	
	enum Buffer_id {
		INVALID_BUFFER_TARGET = 0,
		// The currently bound array buffer.
		// If this is INVALID_BOUND it is illegal to call
		// glVertexAttribPointer.
		ARRAY_BUFFER,
		// The currently bound element array buffer.
		// If this is INVALID_BOUND it is illegal to call glDrawElements.
		ELEMENT_ARRAY_BUFFER,
		BUFFER_SIZE,
		INVALID_BOUND = 0xFFFFFFFF
	};

	struct Buffer_t {
		Buffer_t() : target(INVALID_BUFFER_TARGET),id(INVALID_BOUND),
			valid(0), size(0),flags(NEW_BUFFER),data(nullptr),mapped(0),
			mapPointer(nullptr), mapOffset(0),mapLength(0) {}
		Buffer_t(const Buffer_t& b) : id(b.id), valid(b.valid),
			size(b.size), flags(b.flags), data(b.data), mapped(b.mapped),
			mapPointer(b.mapPointer), mapOffset(b.mapOffset),
			mapLength(b.mapLength) { }

		inline void clear() 
		{
			target = INVALID_BUFFER_TARGET;
			id = INVALID_BOUND;
			valid = 0;
			size = 0;
			flags = 0;
			data = nullptr;
			mapped = 0;
			mapPointer = nullptr;
			mapOffset = 0;
			mapLength = 0;
		}

		bool operator==(const Buffer_t& rhs) const {
			return memcmp((void*)this, (void*)&rhs, sizeof(*this)) == 0;
		};
		bool operator!=(const Buffer_t& rhs) const {
			return !(*this == rhs);
		};
		bool operator<(const Buffer_t& rhs) const {
			return memcmp((void*)this, (void*)&rhs, sizeof(*this))>0;
		};

		uint target;
		uint id;
		uchar valid;
		uint size;
		uint flags;
		const void* data;
		uchar mapped;
		void* mapPointer;
		int64_t mapOffset;
		int64_t mapLength;
		// todo: other stuff to check
	}; typedef Vector<Buffer_t> Buffer_v;

	// -----------------------------------------------------------------
	// functions
	// -----------------------------------------------------------------

	/** return true if the texture is registerd, else false. */
	static bool is_registered_texture(GLenum id);
	/** return true if the shader is registerd, else false. */
	static bool is_registered_shader(GLuint id);
	/** return true if the buffer is registerd, else false. */
	static bool is_registered_buffer(GLuint id);
	/** return true if the program is registerd, else false. */
	static bool is_registered_program(GLuint id);
	/** return true if the cap is enabled, else false. */
	static bool is_cap_enabled(GLenum cap);
	/** enable or disable a cap (local) */
	static void setStates(GLenum cap, uchar state);
	static void enableStates(GLenum cap);
	static void disableStates(GLenum cap);
	/** return true if the cap for enable disable state is allowed,
	 * else false. */
	static bool is_allowed_capability_enable_disable(GLenum cap);
	/** return the string name of the capability or GL_NONE
	 * if not found (Used in gl::Enable() / gl::Disable()) */
	static const char* getCapabilityName(GLenum cap);
	/** return the string name of the define or GL_NONE
	 * if not found */
	static const char* getDefineName(GLenum name);
	/** add a called glXXX function with result code */
	static void addCall(const char* result, const char* function_name,
		const char* file, int line);

	/** append buffer to console */
	static void appendConsole(const String& buffer);

	/** standard get proc */
	static void* getProcAddr(const char* function_name);
	/** break on error */
	static void breakOnError(const bool& value, const char* message = nullptr);
	/** break on warning */
	static void breakOnWarning(const bool& value, const char* message = nullptr);
	/*! like getProcAddr, but for local function. */
	//! like getProcAddr, but for local function.
	static void* getProc(const char* function_name);
	/** get last opengl error message */
	static const char* get_last_error();
	/** format input (remove all path, except the last one. */
	static String get_path(const String& file);
	/** set program bound (internal) */
	static inline void setUseProgram(uint p) { _program_bound = p; }
	/** get currently bound program
	 * return a valid opengl id or INVALID_BOUND if no program is bound.
	 */
	static inline uint get_program_bound() { return _program_bound; }
	/** return true if a program is currently bound, else false. */
	static inline bool is_program_bound()
	{
		return (INVALID_BOUND != _program_bound);
	}

	/** (un)register textures (gl::GenTextures/gl::DeleteTextures) */
	static bool register_texture(uint id);
	static bool unregister_texture(uint id);

	/** (un)register a buffer (gl::GenBuffers/gl::DeleteBuffers) */
	static bool register_buffer(uint id);
	static bool unregister_buffer(uint id);

	/** (un)register a program (gl::CreateProgram/gl::DeleteProgram) */
	static bool unregister_program(uint id);
	static bool register_program(uint id);

	/** (un)register a shader(gl::CreateShader/gl::DeleteShader) */
	static bool unregister_shader(uint id);
	static bool register_shader(uint id);

	/** set buffer content (used in gl::BufferData()) */
	static bool setBuffer(uint target,uint id,uint size,const void* data);
	/** return pointer to registered buffer with id, else nullptr */
	static Buffer_t* getBuffer(uint target, uint id);
	/** set bound buffer */
	static inline void setBoundBuffer(uint target,uint id);
	/** set data to current bound buffer */
	static void setBoundBufferData(uint target,uint size,const void* data);
	/** get ID of current bound buffer of type target */
	static inline uint getBoundBufferId(uint target);

	/** fast access to bound array buffer */
	static inline Buffer_t* arrayBuffer()
	{
		return _bound_buffer[ARRAY_BUFFER];
	}
	
	/** fast access to bound element array buffer */
	static inline Buffer_t* elementArrayBuffer()
	{
		return _bound_buffer[ELEMENT_ARRAY_BUFFER];
	}

	/** send message to console */
	static inline void setConsole(const String& buffer)
	{
		if (nullptr != _console_cb)
			_console_cb(buffer);
	}
	
	// function take directly from libANGLE
	static Type GenTypeInfo(GLuint bytes, bool specialInterpretation);
	static const Type& GetTypeInfo(GLenum type);

	/** return the string name if enum is allowed inside
	 * gl:GetXXX function, else return nullptr.
	 */
	static const char* is_allowed_enum_get_function(GLenum pname);

	/** allowed define for getXXX inside extension */
	static const char* is_allowed_get_function_in_extensions(GLenum cap);

	// all the define mess ---------------------------------------------

	enum GL2EXTHash_e {
		hash_GL_KHR_blend_equation_advanced = 0xb9334dba,
        hash_GL_KHR_blend_equation_advanced_coherent = 0xd20764d1,
        hash_GL_KHR_context_flush_control = 0x7c2c6e41,
        hash_GL_KHR_debug = 0x23987e22,
        hash_GL_KHR_no_error = 0x16020d21,
        hash_GL_KHR_robust_buffer_access_behavior = 0x50012f53,
        hash_GL_KHR_robustness = 0x45c60dd3,
        hash_GL_KHR_texture_compression_astc_hdr = 0x7f0f1144,
        hash_GL_KHR_texture_compression_astc_ldr = 0x7f0f2248,
        hash_GL_KHR_texture_compression_astc_sliced_3d = 0xfbb625b0,
        hash_GL_OES_EGL_image = 0xaed3caf7,
        hash_GL_OES_EGL_image_external = 0xe456ef59,
        hash_GL_OES_EGL_image_external_essl3 = 0x938c13a2,
        hash_GL_OES_compressed_ETC1_RGB8_sub_texture = 0xd384bec9,
        hash_GL_OES_compressed_ETC1_RGB8_texture = 0x79add20,
        hash_GL_OES_compressed_paletted_texture = 0xf41b3614,
        hash_GL_OES_copy_image = 0x2912727a,
        hash_GL_OES_depth24 = 0xea7cf6d8,
        hash_GL_OES_depth32 = 0xea7cf6f7,
        hash_GL_OES_depth_texture = 0xfb787562,
        hash_GL_OES_draw_buffers_indexed = 0xeab25397,
        hash_GL_OES_draw_elements_base_vertex = 0x8e048a5e,
        hash_GL_OES_element_index_uint = 0x22c425bd,
        hash_GL_OES_fbo_render_mipmap = 0xb9f0d7f6,
        hash_GL_OES_fragment_precision_high = 0xa91b3ebb,
        hash_GL_OES_geometry_point_size = 0x86e3d02c,
        hash_GL_OES_geometry_shader = 0xc874f03f,
        hash_GL_OES_get_program_binary = 0x4b73d638,
        hash_GL_OES_gpu_shader5 = 0xacfcccf4,
        hash_GL_OES_mapbuffer = 0xbbfa7b75,
        hash_GL_OES_packed_depth_stencil = 0x3228d00a,
        hash_GL_OES_primitive_bounding_box = 0x38ced073,
        hash_GL_OES_required_internalformat = 0x6abd6a43,
        hash_GL_OES_rgb8_rgba8 = 0x4b49efa3,
        hash_GL_OES_sample_shading = 0x762b62bc,
        hash_GL_OES_sample_variables = 0xbde820b7,
        hash_GL_OES_shader_image_atomic = 0xc7dd2c72,
        hash_GL_OES_shader_io_blocks = 0x5fa1f388,
        hash_GL_OES_shader_multisample_interpolation = 0xe5e3ba47,
        hash_GL_OES_standard_derivatives = 0xe35b6a73,
        hash_GL_OES_stencil1 = 0x7a89bbe0,
        hash_GL_OES_stencil4 = 0x7a89bbe3,
        hash_GL_OES_surfaceless_context = 0x7f5c7f61,
        hash_GL_OES_tessellation_point_size = 0x82a7cfb7,
        hash_GL_OES_tessellation_shader = 0xb6c8e24a,
        hash_GL_OES_texture_3D = 0xf6050ae4,
        hash_GL_OES_texture_border_clamp = 0x78bad017,
        hash_GL_OES_texture_buffer = 0xa6eab127,
        hash_GL_OES_texture_compression_astc = 0x592ef0e9,
        hash_GL_OES_texture_cube_map_array = 0x648b6aa7,
        hash_GL_OES_texture_float = 0xfd908663,
        hash_GL_OES_texture_float_linear = 0x82e6e1bd,
        hash_GL_OES_texture_half_float = 0xb1380dd,
        hash_GL_OES_texture_half_float_linear = 0xeb6c0ef7,
        hash_GL_OES_texture_npot = 0x8b94782e,
        hash_GL_OES_texture_stencil8 = 0x6dfae797,
        hash_GL_OES_texture_storage_multisample_2d_array = 0x94ee0f01,
        hash_GL_OES_texture_view = 0x8b98bc28,
        hash_GL_OES_vertex_array_object = 0x28f78fcf,
        hash_GL_OES_vertex_half_float = 0x4abbc22a,
        hash_GL_OES_vertex_type_10_10_10_2 = 0x7615cb2d,
        hash_GL_AMD_compressed_3DC_texture = 0xb6955446,
        hash_GL_AMD_compressed_ATC_texture = 0x42504fe4,
        hash_GL_AMD_performance_monitor = 0x47d68281,
        hash_GL_AMD_program_binary_Z400 = 0xab320791,
        hash_GL_ANDROID_extension_pack_es31a = 0x423cb3ce,
        hash_GL_ANGLE_depth_texture = 0xdeea442,
        hash_GL_ANGLE_framebuffer_blit = 0xd60ffc0c,
        hash_GL_ANGLE_framebuffer_multisample = 0x529fd1ee,
        hash_GL_ANGLE_instanced_arrays = 0xb6570ca7,
        hash_GL_ANGLE_pack_reverse_row_order = 0x5137f109,
        hash_GL_ANGLE_program_binary = 0x3d8c7059,
        hash_GL_ANGLE_texture_compression_dxt3 = 0xc59fce01,
        hash_GL_ANGLE_texture_compression_dxt5 = 0xc59fce03,
        hash_GL_ANGLE_texture_usage = 0x1119c0a2,
        hash_GL_ANGLE_translated_shader_source = 0x8bd23f5,
        hash_GL_APPLE_clip_distance = 0xbbd7045a,
        hash_GL_APPLE_color_buffer_packed_float = 0x6d9bb75c,
        hash_GL_APPLE_copy_texture_levels = 0xd220277d,
        hash_GL_APPLE_framebuffer_multisample = 0x719a1f9,
        hash_GL_APPLE_rgb_422 = 0x3aaa835a,
        hash_GL_APPLE_sync = 0x3a58d945,
        hash_GL_APPLE_texture_format_BGRA8888 = 0x9225403c,
        hash_GL_APPLE_texture_max_level = 0xea406715,
        hash_GL_APPLE_texture_packed_float = 0xb63e4fb5,
        hash_GL_ARM_mali_program_binary = 0x32db28b4,
        hash_GL_ARM_mali_shader_binary = 0xc2f22833,
        hash_GL_ARM_rgba8 = 0x5169a22a,
        hash_GL_ARM_shader_framebuffer_fetch = 0x8aac8afa,
        hash_GL_ARM_shader_framebuffer_fetch_depth_stencil = 0xe146837f,
        hash_GL_DMP_program_binary = 0x138c8c73,
        hash_GL_DMP_shader_binary = 0x1f169012,
        hash_GL_EXT_YUV_target = 0x50d8bcb1,
        hash_GL_EXT_base_instance = 0x2fc52c36,
        hash_GL_EXT_blend_func_extended = 0x90244c47,
        hash_GL_EXT_blend_minmax = 0x4433db15,
        hash_GL_EXT_buffer_storage = 0xbe9bcfb5,
        hash_GL_EXT_color_buffer_float = 0x3c3434b4,
        hash_GL_EXT_color_buffer_half_float = 0x8625c9ce,
        hash_GL_EXT_copy_image = 0xed914424,
        hash_GL_EXT_debug_label = 0x3d43736d,
        hash_GL_EXT_debug_marker = 0xe80fe8cf,
        hash_GL_EXT_discard_framebuffer = 0xcecc8205,
        hash_GL_EXT_disjoint_timer_query = 0xde8d2ae0,
        hash_GL_EXT_draw_buffers = 0x2a909061,
        hash_GL_EXT_draw_buffers_indexed = 0x243ac1c1,
        hash_GL_EXT_draw_elements_base_vertex = 0xf5fa62c8,
        hash_GL_EXT_draw_instanced = 0xd3a2f5ed,
        hash_GL_EXT_float_blend = 0xc17a2821,
        hash_GL_EXT_geometry_point_size = 0x88a22116,
        hash_GL_EXT_geometry_shader = 0x1e6b5c29,
        hash_GL_EXT_gpu_shader5 = 0x155d3de,
        hash_GL_EXT_instanced_arrays = 0x2ab0b2f1,
        hash_GL_EXT_map_buffer_range = 0xd191a22a,
        hash_GL_EXT_multi_draw_arrays = 0x86decb0,
        hash_GL_EXT_multi_draw_indirect = 0xe9a612f0,
        hash_GL_EXT_multisampled_compatibility = 0xafcb9831,
        hash_GL_EXT_multisampled_render_to_texture = 0x327de6e9,
        hash_GL_EXT_multiview_draw_buffers = 0x55e3f226,
        hash_GL_EXT_occlusion_query_boolean = 0x9f68eb0a,
        hash_GL_EXT_polygon_offset_clamp = 0xa8c3a881,
        hash_GL_EXT_post_depth_coverage = 0xb6fb676c,
        hash_GL_EXT_primitive_bounding_box = 0xf62b711d,
        hash_GL_EXT_pvrtc_sRGB = 0xb1c4e783,
        hash_GL_EXT_raster_multisample = 0x1a5fcd24,
        hash_GL_EXT_read_format_bgra = 0x5bd226c6,
        hash_GL_EXT_render_snorm = 0xc8448a95,
        hash_GL_EXT_robustness = 0x3a1c06df,
        hash_GL_EXT_sRGB = 0x441d3795,
        hash_GL_EXT_sRGB_write_control = 0xbb779e3f,
        hash_GL_EXT_separate_shader_objects = 0x55ee4c7b,
        hash_GL_EXT_shader_framebuffer_fetch = 0x53e5f9eb,
        hash_GL_EXT_shader_group_vote = 0xd96f0b47,
        hash_GL_EXT_shader_implicit_conversions = 0x6a20a230,
        hash_GL_EXT_shader_integer_mix = 0xdd120d38,
        hash_GL_EXT_shader_io_blocks = 0x7465dcb2,
        hash_GL_EXT_shader_pixel_local_storage = 0xa2966d5d,
        hash_GL_EXT_shader_pixel_local_storage2 = 0xf564192f,
        hash_GL_EXT_shader_texture_lod = 0x70931aac,
        hash_GL_EXT_shadow_samplers = 0x7482013,
        hash_GL_EXT_sparse_texture = 0xe57f79e5,
        hash_GL_EXT_tessellation_point_size = 0xeb9885a1,
        hash_GL_EXT_tessellation_shader = 0xb8873334,
        hash_GL_EXT_texture_border_clamp = 0xb2433e41,
        hash_GL_EXT_texture_buffer = 0x4c6e47d1,
        hash_GL_EXT_texture_compression_dxt1 = 0xe0381e49,
        hash_GL_EXT_texture_compression_s3tc = 0xe03f32a5,
        hash_GL_EXT_texture_cube_map_array = 0x21e80b51,
        hash_GL_EXT_texture_filter_anisotropic = 0x65ec7b67,
        hash_GL_EXT_texture_filter_minmax = 0x4a143ae6,
        hash_GL_EXT_texture_format_BGRA8888 = 0xcef5df3b,
        hash_GL_EXT_texture_norm16 = 0x680647ba,
        hash_GL_EXT_texture_rg = 0xba83e4d0,
        hash_GL_EXT_texture_sRGB_R8 = 0xa2ebea0e,
        hash_GL_EXT_texture_sRGB_RG8 = 0x692df5,
        hash_GL_EXT_texture_sRGB_decode = 0xd9319a08,
        hash_GL_EXT_texture_storage = 0xf529812c,
        hash_GL_EXT_texture_type_2_10_10_10_REV = 0xe5f351d6,
        hash_GL_EXT_texture_view = 0x6b12a052,
        hash_GL_EXT_unpack_subimage = 0x3d9e4db5,
        hash_GL_FJ_shader_binary_GCCSO = 0x646fbaef,
        hash_GL_IMG_framebuffer_downsample = 0x57f0a331,
        hash_GL_IMG_multisampled_render_to_texture = 0x300964f5,
        hash_GL_IMG_program_binary = 0x4c1ce22f,
        hash_GL_IMG_read_format = 0x5c38b37,
        hash_GL_IMG_shader_binary = 0x3fd51e4e,
        hash_GL_IMG_texture_compression_pvrtc = 0xa6a97bc3,
        hash_GL_IMG_texture_compression_pvrtc2 = 0x7bd8f455,
        hash_GL_IMG_texture_filter_cubic = 0x54094fae,
        hash_GL_INTEL_framebuffer_CMAA = 0x76004628,
        hash_GL_INTEL_performance_query = 0x5ca91d59,
        hash_GL_NV_bindless_texture = 0x527e355e,
        hash_GL_NV_blend_equation_advanced = 0xffd2c199,
        hash_GL_NV_blend_equation_advanced_coherent = 0x3feca390,
        hash_GL_NV_conditional_render = 0xc2dfd96d,
        hash_GL_NV_conservative_raster = 0xeaa46ca3,
        hash_GL_NV_copy_buffer = 0xbdb37d2e,
        hash_GL_NV_coverage_sample = 0xdd42d067,
        hash_GL_NV_depth_nonlinear = 0xd13c01b4,
        hash_GL_NV_draw_buffers = 0xf953d94,
        hash_GL_NV_draw_instanced = 0xc87bbe0,
        hash_GL_NV_explicit_attrib_location = 0x880b5f9,
        hash_GL_NV_fbo_color_attachments = 0x858832aa,
        hash_GL_NV_fence = 0x129971fb,
        hash_GL_NV_fill_rectangle = 0x1295d5f5,
        hash_GL_NV_fragment_coverage_to_color = 0xf4bff279,
        hash_GL_NV_fragment_shader_interlock = 0x28ff104e,
        hash_GL_NV_framebuffer_blit = 0x4f97b109,
        hash_GL_NV_framebuffer_mixed_samples = 0x51e4df49,
        hash_GL_NV_framebuffer_multisample = 0xb504684b,
        hash_GL_NV_generate_mipmap_sRGB = 0xdc268e15,
        hash_GL_NV_geometry_shader_passthrough = 0x86e49fb3,
        hash_GL_NV_image_formats = 0xe30581b8,
        hash_GL_NV_instanced_arrays = 0x2fdec1a4,
        hash_GL_NV_internalformat_sample_query = 0xd5acdcb6,
        hash_GL_NV_non_square_matrices = 0x62f2116c,
        hash_GL_NV_path_rendering = 0x84008ec4,
        hash_GL_NV_path_rendering_shared_edge = 0x5eae832e,
        hash_GL_NV_polygon_mode = 0x1a499106,
        hash_GL_NV_read_buffer = 0x31075b2f,
        hash_GL_NV_read_buffer_front = 0x94e10397,
        hash_GL_NV_read_depth = 0xf2140e6a,
        hash_GL_NV_read_depth_stencil = 0x26983b3b,
        hash_GL_NV_read_stencil = 0x6c2de287,
        hash_GL_NV_sRGB_formats = 0xe231d6c3,
        hash_GL_NV_sample_locations = 0xa2a034a7,
        hash_GL_NV_sample_mask_override_coverage = 0xbd74ff91,
        hash_GL_NV_shader_noperspective_interpolation = 0x1380da9e,
        hash_GL_NV_shadow_samplers_array = 0xfbdaaaa4,
        hash_GL_NV_shadow_samplers_cube = 0xa2c9b444,
        hash_GL_NV_texture_border_clamp = 0xa86f6e74,
        hash_GL_NV_texture_compression_s3tc_update = 0xb3f648da,
        hash_GL_NV_texture_npot_2D_mipmap = 0x80958a3,
        hash_GL_NV_viewport_array = 0xfd27de58,
        hash_GL_NV_viewport_array2 = 0xa223a98a,
        hash_GL_OVR_multiview = 0xe12f3053,
        hash_GL_OVR_multiview2 = 0x7153ae5,
        hash_GL_OVR_multiview_multisampled_render_to_texture = 0xd985e8f4,
        hash_GL_QCOM_alpha_test = 0x9fbd5a0b,
        hash_GL_QCOM_binning_control = 0xa0ad278b,
        hash_GL_QCOM_driver_control = 0xc692bb12,
        hash_GL_QCOM_extended_get = 0x987752b6,
        hash_GL_QCOM_extended_get2 = 0xa761a9a8,
        hash_GL_QCOM_perfmon_global_mode = 0x88ee1491,
        hash_GL_QCOM_tiled_rendering = 0xac501275,
        hash_GL_QCOM_writeonly_rendering = 0x7e174730,
        hash_GL_VIV_shader_binary = 0x15406f26
	}; // GL2Hash_e

	enum EGLEXTHash_e {
		hash_EGL_KHR_cl_event = 0x773aad90,
        hash_EGL_KHR_cl_event2 = 0x5e905fc2,
        hash_EGL_KHR_client_get_all_proc_addresses = 0xc8621f06,
        hash_EGL_KHR_config_attribs = 0x71523e2e,
        hash_EGL_KHR_context_flush_control = 0x4d58b8a6,
        hash_EGL_KHR_create_context = 0xdd6c0858,
        hash_EGL_KHR_create_context_no_error = 0x730ce09d,
        hash_EGL_KHR_debug = 0x9c8e9e87,
        hash_EGL_KHR_fence_sync = 0x232a899d,
        hash_EGL_KHR_get_all_proc_addresses = 0xff055788,
        hash_EGL_KHR_gl_colorspace = 0xb452681d,
        hash_EGL_KHR_gl_renderbuffer_image = 0x4109cbee,
        hash_EGL_KHR_gl_texture_2D_image = 0x5eca81a,
        hash_EGL_KHR_gl_texture_3D_image = 0xf22e7cfb,
        hash_EGL_KHR_gl_texture_cubemap_image = 0x76e40061,
        hash_EGL_KHR_image = 0x9ced7603,
        hash_EGL_KHR_image_base = 0x7b530bd,
        hash_EGL_KHR_image_pixmap = 0xeb003db1,
        hash_EGL_KHR_lock_surface = 0xe8defe71,
        hash_EGL_KHR_lock_surface2 = 0x4beccc3,
        hash_EGL_KHR_lock_surface3 = 0x4beccc4,
        hash_EGL_KHR_mutable_render_buffer = 0x37524942,
        hash_EGL_KHR_no_config_context = 0x2fcf37b6,
        hash_EGL_KHR_partial_update = 0xd7bfaecf,
        hash_EGL_KHR_platform_android = 0x51b10d45,
        hash_EGL_KHR_platform_gbm = 0x6767587a,
        hash_EGL_KHR_platform_wayland = 0xd257b314,
        hash_EGL_KHR_platform_x11 = 0x67679a3e,
        hash_EGL_KHR_reusable_sync = 0xdd56c5ef,
        hash_EGL_KHR_stream = 0x5277c20c,
        hash_EGL_KHR_stream_attrib = 0x78951671,
        hash_EGL_KHR_stream_consumer_gltexture = 0x4eae20ba,
        hash_EGL_KHR_stream_cross_process_fd = 0xec0ecf1c,
        hash_EGL_KHR_stream_fifo = 0x208fbc4f,
        hash_EGL_KHR_stream_producer_aldatalocator = 0x65e600e9,
        hash_EGL_KHR_stream_producer_eglsurface = 0xf2f760af,
        hash_EGL_KHR_surfaceless_context = 0xadda56e4,
        hash_EGL_KHR_swap_buffers_with_damage = 0xe5ab3120,
        hash_EGL_KHR_vg_parent_image = 0xad0975a8,
        hash_EGL_KHR_wait_sync = 0x74a07711,
        hash_EGL_ANDROID_blob_cache = 0x9273c64e,
        hash_EGL_ANDROID_create_native_client_buffer = 0x89cadbad,
        hash_EGL_ANDROID_framebuffer_target = 0xe06797a7,
        hash_EGL_ANDROID_front_buffer_auto_refresh = 0xdd28b7e4,
        hash_EGL_ANDROID_image_native_buffer = 0xa6de4b1e,
        hash_EGL_ANDROID_native_fence_sync = 0x31e5581f,
        hash_EGL_ANDROID_presentation_time = 0x1cd32726,
        hash_EGL_ANDROID_recordable = 0xa5b8f54f,
        hash_EGL_ANGLE_d3d_share_handle_client_buffer = 0x78d360f1,
        hash_EGL_ANGLE_device_d3d = 0x5e211bac,
        hash_EGL_ANGLE_query_surface_pointer = 0x18aaeec0,
        hash_EGL_ANGLE_surface_d3d_texture_2d_share_handle = 0x361303e7,
        hash_EGL_ANGLE_window_fixed_size = 0x1933fbc3,
        hash_EGL_ARM_implicit_external_sync = 0x4be7ebd4,
        hash_EGL_ARM_pixmap_multisample_discard = 0xec4dbcef,
        hash_EGL_EXT_buffer_age = 0xd549d072,
        hash_EGL_EXT_client_extensions = 0xa73771da,
        hash_EGL_EXT_create_context_robustness = 0x5cde4fdb,
        hash_EGL_EXT_device_base = 0x9cd9b136,
        hash_EGL_EXT_device_drm = 0x3b0e655e,
        hash_EGL_EXT_device_enumeration = 0x68354202,
        hash_EGL_EXT_device_openwf = 0x5965df2a,
        hash_EGL_EXT_device_query = 0x392a05b1,
        hash_EGL_EXT_image_dma_buf_import = 0x6b2cca16,
        hash_EGL_EXT_multiview_window = 0xa8ecf269,
        hash_EGL_EXT_output_base = 0xb1e82437,
        hash_EGL_EXT_output_drm = 0x1ca9fc3f,
        hash_EGL_EXT_output_openwf = 0xebdd166b,
        hash_EGL_EXT_platform_base = 0xed15944b,
        hash_EGL_EXT_platform_device = 0x8dc049a0,
        hash_EGL_EXT_platform_wayland = 0xf33f4520,
        hash_EGL_EXT_platform_x11 = 0xc920064a,
        hash_EGL_EXT_protected_content = 0x84fddf30,
        hash_EGL_EXT_protected_surface = 0x62e1413e,
        hash_EGL_EXT_stream_consumer_egloutput = 0xc3c207cb,
        hash_EGL_EXT_swap_buffers_with_damage = 0x6bda0f2c,
        hash_EGL_EXT_yuv_surface = 0x89dba7f8,
        hash_EGL_HI_clientpixmap = 0x8ded0bda,
        hash_EGL_HI_colorformats = 0x309ec527,
        hash_EGL_IMG_context_priority = 0x2ac9ce9e,
        hash_EGL_IMG_image_plane_attribs = 0xc49f19a2,
        hash_EGL_MESA_drm_image = 0x505d9a46,
        hash_EGL_MESA_image_dma_buf_export = 0x9468c432,
        hash_EGL_MESA_platform_gbm = 0x2a0a0e9b,
        hash_EGL_MESA_platform_surfaceless = 0xf87b20e5,
        hash_EGL_NOK_swap_region = 0x666120a1,
        hash_EGL_NOK_swap_region2 = 0x328534f3,
        hash_EGL_NOK_texture_from_pixmap = 0x45c4c35,
        hash_EGL_NV_3dvision_surface = 0x114b5a96,
        hash_EGL_NV_coverage_sample = 0xb548326c,
        hash_EGL_NV_coverage_sample_resolve = 0xd7afae6b,
        hash_EGL_NV_cuda_event = 0x29586c3d,
        hash_EGL_NV_depth_nonlinear = 0xa94163b9,
        hash_EGL_NV_device_cuda = 0x5e2ff2eb,
        hash_EGL_NV_native_query = 0x7a08297b,
        hash_EGL_NV_post_convert_rounding = 0x987e2bea,
        hash_EGL_NV_post_sub_buffer = 0xc6b1c6a7,
        hash_EGL_NV_robustness_video_memory_purge = 0xe0b04347,
        hash_EGL_NV_stream_consumer_gltexture_yuv = 0x997dde7c,
        hash_EGL_NV_stream_metadata = 0xb83352cb,
        hash_EGL_NV_stream_sync = 0x61019f07,
        hash_EGL_NV_sync = 0x8186cffc,
        hash_EGL_NV_system_time = 0xf5779a12,
        hash_EGL_TIZEN_image_native_buffer = 0x31a7b87,
        hash_EGL_TIZEN_image_native_surface = 0x83e108b6,
	};

	static const char* is_define_gl2_h(GLenum pname);

	static const char* is_define_gl2ext_h(GLenum pname);
	static const char* is_define_khr_blend_equation_advanced(GLenum pname);
	static const char* is_define_khr_blend_equation_advanced_coherent(GLenum pname);
	static const char* is_define_khr_context_flush_control(GLenum pname);
	static const char* is_define_khr_debug(GLenum pname);
	static const char* is_define_khr_no_error(GLenum pname);
	static const char* is_define_khr_robust_buffer_access_behavior(GLenum pname);
	static const char* is_define_khr_robustness(GLenum pname);
	static const char* is_define_khr_texture_compression_astc_hdr(GLenum pname);
	static const char* is_define_khr_texture_compression_astc_ldr(GLenum pname);
	static const char* is_define_khr_texture_compression_astc_sliced_3d(GLenum pname);
	static const char* is_define_oes_eis_define_image(GLenum pname);
	static const char* is_define_oes_eis_define_image_external(GLenum pname);
	static const char* is_define_oes_eis_define_image_external_essl3(GLenum pname);
	static const char* is_define_oes_compressed_etc1_rgb8_sub_texture(GLenum pname);
	static const char* is_define_oes_compressed_etc1_rgb8_texture(GLenum pname);
	static const char* is_define_oes_compressed_paletted_texture(GLenum pname);
	static const char* is_define_oes_copy_image(GLenum pname);
	static const char* is_define_oes_depth24(GLenum pname);
	static const char* is_define_oes_depth32(GLenum pname);
	static const char* is_define_oes_depth_texture(GLenum pname);
	static const char* is_define_oes_draw_buffers_indexed(GLenum pname);
	static const char* is_define_oes_draw_elements_base_vertex(GLenum pname);
	static const char* is_define_oes_element_index_uint(GLenum pname);
	static const char* is_define_oes_fbo_render_mipmap(GLenum pname);
	static const char* is_define_oes_fragment_precision_high(GLenum pname);
	static const char* is_define_oes_geometry_point_size(GLenum pname);
	static const char* is_define_oes_geometry_shader(GLenum pname);
	static const char* is_define_oes_get_program_binary(GLenum pname);
	static const char* is_define_oes_gpu_shader5(GLenum pname);
	static const char* is_define_oes_mapbuffer(GLenum pname);
	static const char* is_define_oes_packed_depth_stencil(GLenum pname);
	static const char* is_define_oes_primitive_bounding_box(GLenum pname);
	static const char* is_define_oes_required_internalformat(GLenum pname);
	static const char* is_define_oes_rgb8_rgba8(GLenum pname);
	static const char* is_define_oes_sample_shading(GLenum pname);
	static const char* is_define_oes_sample_variables(GLenum pname);
	static const char* is_define_oes_shader_image_atomic(GLenum pname);
	static const char* is_define_oes_shader_io_blocks(GLenum pname);
	static const char* is_define_oes_shader_multisample_interpolation(GLenum pname);
	static const char* is_define_oes_standard_derivatives(GLenum pname);
	static const char* is_define_oes_stencil1(GLenum pname);
	static const char* is_define_oes_stencil4(GLenum pname);
	static const char* is_define_oes_surfaceless_context(GLenum pname);
	static const char* is_define_oes_tessellation_point_size(GLenum pname);
	static const char* is_define_oes_tessellation_shader(GLenum pname);
	static const char* is_define_oes_texture_3d(GLenum pname);
	static const char* is_define_oes_texture_border_clamp(GLenum pname);
	static const char* is_define_oes_texture_buffer(GLenum pname);
	static const char* is_define_oes_texture_compression_astc(GLenum pname);
	static const char* is_define_oes_texture_cube_map_array(GLenum pname);
	static const char* is_define_oes_texture_float(GLenum pname);
	static const char* is_define_oes_texture_float_linear(GLenum pname);
	static const char* is_define_oes_texture_half_float(GLenum pname);
	static const char* is_define_oes_texture_half_float_linear(GLenum pname);
	static const char* is_define_oes_texture_npot(GLenum pname);
	static const char* is_define_oes_texture_stencil8(GLenum pname);
	static const char* is_define_oes_texture_storage_multisample_2d_array(GLenum pname);
	static const char* is_define_oes_texture_view(GLenum pname);
	static const char* is_define_oes_vertex_array_object(GLenum pname);
	static const char* is_define_oes_vertex_half_float(GLenum pname);
	static const char* is_define_oes_vertex_type_10_10_10_2(GLenum pname);
	static const char* is_define_amd_compressed_3dc_texture(GLenum pname);
	static const char* is_define_amd_compressed_atc_texture(GLenum pname);
	static const char* is_define_amd_performance_monitor(GLenum pname);
	static const char* is_define_amd_program_binary_z400(GLenum pname);
	static const char* is_define_android_extension_pack_es31a(GLenum pname);
	static const char* is_define_angle_depth_texture(GLenum pname);
	static const char* is_define_angle_framebuffer_blit(GLenum pname);
	static const char* is_define_angle_framebuffer_multisample(GLenum pname);
	static const char* is_define_angle_instanced_arrays(GLenum pname);
	static const char* is_define_angle_pack_reverse_row_order(GLenum pname);
	static const char* is_define_angle_program_binary(GLenum pname);
	static const char* is_define_angle_texture_compression_dxt3(GLenum pname);
	static const char* is_define_angle_texture_compression_dxt5(GLenum pname);
	static const char* is_define_angle_texture_usage(GLenum pname);
	static const char* is_define_angle_translated_shader_source(GLenum pname);
	static const char* is_define_apple_clip_distance(GLenum pname);
	static const char* is_define_apple_color_buffer_packed_float(GLenum pname);
	static const char* is_define_apple_copy_texture_levels(GLenum pname);
	static const char* is_define_apple_framebuffer_multisample(GLenum pname);
	static const char* is_define_apple_rgb_422(GLenum pname);
	static const char* is_define_apple_sync(GLenum pname);
	static const char* is_define_apple_texture_format_bgra8888(GLenum pname);
	static const char* is_define_apple_texture_max_level(GLenum pname);
	static const char* is_define_apple_texture_packed_float(GLenum pname);
	static const char* is_define_arm_mali_program_binary(GLenum pname);
	static const char* is_define_arm_mali_shader_binary(GLenum pname);
	static const char* is_define_arm_rgba8(GLenum pname);
	static const char* is_define_arm_shader_framebuffer_fetch(GLenum pname);
	static const char* is_define_arm_shader_framebuffer_fetch_depth_stencil(GLenum pname);
	static const char* is_define_dmp_program_binary(GLenum pname);
	static const char* is_define_dmp_shader_binary(GLenum pname);
	static const char* is_define_ext_yuv_target(GLenum pname);
	static const char* is_define_ext_base_instance(GLenum pname);
	static const char* is_define_ext_blend_func_extended(GLenum pname);
	static const char* is_define_ext_blend_minmax(GLenum pname);
	static const char* is_define_ext_buffer_storage(GLenum pname);
	static const char* is_define_ext_color_buffer_float(GLenum pname);
	static const char* is_define_ext_color_buffer_half_float(GLenum pname);
	static const char* is_define_ext_copy_image(GLenum pname);
	static const char* is_define_ext_debug_label(GLenum pname);
	static const char* is_define_ext_debug_marker(GLenum pname);
	static const char* is_define_ext_discard_framebuffer(GLenum pname);
	static const char* is_define_ext_disjoint_timer_query(GLenum pname);
	static const char* is_define_ext_draw_buffers(GLenum pname);
	static const char* is_define_ext_draw_buffers_indexed(GLenum pname);
	static const char* is_define_ext_draw_elements_base_vertex(GLenum pname);
	static const char* is_define_ext_draw_instanced(GLenum pname);
	static const char* is_define_ext_float_blend(GLenum pname);
	static const char* is_define_ext_geometry_point_size(GLenum pname);
	static const char* is_define_ext_geometry_shader(GLenum pname);
	static const char* is_define_ext_gpu_shader5(GLenum pname);
	static const char* is_define_ext_instanced_arrays(GLenum pname);
	static const char* is_define_ext_map_buffer_range(GLenum pname);
	static const char* is_define_ext_multi_draw_arrays(GLenum pname);
	static const char* is_define_ext_multi_draw_indirect(GLenum pname);
	static const char* is_define_ext_multisampled_compatibility(GLenum pname);
	static const char* is_define_ext_multisampled_render_to_texture(GLenum pname);
	static const char* is_define_ext_multiview_draw_buffers(GLenum pname);
	static const char* is_define_ext_occlusion_query_boolean(GLenum pname);
	static const char* is_define_ext_polygon_offset_clamp(GLenum pname);
	static const char* is_define_ext_post_depth_coverage(GLenum pname);
	static const char* is_define_ext_primitive_bounding_box(GLenum pname);
	static const char* is_define_ext_pvrtc_srgb(GLenum pname);
	static const char* is_define_ext_raster_multisample(GLenum pname);
	static const char* is_define_ext_read_format_bgra(GLenum pname);
	static const char* is_define_ext_render_snorm(GLenum pname);
	static const char* is_define_ext_robustness(GLenum pname);
	static const char* is_define_ext_srgb(GLenum pname);
	static const char* is_define_ext_srgb_write_control(GLenum pname);
	static const char* is_define_ext_separate_shader_objects(GLenum pname);
	static const char* is_define_ext_shader_framebuffer_fetch(GLenum pname);
	static const char* is_define_ext_shader_group_vote(GLenum pname);
	static const char* is_define_ext_shader_implicit_conversions(GLenum pname);
	static const char* is_define_ext_shader_integer_mix(GLenum pname);
	static const char* is_define_ext_shader_io_blocks(GLenum pname);
	static const char* is_define_ext_shader_pixel_local_storage(GLenum pname);
	static const char* is_define_ext_shader_pixel_local_storage2(GLenum pname);
	static const char* is_define_ext_shader_texture_lod(GLenum pname);
	static const char* is_define_ext_shadow_samplers(GLenum pname);
	static const char* is_define_ext_sparse_texture(GLenum pname);
	static const char* is_define_ext_tessellation_point_size(GLenum pname);
	static const char* is_define_ext_tessellation_shader(GLenum pname);
	static const char* is_define_ext_texture_border_clamp(GLenum pname);
	static const char* is_define_ext_texture_buffer(GLenum pname);
	static const char* is_define_ext_texture_compression_dxt1(GLenum pname);
	static const char* is_define_ext_texture_compression_s3tc(GLenum pname);
	static const char* is_define_ext_texture_cube_map_array(GLenum pname);
	static const char* is_define_ext_texture_filter_anisotropic(GLenum pname);
	static const char* is_define_ext_texture_filter_minmax(GLenum pname);
	static const char* is_define_ext_texture_format_bgra8888(GLenum pname);
	static const char* is_define_ext_texture_norm16(GLenum pname);
	static const char* is_define_ext_texture_rg(GLenum pname);
	static const char* is_define_ext_texture_srgb_r8(GLenum pname);
	static const char* is_define_ext_texture_srgb_rg8(GLenum pname);
	static const char* is_define_ext_texture_srgb_decode(GLenum pname);
	static const char* is_define_ext_texture_storage(GLenum pname);
	static const char* is_define_ext_texture_type_2_10_10_10_rev(GLenum pname);
	static const char* is_define_ext_texture_view(GLenum pname);
	static const char* is_define_ext_unpack_subimage(GLenum pname);
	static const char* is_define_fj_shader_binary_gccso(GLenum pname);
	static const char* is_define_img_framebuffer_downsample(GLenum pname);
	static const char* is_define_img_multisampled_render_to_texture(GLenum pname);
	static const char* is_define_img_program_binary(GLenum pname);
	static const char* is_define_img_read_format(GLenum pname);
	static const char* is_define_img_shader_binary(GLenum pname);
	static const char* is_define_img_texture_compression_pvrtc(GLenum pname);
	static const char* is_define_img_texture_compression_pvrtc2(GLenum pname);
	static const char* is_define_img_texture_filter_cubic(GLenum pname);
	static const char* is_define_intel_framebuffer_cmaa(GLenum pname);
	static const char* is_define_intel_performance_query(GLenum pname);
	static const char* is_define_nv_bindless_texture(GLenum pname);
	static const char* is_define_nv_blend_equation_advanced(GLenum pname);
	static const char* is_define_nv_blend_equation_advanced_coherent(GLenum pname);
	static const char* is_define_nv_conditional_render(GLenum pname);
	static const char* is_define_nv_conservative_raster(GLenum pname);
	static const char* is_define_nv_copy_buffer(GLenum pname);
	static const char* is_define_nv_coverage_sample(GLenum pname);
	static const char* is_define_nv_depth_nonlinear(GLenum pname);
	static const char* is_define_nv_draw_buffers(GLenum pname);
	static const char* is_define_nv_draw_instanced(GLenum pname);
	static const char* is_define_nv_explicit_attrib_location(GLenum pname);
	static const char* is_define_nv_fbo_color_attachments(GLenum pname);
	static const char* is_define_nv_fence(GLenum pname);
	static const char* is_define_nv_fill_rectangle(GLenum pname);
	static const char* is_define_nv_fragment_coverage_to_color(GLenum pname);
	static const char* is_define_nv_fragment_shader_interlock(GLenum pname);
	static const char* is_define_nv_framebuffer_blit(GLenum pname);
	static const char* is_define_nv_framebuffer_mixed_samples(GLenum pname);
	static const char* is_define_nv_framebuffer_multisample(GLenum pname);
	static const char* is_define_nv_generate_mipmap_srgb(GLenum pname);
	static const char* is_define_nv_geometry_shader_passthrough(GLenum pname);
	static const char* is_define_nv_image_formats(GLenum pname);
	static const char* is_define_nv_instanced_arrays(GLenum pname);
	static const char* is_define_nv_internalformat_sample_query(GLenum pname);
	static const char* is_define_nv_non_square_matrices(GLenum pname);
	static const char* is_define_nv_path_rendering(GLenum pname);
	static const char* is_define_nv_path_rendering_shared_edge(GLenum pname);
	static const char* is_define_nv_polygon_mode(GLenum pname);
	static const char* is_define_nv_read_buffer(GLenum pname);
	static const char* is_define_nv_read_buffer_front(GLenum pname);
	static const char* is_define_nv_read_depth(GLenum pname);
	static const char* is_define_nv_read_depth_stencil(GLenum pname);
	static const char* is_define_nv_read_stencil(GLenum pname);
	static const char* is_define_nv_srgb_formats(GLenum pname);
	static const char* is_define_nv_sample_locations(GLenum pname);
	static const char* is_define_nv_sample_mask_override_coverage(GLenum pname);
	static const char* is_define_nv_shader_noperspective_interpolation(GLenum pname);
	static const char* is_define_nv_shadow_samplers_array(GLenum pname);
	static const char* is_define_nv_shadow_samplers_cube(GLenum pname);
	static const char* is_define_nv_texture_border_clamp(GLenum pname);
	static const char* is_define_nv_texture_compression_s3tc_update(GLenum pname);
	static const char* is_define_nv_texture_npot_2d_mipmap(GLenum pname);
	static const char* is_define_nv_viewport_array(GLenum pname);
	static const char* is_define_nv_viewport_array2(GLenum pname);
	static const char* is_define_ovr_multiview(GLenum pname);
	static const char* is_define_ovr_multiview2(GLenum pname);
	static const char* is_define_ovr_multiview_multisampled_render_to_texture(GLenum pname);
	static const char* is_define_qcom_alpha_test(GLenum pname);
	static const char* is_define_qcom_binning_control(GLenum pname);
	static const char* is_define_qcom_driver_control(GLenum pname);
	static const char* is_define_qcom_extended_get(GLenum pname);
	static const char* is_define_qcom_extended_get2(GLenum pname);
	static const char* is_define_qcom_perfmon_global_mode(GLenum pname);
	static const char* is_define_qcom_tiled_rendering(GLenum pname);
	static const char* is_define_qcom_writeonly_rendering(GLenum pname);
	static const char* is_define_viv_shader_binary(GLenum pname);

	// -----------------------------------------------------------------
	// variables
	// -----------------------------------------------------------------
	/** all the function registered */
	static Entry_v _registered;
	/** all registered textures */
	static Texture_v _textures;
	/** all registered program */
	static Program_v _programs;
	/** all registered shader */
	static Shader_v _shaders;
	/** all registered buffer */
	static Buffer_v _buffers;
	/** actual bound buffer */
	static Buffer_t* _bound_buffer[BUFFER_SIZE];
	/** calls function history */
	static History_cb _call_history;
	/** actuel bound program */
	static uint _program_bound;
	/** dump data sate */
	static bool _dump_data;
	/** log sate */
	static bool _append_to_log_calls;
	/** capability allowed in function (gl)Enable/Disable */
	static uint_v _allowed_enable;
	/** opengl state */
	static Map<uint,uchar> _states;
	/** error message */
	static const char* invalid_framebuffer_operation;
	static const char* out_of_memory;
	static const char* invalid_operation;
	static const char* invalid_value;
	static const char* invalid_enum;
	static const char* unknown_error;
	/** break on error state */
	static bool _break_on_error;
	/** break on warning state */
	static bool _break_on_warning;
	/** output buffer */
	static String _output_buffer;
	/** current frame id */
	static uint frame; // frame id
	/** console callback */
	static fnc_console_cb _console_cb;

	// -----------------------------------------------------------------
	// OpenGL/ES function (call it "the mess")
	// -----------------------------------------------------------------
public:
	// -----------------------------------------------------------------
	// eglext.h
	// -----------------------------------------------------------------

	/** all eglXXX function */
	class egl
	{
	public:
		/** todo: all function in egl.h */

		// all the define mess -----------------------------------------
		static const char* is_define_egl_h(GLenum pname);
		static const char* is_define_version_1_0(GLenum pname);
		static const char* is_define_egl_version_1_1(GLenum pname);
		static const char* is_define_egl_version_1_2(GLenum pname);
		static const char* is_define_egl_version_1_3(GLenum pname);
		static const char* is_define_egl_version_1_4(GLenum pname);
		static const char* is_define_egl_version_1_5(GLenum pname);

		static const char* is_define_eglext_h(GLenum pname);

		static const char* is_define_khr_cl_event(GLenum pname);
		static const char* is_define_khr_cl_event2(GLenum pname){ return nullptr; };
		static const char* is_define_khr_client_get_all_proc_addresses(GLenum pname){ return nullptr; };
		static const char* is_define_khr_config_attribs(GLenum pname);
		static const char* is_define_khr_context_flush_control(GLenum pname);
		static const char* is_define_khr_create_context(GLenum pname);
		static const char* is_define_khr_create_context_no_error(GLenum pname);
		static const char* is_define_khr_debug(GLenum pname);
		static const char* is_define_khr_fence_sync(GLenum pname);
		static const char* is_define_khr_get_all_proc_addresses(GLenum pname){ return nullptr; };
		static const char* is_define_khr_gl_colorspace(GLenum pname);
		static const char* is_define_khr_gl_renderbuffer_image(GLenum pname);
		static const char* is_define_khr_gl_texture_2d_image(GLenum pname);
		static const char* is_define_khr_gl_texture_3d_image(GLenum pname);
		static const char* is_define_khr_gl_texture_cubemap_image(GLenum pname);
		static const char* is_define_khr_image(GLenum pname);
		static const char* is_define_khr_image_base(GLenum pname);
		static const char* is_define_khr_image_pixmap(GLenum pname){ return nullptr; };
		static const char* is_define_khr_lock_surface(GLenum pname);
		static const char* is_define_khr_lock_surface2(GLenum pname);
		static const char* is_define_khr_lock_surface3(GLenum pname){ return nullptr; };
		static const char* is_define_khr_mutable_render_buffer(GLenum pname);
		static const char* is_define_khr_no_config_context(GLenum pname){ return nullptr; };
		static const char* is_define_khr_partial_update(GLenum pname);
		static const char* is_define_khr_platform_android(GLenum pname);
		static const char* is_define_khr_platform_gbm(GLenum pname);
		static const char* is_define_khr_platform_wayland(GLenum pname);
		static const char* is_define_khr_platform_x11(GLenum pname);
		static const char* is_define_khr_reusable_sync(GLenum pname);
		static const char* is_define_khr_stream(GLenum pname);
		static const char* is_define_khr_stream_attrib(GLenum pname){ return nullptr; };
		static const char* is_define_khr_stream_consumer_gltexture(GLenum pname);
		static const char* is_define_khr_stream_cross_process_fd(GLenum pname){ return nullptr; };
		static const char* is_define_khr_stream_fifo(GLenum pname);
		static const char* is_define_khr_stream_producer_aldatalocator(GLenum pname){ return nullptr; };
		static const char* is_define_khr_stream_producer_eglsurface(GLenum pname);
		static const char* is_define_khr_surfaceless_context(GLenum pname){ return nullptr; };
		static const char* is_define_khr_swap_buffers_with_damage(GLenum pname){ return nullptr; };
		static const char* is_define_khr_vg_parent_image(GLenum pname);
		static const char* is_define_khr_wait_sync(GLenum pname){ return nullptr; };
		static const char* is_define_android_blob_cache(GLenum pname){ return nullptr; };
		static const char* is_define_android_create_native_client_buffer(GLenum pname);
		static const char* is_define_android_framebuffer_target(GLenum pname);
		static const char* is_define_android_front_buffer_auto_refresh(GLenum pname);
		static const char* is_define_android_image_native_buffer(GLenum pname);
		static const char* is_define_android_native_fence_sync(GLenum pname);
		static const char* is_define_android_presentation_time(GLenum pname){ return nullptr; };
		static const char* is_define_android_recordable(GLenum pname);
		static const char* is_define_angle_d3d_share_handle_client_buffer(GLenum pname);
		static const char* is_define_angle_device_d3d(GLenum pname);
		static const char* is_define_angle_query_surface_pointer(GLenum pname){ return nullptr; };
		static const char* is_define_angle_surface_d3d_texture_2d_share_handle(GLenum pname){ return nullptr; };
		static const char* is_define_angle_window_fixed_size(GLenum pname);
		static const char* is_define_arm_implicit_external_sync(GLenum pname);
		static const char* is_define_arm_pixmap_multisample_discard(GLenum pname);
		static const char* is_define_ext_buffer_age(GLenum pname);
		static const char* is_define_ext_client_extensions(GLenum pname){ return nullptr; };
		static const char* is_define_ext_create_context_robustness(GLenum pname);
		static const char* is_define_ext_device_base(GLenum pname);
		static const char* is_define_ext_device_drm(GLenum pname);
		static const char* is_define_ext_device_enumeration(GLenum pname){ return nullptr; };
		static const char* is_define_ext_device_openwf(GLenum pname);
		static const char* is_define_ext_device_query(GLenum pname){ return nullptr; };
		static const char* is_define_ext_image_dma_buf_import(GLenum pname);
		static const char* is_define_ext_multiview_window(GLenum pname);
		static const char* is_define_ext_output_base(GLenum pname);
		static const char* is_define_ext_output_drm(GLenum pname);
		static const char* is_define_ext_output_openwf(GLenum pname);
		static const char* is_define_ext_platform_base(GLenum pname){ return nullptr; };
		static const char* is_define_ext_platform_device(GLenum pname);
		static const char* is_define_ext_platform_wayland(GLenum pname);
		static const char* is_define_ext_platform_x11(GLenum pname);
		static const char* is_define_ext_protected_content(GLenum pname);
		static const char* is_define_ext_protected_surface(GLenum pname){ return nullptr; };
		static const char* is_define_ext_stream_consumer_egloutput(GLenum pname){ return nullptr; };
		static const char* is_define_ext_swap_buffers_with_damage(GLenum pname){ return nullptr; };
		static const char* is_define_ext_yuv_surface(GLenum pname);
		static const char* is_define_hi_clientpixmap(GLenum pname);
		static const char* is_define_hi_colorformats(GLenum pname);
		static const char* is_define_img_context_priority(GLenum pname);
		static const char* is_define_img_image_plane_attribs(GLenum pname);
		static const char* is_define_mesa_drm_image(GLenum pname);
		static const char* is_define_mesa_image_dma_buf_export(GLenum pname){ return nullptr; };
		static const char* is_define_mesa_platform_gbm(GLenum pname);
		static const char* is_define_mesa_platform_surfaceless(GLenum pname);
		static const char* is_define_nok_swap_region(GLenum pname){ return nullptr; };
		static const char* is_define_nok_swap_region2(GLenum pname){ return nullptr; };
		static const char* is_define_nok_texture_from_pixmap(GLenum pname);
		static const char* is_define_nv_3dvision_surface(GLenum pname);
		static const char* is_define_nv_coverage_sample(GLenum pname);
		static const char* is_define_nv_coverage_sample_resolve(GLenum pname);
		static const char* is_define_nv_cuda_event(GLenum pname);
		static const char* is_define_nv_depth_nonlinear(GLenum pname);
		static const char* is_define_nv_device_cuda(GLenum pname);
		static const char* is_define_nv_native_query(GLenum pname){ return nullptr; };
		static const char* is_define_nv_post_convert_rounding(GLenum pname){ return nullptr; };
		static const char* is_define_nv_post_sub_buffer(GLenum pname);
		static const char* is_define_nv_robustness_video_memory_purge(GLenum pname);
		static const char* is_define_nv_stream_consumer_gltexture_yuv(GLenum pname);
		static const char* is_define_nv_stream_metadata(GLenum pname);
		static const char* is_define_nv_stream_sync(GLenum pname);
		static const char* is_define_nv_sync(GLenum pname);
		static const char* is_define_nv_system_time(GLenum pname){ return nullptr; };
		static const char* is_define_tizen_image_native_buffer(GLenum pname);
		static const char* is_define_tizen_image_native_surface(GLenum pname);

		// EGL_ANDROID_blob_cache
		#define eglSetBlobCacheFuncsANDROID(...) gl::egl::SetBlobCacheFuncsANDROID( __VA_ARGS__, __FILE__,__LINE__ )
		static void SetBlobCacheFuncsANDROID (EGLDisplay dpy, EGLSetBlobFuncANDROID set, EGLGetBlobFuncANDROID get, const char* file, int line);
		static PFNEGLSETBLOBCACHEFUNCSANDROIDPROC egl_SetBlobCacheFuncsANDROID;

		// EGL_ANDROID_create_native_client_buffer
		#define eglCreateNativeClientBufferANDROID(...) gl::egl::CreateNativeClientBufferANDROID( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLClientBuffer CreateNativeClientBufferANDROID (const EGLint *attrib_list, const char* file, int line);
		static PFNEGLCREATENATIVECLIENTBUFFERANDROIDPROC egl_CreateNativeClientBufferANDROID;

		// EGL_ANDROID_native_fence_sync
		#define eglDupNativeFenceFDANDROID(...) gl::egl::DupNativeFenceFDANDROID( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLint DupNativeFenceFDANDROID (EGLDisplay dpy, EGLSyncKHR sync, const char* file, int line);
		static PFNEGLDUPNATIVEFENCEFDANDROIDPROC egl_DupNativeFenceFDANDROID;

		// EGL_ANDROID_presentation_time
		#define eglPresentationTimeANDROID(...) gl::egl::PresentationTimeANDROID( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean PresentationTimeANDROID (EGLDisplay dpy, EGLSurface surface, EGLnsecsANDROID time, const char* file, int line);
		static PFNEGLPRESENTATIONTIMEANDROIDPROC egl_PresentationTimeANDROID;

		// EGL_ANGLE_query_surface_pointer
		#define eglQuerySurfacePointerANGLE(...) gl::egl::QuerySurfacePointerANGLE( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean QuerySurfacePointerANGLE (EGLDisplay dpy, EGLSurface surface, EGLint attribute, void **value, const char* file, int line);
		static PFNEGLQUERYSURFACEPOINTERANGLEPROC egl_QuerySurfacePointerANGLE;

		// EGL_EXT_device_base
		#define eglQueryDeviceAttribEXT(...) gl::egl::QueryDeviceAttribEXT( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean QueryDeviceAttribEXT (EGLDeviceEXT device, EGLint attribute, EGLAttrib *value, const char* file, int line);
		static PFNEGLQUERYDEVICEATTRIBEXTPROC egl_QueryDeviceAttribEXT;

		#define eglQueryDeviceStringEXT(...) gl::egl::QueryDeviceStringEXT( __VA_ARGS__, __FILE__,__LINE__ )
		static const char * QueryDeviceStringEXT (EGLDeviceEXT device, EGLint name, const char* file, int line);
		static PFNEGLQUERYDEVICESTRINGEXTPROC egl_QueryDeviceStringEXT;

		#define eglQueryDevicesEXT(...) gl::egl::QueryDevicesEXT( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean QueryDevicesEXT (EGLint max_devices, EGLDeviceEXT *devices, EGLint *num_devices, const char* file, int line);
		static PFNEGLQUERYDEVICESEXTPROC egl_QueryDevicesEXT;

		#define eglQueryDisplayAttribEXT(...) gl::egl::QueryDisplayAttribEXT( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean QueryDisplayAttribEXT (EGLDisplay dpy, EGLint attribute, EGLAttrib *value, const char* file, int line);
		static PFNEGLQUERYDISPLAYATTRIBEXTPROC egl_QueryDisplayAttribEXT;

		// EGL_EXT_output_base
		#define eglGetOutputLayersEXT(...) gl::egl::GetOutputLayersEXT( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean GetOutputLayersEXT (EGLDisplay dpy, const EGLAttrib *attrib_list, EGLOutputLayerEXT *layers, EGLint max_layers, EGLint *num_layers, const char* file, int line);
		static PFNEGLGETOUTPUTLAYERSEXTPROC egl_GetOutputLayersEXT;

		#define eglGetOutputPortsEXT(...) gl::egl::GetOutputPortsEXT( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean GetOutputPortsEXT (EGLDisplay dpy, const EGLAttrib *attrib_list, EGLOutputPortEXT *ports, EGLint max_ports, EGLint *num_ports, const char* file, int line);
		static PFNEGLGETOUTPUTPORTSEXTPROC egl_GetOutputPortsEXT;

		#define eglOutputLayerAttribEXT(...) gl::egl::OutputLayerAttribEXT( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean OutputLayerAttribEXT (EGLDisplay dpy, EGLOutputLayerEXT layer, EGLint attribute, EGLAttrib value, const char* file, int line);
		static PFNEGLOUTPUTLAYERATTRIBEXTPROC egl_OutputLayerAttribEXT;

		#define eglOutputPortAttribEXT(...) gl::egl::OutputPortAttribEXT( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean OutputPortAttribEXT (EGLDisplay dpy, EGLOutputPortEXT port, EGLint attribute, EGLAttrib value, const char* file, int line);
		static PFNEGLOUTPUTPORTATTRIBEXTPROC egl_OutputPortAttribEXT;

		#define eglQueryOutputLayerAttribEXT(...) gl::egl::QueryOutputLayerAttribEXT( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean QueryOutputLayerAttribEXT (EGLDisplay dpy, EGLOutputLayerEXT layer, EGLint attribute, EGLAttrib *value, const char* file, int line);
		static PFNEGLQUERYOUTPUTLAYERATTRIBEXTPROC egl_QueryOutputLayerAttribEXT;

		#define eglQueryOutputLayerStringEXT(...) gl::egl::QueryOutputLayerStringEXT( __VA_ARGS__, __FILE__,__LINE__ )
		static const char* QueryOutputLayerStringEXT(EGLDisplay dpy, EGLOutputLayerEXT layer, EGLint name, const char* file, int line);
		static PFNEGLQUERYOUTPUTLAYERSTRINGEXTPROC egl_QueryOutputLayerStringEXT;

		#define eglQueryOutputPortAttribEXT(...) gl::egl::QueryOutputPortAttribEXT( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean QueryOutputPortAttribEXT (EGLDisplay dpy, EGLOutputPortEXT port, EGLint attribute, EGLAttrib *value, const char* file, int line);
		static PFNEGLQUERYOUTPUTPORTATTRIBEXTPROC egl_QueryOutputPortAttribEXT;

		#define eglQueryOutputPortStringEXT(...) gl::egl::QueryOutputPortStringEXT( __VA_ARGS__, __FILE__,__LINE__ )
		static const char* QueryOutputPortStringEXT (EGLDisplay dpy, EGLOutputPortEXT port, EGLint name, const char* file, int line);
		static PFNEGLQUERYOUTPUTPORTSTRINGEXTPROC egl_QueryOutputPortStringEXT;

		// EGL_EXT_platform_base
		#define eglCreatePlatformPixmapSurfaceEXT(...) gl::egl::CreatePlatformPixmapSurfaceEXT( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLSurface CreatePlatformPixmapSurfaceEXT (EGLDisplay dpy, EGLConfig config, void *native_pixmap, const EGLint *attrib_list, const char* file, int line);
		static PFNEGLCREATEPLATFORMPIXMAPSURFACEEXTPROC egl_CreatePlatformPixmapSurfaceEXT;

		#define eglCreatePlatformWindowSurfaceEXT(...) gl::egl::CreatePlatformWindowSurfaceEXT( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLSurface CreatePlatformWindowSurfaceEXT (EGLDisplay dpy, EGLConfig config, void *native_window, const EGLint *attrib_list, const char* file, int line);
		static PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC egl_CreatePlatformWindowSurfaceEXT;

		#define eglGetPlatformDisplayEXT(...) gl::egl::GetPlatformDisplayEXT( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLDisplay GetPlatformDisplayEXT (EGLenum platform, void *native_display, const EGLint *attrib_list, const char* file, int line);
		static PFNEGLGETPLATFORMDISPLAYEXTPROC egl_GetPlatformDisplayEXT;

		// EGL_EXT_stream_consumer_egloutput
		#define eglStreamConsumerOutputEXT(...) gl::egl::StreamConsumerOutputEXT( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean StreamConsumerOutputEXT (EGLDisplay dpy, EGLStreamKHR stream, EGLOutputLayerEXT layer, const char* file, int line);
		static PFNEGLSTREAMCONSUMEROUTPUTEXTPROC egl_StreamConsumerOutputEXT;

		// EGL_EXT_swap_buffers_with_damage
		#define eglSwapBuffersWithDamageEXT(...) gl::egl::SwapBuffersWithDamageEXT( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean SwapBuffersWithDamageEXT (EGLDisplay dpy, EGLSurface surface, EGLint *rects, EGLint n_rects, const char* file, int line);
		static PFNEGLSWAPBUFFERSWITHDAMAGEEXTPROC egl_SwapBuffersWithDamageEXT;

		// EGL_HI_clientpixmap
		#define eglCreatePixmapSurfaceHI(...) gl::egl::CreatePixmapSurfaceHI( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLSurface CreatePixmapSurfaceHI (EGLDisplay dpy, EGLConfig config, struct EGLClientPixmapHI *pixmap, const char* file, int line);
		static PFNEGLCREATEPIXMAPSURFACEHIPROC egl_CreatePixmapSurfaceHI;

		// EGL_KHR_cl_event2
		#define eglCreateSync64KHR(...) gl::egl::CreateSync64KHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLSyncKHR CreateSync64KHR (EGLDisplay dpy, EGLenum type, const EGLAttribKHR *attrib_list, const char* file, int line);
		static PFNEGLCREATESYNC64KHRPROC egl_CreateSync64KHR;

		// EGL_KHR_debug
		#define eglLabelObjectKHR(...) gl::egl::LabelObjectKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLint LabelObjectKHR (EGLDisplay display, EGLenum objectType, EGLObjectKHR object, EGLLabelKHR label, const char* file, int line);
		static PFNEGLLABELOBJECTKHRPROC egl_LabelObjectKHR;

		#define eglQueryDebugKHR(...) gl::egl::QueryDebugKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean QueryDebugKHR (EGLint attribute, EGLAttrib *value, const char* file, int line);
		static PFNEGLQUERYDEBUGKHRPROC egl_QueryDebugKHR;

		// EGL_KHR_fence_sync
		#define eglClientWaitSyncKHR(...) gl::egl::ClientWaitSyncKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLint ClientWaitSyncKHR (EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, EGLTimeKHR timeout, const char* file, int line);
		static PFNEGLCLIENTWAITSYNCKHRPROC egl_ClientWaitSyncKHR;

		#define eglCreateSyncKHR(...) gl::egl::CreateSyncKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLSyncKHR CreateSyncKHR (EGLDisplay dpy, EGLenum type, const EGLint *attrib_list, const char* file, int line);
		static PFNEGLCREATESYNCKHRPROC egl_CreateSyncKHR;

		#define eglDestroySyncKHR(...) gl::egl::DestroySyncKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean DestroySyncKHR (EGLDisplay dpy, EGLSyncKHR sync, const char* file, int line);
		static PFNEGLDESTROYSYNCKHRPROC egl_DestroySyncKHR;

		#define eglGetSyncAttribKHR(...) gl::egl::GetSyncAttribKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean GetSyncAttribKHR (EGLDisplay dpy, EGLSyncKHR sync, EGLint attribute, EGLint *value, const char* file, int line);
		static PFNEGLGETSYNCATTRIBKHRPROC egl_GetSyncAttribKHR;

		// EGL_KHR_image
		#define eglCreateImageKHR(...) gl::egl::CreateImageKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLImageKHR CreateImageKHR (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list, const char* file, int line);
		static PFNEGLCREATEIMAGEKHRPROC egl_CreateImageKHR;

		#define eglDestroyImageKHR(...) gl::egl::DestroyImageKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean DestroyImageKHR (EGLDisplay dpy, EGLImageKHR image, const char* file, int line);
		static PFNEGLDESTROYIMAGEKHRPROC egl_DestroyImageKHR;

		// EGL_KHR_lock_surface
		#define eglLockSurfaceKHR(...) gl::egl::LockSurfaceKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean LockSurfaceKHR (EGLDisplay dpy, EGLSurface surface, const EGLint *attrib_list, const char* file, int line);
		static PFNEGLLOCKSURFACEKHRPROC egl_LockSurfaceKHR;

		#define eglUnlockSurfaceKHR(...) gl::egl::UnlockSurfaceKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean UnlockSurfaceKHR (EGLDisplay dpy, EGLSurface surface, const char* file, int line);
		static PFNEGLUNLOCKSURFACEKHRPROC egl_UnlockSurfaceKHR;

		// EGL_KHR_lock_surface3
		#define eglQuerySurface64KHR(...) gl::egl::QuerySurface64KHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean QuerySurface64KHR (EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLAttribKHR *value, const char* file, int line);
		static PFNEGLQUERYSURFACE64KHRPROC egl_QuerySurface64KHR;

		// EGL_KHR_partial_update
		#define eglSetDamageRegionKHR(...) gl::egl::SetDamageRegionKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean SetDamageRegionKHR (EGLDisplay dpy, EGLSurface surface, EGLint *rects, EGLint n_rects, const char* file, int line);
		static PFNEGLSETDAMAGEREGIONKHRPROC egl_SetDamageRegionKHR;

		// EGL_KHR_reusable_sync
		#define eglSignalSyncKHR(...) gl::egl::SignalSyncKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean SignalSyncKHR (EGLDisplay dpy, EGLSyncKHR sync, EGLenum mode, const char* file, int line);
		static PFNEGLSIGNALSYNCKHRPROC egl_SignalSyncKHR;

		// EGL_KHR_stream
		#define eglCreateStreamKHR(...) gl::egl::CreateStreamKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLStreamKHR CreateStreamKHR (EGLDisplay dpy, const EGLint *attrib_list, const char* file, int line);
		static PFNEGLCREATESTREAMKHRPROC egl_CreateStreamKHR;

		#define eglDestroyStreamKHR(...) gl::egl::DestroyStreamKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean DestroyStreamKHR (EGLDisplay dpy, EGLStreamKHR stream, const char* file, int line);
		static PFNEGLDESTROYSTREAMKHRPROC egl_DestroyStreamKHR;

		#define eglQueryStreamKHR(...) gl::egl::QueryStreamKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean QueryStreamKHR (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLint *value, const char* file, int line);
		static PFNEGLQUERYSTREAMKHRPROC egl_QueryStreamKHR;

		#define eglQueryStreamu64KHR(...) gl::egl::QueryStreamu64KHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean QueryStreamu64KHR (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLuint64KHR *value, const char* file, int line);
		static PFNEGLQUERYSTREAMU64KHRPROC egl_QueryStreamu64KHR;

		#define eglStreamAttribKHR(...) gl::egl::StreamAttribKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean StreamAttribKHR (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLint value, const char* file, int line);
		static PFNEGLSTREAMATTRIBKHRPROC egl_StreamAttribKHR;

		// EGL_KHR_stream_attrib
		#define eglCreateStreamAttribKHR(...) gl::egl::CreateStreamAttribKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLStreamKHR CreateStreamAttribKHR (EGLDisplay dpy, const EGLAttrib *attrib_list, const char* file, int line);
		static PFNEGLCREATESTREAMATTRIBKHRPROC egl_CreateStreamAttribKHR;

		#define eglQueryStreamAttribKHR(...) gl::egl::QueryStreamAttribKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean QueryStreamAttribKHR (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLAttrib *value, const char* file, int line);
		static PFNEGLQUERYSTREAMATTRIBKHRPROC egl_QueryStreamAttribKHR;

		#define eglSetStreamAttribKHR(...) gl::egl::SetStreamAttribKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean SetStreamAttribKHR (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLAttrib value, const char* file, int line);
		static PFNEGLSETSTREAMATTRIBKHRPROC egl_SetStreamAttribKHR;

		#define eglStreamConsumerAcquireAttribKHR(...) gl::egl::StreamConsumerAcquireAttribKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean StreamConsumerAcquireAttribKHR (EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib *attrib_list, const char* file, int line);
		static PFNEGLSTREAMCONSUMERACQUIREATTRIBKHRPROC egl_StreamConsumerAcquireAttribKHR;

		#define eglStreamConsumerReleaseAttribKHR(...) gl::egl::StreamConsumerReleaseAttribKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean StreamConsumerReleaseAttribKHR (EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib *attrib_list, const char* file, int line);
		static PFNEGLSTREAMCONSUMERRELEASEATTRIBKHRPROC egl_StreamConsumerReleaseAttribKHR;

		// EGL_KHR_stream_consumer_gltexture
		#define eglStreamConsumerAcquireKHR(...) gl::egl::StreamConsumerAcquireKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean StreamConsumerAcquireKHR (EGLDisplay dpy, EGLStreamKHR stream, const char* file, int line);
		static PFNEGLSTREAMCONSUMERACQUIREKHRPROC egl_StreamConsumerAcquireKHR;

		#define eglStreamConsumerGLTextureExternalKHR(...) gl::egl::StreamConsumerGLTextureExternalKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean StreamConsumerGLTextureExternalKHR (EGLDisplay dpy, EGLStreamKHR stream, const char* file, int line);
		static PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALKHRPROC egl_StreamConsumerGLTextureExternalKHR;

		#define eglStreamConsumerReleaseKHR(...) gl::egl::StreamConsumerReleaseKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean StreamConsumerReleaseKHR (EGLDisplay dpy, EGLStreamKHR stream, const char* file, int line);
		static PFNEGLSTREAMCONSUMERRELEASEKHRPROC egl_StreamConsumerReleaseKHR;

		// EGL_KHR_stream_cross_process_fd
		#define eglCreateStreamFromFileDescriptorKHR(...) gl::egl::CreateStreamFromFileDescriptorKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLStreamKHR CreateStreamFromFileDescriptorKHR (EGLDisplay dpy, EGLNativeFileDescriptorKHR file_descriptor, const char* file, int line);
		static PFNEGLCREATESTREAMFROMFILEDESCRIPTORKHRPROC egl_CreateStreamFromFileDescriptorKHR;

		#define eglGetStreamFileDescriptorKHR(...) gl::egl::GetStreamFileDescriptorKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLNativeFileDescriptorKHR GetStreamFileDescriptorKHR (EGLDisplay dpy, EGLStreamKHR stream, const char* file, int line);
		static PFNEGLGETSTREAMFILEDESCRIPTORKHRPROC egl_GetStreamFileDescriptorKHR;

		// EGL_KHR_stream_fifo
		#define eglQueryStreamTimeKHR(...) gl::egl::QueryStreamTimeKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean QueryStreamTimeKHR (EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLTimeKHR *value, const char* file, int line);
		static PFNEGLQUERYSTREAMTIMEKHRPROC egl_QueryStreamTimeKHR;

		// EGL_KHR_stream_producer_eglsurface
		#define eglCreateStreamProducerSurfaceKHR(...) gl::egl::CreateStreamProducerSurfaceKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLSurface CreateStreamProducerSurfaceKHR (EGLDisplay dpy, EGLConfig config, EGLStreamKHR stream, const EGLint *attrib_list, const char* file, int line);
		static PFNEGLCREATESTREAMPRODUCERSURFACEKHRPROC egl_CreateStreamProducerSurfaceKHR;

		// EGL_KHR_swap_buffers_with_damage
		#define eglSwapBuffersWithDamageKHR(...) gl::egl::SwapBuffersWithDamageKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean SwapBuffersWithDamageKHR (EGLDisplay dpy, EGLSurface surface, EGLint *rects, EGLint n_rects, const char* file, int line);
		static PFNEGLSWAPBUFFERSWITHDAMAGEKHRPROC egl_SwapBuffersWithDamageKHR;

		// EGL_KHR_wait_sync
		#define eglWaitSyncKHR(...) gl::egl::WaitSyncKHR( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLint WaitSyncKHR (EGLDisplay dpy, EGLSyncKHR sync, EGLint flags, const char* file, int line);
		static PFNEGLWAITSYNCKHRPROC egl_WaitSyncKHR;

		// EGL_MESA_drm_image
		#define eglCreateDRMImageMESA(...) gl::egl::CreateDRMImageMESA( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLImageKHR CreateDRMImageMESA (EGLDisplay dpy, const EGLint *attrib_list, const char* file, int line);
		static PFNEGLCREATEDRMIMAGEMESAPROC egl_CreateDRMImageMESA;

		#define eglExportDRMImageMESA(...) gl::egl::ExportDRMImageMESA( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean ExportDRMImageMESA (EGLDisplay dpy, EGLImageKHR image, EGLint *name, EGLint *handle, EGLint *stride, const char* file, int line);
		static PFNEGLEXPORTDRMIMAGEMESAPROC egl_ExportDRMImageMESA;

		// EGL_MESA_image_dma_buf_export
		#define eglExportDMABUFImageMESA(...) gl::egl::ExportDMABUFImageMESA( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean ExportDMABUFImageMESA (EGLDisplay dpy, EGLImageKHR image, int *fds, EGLint *strides, EGLint *offsets, const char* file, int line);
		static PFNEGLEXPORTDMABUFIMAGEMESAPROC egl_ExportDMABUFImageMESA;

		#define eglExportDMABUFImageQueryMESA(...) gl::egl::ExportDMABUFImageQueryMESA( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean ExportDMABUFImageQueryMESA (EGLDisplay dpy, EGLImageKHR image, int *fourcc, int *num_planes, EGLuint64KHR *modifiers, const char* file, int line);
		static PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC egl_ExportDMABUFImageQueryMESA;

		// EGL_NOK_swap_region
		#define eglSwapBuffersRegionNOK(...) gl::egl::SwapBuffersRegionNOK( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean SwapBuffersRegionNOK (EGLDisplay dpy, EGLSurface surface, EGLint numRects, const EGLint *rects, const char* file, int line);
		static PFNEGLSWAPBUFFERSREGIONNOKPROC egl_SwapBuffersRegionNOK;

		// EGL_NOK_swap_region2
		#define eglSwapBuffersRegion2NOK(...) gl::egl::SwapBuffersRegion2NOK( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean SwapBuffersRegion2NOK (EGLDisplay dpy, EGLSurface surface, EGLint numRects, const EGLint *rects, const char* file, int line);
		static PFNEGLSWAPBUFFERSREGION2NOKPROC egl_SwapBuffersRegion2NOK;

		// EGL_NV_native_query
		#define eglQueryNativeDisplayNV(...) gl::egl::QueryNativeDisplayNV( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean QueryNativeDisplayNV (EGLDisplay dpy, EGLNativeDisplayType *display_id, const char* file, int line);
		static PFNEGLQUERYNATIVEDISPLAYNVPROC egl_QueryNativeDisplayNV;

		#define eglQueryNativePixmapNV(...) gl::egl::QueryNativePixmapNV( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean QueryNativePixmapNV (EGLDisplay dpy, EGLSurface surf, EGLNativePixmapType *pixmap, const char* file, int line);
		static PFNEGLQUERYNATIVEPIXMAPNVPROC egl_QueryNativePixmapNV;

		#define eglQueryNativeWindowNV(...) gl::egl::QueryNativeWindowNV( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean QueryNativeWindowNV (EGLDisplay dpy, EGLSurface surf, EGLNativeWindowType *window, const char* file, int line);
		static PFNEGLQUERYNATIVEWINDOWNVPROC egl_QueryNativeWindowNV;

		// EGL_NV_post_sub_buffer
		#define eglPostSubBufferNV(...) gl::egl::PostSubBufferNV( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean PostSubBufferNV (EGLDisplay dpy, EGLSurface surface, EGLint x, EGLint y, EGLint width, EGLint height, const char* file, int line);
		static PFNEGLPOSTSUBBUFFERNVPROC egl_PostSubBufferNV;

		// EGL_NV_stream_consumer_gltexture_yuv
		#define eglStreamConsumerGLTextureExternalAttribsNV(...) gl::egl::StreamConsumerGLTextureExternalAttribsNV( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean StreamConsumerGLTextureExternalAttribsNV (EGLDisplay dpy, EGLStreamKHR stream, EGLAttrib *attrib_list, const char* file, int line);
		static PFNEGLSTREAMCONSUMERGLTEXTUREEXTERNALATTRIBSNVPROC egl_StreamConsumerGLTextureExternalAttribsNV;

		// EGL_NV_stream_metadata
		#define eglQueryDisplayAttribNV(...) gl::egl::QueryDisplayAttribNV( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean QueryDisplayAttribNV (EGLDisplay dpy, EGLint attribute, EGLAttrib *value, const char* file, int line);
		static PFNEGLQUERYDISPLAYATTRIBNVPROC egl_QueryDisplayAttribNV;

		#define eglQueryStreamMetadataNV(...) gl::egl::QueryStreamMetadataNV( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean QueryStreamMetadataNV (EGLDisplay dpy, EGLStreamKHR stream, EGLenum name, EGLint n, EGLint offset, EGLint size, void *data, const char* file, int line);
		static PFNEGLQUERYSTREAMMETADATANVPROC egl_QueryStreamMetadataNV;

		#define eglSetStreamMetadataNV(...) gl::egl::SetStreamMetadataNV( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean SetStreamMetadataNV (EGLDisplay dpy, EGLStreamKHR stream, EGLint n, EGLint offset, EGLint size, const void *data, const char* file, int line);
		static PFNEGLSETSTREAMMETADATANVPROC egl_SetStreamMetadataNV;

		// EGL_NV_stream_sync
		#define eglCreateStreamSyncNV(...) gl::egl::CreateStreamSyncNV( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLSyncKHR CreateStreamSyncNV (EGLDisplay dpy, EGLStreamKHR stream, EGLenum type, const EGLint *attrib_list, const char* file, int line);
		static PFNEGLCREATESTREAMSYNCNVPROC egl_CreateStreamSyncNV;

		// EGL_NV_sync
		#define eglClientWaitSyncNV(...) gl::egl::ClientWaitSyncNV( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLint ClientWaitSyncNV (EGLSyncNV sync, EGLint flags, EGLTimeNV timeout, const char* file, int line);
		static PFNEGLCLIENTWAITSYNCNVPROC egl_ClientWaitSyncNV;

		#define eglCreateFenceSyncNV(...) gl::egl::CreateFenceSyncNV( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLSyncNV CreateFenceSyncNV (EGLDisplay dpy, EGLenum condition, const EGLint *attrib_list, const char* file, int line);
		static PFNEGLCREATEFENCESYNCNVPROC egl_CreateFenceSyncNV;

		#define eglDestroySyncNV(...) gl::egl::DestroySyncNV( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean DestroySyncNV (EGLSyncNV sync, const char* file, int line);
		static PFNEGLDESTROYSYNCNVPROC egl_DestroySyncNV;

		#define eglFenceNV(...) gl::egl::FenceNV( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean FenceNV (EGLSyncNV sync, const char* file, int line);
		static PFNEGLFENCENVPROC egl_FenceNV;

		#define eglGetSyncAttribNV(...) gl::egl::GetSyncAttribNV( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean GetSyncAttribNV (EGLSyncNV sync, EGLint attribute, EGLint *value, const char* file, int line);
		static PFNEGLGETSYNCATTRIBNVPROC egl_GetSyncAttribNV;

		#define eglSignalSyncNV(...) gl::egl::SignalSyncNV( __VA_ARGS__, __FILE__,__LINE__ )
		static EGLBoolean SignalSyncNV (EGLSyncNV sync, EGLenum mode, const char* file, int line);
		static PFNEGLSIGNALSYNCNVPROC egl_SignalSyncNV;

		// EGL_NV_system_time
		#define eglGetSystemTimeFrequencyNV() gl::egl::GetSystemTimeFrequencyNV( __FILE__,__LINE__ )
		static EGLuint64NV GetSystemTimeFrequencyNV (const char* file, int line);
		static PFNEGLGETSYSTEMTIMEFREQUENCYNVPROC egl_GetSystemTimeFrequencyNV;

		#define eglGetSystemTimeNV() gl::egl::GetSystemTimeNV( __FILE__,__LINE__ )
		static EGLuint64NV GetSystemTimeNV (const char* file, int line);
		static PFNEGLGETSYSTEMTIMENVPROC egl_GetSystemTimeNV;
	}; // end of class egl

	// -----------------------------------------------------------------
	// gl2.h
	// -----------------------------------------------------------------

	// GL_ES_VERSION_2_0
	#define glActiveTexture(...) gl::ActiveTexture( __VA_ARGS__, __FILE__,__LINE__ )
	static void ActiveTexture (GLenum texture, const char* file, int line);
	static PFNGLACTIVETEXTUREPROC gl_ActiveTexture;

	#define glAttachShader(...) gl::AttachShader( __VA_ARGS__, __FILE__,__LINE__ )
	static void AttachShader (GLuint program, GLuint shader, const char* file, int line);
	static PFNGLATTACHSHADERPROC gl_AttachShader;

	#define glBindAttribLocation(...) gl::BindAttribLocation( __VA_ARGS__, __FILE__,__LINE__ )
	static void BindAttribLocation (GLuint program, GLuint index, const GLchar *name, const char* file, int line);
	static PFNGLBINDATTRIBLOCATIONPROC gl_BindAttribLocation;

	#define glBindBuffer(...) gl::BindBuffer( __VA_ARGS__, __FILE__,__LINE__ )
	static void BindBuffer (GLenum target, GLuint buffer, const char* file, int line);
	static PFNGLBINDBUFFERPROC gl_BindBuffer;

	#define glBindFramebuffer(...) gl::BindFramebuffer( __VA_ARGS__, __FILE__,__LINE__ )
	static void BindFramebuffer (GLenum target, GLuint framebuffer, const char* file, int line);
	static PFNGLBINDFRAMEBUFFERPROC gl_BindFramebuffer;

	#define glBindRenderbuffer(...) gl::BindRenderbuffer( __VA_ARGS__, __FILE__,__LINE__ )
	static void BindRenderbuffer (GLenum target, GLuint renderbuffer, const char* file, int line);
	static PFNGLBINDRENDERBUFFERPROC gl_BindRenderbuffer;

	#define glBindTexture(...) gl::BindTexture( __VA_ARGS__, __FILE__,__LINE__ )
	static void BindTexture (GLenum target, GLuint texture, const char* file, int line);
	static PFNGLBINDTEXTUREPROC gl_BindTexture;

	#define glBlendColor(...) gl::BlendColor( __VA_ARGS__, __FILE__,__LINE__ )
	static void BlendColor (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha, const char* file, int line);
	static PFNGLBLENDCOLORPROC gl_BlendColor;

	#define glBlendEquation(...) gl::BlendEquation( __VA_ARGS__, __FILE__,__LINE__ )
	static void BlendEquation (GLenum mode, const char* file, int line);
	static PFNGLBLENDEQUATIONPROC gl_BlendEquation;

	#define glBlendEquationSeparate(...) gl::BlendEquationSeparate( __VA_ARGS__, __FILE__,__LINE__ )
	static void BlendEquationSeparate (GLenum modeRGB, GLenum modeAlpha, const char* file, int line);
	static PFNGLBLENDEQUATIONSEPARATEPROC gl_BlendEquationSeparate;

	#define glBlendFunc(...) gl::BlendFunc( __VA_ARGS__, __FILE__,__LINE__ )
	static void BlendFunc (GLenum sfactor, GLenum dfactor, const char* file, int line);
	static PFNGLBLENDFUNCPROC gl_BlendFunc;

	#define glBlendFuncSeparate(...) gl::BlendFuncSeparate( __VA_ARGS__, __FILE__,__LINE__ )
	static void BlendFuncSeparate (GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha, const char* file, int line);
	static PFNGLBLENDFUNCSEPARATEPROC gl_BlendFuncSeparate;

	#define glBufferData(...) gl::BufferData( __VA_ARGS__, __FILE__,__LINE__ )
	static void BufferData (GLenum target, GLsizeiptr size, const void *data, GLenum usage, const char* file, int line);
	static PFNGLBUFFERDATAPROC gl_BufferData;

	#define glBufferSubData(...) gl::BufferSubData( __VA_ARGS__, __FILE__,__LINE__ )
	static void BufferSubData (GLenum target, GLintptr offset, GLsizeiptr size, const void *data, const char* file, int line);
	static PFNGLBUFFERSUBDATAPROC gl_BufferSubData;

	#define glCheckFramebufferStatus(...) gl::CheckFramebufferStatus( __VA_ARGS__, __FILE__,__LINE__ )
	static GLenum CheckFramebufferStatus (GLenum target, const char* file, int line);
	static PFNGLCHECKFRAMEBUFFERSTATUSPROC gl_CheckFramebufferStatus;

	#define glClear(...) gl::Clear( __VA_ARGS__, __FILE__,__LINE__ )
	static void Clear (GLbitfield mask, const char* file, int line);
	static PFNGLCLEARPROC gl_Clear;

	#define glClearColor(...) gl::ClearColor( __VA_ARGS__, __FILE__,__LINE__ )
	static void ClearColor (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha, const char* file, int line);
	static PFNGLCLEARCOLORPROC gl_ClearColor;

	#define glClearDepthf(...) gl::ClearDepthf( __VA_ARGS__, __FILE__,__LINE__ )
	static void ClearDepthf (GLfloat d, const char* file, int line);
	static PFNGLCLEARDEPTHFPROC gl_ClearDepthf;

	#define glClearStencil(...) gl::ClearStencil( __VA_ARGS__, __FILE__,__LINE__ )
	static void ClearStencil (GLint s, const char* file, int line);
	static PFNGLCLEARSTENCILPROC gl_ClearStencil;

	#define glColorMask(...) gl::ColorMask( __VA_ARGS__, __FILE__,__LINE__ )
	static void ColorMask (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha, const char* file, int line);
	static PFNGLCOLORMASKPROC gl_ColorMask;

	#define glCompileShader(...) gl::CompileShader( __VA_ARGS__, __FILE__,__LINE__ )
	static void CompileShader (GLuint shader, const char* file, int line);
	static PFNGLCOMPILESHADERPROC gl_CompileShader;

	#define glCompressedTexImage2D(...) gl::CompressedTexImage2D( __VA_ARGS__, __FILE__,__LINE__ )
	static void CompressedTexImage2D (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data, const char* file, int line);
	static PFNGLCOMPRESSEDTEXIMAGE2DPROC gl_CompressedTexImage2D;

	#define glCompressedTexSubImage2D(...) gl::CompressedTexSubImage2D( __VA_ARGS__, __FILE__,__LINE__ )
	static void CompressedTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const void *data, const char* file, int line);
	static PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC gl_CompressedTexSubImage2D;

	#define glCopyTexImage2D(...) gl::CopyTexImage2D( __VA_ARGS__, __FILE__,__LINE__ )
	static void CopyTexImage2D (GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border, const char* file, int line);
	static PFNGLCOPYTEXIMAGE2DPROC gl_CopyTexImage2D;

	#define glCopyTexSubImage2D(...) gl::CopyTexSubImage2D( __VA_ARGS__, __FILE__,__LINE__ )
	static void CopyTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height, const char* file, int line);
	static PFNGLCOPYTEXSUBIMAGE2DPROC gl_CopyTexSubImage2D;

	#define glCreateProgram() gl::CreateProgram( __FILE__,__LINE__ )
	static GLuint CreateProgram (const char* file, int line);
	static PFNGLCREATEPROGRAMPROC gl_CreateProgram;

	#define glCreateShader(...) gl::CreateShader( __VA_ARGS__, __FILE__,__LINE__ )
	static GLuint CreateShader (GLenum type, const char* file, int line);
	static PFNGLCREATESHADERPROC gl_CreateShader;

	#define glCullFace(...) gl::CullFace( __VA_ARGS__, __FILE__,__LINE__ )
	static void CullFace (GLenum mode, const char* file, int line);
	static PFNGLCULLFACEPROC gl_CullFace;

	#define glDeleteBuffers(...) gl::DeleteBuffers( __VA_ARGS__, __FILE__,__LINE__ )
	static void DeleteBuffers (GLsizei n, const GLuint *buffers, const char* file, int line);
	static PFNGLDELETEBUFFERSPROC gl_DeleteBuffers;

	#define glDeleteFramebuffers(...) gl::DeleteFramebuffers( __VA_ARGS__, __FILE__,__LINE__ )
	static void DeleteFramebuffers (GLsizei n, const GLuint *framebuffers, const char* file, int line);
	static PFNGLDELETEFRAMEBUFFERSPROC gl_DeleteFramebuffers;

	#define glDeleteProgram(...) gl::DeleteProgram( __VA_ARGS__, __FILE__,__LINE__ )
	static void DeleteProgram (GLuint program, const char* file, int line);
	static PFNGLDELETEPROGRAMPROC gl_DeleteProgram;

	#define glDeleteRenderbuffers(...) gl::DeleteRenderbuffers( __VA_ARGS__, __FILE__,__LINE__ )
	static void DeleteRenderbuffers (GLsizei n, const GLuint *renderbuffers, const char* file, int line);
	static PFNGLDELETERENDERBUFFERSPROC gl_DeleteRenderbuffers;

	#define glDeleteShader(...) gl::DeleteShader( __VA_ARGS__, __FILE__,__LINE__ )
	static void DeleteShader (GLuint shader, const char* file, int line);
	static PFNGLDELETESHADERPROC gl_DeleteShader;

	#define glDeleteTextures(...) gl::DeleteTextures( __VA_ARGS__, __FILE__,__LINE__ )
	static void DeleteTextures (GLsizei n, const GLuint *textures, const char* file, int line);
	static PFNGLDELETETEXTURESPROC gl_DeleteTextures;

	#define glDepthFunc(...) gl::DepthFunc( __VA_ARGS__, __FILE__,__LINE__ )
	static void DepthFunc (GLenum func, const char* file, int line);
	static PFNGLDEPTHFUNCPROC gl_DepthFunc;

	#define glDepthMask(...) gl::DepthMask( __VA_ARGS__, __FILE__,__LINE__ )
	static void DepthMask (GLboolean flag, const char* file, int line);
	static PFNGLDEPTHMASKPROC gl_DepthMask;

	#define glDepthRangef(...) gl::DepthRangef( __VA_ARGS__, __FILE__,__LINE__ )
	static void DepthRangef (GLfloat n, GLfloat f, const char* file, int line);
	static PFNGLDEPTHRANGEFPROC gl_DepthRangef;

	#define glDetachShader(...) gl::DetachShader( __VA_ARGS__, __FILE__,__LINE__ )
	static void DetachShader (GLuint program, GLuint shader, const char* file, int line);
	static PFNGLDETACHSHADERPROC gl_DetachShader;

	#define glDisable(...) gl::Disable( __VA_ARGS__, __FILE__,__LINE__ )
	static void Disable (GLenum cap, const char* file, int line);
	static PFNGLDISABLEPROC gl_Disable;

	#define glDisableVertexAttribArray(...) gl::DisableVertexAttribArray( __VA_ARGS__, __FILE__,__LINE__ )
	static void DisableVertexAttribArray (GLuint index, const char* file, int line);
	static PFNGLDISABLEVERTEXATTRIBARRAYPROC gl_DisableVertexAttribArray;

	#define glDrawArrays(...) gl::DrawArrays( __VA_ARGS__, __FILE__,__LINE__ )
	static void DrawArrays (GLenum mode, GLint first, GLsizei count, const char* file, int line);
	static PFNGLDRAWARRAYSPROC gl_DrawArrays;

	#define glDrawElements(...) gl::DrawElements( __VA_ARGS__, __FILE__,__LINE__ )
	static void DrawElements (GLenum mode, GLsizei count, GLenum type, const void *indices, const char* file, int line);
	static PFNGLDRAWELEMENTSPROC gl_DrawElements;

	#define glEnable(...) gl::Enable( __VA_ARGS__, __FILE__,__LINE__ )
	static void Enable (GLenum cap, const char* file, int line);
	static PFNGLENABLEPROC gl_Enable;

	#define glEnableVertexAttribArray(...) gl::EnableVertexAttribArray( __VA_ARGS__, __FILE__,__LINE__ )
	static void EnableVertexAttribArray (GLuint index, const char* file, int line);
	static PFNGLENABLEVERTEXATTRIBARRAYPROC gl_EnableVertexAttribArray;

	#define glFinish()	gl::Finish( __FILE__,__LINE__ )
	static void Finish (const char* file, int line);
	static PFNGLFINISHPROC gl_Finish;

	#define glFlush()	gl::Flush( __FILE__,__LINE__ )
	static void Flush (const char* file, int line);
	static PFNGLFLUSHPROC gl_Flush;

	#define glFramebufferRenderbuffer(...) gl::FramebufferRenderbuffer( __VA_ARGS__, __FILE__,__LINE__ )
	static void FramebufferRenderbuffer (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer, const char* file, int line);
	static PFNGLFRAMEBUFFERRENDERBUFFERPROC gl_FramebufferRenderbuffer;

	#define glFramebufferTexture2D(...) gl::FramebufferTexture2D( __VA_ARGS__, __FILE__,__LINE__ )
	static void FramebufferTexture2D (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, const char* file, int line);
	static PFNGLFRAMEBUFFERTEXTURE2DPROC gl_FramebufferTexture2D;

	#define glFrontFace(...) gl::FrontFace( __VA_ARGS__, __FILE__,__LINE__ )
	static void FrontFace (GLenum mode, const char* file, int line);
	static PFNGLFRONTFACEPROC gl_FrontFace;

	#define glGenBuffers(...) gl::GenBuffers( __VA_ARGS__, __FILE__,__LINE__ )
	static void GenBuffers (GLsizei n, GLuint *buffers, const char* file, int line);
	static PFNGLGENBUFFERSPROC gl_GenBuffers;

	#define glGenFramebuffers(...) gl::GenFramebuffers( __VA_ARGS__, __FILE__,__LINE__ )
	static void GenFramebuffers (GLsizei n, GLuint *framebuffers, const char* file, int line);
	static PFNGLGENFRAMEBUFFERSPROC gl_GenFramebuffers;

	#define glGenRenderbuffers(...) gl::GenRenderbuffers( __VA_ARGS__, __FILE__,__LINE__ )
	static void GenRenderbuffers (GLsizei n, GLuint *renderbuffers, const char* file, int line);
	static PFNGLGENRENDERBUFFERSPROC gl_GenRenderbuffers;

	#define glGenTextures(...) gl::GenTextures( __VA_ARGS__, __FILE__,__LINE__ )
	static void GenTextures (GLsizei n, GLuint *textures, const char* file, int line);
	static PFNGLGENTEXTURESPROC gl_GenTextures;

	#define glGenerateMipmap(...) gl::GenerateMipmap( __VA_ARGS__, __FILE__,__LINE__ )
	static void GenerateMipmap (GLenum target, const char* file, int line);
	static PFNGLGENERATEMIPMAPPROC gl_GenerateMipmap;

	#define glGetActiveAttrib(...) gl::GetActiveAttrib( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetActiveAttrib (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name, const char* file, int line);
	static PFNGLGETACTIVEATTRIBPROC gl_GetActiveAttrib;

	#define glGetActiveUniform(...) gl::GetActiveUniform( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetActiveUniform (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name, const char* file, int line);
	static PFNGLGETACTIVEUNIFORMPROC gl_GetActiveUniform;

	#define glGetAttachedShaders(...) gl::GetAttachedShaders( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetAttachedShaders (GLuint program, GLsizei maxCount, GLsizei *count, GLuint *shaders, const char* file, int line);
	static PFNGLGETATTACHEDSHADERSPROC gl_GetAttachedShaders;

	#define glGetAttribLocation(...) gl::GetAttribLocation( __VA_ARGS__, __FILE__,__LINE__ )
	static GLint GetAttribLocation (GLuint program, const GLchar *name, const char* file, int line);
	static PFNGLGETATTRIBLOCATIONPROC gl_GetAttribLocation;

	#define glGetBooleanv(...) gl::GetBooleanv( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetBooleanv (GLenum pname, GLboolean *data, const char* file, int line);
	static PFNGLGETBOOLEANVPROC gl_GetBooleanv;

	#define glGetBufferParameteriv(...) gl::GetBufferParameteriv( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetBufferParameteriv (GLenum target, GLenum pname, GLint *params, const char* file, int line);
	static PFNGLGETBUFFERPARAMETERIVPROC gl_GetBufferParameteriv;

	#define glGetError() gl::GetError(__FILE__,__LINE__)
	static GLenum GetError (const char* file, int line);
	static PFNGLGETERRORPROC gl_GetError;

	#define glGetFloatv(...) gl::GetFloatv( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetFloatv (GLenum pname, GLfloat *data, const char* file, int line);
	static PFNGLGETFLOATVPROC gl_GetFloatv;

	#define glGetFramebufferAttachmentParameteriv(...) gl::GetFramebufferAttachmentParameteriv( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetFramebufferAttachmentParameteriv (GLenum target, GLenum attachment, GLenum pname, GLint *params, const char* file, int line);
	static PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC gl_GetFramebufferAttachmentParameteriv;

	#define glGetIntegerv(...) gl::GetIntegerv( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetIntegerv (GLenum pname, GLint *data, const char* file, int line);
	static PFNGLGETINTEGERVPROC gl_GetIntegerv;

	#define glGetProgramInfoLog(...) gl::GetProgramInfoLog( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetProgramInfoLog (GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog, const char* file, int line);
	static PFNGLGETPROGRAMINFOLOGPROC gl_GetProgramInfoLog;

	#define glGetProgramiv(...) gl::GetProgramiv( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetProgramiv (GLuint program, GLenum pname, GLint *params, const char* file, int line);
	static PFNGLGETPROGRAMIVPROC gl_GetProgramiv;

	#define glGetRenderbufferParameteriv(...) gl::GetRenderbufferParameteriv( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetRenderbufferParameteriv (GLenum target, GLenum pname, GLint *params, const char* file, int line);
	static PFNGLGETRENDERBUFFERPARAMETERIVPROC gl_GetRenderbufferParameteriv;

	#define glGetShaderInfoLog(...) gl::GetShaderInfoLog( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetShaderInfoLog (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog, const char* file, int line);
	static PFNGLGETSHADERINFOLOGPROC gl_GetShaderInfoLog;

	#define glGetShaderPrecisionFormat(...) gl::GetShaderPrecisionFormat( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetShaderPrecisionFormat (GLenum shadertype, GLenum precisiontype, GLint *range, GLint *precision, const char* file, int line);
	static PFNGLGETSHADERPRECISIONFORMATPROC gl_GetShaderPrecisionFormat;

	#define glGetShaderSource(...) gl::GetShaderSource( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetShaderSource (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *source, const char* file, int line);
	static PFNGLGETSHADERSOURCEPROC gl_GetShaderSource;

	#define glGetShaderiv(...) gl::GetShaderiv( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetShaderiv (GLuint shader, GLenum pname, GLint *params, const char* file, int line);
	static PFNGLGETSHADERIVPROC gl_GetShaderiv;

	#define glGetString(...) gl::GetString( __VA_ARGS__, __FILE__,__LINE__ )
	static const GLubyte* GetString(GLenum name, const char* file, int line);
	static PFNGLGETSTRINGPROC gl_GetString;

	#define glGetTexParameterfv(...) gl::GetTexParameterfv( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetTexParameterfv (GLenum target, GLenum pname, GLfloat *params, const char* file, int line);
	static PFNGLGETTEXPARAMETERFVPROC gl_GetTexParameterfv;

	#define glGetTexParameteriv(...) gl::GetTexParameteriv( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetTexParameteriv (GLenum target, GLenum pname, GLint *params, const char* file, int line);
	static PFNGLGETTEXPARAMETERIVPROC gl_GetTexParameteriv;

	#define glGetUniformLocation(...) gl::GetUniformLocation( __VA_ARGS__, __FILE__,__LINE__ )
	static GLint GetUniformLocation (GLuint program, const GLchar *name, const char* file, int line);
	static PFNGLGETUNIFORMLOCATIONPROC gl_GetUniformLocation;

	#define glGetUniformfv(...) gl::GetUniformfv( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetUniformfv (GLuint program, GLint location, GLfloat *params, const char* file, int line);
	static PFNGLGETUNIFORMFVPROC gl_GetUniformfv;

	#define glGetUniformiv(...) gl::GetUniformiv( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetUniformiv (GLuint program, GLint location, GLint *params, const char* file, int line);
	static PFNGLGETUNIFORMIVPROC gl_GetUniformiv;

	#define glGetVertexAttribPointerv(...) gl::GetVertexAttribPointerv( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetVertexAttribPointerv (GLuint index, GLenum pname, void **pointer, const char* file, int line);
	static PFNGLGETVERTEXATTRIBPOINTERVPROC gl_GetVertexAttribPointerv;

	#define glGetVertexAttribfv(...) gl::GetVertexAttribfv( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetVertexAttribfv (GLuint index, GLenum pname, GLfloat *params, const char* file, int line);
	static PFNGLGETVERTEXATTRIBFVPROC gl_GetVertexAttribfv;

	#define glGetVertexAttribiv(...) gl::GetVertexAttribiv( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetVertexAttribiv (GLuint index, GLenum pname, GLint *params, const char* file, int line);
	static PFNGLGETVERTEXATTRIBIVPROC gl_GetVertexAttribiv;

	#define glHint(...) gl::Hint( __VA_ARGS__, __FILE__,__LINE__ )
	static void Hint (GLenum target, GLenum mode, const char* file, int line);
	static PFNGLHINTPROC gl_Hint;

	#define glIsBuffer(...) gl::IsBuffer( __VA_ARGS__, __FILE__,__LINE__ )
	static GLboolean IsBuffer (GLuint buffer, const char* file, int line);
	static PFNGLISBUFFERPROC gl_IsBuffer;

	#define glIsEnabled(...) gl::IsEnabled( __VA_ARGS__, __FILE__,__LINE__ )
	static GLboolean IsEnabled (GLenum cap, const char* file, int line);
	static PFNGLISENABLEDPROC gl_IsEnabled;

	#define glIsFramebuffer(...) gl::IsFramebuffer( __VA_ARGS__, __FILE__,__LINE__ )
	static GLboolean IsFramebuffer (GLuint framebuffer, const char* file, int line);
	static PFNGLISFRAMEBUFFERPROC gl_IsFramebuffer;

	#define glIsProgram(...) gl::IsProgram( __VA_ARGS__, __FILE__,__LINE__ )
	static GLboolean IsProgram (GLuint program, const char* file, int line);
	static PFNGLISPROGRAMPROC gl_IsProgram;

	#define glIsRenderbuffer(...) gl::IsRenderbuffer( __VA_ARGS__, __FILE__,__LINE__ )
	static GLboolean IsRenderbuffer (GLuint renderbuffer, const char* file, int line);
	static PFNGLISRENDERBUFFERPROC gl_IsRenderbuffer;

	#define glIsShader(...) gl::IsShader( __VA_ARGS__, __FILE__,__LINE__ )
	static GLboolean IsShader (GLuint shader, const char* file, int line);
	static PFNGLISSHADERPROC gl_IsShader;

	#define glIsTexture(...) gl::IsTexture( __VA_ARGS__, __FILE__,__LINE__ )
	static GLboolean IsTexture (GLuint texture, const char* file, int line);
	static PFNGLISTEXTUREPROC gl_IsTexture;

	#define glLineWidth(...) gl::LineWidth( __VA_ARGS__, __FILE__,__LINE__ )
	static void LineWidth (GLfloat width, const char* file, int line);
	static PFNGLLINEWIDTHPROC gl_LineWidth;

	#define glLinkProgram(...) gl::LinkProgram( __VA_ARGS__, __FILE__,__LINE__ )
	static void LinkProgram (GLuint program, const char* file, int line);
	static PFNGLLINKPROGRAMPROC gl_LinkProgram;

	#define glPixelStorei(...) gl::PixelStorei( __VA_ARGS__, __FILE__,__LINE__ )
	static void PixelStorei (GLenum pname, GLint param, const char* file, int line);
	static PFNGLPIXELSTOREIPROC gl_PixelStorei;

	#define glPolygonOffset(...) gl::PolygonOffset( __VA_ARGS__, __FILE__,__LINE__ )
	static void PolygonOffset (GLfloat factor, GLfloat units, const char* file, int line);
	static PFNGLPOLYGONOFFSETPROC gl_PolygonOffset;

	#define glReadPixels(...) gl::ReadPixels( __VA_ARGS__, __FILE__,__LINE__ )
	static void ReadPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels, const char* file, int line);
	static PFNGLREADPIXELSPROC gl_ReadPixels;

	#define glReleaseShaderCompiler() gl::ReleaseShaderCompiler( __FILE__,__LINE__ )
	static void ReleaseShaderCompiler (const char* file, int line);
	static PFNGLRELEASESHADERCOMPILERPROC gl_ReleaseShaderCompiler;

	#define glRenderbufferStorage(...) gl::RenderbufferStorage( __VA_ARGS__, __FILE__,__LINE__ )
	static void RenderbufferStorage (GLenum target, GLenum internalformat, GLsizei width, GLsizei height, const char* file, int line);
	static PFNGLRENDERBUFFERSTORAGEPROC gl_RenderbufferStorage;

	#define glSampleCoverage(...) gl::SampleCoverage( __VA_ARGS__, __FILE__,__LINE__ )
	static void SampleCoverage (GLfloat value, GLboolean invert, const char* file, int line);
	static PFNGLSAMPLECOVERAGEPROC gl_SampleCoverage;

	#define glScissor(...) gl::Scissor( __VA_ARGS__, __FILE__,__LINE__ )
	static void Scissor (GLint x, GLint y, GLsizei width, GLsizei height, const char* file, int line);
	static PFNGLSCISSORPROC gl_Scissor;

	#define glShaderBinary(...) gl::ShaderBinary( __VA_ARGS__, __FILE__,__LINE__ )
	static void ShaderBinary (GLsizei count, const GLuint *shaders, GLenum binaryformat, const void *binary, GLsizei length, const char* file, int line);
	static PFNGLSHADERBINARYPROC gl_ShaderBinary;

	#define glShaderSource(...) gl::ShaderSource( __VA_ARGS__, __FILE__,__LINE__ )
	static void ShaderSource (GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length, const char* file, int line);
	static PFNGLSHADERSOURCEPROC gl_ShaderSource;

	#define glStencilFunc(...) gl::StencilFunc( __VA_ARGS__, __FILE__,__LINE__ )
	static void StencilFunc (GLenum func, GLint ref, GLuint mask, const char* file, int line);
	static PFNGLSTENCILFUNCPROC gl_StencilFunc;

	#define glStencilFuncSeparate(...) gl::StencilFuncSeparate( __VA_ARGS__, __FILE__,__LINE__ )
	static void StencilFuncSeparate (GLenum face, GLenum func, GLint ref, GLuint mask, const char* file, int line);
	static PFNGLSTENCILFUNCSEPARATEPROC gl_StencilFuncSeparate;

	#define glStencilMask(...) gl::StencilMask( __VA_ARGS__, __FILE__,__LINE__ )
	static void StencilMask (GLuint mask, const char* file, int line);
	static PFNGLSTENCILMASKPROC gl_StencilMask;

	#define glStencilMaskSeparate(...) gl::StencilMaskSeparate( __VA_ARGS__, __FILE__,__LINE__ )
	static void StencilMaskSeparate (GLenum face, GLuint mask, const char* file, int line);
	static PFNGLSTENCILMASKSEPARATEPROC gl_StencilMaskSeparate;

	#define glStencilOp(...) gl::StencilOp( __VA_ARGS__, __FILE__,__LINE__ )
	static void StencilOp (GLenum fail, GLenum zfail, GLenum zpass, const char* file, int line);
	static PFNGLSTENCILOPPROC gl_StencilOp;

	#define glStencilOpSeparate(...) gl::StencilOpSeparate( __VA_ARGS__, __FILE__,__LINE__ )
	static void StencilOpSeparate (GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass, const char* file, int line);
	static PFNGLSTENCILOPSEPARATEPROC gl_StencilOpSeparate;

	#define glTexImage2D(...) gl::TexImage2D( __VA_ARGS__, __FILE__,__LINE__ )
	static void TexImage2D (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels, const char* file, int line);
	static PFNGLTEXIMAGE2DPROC gl_TexImage2D;

	#define glTexParameterf(...) gl::TexParameterf( __VA_ARGS__, __FILE__,__LINE__ )
	static void TexParameterf (GLenum target, GLenum pname, GLfloat param, const char* file, int line);
	static PFNGLTEXPARAMETERFPROC gl_TexParameterf;

	#define glTexParameterfv(...) gl::TexParameterfv( __VA_ARGS__, __FILE__,__LINE__ )
	static void TexParameterfv (GLenum target, GLenum pname, const GLfloat *params, const char* file, int line);
	static PFNGLTEXPARAMETERFVPROC gl_TexParameterfv;

	#define glTexParameteri(...) gl::TexParameteri( __VA_ARGS__, __FILE__,__LINE__ )
	static void TexParameteri (GLenum target, GLenum pname, GLint param, const char* file, int line);
	static PFNGLTEXPARAMETERIPROC gl_TexParameteri;

	#define glTexParameteriv(...) gl::TexParameteriv( __VA_ARGS__, __FILE__,__LINE__ )
	static void TexParameteriv (GLenum target, GLenum pname, const GLint *params, const char* file, int line);
	static PFNGLTEXPARAMETERIVPROC gl_TexParameteriv;

	#define glTexSubImage2D(...) gl::TexSubImage2D( __VA_ARGS__, __FILE__,__LINE__ )
	static void TexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels, const char* file, int line);
	static PFNGLTEXSUBIMAGE2DPROC gl_TexSubImage2D;

	#define glUniform1f(...) gl::Uniform1f( __VA_ARGS__, __FILE__,__LINE__ )
	static void Uniform1f (GLint location, GLfloat v0, const char* file, int line);
	static PFNGLUNIFORM1FPROC gl_Uniform1f;

	#define glUniform1fv(...) gl::Uniform1fv( __VA_ARGS__, __FILE__,__LINE__ )
	static void Uniform1fv (GLint location, GLsizei count, const GLfloat *value, const char* file, int line);
	static PFNGLUNIFORM1FVPROC gl_Uniform1fv;

	#define glUniform1i(...) gl::Uniform1i( __VA_ARGS__, __FILE__,__LINE__ )
	static void Uniform1i (GLint location, GLint v0, const char* file, int line);
	static PFNGLUNIFORM1IPROC gl_Uniform1i;

	#define glUniform1iv(...) gl::Uniform1iv( __VA_ARGS__, __FILE__,__LINE__ )
	static void Uniform1iv (GLint location, GLsizei count, const GLint *value, const char* file, int line);
	static PFNGLUNIFORM1IVPROC gl_Uniform1iv;

	#define glUniform2f(...) gl::Uniform2f( __VA_ARGS__, __FILE__,__LINE__ )
	static void Uniform2f (GLint location, GLfloat v0, GLfloat v1, const char* file, int line);
	static PFNGLUNIFORM2FPROC gl_Uniform2f;

	#define glUniform2fv(...) gl::Uniform2fv( __VA_ARGS__, __FILE__,__LINE__ )
	static void Uniform2fv (GLint location, GLsizei count, const GLfloat *value, const char* file, int line);
	static PFNGLUNIFORM2FVPROC gl_Uniform2fv;

	#define glUniform2i(...) gl::Uniform2i( __VA_ARGS__, __FILE__,__LINE__ )
	static void Uniform2i (GLint location, GLint v0, GLint v1, const char* file, int line);
	static PFNGLUNIFORM2IPROC gl_Uniform2i;

	#define glUniform2iv(...) gl::Uniform2iv( __VA_ARGS__, __FILE__,__LINE__ )
	static void Uniform2iv (GLint location, GLsizei count, const GLint *value, const char* file, int line);
	static PFNGLUNIFORM2IVPROC gl_Uniform2iv;

	#define glUniform3f(...) gl::Uniform3f( __VA_ARGS__, __FILE__,__LINE__ )
	static void Uniform3f (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, const char* file, int line);
	static PFNGLUNIFORM3FPROC gl_Uniform3f;

	#define glUniform3fv(...) gl::Uniform3fv( __VA_ARGS__, __FILE__,__LINE__ )
	static void Uniform3fv (GLint location, GLsizei count, const GLfloat *value, const char* file, int line);
	static PFNGLUNIFORM3FVPROC gl_Uniform3fv;

	#define glUniform3i(...) gl::Uniform3i( __VA_ARGS__, __FILE__,__LINE__ )
	static void Uniform3i (GLint location, GLint v0, GLint v1, GLint v2, const char* file, int line);
	static PFNGLUNIFORM3IPROC gl_Uniform3i;

	#define glUniform3iv(...) gl::Uniform3iv( __VA_ARGS__, __FILE__,__LINE__ )
	static void Uniform3iv (GLint location, GLsizei count, const GLint *value, const char* file, int line);
	static PFNGLUNIFORM3IVPROC gl_Uniform3iv;

	#define glUniform4f(...) gl::Uniform4f( __VA_ARGS__, __FILE__,__LINE__ )
	static void Uniform4f (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3, const char* file, int line);
	static PFNGLUNIFORM4FPROC gl_Uniform4f;

	#define glUniform4fv(...) gl::Uniform4fv( __VA_ARGS__, __FILE__,__LINE__ )
	static void Uniform4fv (GLint location, GLsizei count, const GLfloat *value, const char* file, int line);
	static PFNGLUNIFORM4FVPROC gl_Uniform4fv;

	#define glUniform4i(...) gl::Uniform4i( __VA_ARGS__, __FILE__,__LINE__ )
	static void Uniform4i (GLint location, GLint v0, GLint v1, GLint v2, GLint v3, const char* file, int line);
	static PFNGLUNIFORM4IPROC gl_Uniform4i;

	#define glUniform4iv(...) gl::Uniform4iv( __VA_ARGS__, __FILE__,__LINE__ )
	static void Uniform4iv (GLint location, GLsizei count, const GLint *value, const char* file, int line);
	static PFNGLUNIFORM4IVPROC gl_Uniform4iv;

	#define glUniformMatrix2fv(...) gl::UniformMatrix2fv( __VA_ARGS__, __FILE__,__LINE__ )
	static void UniformMatrix2fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line);
	static PFNGLUNIFORMMATRIX2FVPROC gl_UniformMatrix2fv;

	#define glUniformMatrix3fv(...) gl::UniformMatrix3fv( __VA_ARGS__, __FILE__,__LINE__ )
	static void UniformMatrix3fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line);
	static PFNGLUNIFORMMATRIX3FVPROC gl_UniformMatrix3fv;

	#define glUniformMatrix4fv(...) gl::UniformMatrix4fv( __VA_ARGS__, __FILE__,__LINE__ )
	static void UniformMatrix4fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line);
	static PFNGLUNIFORMMATRIX4FVPROC gl_UniformMatrix4fv;

	#define glUseProgram(...) gl::UseProgram( __VA_ARGS__, __FILE__,__LINE__ )
	static void UseProgram (GLuint program, const char* file, int line);
	static PFNGLUSEPROGRAMPROC gl_UseProgram;

	#define glValidateProgram(...) gl::ValidateProgram( __VA_ARGS__, __FILE__,__LINE__ )
	static void ValidateProgram (GLuint program, const char* file, int line);
	static PFNGLVALIDATEPROGRAMPROC gl_ValidateProgram;

	#define glVertexAttrib1f(...) gl::VertexAttrib1f( __VA_ARGS__, __FILE__,__LINE__ )
	static void VertexAttrib1f (GLuint index, GLfloat x, const char* file, int line);
	static PFNGLVERTEXATTRIB1FPROC gl_VertexAttrib1f;

	#define glVertexAttrib1fv(...) gl::VertexAttrib1fv( __VA_ARGS__, __FILE__,__LINE__ )
	static void VertexAttrib1fv (GLuint index, const GLfloat *v, const char* file, int line);
	static PFNGLVERTEXATTRIB1FVPROC gl_VertexAttrib1fv;

	#define glVertexAttrib2f(...) gl::VertexAttrib2f( __VA_ARGS__, __FILE__,__LINE__ )
	static void VertexAttrib2f (GLuint index, GLfloat x, GLfloat y, const char* file, int line);
	static PFNGLVERTEXATTRIB2FPROC gl_VertexAttrib2f;

	#define glVertexAttrib2fv(...) gl::VertexAttrib2fv( __VA_ARGS__, __FILE__,__LINE__ )
	static void VertexAttrib2fv (GLuint index, const GLfloat *v, const char* file, int line);
	static PFNGLVERTEXATTRIB2FVPROC gl_VertexAttrib2fv;

	#define glVertexAttrib3f(...) gl::VertexAttrib3f( __VA_ARGS__, __FILE__,__LINE__ )
	static void VertexAttrib3f (GLuint index, GLfloat x, GLfloat y, GLfloat z, const char* file, int line);
	static PFNGLVERTEXATTRIB3FPROC gl_VertexAttrib3f;

	#define glVertexAttrib3fv(...) gl::VertexAttrib3fv( __VA_ARGS__, __FILE__,__LINE__ )
	static void VertexAttrib3fv (GLuint index, const GLfloat *v, const char* file, int line);
	static PFNGLVERTEXATTRIB3FVPROC gl_VertexAttrib3fv;

	#define glVertexAttrib4f(...) gl::VertexAttrib4f( __VA_ARGS__, __FILE__,__LINE__ )
	static void VertexAttrib4f (GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w, const char* file, int line);
	static PFNGLVERTEXATTRIB4FPROC gl_VertexAttrib4f;

	#define glVertexAttrib4fv(...) gl::VertexAttrib4fv( __VA_ARGS__, __FILE__,__LINE__ )
	static void VertexAttrib4fv (GLuint index, const GLfloat *v, const char* file, int line);
	static PFNGLVERTEXATTRIB4FVPROC gl_VertexAttrib4fv;

	#define glVertexAttribPointer(...) gl::VertexAttribPointer( __VA_ARGS__, __FILE__,__LINE__ )
	static void VertexAttribPointer (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer, const char* file, int line);
	static PFNGLVERTEXATTRIBPOINTERPROC gl_VertexAttribPointer;

	#define glViewport(...) gl::Viewport( __VA_ARGS__, __FILE__,__LINE__ )
	static void Viewport (GLint x, GLint y, GLsizei width, GLsizei height, const char* file, int line);
	static PFNGLVIEWPORTPROC gl_Viewport;

	// -----------------------------------------------------------------
	// gl2ext.h
	// -----------------------------------------------------------------

	// GL_AMD_performance_monitor
	#define glBeginPerfMonitorAMD(...) gl::BeginPerfMonitorAMD( __VA_ARGS__, __FILE__,__LINE__ )
	static void BeginPerfMonitorAMD (GLuint monitor, const char* file, int line);
	static PFNGLBEGINPERFMONITORAMDPROC gl_BeginPerfMonitorAMD;

	#define glDeletePerfMonitorsAMD(...) gl::DeletePerfMonitorsAMD( __VA_ARGS__, __FILE__,__LINE__ )
	static void DeletePerfMonitorsAMD (GLsizei n, GLuint *monitors, const char* file, int line);
	static PFNGLDELETEPERFMONITORSAMDPROC gl_DeletePerfMonitorsAMD;

	#define glEndPerfMonitorAMD(...) gl::EndPerfMonitorAMD( __VA_ARGS__, __FILE__,__LINE__ )
	static void EndPerfMonitorAMD (GLuint monitor, const char* file, int line);
	static PFNGLENDPERFMONITORAMDPROC gl_EndPerfMonitorAMD;

	#define glGenPerfMonitorsAMD(...) gl::GenPerfMonitorsAMD( __VA_ARGS__, __FILE__,__LINE__ )
	static void GenPerfMonitorsAMD (GLsizei n, GLuint *monitors, const char* file, int line);
	static PFNGLGENPERFMONITORSAMDPROC gl_GenPerfMonitorsAMD;

	#define glGetPerfMonitorCounterDataAMD(...) gl::GetPerfMonitorCounterDataAMD( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetPerfMonitorCounterDataAMD (GLuint monitor, GLenum pname, GLsizei dataSize, GLuint *data, GLint *bytesWritten, const char* file, int line);
	static PFNGLGETPERFMONITORCOUNTERDATAAMDPROC gl_GetPerfMonitorCounterDataAMD;

	#define glGetPerfMonitorCounterInfoAMD(...) gl::GetPerfMonitorCounterInfoAMD( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetPerfMonitorCounterInfoAMD (GLuint group, GLuint counter, GLenum pname, void *data, const char* file, int line);
	static PFNGLGETPERFMONITORCOUNTERINFOAMDPROC gl_GetPerfMonitorCounterInfoAMD;

	#define glGetPerfMonitorCounterStringAMD(...) gl::GetPerfMonitorCounterStringAMD( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetPerfMonitorCounterStringAMD (GLuint group, GLuint counter, GLsizei bufSize, GLsizei *length, GLchar *counterString, const char* file, int line);
	static PFNGLGETPERFMONITORCOUNTERSTRINGAMDPROC gl_GetPerfMonitorCounterStringAMD;

	#define glGetPerfMonitorCountersAMD(...) gl::GetPerfMonitorCountersAMD( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetPerfMonitorCountersAMD (GLuint group, GLint *numCounters, GLint *maxActiveCounters, GLsizei counterSize, GLuint *counters, const char* file, int line);
	static PFNGLGETPERFMONITORCOUNTERSAMDPROC gl_GetPerfMonitorCountersAMD;

	#define glGetPerfMonitorGroupStringAMD(...) gl::GetPerfMonitorGroupStringAMD( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetPerfMonitorGroupStringAMD (GLuint group, GLsizei bufSize, GLsizei *length, GLchar *groupString, const char* file, int line);
	static PFNGLGETPERFMONITORGROUPSTRINGAMDPROC gl_GetPerfMonitorGroupStringAMD;

	#define glGetPerfMonitorGroupsAMD(...) gl::GetPerfMonitorGroupsAMD( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetPerfMonitorGroupsAMD (GLint *numGroups, GLsizei groupsSize, GLuint *groups, const char* file, int line);
	static PFNGLGETPERFMONITORGROUPSAMDPROC gl_GetPerfMonitorGroupsAMD;

	#define glSelectPerfMonitorCountersAMD(...) gl::SelectPerfMonitorCountersAMD( __VA_ARGS__, __FILE__,__LINE__ )
	static void SelectPerfMonitorCountersAMD (GLuint monitor, GLboolean enable, GLuint group, GLint numCounters, GLuint *counterList, const char* file, int line);
	static PFNGLSELECTPERFMONITORCOUNTERSAMDPROC gl_SelectPerfMonitorCountersAMD;

	// GL_ANGLE_framebuffer_blit
	#define glBlitFramebufferANGLE(...) gl::BlitFramebufferANGLE( __VA_ARGS__, __FILE__,__LINE__ )
	static void BlitFramebufferANGLE (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter, const char* file, int line);
	static PFNGLBLITFRAMEBUFFERANGLEPROC gl_BlitFramebufferANGLE;

	// GL_ANGLE_framebuffer_multisample
	#define glRenderbufferStorageMultisampleANGLE(...) gl::RenderbufferStorageMultisampleANGLE( __VA_ARGS__, __FILE__,__LINE__ )
	static void RenderbufferStorageMultisampleANGLE (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, const char* file, int line);
	static PFNGLRENDERBUFFERSTORAGEMULTISAMPLEANGLEPROC gl_RenderbufferStorageMultisampleANGLE;

	// GL_ANGLE_instanced_arrays
	#define glDrawArraysInstancedANGLE(...) gl::DrawArraysInstancedANGLE( __VA_ARGS__, __FILE__,__LINE__ )
	static void DrawArraysInstancedANGLE (GLenum mode, GLint first, GLsizei count, GLsizei primcount, const char* file, int line);
	static PFNGLDRAWARRAYSINSTANCEDANGLEPROC gl_DrawArraysInstancedANGLE;

	#define glDrawElementsInstancedANGLE(...) gl::DrawElementsInstancedANGLE( __VA_ARGS__, __FILE__,__LINE__ )
	static void DrawElementsInstancedANGLE (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei primcount, const char* file, int line);
	static PFNGLDRAWELEMENTSINSTANCEDANGLEPROC gl_DrawElementsInstancedANGLE;

	#define glVertexAttribDivisorANGLE(...) gl::VertexAttribDivisorANGLE( __VA_ARGS__, __FILE__,__LINE__ )
	static void VertexAttribDivisorANGLE (GLuint index, GLuint divisor, const char* file, int line);
	static PFNGLVERTEXATTRIBDIVISORANGLEPROC gl_VertexAttribDivisorANGLE;

	// GL_ANGLE_translated_shader_source
	#define glGetTranslatedShaderSourceANGLE(...) gl::GetTranslatedShaderSourceANGLE( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetTranslatedShaderSourceANGLE (GLuint shader, GLsizei bufsize, GLsizei *length, GLchar *source, const char* file, int line);
	static PFNGLGETTRANSLATEDSHADERSOURCEANGLEPROC gl_GetTranslatedShaderSourceANGLE;

	// GL_APPLE_copy_texture_levels
	#define glCopyTextureLevelsAPPLE(...) gl::CopyTextureLevelsAPPLE( __VA_ARGS__, __FILE__,__LINE__ )
	static void CopyTextureLevelsAPPLE (GLuint destinationTexture, GLuint sourceTexture, GLint sourceBaseLevel, GLsizei sourceLevelCount, const char* file, int line);
	static PFNGLCOPYTEXTURELEVELSAPPLEPROC gl_CopyTextureLevelsAPPLE;

	// GL_APPLE_framebuffer_multisample
	#define glRenderbufferStorageMultisampleAPPLE(...) gl::RenderbufferStorageMultisampleAPPLE( __VA_ARGS__, __FILE__,__LINE__ )
	static void RenderbufferStorageMultisampleAPPLE (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, const char* file, int line);
	static PFNGLRENDERBUFFERSTORAGEMULTISAMPLEAPPLEPROC gl_RenderbufferStorageMultisampleAPPLE;

	#define glResolveMultisampleFramebufferAPPLE() gl::ResolveMultisampleFramebufferAPPLE( __FILE__,__LINE__ )
	static void ResolveMultisampleFramebufferAPPLE (const char* file, int line);
	static PFNGLRESOLVEMULTISAMPLEFRAMEBUFFERAPPLEPROC gl_ResolveMultisampleFramebufferAPPLE;

	// GL_APPLE_sync
	#define glClientWaitSyncAPPLE(...) gl::ClientWaitSyncAPPLE( __VA_ARGS__, __FILE__,__LINE__ )
	static GLenum ClientWaitSyncAPPLE (GLsync sync, GLbitfield flags, GLuint64 timeout, const char* file, int line);
	static PFNGLCLIENTWAITSYNCAPPLEPROC gl_ClientWaitSyncAPPLE;

	#define glDeleteSyncAPPLE(...) gl::DeleteSyncAPPLE( __VA_ARGS__, __FILE__,__LINE__ )
	static void DeleteSyncAPPLE (GLsync sync, const char* file, int line);
	static PFNGLDELETESYNCAPPLEPROC gl_DeleteSyncAPPLE;

	#define glFenceSyncAPPLE(...) gl::FenceSyncAPPLE( __VA_ARGS__, __FILE__,__LINE__ )
	static GLsync FenceSyncAPPLE (GLenum condition, GLbitfield flags, const char* file, int line);
	static PFNGLFENCESYNCAPPLEPROC gl_FenceSyncAPPLE;

	#define glGetInteger64vAPPLE(...) gl::GetInteger64vAPPLE( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetInteger64vAPPLE (GLenum pname, GLint64 *params, const char* file, int line);
	static PFNGLGETINTEGER64VAPPLEPROC gl_GetInteger64vAPPLE;

	#define glGetSyncivAPPLE(...) gl::GetSyncivAPPLE( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetSyncivAPPLE (GLsync sync, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *values, const char* file, int line);
	static PFNGLGETSYNCIVAPPLEPROC gl_GetSyncivAPPLE;

	#define glIsSyncAPPLE(...) gl::IsSyncAPPLE( __VA_ARGS__, __FILE__,__LINE__ )
	static GLboolean IsSyncAPPLE (GLsync sync, const char* file, int line);
	static PFNGLISSYNCAPPLEPROC gl_IsSyncAPPLE;

	#define glWaitSyncAPPLE(...) gl::WaitSyncAPPLE( __VA_ARGS__, __FILE__,__LINE__ )
	static void WaitSyncAPPLE (GLsync sync, GLbitfield flags, GLuint64 timeout, const char* file, int line);
	static PFNGLWAITSYNCAPPLEPROC gl_WaitSyncAPPLE;

	// GL_EXT_base_instance
	#define glDrawArraysInstancedBaseInstanceEXT(...) gl::DrawArraysInstancedBaseInstanceEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void DrawArraysInstancedBaseInstanceEXT (GLenum mode, GLint first, GLsizei count, GLsizei instancecount, GLuint baseinstance, const char* file, int line);
	static PFNGLDRAWARRAYSINSTANCEDBASEINSTANCEEXTPROC gl_DrawArraysInstancedBaseInstanceEXT;

	#define glDrawElementsInstancedBaseInstanceEXT(...) gl::DrawElementsInstancedBaseInstanceEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void DrawElementsInstancedBaseInstanceEXT (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLuint baseinstance, const char* file, int line);
	static PFNGLDRAWELEMENTSINSTANCEDBASEINSTANCEEXTPROC gl_DrawElementsInstancedBaseInstanceEXT;

	#define glDrawElementsInstancedBaseVertexBaseInstanceEXT(...) gl::DrawElementsInstancedBaseVertexBaseInstanceEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void DrawElementsInstancedBaseVertexBaseInstanceEXT (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex, GLuint baseinstance, const char* file, int line);
	static PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXBASEINSTANCEEXTPROC gl_DrawElementsInstancedBaseVertexBaseInstanceEXT;

	// GL_EXT_blend_func_extended
	#define glBindFragDataLocationEXT(...) gl::BindFragDataLocationEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void BindFragDataLocationEXT (GLuint program, GLuint color, const GLchar *name, const char* file, int line);
	static PFNGLBINDFRAGDATALOCATIONEXTPROC gl_BindFragDataLocationEXT;

	#define glBindFragDataLocationIndexedEXT(...) gl::BindFragDataLocationIndexedEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void BindFragDataLocationIndexedEXT (GLuint program, GLuint colorNumber, GLuint index, const GLchar *name, const char* file, int line);
	static PFNGLBINDFRAGDATALOCATIONINDEXEDEXTPROC gl_BindFragDataLocationIndexedEXT;

	#define glGetFragDataIndexEXT(...) gl::GetFragDataIndexEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static GLint GetFragDataIndexEXT (GLuint program, const GLchar *name, const char* file, int line);
	static PFNGLGETFRAGDATAINDEXEXTPROC gl_GetFragDataIndexEXT;

	#define glGetProgramResourceLocationIndexEXT(...) gl::GetProgramResourceLocationIndexEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static GLint GetProgramResourceLocationIndexEXT (GLuint program, GLenum programInterface, const GLchar *name, const char* file, int line);
	static PFNGLGETPROGRAMRESOURCELOCATIONINDEXEXTPROC gl_GetProgramResourceLocationIndexEXT;

	// GL_EXT_buffer_storage
	#define glBufferStorageEXT(...) gl::BufferStorageEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void BufferStorageEXT (GLenum target, GLsizeiptr size, const void *data, GLbitfield flags, const char* file, int line);
	static PFNGLBUFFERSTORAGEEXTPROC gl_BufferStorageEXT;

	// GL_EXT_copy_image
	#define glCopyImageSubDataEXT(...) gl::CopyImageSubDataEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void CopyImageSubDataEXT (GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth, const char* file, int line);
	static PFNGLCOPYIMAGESUBDATAEXTPROC gl_CopyImageSubDataEXT;

	// GL_EXT_debug_label
	#define glGetObjectLabelEXT(...) gl::GetObjectLabelEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetObjectLabelEXT (GLenum type, GLuint object, GLsizei bufSize, GLsizei *length, GLchar *label, const char* file, int line);
	static PFNGLGETOBJECTLABELEXTPROC gl_GetObjectLabelEXT;

	#define glLabelObjectEXT(...) gl::LabelObjectEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void LabelObjectEXT (GLenum type, GLuint object, GLsizei length, const GLchar *label, const char* file, int line);
	static PFNGLLABELOBJECTEXTPROC gl_LabelObjectEXT;

	// GL_EXT_debug_marker
	#define glInsertEventMarkerEXT(...) gl::InsertEventMarkerEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void InsertEventMarkerEXT (GLsizei length, const GLchar *marker, const char* file, int line);
	static PFNGLINSERTEVENTMARKEREXTPROC gl_InsertEventMarkerEXT;

	#define glPopGroupMarkerEXT() gl::PopGroupMarkerEXT( __FILE__,__LINE__ )
	static void PopGroupMarkerEXT (const char* file, int line);
	static PFNGLPOPGROUPMARKEREXTPROC gl_PopGroupMarkerEXT;

	#define glPushGroupMarkerEXT(...) gl::PushGroupMarkerEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void PushGroupMarkerEXT (GLsizei length, const GLchar *marker, const char* file, int line);
	static PFNGLPUSHGROUPMARKEREXTPROC gl_PushGroupMarkerEXT;

	// GL_EXT_discard_framebuffer
	#define glDiscardFramebufferEXT(...) gl::DiscardFramebufferEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void DiscardFramebufferEXT (GLenum target, GLsizei numAttachments, const GLenum *attachments, const char* file, int line);
	static PFNGLDISCARDFRAMEBUFFEREXTPROC gl_DiscardFramebufferEXT;

	// GL_EXT_disjoint_timer_query
	#define glBeginQueryEXT(...) gl::BeginQueryEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void BeginQueryEXT (GLenum target, GLuint id, const char* file, int line);
	static PFNGLBEGINQUERYEXTPROC gl_BeginQueryEXT;

	#define glDeleteQueriesEXT(...) gl::DeleteQueriesEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void DeleteQueriesEXT (GLsizei n, const GLuint *ids, const char* file, int line);
	static PFNGLDELETEQUERIESEXTPROC gl_DeleteQueriesEXT;

	#define glEndQueryEXT(...) gl::EndQueryEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void EndQueryEXT (GLenum target, const char* file, int line);
	static PFNGLENDQUERYEXTPROC gl_EndQueryEXT;

	#define glGenQueriesEXT(...) gl::GenQueriesEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void GenQueriesEXT (GLsizei n, GLuint *ids, const char* file, int line);
	static PFNGLGENQUERIESEXTPROC gl_GenQueriesEXT;

	#define glGetQueryObjecti64vEXT(...) gl::GetQueryObjecti64vEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetQueryObjecti64vEXT (GLuint id, GLenum pname, GLint64 *params, const char* file, int line);
	static PFNGLGETQUERYOBJECTI64VEXTPROC gl_GetQueryObjecti64vEXT;

	#define glGetQueryObjectivEXT(...) gl::GetQueryObjectivEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetQueryObjectivEXT (GLuint id, GLenum pname, GLint *params, const char* file, int line);
	static PFNGLGETQUERYOBJECTIVEXTPROC gl_GetQueryObjectivEXT;

	#define glGetQueryObjectui64vEXT(...) gl::GetQueryObjectui64vEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetQueryObjectui64vEXT (GLuint id, GLenum pname, GLuint64 *params, const char* file, int line);
	static PFNGLGETQUERYOBJECTUI64VEXTPROC gl_GetQueryObjectui64vEXT;

	#define glGetQueryObjectuivEXT(...) gl::GetQueryObjectuivEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetQueryObjectuivEXT (GLuint id, GLenum pname, GLuint *params, const char* file, int line);
	static PFNGLGETQUERYOBJECTUIVEXTPROC gl_GetQueryObjectuivEXT;

	#define glGetQueryivEXT(...) gl::GetQueryivEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetQueryivEXT (GLenum target, GLenum pname, GLint *params, const char* file, int line);
	static PFNGLGETQUERYIVEXTPROC gl_GetQueryivEXT;

	#define glIsQueryEXT(...) gl::IsQueryEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static GLboolean IsQueryEXT (GLuint id, const char* file, int line);
	static PFNGLISQUERYEXTPROC gl_IsQueryEXT;

	#define glQueryCounterEXT(...) gl::QueryCounterEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void QueryCounterEXT (GLuint id, GLenum target, const char* file, int line);
	static PFNGLQUERYCOUNTEREXTPROC gl_QueryCounterEXT;

	// GL_EXT_draw_buffers
	#define glDrawBuffersEXT(...) gl::DrawBuffersEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void DrawBuffersEXT (GLsizei n, const GLenum *bufs, const char* file, int line);
	static PFNGLDRAWBUFFERSEXTPROC gl_DrawBuffersEXT;

	// GL_EXT_draw_buffers_indexed
	#define glBlendEquationSeparateiEXT(...) gl::BlendEquationSeparateiEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void BlendEquationSeparateiEXT (GLuint buf, GLenum modeRGB, GLenum modeAlpha, const char* file, int line);
	static PFNGLBLENDEQUATIONSEPARATEIEXTPROC gl_BlendEquationSeparateiEXT;

	#define glBlendEquationiEXT(...) gl::BlendEquationiEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void BlendEquationiEXT (GLuint buf, GLenum mode, const char* file, int line);
	static PFNGLBLENDEQUATIONIEXTPROC gl_BlendEquationiEXT;

	#define glBlendFuncSeparateiEXT(...) gl::BlendFuncSeparateiEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void BlendFuncSeparateiEXT (GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha, const char* file, int line);
	static PFNGLBLENDFUNCSEPARATEIEXTPROC gl_BlendFuncSeparateiEXT;

	#define glBlendFunciEXT(...) gl::BlendFunciEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void BlendFunciEXT (GLuint buf, GLenum src, GLenum dst, const char* file, int line);
	static PFNGLBLENDFUNCIEXTPROC gl_BlendFunciEXT;

	#define glColorMaskiEXT(...) gl::ColorMaskiEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ColorMaskiEXT (GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a, const char* file, int line);
	static PFNGLCOLORMASKIEXTPROC gl_ColorMaskiEXT;

	#define glDisableiEXT(...) gl::DisableiEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void DisableiEXT (GLenum target, GLuint index, const char* file, int line);
	static PFNGLDISABLEIEXTPROC gl_DisableiEXT;

	#define glEnableiEXT(...) gl::EnableiEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void EnableiEXT (GLenum target, GLuint index, const char* file, int line);
	static PFNGLENABLEIEXTPROC gl_EnableiEXT;

	#define glIsEnablediEXT(...) gl::IsEnablediEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static GLboolean IsEnablediEXT (GLenum target, GLuint index, const char* file, int line);
	static PFNGLISENABLEDIEXTPROC gl_IsEnablediEXT;

	// GL_EXT_draw_elements_base_vertex
	#define glDrawElementsBaseVertexEXT(...) gl::DrawElementsBaseVertexEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void DrawElementsBaseVertexEXT (GLenum mode, GLsizei count, GLenum type, const void *indices, GLint basevertex, const char* file, int line);
	static PFNGLDRAWELEMENTSBASEVERTEXEXTPROC gl_DrawElementsBaseVertexEXT;

	#define glDrawElementsInstancedBaseVertexEXT(...) gl::DrawElementsInstancedBaseVertexEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void DrawElementsInstancedBaseVertexEXT (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex, const char* file, int line);
	static PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXEXTPROC gl_DrawElementsInstancedBaseVertexEXT;

	#define glDrawRangeElementsBaseVertexEXT(...) gl::DrawRangeElementsBaseVertexEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void DrawRangeElementsBaseVertexEXT (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices, GLint basevertex, const char* file, int line);
	static PFNGLDRAWRANGEELEMENTSBASEVERTEXEXTPROC gl_DrawRangeElementsBaseVertexEXT;

	#define glMultiDrawElementsBaseVertexEXT(...) gl::MultiDrawElementsBaseVertexEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void MultiDrawElementsBaseVertexEXT (GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei primcount, const GLint *basevertex, const char* file, int line);
	static PFNGLMULTIDRAWELEMENTSBASEVERTEXEXTPROC gl_MultiDrawElementsBaseVertexEXT;

	// GL_EXT_draw_instanced
	#define glDrawArraysInstancedEXT(...) gl::DrawArraysInstancedEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void DrawArraysInstancedEXT (GLenum mode, GLint start, GLsizei count, GLsizei primcount, const char* file, int line);
	static PFNGLDRAWARRAYSINSTANCEDEXTPROC gl_DrawArraysInstancedEXT;

	#define glDrawElementsInstancedEXT(...) gl::DrawElementsInstancedEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void DrawElementsInstancedEXT (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei primcount, const char* file, int line);
	static PFNGLDRAWELEMENTSINSTANCEDEXTPROC gl_DrawElementsInstancedEXT;

	// GL_EXT_geometry_shader
	#define glFramebufferTextureEXT(...) gl::FramebufferTextureEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void FramebufferTextureEXT (GLenum target, GLenum attachment, GLuint texture, GLint level, const char* file, int line);
	static PFNGLFRAMEBUFFERTEXTUREEXTPROC gl_FramebufferTextureEXT;

	// GL_EXT_instanced_arrays
	#define glVertexAttribDivisorEXT(...) gl::VertexAttribDivisorEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void VertexAttribDivisorEXT (GLuint index, GLuint divisor, const char* file, int line);
	static PFNGLVERTEXATTRIBDIVISOREXTPROC gl_VertexAttribDivisorEXT;

	// GL_EXT_map_buffer_range
	#define glFlushMappedBufferRangeEXT(...) gl::FlushMappedBufferRangeEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void FlushMappedBufferRangeEXT (GLenum target, GLintptr offset, GLsizeiptr length, const char* file, int line);
	static PFNGLFLUSHMAPPEDBUFFERRANGEEXTPROC gl_FlushMappedBufferRangeEXT;

	#define glMapBufferRangeEXT(...) gl::MapBufferRangeEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void MapBufferRangeEXT (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access, const char* file, int line);
	static PFNGLMAPBUFFERRANGEEXTPROC gl_MapBufferRangeEXT;

	// GL_EXT_multi_draw_arrays
	#define glMultiDrawArraysEXT(...) gl::MultiDrawArraysEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void MultiDrawArraysEXT (GLenum mode, const GLint *first, const GLsizei *count, GLsizei primcount, const char* file, int line);
	static PFNGLMULTIDRAWARRAYSEXTPROC gl_MultiDrawArraysEXT;

	#define glMultiDrawElementsEXT(...) gl::MultiDrawElementsEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void MultiDrawElementsEXT (GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei primcount, const char* file, int line);
	static PFNGLMULTIDRAWELEMENTSEXTPROC gl_MultiDrawElementsEXT;

	// GL_EXT_multi_draw_indirect
	#define glMultiDrawArraysIndirectEXT(...) gl::MultiDrawArraysIndirectEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void MultiDrawArraysIndirectEXT (GLenum mode, const void *indirect, GLsizei drawcount, GLsizei stride, const char* file, int line);
	static PFNGLMULTIDRAWARRAYSINDIRECTEXTPROC gl_MultiDrawArraysIndirectEXT;

	#define glMultiDrawElementsIndirectEXT(...) gl::MultiDrawElementsIndirectEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void MultiDrawElementsIndirectEXT (GLenum mode, GLenum type, const void *indirect, GLsizei drawcount, GLsizei stride, const char* file, int line);
	static PFNGLMULTIDRAWELEMENTSINDIRECTEXTPROC gl_MultiDrawElementsIndirectEXT;

	// GL_EXT_multisampled_render_to_texture
	#define glFramebufferTexture2DMultisampleEXT(...) gl::FramebufferTexture2DMultisampleEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void FramebufferTexture2DMultisampleEXT (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples, const char* file, int line);
	static PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEEXTPROC gl_FramebufferTexture2DMultisampleEXT;

	#define glRenderbufferStorageMultisampleEXT(...) gl::RenderbufferStorageMultisampleEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void RenderbufferStorageMultisampleEXT (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, const char* file, int line);
	static PFNGLRENDERBUFFERSTORAGEMULTISAMPLEEXTPROC gl_RenderbufferStorageMultisampleEXT;

	// GL_EXT_multiview_draw_buffers
	#define glDrawBuffersIndexedEXT(...) gl::DrawBuffersIndexedEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void DrawBuffersIndexedEXT (GLint n, const GLenum *location, const GLint *indices, const char* file, int line);
	static PFNGLDRAWBUFFERSINDEXEDEXTPROC gl_DrawBuffersIndexedEXT;

	#define glGetIntegeri_vEXT(...) gl::GetIntegeri_vEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetIntegeri_vEXT (GLenum target, GLuint index, GLint *data, const char* file, int line);
	static PFNGLGETINTEGERI_VEXTPROC gl_GetIntegeri_vEXT;

	#define glReadBufferIndexedEXT(...) gl::ReadBufferIndexedEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ReadBufferIndexedEXT (GLenum src, GLint index, const char* file, int line);
	static PFNGLREADBUFFERINDEXEDEXTPROC gl_ReadBufferIndexedEXT;

	// GL_EXT_polygon_offset_clamp
	#define glPolygonOffsetClampEXT(...) gl::PolygonOffsetClampEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void PolygonOffsetClampEXT (GLfloat factor, GLfloat units, GLfloat clamp, const char* file, int line);
	static PFNGLPOLYGONOFFSETCLAMPEXTPROC gl_PolygonOffsetClampEXT;

	// GL_EXT_primitive_bounding_box
	#define glPrimitiveBoundingBoxEXT(...) gl::PrimitiveBoundingBoxEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void PrimitiveBoundingBoxEXT (GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW, GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW, const char* file, int line);
	static PFNGLPRIMITIVEBOUNDINGBOXEXTPROC gl_PrimitiveBoundingBoxEXT;

	// GL_EXT_raster_multisample
	#define glRasterSamplesEXT(...) gl::RasterSamplesEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void RasterSamplesEXT (GLuint samples, GLboolean fixedsamplelocations, const char* file, int line);
	static PFNGLRASTERSAMPLESEXTPROC gl_RasterSamplesEXT;

	// GL_EXT_robustness
	#define glGetGraphicsResetStatusEXT() gl::GetGraphicsResetStatusEXT( __FILE__,__LINE__ )
	static GLenum GetGraphicsResetStatusEXT(const char* file, int line);
	static PFNGLGETGRAPHICSRESETSTATUSEXTPROC gl_GetGraphicsResetStatusEXT;

	#define glGetnUniformfvEXT(...) gl::GetnUniformfvEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetnUniformfvEXT (GLuint program, GLint location, GLsizei bufSize, GLfloat *params, const char* file, int line);
	static PFNGLGETNUNIFORMFVEXTPROC gl_GetnUniformfvEXT;

	#define glGetnUniformivEXT(...) gl::GetnUniformivEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetnUniformivEXT (GLuint program, GLint location, GLsizei bufSize, GLint *params, const char* file, int line);
	static PFNGLGETNUNIFORMIVEXTPROC gl_GetnUniformivEXT;

	#define glReadnPixelsEXT(...) gl::ReadnPixelsEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ReadnPixelsEXT (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void *data, const char* file, int line);
	static PFNGLREADNPIXELSEXTPROC gl_ReadnPixelsEXT;

	// GL_EXT_separate_shader_objects
	#define glActiveShaderProgramEXT(...) gl::ActiveShaderProgramEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ActiveShaderProgramEXT (GLuint pipeline, GLuint program, const char* file, int line);
	static PFNGLACTIVESHADERPROGRAMEXTPROC gl_ActiveShaderProgramEXT;

	#define glBindProgramPipelineEXT(...) gl::BindProgramPipelineEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void BindProgramPipelineEXT (GLuint pipeline, const char* file, int line);
	static PFNGLBINDPROGRAMPIPELINEEXTPROC gl_BindProgramPipelineEXT;

	#define glCreateShaderProgramvEXT(...) gl::CreateShaderProgramvEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static GLuint CreateShaderProgramvEXT (GLenum type, GLsizei count, const GLchar **strings, const char* file, int line);
	static PFNGLCREATESHADERPROGRAMVEXTPROC gl_CreateShaderProgramvEXT;

	#define glDeleteProgramPipelinesEXT(...) gl::DeleteProgramPipelinesEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void DeleteProgramPipelinesEXT (GLsizei n, const GLuint *pipelines, const char* file, int line);
	static PFNGLDELETEPROGRAMPIPELINESEXTPROC gl_DeleteProgramPipelinesEXT;

	#define glGenProgramPipelinesEXT(...) gl::GenProgramPipelinesEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void GenProgramPipelinesEXT (GLsizei n, GLuint *pipelines, const char* file, int line);
	static PFNGLGENPROGRAMPIPELINESEXTPROC gl_GenProgramPipelinesEXT;

	#define glGetProgramPipelineInfoLogEXT(...) gl::GetProgramPipelineInfoLogEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetProgramPipelineInfoLogEXT (GLuint pipeline, GLsizei bufSize, GLsizei *length, GLchar *infoLog, const char* file, int line);
	static PFNGLGETPROGRAMPIPELINEINFOLOGEXTPROC gl_GetProgramPipelineInfoLogEXT;

	#define glGetProgramPipelineivEXT(...) gl::GetProgramPipelineivEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetProgramPipelineivEXT (GLuint pipeline, GLenum pname, GLint *params, const char* file, int line);
	static PFNGLGETPROGRAMPIPELINEIVEXTPROC gl_GetProgramPipelineivEXT;

	#define glIsProgramPipelineEXT(...) gl::IsProgramPipelineEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static GLboolean IsProgramPipelineEXT (GLuint pipeline, const char* file, int line);
	static PFNGLISPROGRAMPIPELINEEXTPROC gl_IsProgramPipelineEXT;

	#define glProgramParameteriEXT(...) gl::ProgramParameteriEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramParameteriEXT (GLuint program, GLenum pname, GLint value, const char* file, int line);
	static PFNGLPROGRAMPARAMETERIEXTPROC gl_ProgramParameteriEXT;

	#define glProgramUniform1fEXT(...) gl::ProgramUniform1fEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniform1fEXT (GLuint program, GLint location, GLfloat v0, const char* file, int line);
	static PFNGLPROGRAMUNIFORM1FEXTPROC gl_ProgramUniform1fEXT;

	#define glProgramUniform1fvEXT(...) gl::ProgramUniform1fvEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniform1fvEXT (GLuint program, GLint location, GLsizei count, const GLfloat *value, const char* file, int line);
	static PFNGLPROGRAMUNIFORM1FVEXTPROC gl_ProgramUniform1fvEXT;

	#define glProgramUniform1iEXT(...) gl::ProgramUniform1iEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniform1iEXT (GLuint program, GLint location, GLint v0, const char* file, int line);
	static PFNGLPROGRAMUNIFORM1IEXTPROC gl_ProgramUniform1iEXT;

	#define glProgramUniform1ivEXT(...) gl::ProgramUniform1ivEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniform1ivEXT (GLuint program, GLint location, GLsizei count, const GLint *value, const char* file, int line);
	static PFNGLPROGRAMUNIFORM1IVEXTPROC gl_ProgramUniform1ivEXT;

	#define glProgramUniform1uiEXT(...) gl::ProgramUniform1uiEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniform1uiEXT (GLuint program, GLint location, GLuint v0, const char* file, int line);
	static PFNGLPROGRAMUNIFORM1UIEXTPROC gl_ProgramUniform1uiEXT;

	#define glProgramUniform1uivEXT(...) gl::ProgramUniform1uivEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniform1uivEXT (GLuint program, GLint location, GLsizei count, const GLuint *value, const char* file, int line);
	static PFNGLPROGRAMUNIFORM1UIVEXTPROC gl_ProgramUniform1uivEXT;

	#define glProgramUniform2fEXT(...) gl::ProgramUniform2fEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniform2fEXT (GLuint program, GLint location, GLfloat v0, GLfloat v1, const char* file, int line);
	static PFNGLPROGRAMUNIFORM2FEXTPROC gl_ProgramUniform2fEXT;

	#define glProgramUniform2fvEXT(...) gl::ProgramUniform2fvEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniform2fvEXT (GLuint program, GLint location, GLsizei count, const GLfloat *value, const char* file, int line);
	static PFNGLPROGRAMUNIFORM2FVEXTPROC gl_ProgramUniform2fvEXT;

	#define glProgramUniform2iEXT(...) gl::ProgramUniform2iEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniform2iEXT (GLuint program, GLint location, GLint v0, GLint v1, const char* file, int line);
	static PFNGLPROGRAMUNIFORM2IEXTPROC gl_ProgramUniform2iEXT;

	#define glProgramUniform2ivEXT(...) gl::ProgramUniform2ivEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniform2ivEXT (GLuint program, GLint location, GLsizei count, const GLint *value, const char* file, int line);
	static PFNGLPROGRAMUNIFORM2IVEXTPROC gl_ProgramUniform2ivEXT;

	#define glProgramUniform2uiEXT(...) gl::ProgramUniform2uiEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniform2uiEXT (GLuint program, GLint location, GLuint v0, GLuint v1, const char* file, int line);
	static PFNGLPROGRAMUNIFORM2UIEXTPROC gl_ProgramUniform2uiEXT;

	#define glProgramUniform2uivEXT(...) gl::ProgramUniform2uivEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniform2uivEXT (GLuint program, GLint location, GLsizei count, const GLuint *value, const char* file, int line);
	static PFNGLPROGRAMUNIFORM2UIVEXTPROC gl_ProgramUniform2uivEXT;

	#define glProgramUniform3fEXT(...) gl::ProgramUniform3fEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniform3fEXT (GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, const char* file, int line);
	static PFNGLPROGRAMUNIFORM3FEXTPROC gl_ProgramUniform3fEXT;

	#define glProgramUniform3fvEXT(...) gl::ProgramUniform3fvEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniform3fvEXT (GLuint program, GLint location, GLsizei count, const GLfloat *value, const char* file, int line);
	static PFNGLPROGRAMUNIFORM3FVEXTPROC gl_ProgramUniform3fvEXT;

	#define glProgramUniform3iEXT(...) gl::ProgramUniform3iEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniform3iEXT (GLuint program, GLint location, GLint v0, GLint v1, GLint v2, const char* file, int line);
	static PFNGLPROGRAMUNIFORM3IEXTPROC gl_ProgramUniform3iEXT;

	#define glProgramUniform3ivEXT(...) gl::ProgramUniform3ivEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniform3ivEXT (GLuint program, GLint location, GLsizei count, const GLint *value, const char* file, int line);
	static PFNGLPROGRAMUNIFORM3IVEXTPROC gl_ProgramUniform3ivEXT;

	#define glProgramUniform3uiEXT(...) gl::ProgramUniform3uiEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniform3uiEXT (GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, const char* file, int line);
	static PFNGLPROGRAMUNIFORM3UIEXTPROC gl_ProgramUniform3uiEXT;

	#define glProgramUniform3uivEXT(...) gl::ProgramUniform3uivEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniform3uivEXT (GLuint program, GLint location, GLsizei count, const GLuint *value, const char* file, int line);
	static PFNGLPROGRAMUNIFORM3UIVEXTPROC gl_ProgramUniform3uivEXT;

	#define glProgramUniform4fEXT(...) gl::ProgramUniform4fEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniform4fEXT (GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3, const char* file, int line);
	static PFNGLPROGRAMUNIFORM4FEXTPROC gl_ProgramUniform4fEXT;

	#define glProgramUniform4fvEXT(...) gl::ProgramUniform4fvEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniform4fvEXT (GLuint program, GLint location, GLsizei count, const GLfloat *value, const char* file, int line);
	static PFNGLPROGRAMUNIFORM4FVEXTPROC gl_ProgramUniform4fvEXT;

	#define glProgramUniform4iEXT(...) gl::ProgramUniform4iEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniform4iEXT (GLuint program, GLint location, GLint v0, GLint v1, GLint v2, GLint v3, const char* file, int line);
	static PFNGLPROGRAMUNIFORM4IEXTPROC gl_ProgramUniform4iEXT;

	#define glProgramUniform4ivEXT(...) gl::ProgramUniform4ivEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniform4ivEXT (GLuint program, GLint location, GLsizei count, const GLint *value, const char* file, int line);
	static PFNGLPROGRAMUNIFORM4IVEXTPROC gl_ProgramUniform4ivEXT;

	#define glProgramUniform4uiEXT(...) gl::ProgramUniform4uiEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniform4uiEXT (GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3, const char* file, int line);
	static PFNGLPROGRAMUNIFORM4UIEXTPROC gl_ProgramUniform4uiEXT;

	#define glProgramUniform4uivEXT(...) gl::ProgramUniform4uivEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniform4uivEXT (GLuint program, GLint location, GLsizei count, const GLuint *value, const char* file, int line);
	static PFNGLPROGRAMUNIFORM4UIVEXTPROC gl_ProgramUniform4uivEXT;

	#define glProgramUniformMatrix2fvEXT(...) gl::ProgramUniformMatrix2fvEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniformMatrix2fvEXT (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line);
	static PFNGLPROGRAMUNIFORMMATRIX2FVEXTPROC gl_ProgramUniformMatrix2fvEXT;

	#define glProgramUniformMatrix2x3fvEXT(...) gl::ProgramUniformMatrix2x3fvEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniformMatrix2x3fvEXT (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line);
	static PFNGLPROGRAMUNIFORMMATRIX2X3FVEXTPROC gl_ProgramUniformMatrix2x3fvEXT;

	#define glProgramUniformMatrix2x4fvEXT(...) gl::ProgramUniformMatrix2x4fvEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniformMatrix2x4fvEXT (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line);
	static PFNGLPROGRAMUNIFORMMATRIX2X4FVEXTPROC gl_ProgramUniformMatrix2x4fvEXT;

	#define glProgramUniformMatrix3fvEXT(...) gl::ProgramUniformMatrix3fvEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniformMatrix3fvEXT (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line);
	static PFNGLPROGRAMUNIFORMMATRIX3FVEXTPROC gl_ProgramUniformMatrix3fvEXT;

	#define glProgramUniformMatrix3x2fvEXT(...) gl::ProgramUniformMatrix3x2fvEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniformMatrix3x2fvEXT (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line);
	static PFNGLPROGRAMUNIFORMMATRIX3X2FVEXTPROC gl_ProgramUniformMatrix3x2fvEXT;

	#define glProgramUniformMatrix3x4fvEXT(...) gl::ProgramUniformMatrix3x4fvEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniformMatrix3x4fvEXT (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line);
	static PFNGLPROGRAMUNIFORMMATRIX3X4FVEXTPROC gl_ProgramUniformMatrix3x4fvEXT;

	#define glProgramUniformMatrix4fvEXT(...) gl::ProgramUniformMatrix4fvEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniformMatrix4fvEXT (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line);
	static PFNGLPROGRAMUNIFORMMATRIX4FVEXTPROC gl_ProgramUniformMatrix4fvEXT;

	#define glProgramUniformMatrix4x2fvEXT(...) gl::ProgramUniformMatrix4x2fvEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniformMatrix4x2fvEXT (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line);
	static PFNGLPROGRAMUNIFORMMATRIX4X2FVEXTPROC gl_ProgramUniformMatrix4x2fvEXT;

	#define glProgramUniformMatrix4x3fvEXT(...) gl::ProgramUniformMatrix4x3fvEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniformMatrix4x3fvEXT (GLuint program, GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line);
	static PFNGLPROGRAMUNIFORMMATRIX4X3FVEXTPROC gl_ProgramUniformMatrix4x3fvEXT;

	#define glUseProgramStagesEXT(...) gl::UseProgramStagesEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void UseProgramStagesEXT (GLuint pipeline, GLbitfield stages, GLuint program, const char* file, int line);
	static PFNGLUSEPROGRAMSTAGESEXTPROC gl_UseProgramStagesEXT;

	#define glValidateProgramPipelineEXT(...) gl::ValidateProgramPipelineEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ValidateProgramPipelineEXT (GLuint pipeline, const char* file, int line);
	static PFNGLVALIDATEPROGRAMPIPELINEEXTPROC gl_ValidateProgramPipelineEXT;

	// GL_EXT_shader_pixel_local_storage2
	#define glClearPixelLocalStorageuiEXT(...) gl::ClearPixelLocalStorageuiEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void ClearPixelLocalStorageuiEXT (GLsizei offset, GLsizei n, const GLuint *values, const char* file, int line);
	static PFNGLCLEARPIXELLOCALSTORAGEUIEXTPROC gl_ClearPixelLocalStorageuiEXT;

	#define glFramebufferPixelLocalStorageSizeEXT(...) gl::FramebufferPixelLocalStorageSizeEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void FramebufferPixelLocalStorageSizeEXT (GLuint target, GLsizei size, const char* file, int line);
	static PFNGLFRAMEBUFFERPIXELLOCALSTORAGESIZEEXTPROC gl_FramebufferPixelLocalStorageSizeEXT;

	#define glGetFramebufferPixelLocalStorageSizeEXT(...) gl::GetFramebufferPixelLocalStorageSizeEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static GLsizei GetFramebufferPixelLocalStorageSizeEXT (GLuint target, const char* file, int line);
	static PFNGLGETFRAMEBUFFERPIXELLOCALSTORAGESIZEEXTPROC gl_GetFramebufferPixelLocalStorageSizeEXT;

	// GL_EXT_sparse_texture
	#define glTexPageCommitmentEXT(...) gl::TexPageCommitmentEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void TexPageCommitmentEXT (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLboolean commit, const char* file, int line);
	static PFNGLTEXPAGECOMMITMENTEXTPROC gl_TexPageCommitmentEXT;

	// GL_EXT_tessellation_shader
	#define glPatchParameteriEXT(...) gl::PatchParameteriEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void PatchParameteriEXT (GLenum pname, GLint value, const char* file, int line);
	static PFNGLPATCHPARAMETERIEXTPROC gl_PatchParameteriEXT;

	// GL_EXT_texture_border_clamp
	#define glGetSamplerParameterIivEXT(...) gl::GetSamplerParameterIivEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetSamplerParameterIivEXT (GLuint sampler, GLenum pname, GLint *params, const char* file, int line);
	static PFNGLGETSAMPLERPARAMETERIIVEXTPROC gl_GetSamplerParameterIivEXT;

	#define glGetSamplerParameterIuivEXT(...) gl::GetSamplerParameterIuivEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetSamplerParameterIuivEXT (GLuint sampler, GLenum pname, GLuint *params, const char* file, int line);
	static PFNGLGETSAMPLERPARAMETERIUIVEXTPROC gl_GetSamplerParameterIuivEXT;

	#define glGetTexParameterIivEXT(...) gl::GetTexParameterIivEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetTexParameterIivEXT (GLenum target, GLenum pname, GLint *params, const char* file, int line);
	static PFNGLGETTEXPARAMETERIIVEXTPROC gl_GetTexParameterIivEXT;

	#define glGetTexParameterIuivEXT(...) gl::GetTexParameterIuivEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetTexParameterIuivEXT (GLenum target, GLenum pname, GLuint *params, const char* file, int line);
	static PFNGLGETTEXPARAMETERIUIVEXTPROC gl_GetTexParameterIuivEXT;

	#define glSamplerParameterIivEXT(...) gl::SamplerParameterIivEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void SamplerParameterIivEXT (GLuint sampler, GLenum pname, const GLint *param, const char* file, int line);
	static PFNGLSAMPLERPARAMETERIIVEXTPROC gl_SamplerParameterIivEXT;

	#define glSamplerParameterIuivEXT(...) gl::SamplerParameterIuivEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void SamplerParameterIuivEXT (GLuint sampler, GLenum pname, const GLuint *param, const char* file, int line);
	static PFNGLSAMPLERPARAMETERIUIVEXTPROC gl_SamplerParameterIuivEXT;

	#define glTexParameterIivEXT(...) gl::TexParameterIivEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void TexParameterIivEXT (GLenum target, GLenum pname, const GLint *params, const char* file, int line);
	static PFNGLTEXPARAMETERIIVEXTPROC gl_TexParameterIivEXT;

	#define glTexParameterIuivEXT(...) gl::TexParameterIuivEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void TexParameterIuivEXT (GLenum target, GLenum pname, const GLuint *params, const char* file, int line);
	static PFNGLTEXPARAMETERIUIVEXTPROC gl_TexParameterIuivEXT;

	// GL_EXT_texture_buffer
	#define glTexBufferEXT(...) gl::TexBufferEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void TexBufferEXT (GLenum target, GLenum internalformat, GLuint buffer, const char* file, int line);
	static PFNGLTEXBUFFEREXTPROC gl_TexBufferEXT;

	#define glTexBufferRangeEXT(...) gl::TexBufferRangeEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void TexBufferRangeEXT (GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size, const char* file, int line);
	static PFNGLTEXBUFFERRANGEEXTPROC gl_TexBufferRangeEXT;

	// GL_EXT_texture_storage
	#define glTexStorage1DEXT(...) gl::TexStorage1DEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void TexStorage1DEXT (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, const char* file, int line);
	static PFNGLTEXSTORAGE1DEXTPROC gl_TexStorage1DEXT;

	#define glTexStorage2DEXT(...) gl::TexStorage2DEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void TexStorage2DEXT (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, const char* file, int line);
	static PFNGLTEXSTORAGE2DEXTPROC gl_TexStorage2DEXT;

	#define glTexStorage3DEXT(...) gl::TexStorage3DEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void TexStorage3DEXT (GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, const char* file, int line);
	static PFNGLTEXSTORAGE3DEXTPROC gl_TexStorage3DEXT;

	#define glTextureStorage1DEXT(...) gl::TextureStorage1DEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void TextureStorage1DEXT (GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, const char* file, int line);
	static PFNGLTEXTURESTORAGE1DEXTPROC gl_TextureStorage1DEXT;

	#define glTextureStorage2DEXT(...) gl::TextureStorage2DEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void TextureStorage2DEXT (GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, const char* file, int line);
	static PFNGLTEXTURESTORAGE2DEXTPROC gl_TextureStorage2DEXT;

	#define glTextureStorage3DEXT(...) gl::TextureStorage3DEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void TextureStorage3DEXT (GLuint texture, GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, const char* file, int line);
	static PFNGLTEXTURESTORAGE3DEXTPROC gl_TextureStorage3DEXT;

	// GL_EXT_texture_view
	#define glTextureViewEXT(...) gl::TextureViewEXT( __VA_ARGS__, __FILE__,__LINE__ )
	static void TextureViewEXT (GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers, const char* file, int line);
	static PFNGLTEXTUREVIEWEXTPROC gl_TextureViewEXT;

	// GL_IMG_framebuffer_downsample
	#define glFramebufferTexture2DDownsampleIMG(...) gl::FramebufferTexture2DDownsampleIMG( __VA_ARGS__, __FILE__,__LINE__ )
	static void FramebufferTexture2DDownsampleIMG (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint xscale, GLint yscale, const char* file, int line);
	static PFNGLFRAMEBUFFERTEXTURE2DDOWNSAMPLEIMGPROC gl_FramebufferTexture2DDownsampleIMG;

	#define glFramebufferTextureLayerDownsampleIMG(...) gl::FramebufferTextureLayerDownsampleIMG( __VA_ARGS__, __FILE__,__LINE__ )
	static void FramebufferTextureLayerDownsampleIMG (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint layer, GLint xscale, GLint yscale, const char* file, int line);
	static PFNGLFRAMEBUFFERTEXTURELAYERDOWNSAMPLEIMGPROC gl_FramebufferTextureLayerDownsampleIMG;

	// GL_IMG_multisampled_render_to_texture
	#define glFramebufferTexture2DMultisampleIMG(...) gl::FramebufferTexture2DMultisampleIMG( __VA_ARGS__, __FILE__,__LINE__ )
	static void FramebufferTexture2DMultisampleIMG (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLsizei samples, const char* file, int line);
	static PFNGLFRAMEBUFFERTEXTURE2DMULTISAMPLEIMGPROC gl_FramebufferTexture2DMultisampleIMG;

	#define glRenderbufferStorageMultisampleIMG(...) gl::RenderbufferStorageMultisampleIMG( __VA_ARGS__, __FILE__,__LINE__ )
	static void RenderbufferStorageMultisampleIMG (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, const char* file, int line);
	static PFNGLRENDERBUFFERSTORAGEMULTISAMPLEIMGPROC gl_RenderbufferStorageMultisampleIMG;

	// GL_INTEL_framebuffer_CMAA
	#define glApplyFramebufferAttachmentCMAAINTEL() gl::ApplyFramebufferAttachmentCMAAINTEL( __FILE__,__LINE__ )
	static void ApplyFramebufferAttachmentCMAAINTEL (const char* file, int line);
	static PFNGLAPPLYFRAMEBUFFERATTACHMENTCMAAINTELPROC gl_ApplyFramebufferAttachmentCMAAINTEL;

	// GL_INTEL_performance_query
	#define glBeginPerfQueryINTEL(...) gl::BeginPerfQueryINTEL( __VA_ARGS__, __FILE__,__LINE__ )
	static void BeginPerfQueryINTEL (GLuint queryHandle, const char* file, int line);
	static PFNGLBEGINPERFQUERYINTELPROC gl_BeginPerfQueryINTEL;

	#define glCreatePerfQueryINTEL(...) gl::CreatePerfQueryINTEL( __VA_ARGS__, __FILE__,__LINE__ )
	static void CreatePerfQueryINTEL (GLuint queryId, GLuint *queryHandle, const char* file, int line);
	static PFNGLCREATEPERFQUERYINTELPROC gl_CreatePerfQueryINTEL;

	#define glDeletePerfQueryINTEL(...) gl::DeletePerfQueryINTEL( __VA_ARGS__, __FILE__,__LINE__ )
	static void DeletePerfQueryINTEL (GLuint queryHandle, const char* file, int line);
	static PFNGLDELETEPERFQUERYINTELPROC gl_DeletePerfQueryINTEL;

	#define glEndPerfQueryINTEL(...) gl::EndPerfQueryINTEL( __VA_ARGS__, __FILE__,__LINE__ )
	static void EndPerfQueryINTEL (GLuint queryHandle, const char* file, int line);
	static PFNGLENDPERFQUERYINTELPROC gl_EndPerfQueryINTEL;

	#define glGetFirstPerfQueryIdINTEL(...) gl::GetFirstPerfQueryIdINTEL( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetFirstPerfQueryIdINTEL (GLuint *queryId, const char* file, int line);
	static PFNGLGETFIRSTPERFQUERYIDINTELPROC gl_GetFirstPerfQueryIdINTEL;

	#define glGetNextPerfQueryIdINTEL(...) gl::GetNextPerfQueryIdINTEL( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetNextPerfQueryIdINTEL (GLuint queryId, GLuint *nextQueryId, const char* file, int line);
	static PFNGLGETNEXTPERFQUERYIDINTELPROC gl_GetNextPerfQueryIdINTEL;

	#define glGetPerfCounterInfoINTEL(...) gl::GetPerfCounterInfoINTEL( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetPerfCounterInfoINTEL (GLuint queryId, GLuint counterId, GLuint counterNameLength, GLchar *counterName, GLuint counterDescLength, GLchar *counterDesc, GLuint *counterOffset, GLuint *counterDataSize, GLuint *counterTypeEnum, GLuint *counterDataTypeEnum, GLuint64 *rawCounterMaxValue, const char* file, int line);
	static PFNGLGETPERFCOUNTERINFOINTELPROC gl_GetPerfCounterInfoINTEL;

	#define glGetPerfQueryDataINTEL(...) gl::GetPerfQueryDataINTEL( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetPerfQueryDataINTEL (GLuint queryHandle, GLuint flags, GLsizei dataSize, GLvoid *data, GLuint *bytesWritten, const char* file, int line);
	static PFNGLGETPERFQUERYDATAINTELPROC gl_GetPerfQueryDataINTEL;

	#define glGetPerfQueryIdByNameINTEL(...) gl::GetPerfQueryIdByNameINTEL( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetPerfQueryIdByNameINTEL (GLchar *queryName, GLuint *queryId, const char* file, int line);
	static PFNGLGETPERFQUERYIDBYNAMEINTELPROC gl_GetPerfQueryIdByNameINTEL;

	#define glGetPerfQueryInfoINTEL(...) gl::GetPerfQueryInfoINTEL( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetPerfQueryInfoINTEL (GLuint queryId, GLuint queryNameLength, GLchar *queryName, GLuint *dataSize, GLuint *noCounters, GLuint *noInstances, GLuint *capsMask, const char* file, int line);
	static PFNGLGETPERFQUERYINFOINTELPROC gl_GetPerfQueryInfoINTEL;

	// GL_KHR_blend_equation_advanced
	#define glBlendBarrierKHR() gl::BlendBarrierKHR( __FILE__,__LINE__ )
	static void BlendBarrierKHR (const char* file, int line);
	static PFNGLBLENDBARRIERKHRPROC gl_BlendBarrierKHR;

	// GL_KHR_debug
	#define glDebugMessageControlKHR(...) gl::DebugMessageControlKHR( __VA_ARGS__, __FILE__,__LINE__ )
	static void DebugMessageControlKHR (GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled, const char* file, int line);
	static PFNGLDEBUGMESSAGECONTROLKHRPROC gl_DebugMessageControlKHR;

	#define glDebugMessageInsertKHR(...) gl::DebugMessageInsertKHR( __VA_ARGS__, __FILE__,__LINE__ )
	static void DebugMessageInsertKHR(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *buf, const char* file, int line);
	static PFNGLDEBUGMESSAGEINSERTKHRPROC gl_DebugMessageInsertKHR;

	#define glDebugMessageCallbackKHR(...) gl::DebugMessageCallbackKHR( __VA_ARGS__, __FILE__,__LINE__ )
	static void DebugMessageCallbackKHR(GLDEBUGPROCKHR callback, const void *userParam, const char* file, int line);
	static PFNGLDEBUGMESSAGECALLBACKKHRPROC gl_DebugMessageCallbackKHR;

	#define glGetDebugMessageLogKHR(...) gl::GetDebugMessageLogKHR( __VA_ARGS__, __FILE__,__LINE__ )
	static GLuint GetDebugMessageLogKHR (GLuint count, GLsizei bufSize, GLenum *sources, GLenum *types, GLuint *ids, GLenum *severities, GLsizei *lengths, GLchar *messageLog, const char* file, int line);
	static PFNGLGETDEBUGMESSAGELOGKHRPROC gl_GetDebugMessageLogKHR;

	#define glGetObjectLabelKHR(...) gl::GetObjectLabelKHR( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetObjectLabelKHR (GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length, GLchar *label, const char* file, int line);
	static PFNGLGETOBJECTLABELKHRPROC gl_GetObjectLabelKHR;

	#define glGetObjectPtrLabelKHR(...) gl::GetObjectPtrLabelKHR( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetObjectPtrLabelKHR (const void *ptr, GLsizei bufSize, GLsizei *length, GLchar *label, const char* file, int line);
	static PFNGLGETOBJECTPTRLABELKHRPROC gl_GetObjectPtrLabelKHR;

	#define glGetPointervKHR(...) gl::GetPointervKHR( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetPointervKHR (GLenum pname, void **params, const char* file, int line);
	static PFNGLGETPOINTERVKHRPROC gl_GetPointervKHR;

	#define glObjectLabelKHR(...) gl::ObjectLabelKHR( __VA_ARGS__, __FILE__,__LINE__ )
	static void ObjectLabelKHR (GLenum identifier, GLuint name, GLsizei length, const GLchar *label, const char* file, int line);
	static PFNGLOBJECTLABELKHRPROC gl_ObjectLabelKHR;

	#define glObjectPtrLabelKHR(...) gl::ObjectPtrLabelKHR( __VA_ARGS__, __FILE__,__LINE__ )
	static void ObjectPtrLabelKHR (const void *ptr, GLsizei length, const GLchar *label, const char* file, int line);
	static PFNGLOBJECTPTRLABELKHRPROC gl_ObjectPtrLabelKHR;

	#define glPopDebugGroupKHR() gl::PopDebugGroupKHR( __FILE__,__LINE__ )
	static void PopDebugGroupKHR (const char* file, int line);
	static PFNGLPOPDEBUGGROUPKHRPROC gl_PopDebugGroupKHR;

	#define glPushDebugGroupKHR(...) gl::PushDebugGroupKHR( __VA_ARGS__, __FILE__,__LINE__ )
	static void PushDebugGroupKHR (GLenum source, GLuint id, GLsizei length, const GLchar *message, const char* file, int line);
	static PFNGLPUSHDEBUGGROUPKHRPROC gl_PushDebugGroupKHR;

	// GL_KHR_robustness
	#define glGetGraphicsResetStatusKHR() gl::GetGraphicsResetStatusKHR(__FILE__,__LINE__ )
	static GLenum GetGraphicsResetStatusKHR (const char* file, int line);
	static PFNGLGETGRAPHICSRESETSTATUSKHRPROC gl_GetGraphicsResetStatusKHR;

	#define glGetnUniformfvKHR(...) gl::GetnUniformfvKHR( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetnUniformfvKHR (GLuint program, GLint location, GLsizei bufSize, GLfloat *params, const char* file, int line);
	static PFNGLGETNUNIFORMFVKHRPROC gl_GetnUniformfvKHR;

	#define glGetnUniformivKHR(...) gl::GetnUniformivKHR( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetnUniformivKHR (GLuint program, GLint location, GLsizei bufSize, GLint *params, const char* file, int line);
	static PFNGLGETNUNIFORMIVKHRPROC gl_GetnUniformivKHR;

	#define glGetnUniformuivKHR(...) gl::GetnUniformuivKHR( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetnUniformuivKHR (GLuint program, GLint location, GLsizei bufSize, GLuint *params, const char* file, int line);
	static PFNGLGETNUNIFORMUIVKHRPROC gl_GetnUniformuivKHR;

	#define glReadnPixelsKHR(...) gl::ReadnPixelsKHR( __VA_ARGS__, __FILE__,__LINE__ )
	static void ReadnPixelsKHR (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLsizei bufSize, void *data, const char* file, int line);
	static PFNGLREADNPIXELSKHRPROC gl_ReadnPixelsKHR;

	// GL_NV_bindless_texture
	#define glGetImageHandleNV(...) gl::GetImageHandleNV( __VA_ARGS__, __FILE__,__LINE__ )
	static GLuint64 GetImageHandleNV (GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum format, const char* file, int line);
	static PFNGLGETIMAGEHANDLENVPROC gl_GetImageHandleNV;

	#define glGetTextureHandleNV(...) gl::GetTextureHandleNV( __VA_ARGS__, __FILE__,__LINE__ )
	static GLuint64 GetTextureHandleNV (GLuint texture, const char* file, int line);
	static PFNGLGETTEXTUREHANDLENVPROC gl_GetTextureHandleNV;

	#define glGetTextureSamplerHandleNV(...) gl::GetTextureSamplerHandleNV( __VA_ARGS__, __FILE__,__LINE__ )
	static GLuint64 GetTextureSamplerHandleNV (GLuint texture, GLuint sampler, const char* file, int line);
	static PFNGLGETTEXTURESAMPLERHANDLENVPROC gl_GetTextureSamplerHandleNV;

	#define glIsImageHandleResidentNV(...) gl::IsImageHandleResidentNV( __VA_ARGS__, __FILE__,__LINE__ )
	static GLboolean IsImageHandleResidentNV (GLuint64 handle, const char* file, int line);
	static PFNGLISIMAGEHANDLERESIDENTNVPROC gl_IsImageHandleResidentNV;

	#define glIsTextureHandleResidentNV(...) gl::IsTextureHandleResidentNV( __VA_ARGS__, __FILE__,__LINE__ )
	static GLboolean IsTextureHandleResidentNV (GLuint64 handle, const char* file, int line);
	static PFNGLISTEXTUREHANDLERESIDENTNVPROC gl_IsTextureHandleResidentNV;

	#define glMakeImageHandleNonResidentNV(...) gl::MakeImageHandleNonResidentNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void MakeImageHandleNonResidentNV (GLuint64 handle, const char* file, int line);
	static PFNGLMAKEIMAGEHANDLENONRESIDENTNVPROC gl_MakeImageHandleNonResidentNV;

	#define glMakeImageHandleResidentNV(...) gl::MakeImageHandleResidentNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void MakeImageHandleResidentNV (GLuint64 handle, GLenum access, const char* file, int line);
	static PFNGLMAKEIMAGEHANDLERESIDENTNVPROC gl_MakeImageHandleResidentNV;

	#define glMakeTextureHandleNonResidentNV(...) gl::MakeTextureHandleNonResidentNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void MakeTextureHandleNonResidentNV (GLuint64 handle, const char* file, int line);
	static PFNGLMAKETEXTUREHANDLENONRESIDENTNVPROC gl_MakeTextureHandleNonResidentNV;

	#define glMakeTextureHandleResidentNV(...) gl::MakeTextureHandleResidentNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void MakeTextureHandleResidentNV (GLuint64 handle, const char* file, int line);
	static PFNGLMAKETEXTUREHANDLERESIDENTNVPROC gl_MakeTextureHandleResidentNV;

	#define glProgramUniformHandleui64NV(...) gl::ProgramUniformHandleui64NV( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniformHandleui64NV (GLuint program, GLint location, GLuint64 value, const char* file, int line);
	static PFNGLPROGRAMUNIFORMHANDLEUI64NVPROC gl_ProgramUniformHandleui64NV;

	#define glProgramUniformHandleui64vNV(...) gl::ProgramUniformHandleui64vNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramUniformHandleui64vNV (GLuint program, GLint location, GLsizei count, const GLuint64 *values, const char* file, int line);
	static PFNGLPROGRAMUNIFORMHANDLEUI64VNVPROC gl_ProgramUniformHandleui64vNV;

	#define glUniformHandleui64NV(...) gl::UniformHandleui64NV( __VA_ARGS__, __FILE__,__LINE__ )
	static void UniformHandleui64NV (GLint location, GLuint64 value, const char* file, int line);
	static PFNGLUNIFORMHANDLEUI64NVPROC gl_UniformHandleui64NV;

	#define glUniformHandleui64vNV(...) gl::UniformHandleui64vNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void UniformHandleui64vNV (GLint location, GLsizei count, const GLuint64 *value, const char* file, int line);
	static PFNGLUNIFORMHANDLEUI64VNVPROC gl_UniformHandleui64vNV;

	// GL_NV_blend_equation_advanced
	#define glBlendBarrierNV() gl::BlendBarrierNV(__FILE__,__LINE__ )
	static void BlendBarrierNV (const char* file, int line);
	static PFNGLBLENDBARRIERNVPROC gl_BlendBarrierNV;

	#define glBlendParameteriNV(...) gl::BlendParameteriNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void BlendParameteriNV (GLenum pname, GLint value, const char* file, int line);
	static PFNGLBLENDPARAMETERINVPROC gl_BlendParameteriNV;

	// GL_NV_conditional_render
	#define glBeginConditionalRenderNV(...) gl::BeginConditionalRenderNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void BeginConditionalRenderNV (GLuint id, GLenum mode, const char* file, int line);
	static PFNGLBEGINCONDITIONALRENDERNVPROC gl_BeginConditionalRenderNV;

	#define glEndConditionalRenderNV() gl::EndConditionalRenderNV( __FILE__,__LINE__ )
	static void EndConditionalRenderNV (const char* file, int line);
	static PFNGLENDCONDITIONALRENDERNVPROC gl_EndConditionalRenderNV;

	// GL_NV_conservative_raster
	#define glSubpixelPrecisionBiasNV(...) gl::SubpixelPrecisionBiasNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void SubpixelPrecisionBiasNV (GLuint xbits, GLuint ybits, const char* file, int line);
	static PFNGLSUBPIXELPRECISIONBIASNVPROC gl_SubpixelPrecisionBiasNV;

	// GL_NV_copy_buffer
	#define glCopyBufferSubDataNV(...) gl::CopyBufferSubDataNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void CopyBufferSubDataNV (GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size, const char* file, int line);
	static PFNGLCOPYBUFFERSUBDATANVPROC gl_CopyBufferSubDataNV;

	// GL_NV_coverage_sample
	#define glCoverageMaskNV(...) gl::CoverageMaskNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void CoverageMaskNV (GLboolean mask, const char* file, int line);
	static PFNGLCOVERAGEMASKNVPROC gl_CoverageMaskNV;

	#define glCoverageOperationNV(...) gl::CoverageOperationNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void CoverageOperationNV (GLenum operation, const char* file, int line);
	static PFNGLCOVERAGEOPERATIONNVPROC gl_CoverageOperationNV;

	// GL_NV_draw_buffers
	#define glDrawBuffersNV(...) gl::DrawBuffersNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void DrawBuffersNV (GLsizei n, const GLenum *bufs, const char* file, int line);
	static PFNGLDRAWBUFFERSNVPROC gl_DrawBuffersNV;

	// GL_NV_draw_instanced
	#define glDrawArraysInstancedNV(...) gl::DrawArraysInstancedNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void DrawArraysInstancedNV (GLenum mode, GLint first, GLsizei count, GLsizei primcount, const char* file, int line);
	static PFNGLDRAWARRAYSINSTANCEDNVPROC gl_DrawArraysInstancedNV;

	#define glDrawElementsInstancedNV(...) gl::DrawElementsInstancedNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void DrawElementsInstancedNV (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei primcount, const char* file, int line);
	static PFNGLDRAWELEMENTSINSTANCEDNVPROC gl_DrawElementsInstancedNV;

	// GL_NV_fence
	#define glDeleteFencesNV(...) gl::DeleteFencesNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void DeleteFencesNV (GLsizei n, const GLuint *fences, const char* file, int line);
	static PFNGLDELETEFENCESNVPROC gl_DeleteFencesNV;

	#define glFinishFenceNV(...) gl::FinishFenceNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void FinishFenceNV (GLuint fence, const char* file, int line);
	static PFNGLFINISHFENCENVPROC gl_FinishFenceNV;

	#define glGenFencesNV(...) gl::GenFencesNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void GenFencesNV (GLsizei n, GLuint *fences, const char* file, int line);
	static PFNGLGENFENCESNVPROC gl_GenFencesNV;

	#define glGetFenceivNV(...) gl::GetFenceivNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetFenceivNV (GLuint fence, GLenum pname, GLint *params, const char* file, int line);
	static PFNGLGETFENCEIVNVPROC gl_GetFenceivNV;

	#define glIsFenceNV(...) gl::IsFenceNV( __VA_ARGS__, __FILE__,__LINE__ )
	static GLboolean IsFenceNV (GLuint fence, const char* file, int line);
	static PFNGLISFENCENVPROC gl_IsFenceNV;

	#define glSetFenceNV(...) gl::SetFenceNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void SetFenceNV (GLuint fence, GLenum condition, const char* file, int line);
	static PFNGLSETFENCENVPROC gl_SetFenceNV;

	#define glTestFenceNV(...) gl::TestFenceNV( __VA_ARGS__, __FILE__,__LINE__ )
	static GLboolean TestFenceNV (GLuint fence, const char* file, int line);
	static PFNGLTESTFENCENVPROC gl_TestFenceNV;

	// GL_NV_fragment_coverage_to_color
	#define glFragmentCoverageColorNV(...) gl::FragmentCoverageColorNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void FragmentCoverageColorNV (GLuint color, const char* file, int line);
	static PFNGLFRAGMENTCOVERAGECOLORNVPROC gl_FragmentCoverageColorNV;

	// GL_NV_framebuffer_blit
	#define glBlitFramebufferNV(...) gl::BlitFramebufferNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void BlitFramebufferNV (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter, const char* file, int line);
	static PFNGLBLITFRAMEBUFFERNVPROC gl_BlitFramebufferNV;

	// GL_NV_framebuffer_mixed_samples
	#define glCoverageModulationNV(...) gl::CoverageModulationNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void CoverageModulationNV (GLenum components, const char* file, int line);
	static PFNGLCOVERAGEMODULATIONNVPROC gl_CoverageModulationNV;

	#define glCoverageModulationTableNV(...) gl::CoverageModulationTableNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void CoverageModulationTableNV (GLsizei n, const GLfloat *v, const char* file, int line);
	static PFNGLCOVERAGEMODULATIONTABLENVPROC gl_CoverageModulationTableNV;

	#define glGetCoverageModulationTableNV(...) gl::GetCoverageModulationTableNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetCoverageModulationTableNV (GLsizei bufsize, GLfloat *v, const char* file, int line);
	static PFNGLGETCOVERAGEMODULATIONTABLENVPROC gl_GetCoverageModulationTableNV;

	// GL_NV_framebuffer_multisample
	#define glRenderbufferStorageMultisampleNV(...) gl::RenderbufferStorageMultisampleNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void RenderbufferStorageMultisampleNV (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, const char* file, int line);
	static PFNGLRENDERBUFFERSTORAGEMULTISAMPLENVPROC gl_RenderbufferStorageMultisampleNV;

	// GL_NV_instanced_arrays
	#define glVertexAttribDivisorNV(...) gl::VertexAttribDivisorNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void VertexAttribDivisorNV (GLuint index, GLuint divisor, const char* file, int line);
	static PFNGLVERTEXATTRIBDIVISORNVPROC gl_VertexAttribDivisorNV;

	// GL_NV_internalformat_sample_query
	#define glGetInternalformatSampleivNV(...) gl::GetInternalformatSampleivNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetInternalformatSampleivNV (GLenum target, GLenum internalformat, GLsizei samples, GLenum pname, GLsizei bufSize, GLint *params, const char* file, int line);
	static PFNGLGETINTERNALFORMATSAMPLEIVNVPROC gl_GetInternalformatSampleivNV;

	// GL_NV_non_square_matrices
	#define glUniformMatrix2x3fvNV(...) gl::UniformMatrix2x3fvNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void UniformMatrix2x3fvNV (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line);
	static PFNGLUNIFORMMATRIX2X3FVNVPROC gl_UniformMatrix2x3fvNV;

	#define glUniformMatrix2x4fvNV(...) gl::UniformMatrix2x4fvNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void UniformMatrix2x4fvNV (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line);
	static PFNGLUNIFORMMATRIX2X4FVNVPROC gl_UniformMatrix2x4fvNV;

	#define glUniformMatrix3x2fvNV(...) gl::UniformMatrix3x2fvNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void UniformMatrix3x2fvNV (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line);
	static PFNGLUNIFORMMATRIX3X2FVNVPROC gl_UniformMatrix3x2fvNV;

	#define glUniformMatrix3x4fvNV(...) gl::UniformMatrix3x4fvNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void UniformMatrix3x4fvNV (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line);
	static PFNGLUNIFORMMATRIX3X4FVNVPROC gl_UniformMatrix3x4fvNV;

	#define glUniformMatrix4x2fvNV(...) gl::UniformMatrix4x2fvNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void UniformMatrix4x2fvNV (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line);
	static PFNGLUNIFORMMATRIX4X2FVNVPROC gl_UniformMatrix4x2fvNV;

	#define glUniformMatrix4x3fvNV(...) gl::UniformMatrix4x3fvNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void UniformMatrix4x3fvNV (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value, const char* file, int line);
	static PFNGLUNIFORMMATRIX4X3FVNVPROC gl_UniformMatrix4x3fvNV;

	// GL_NV_path_rendering
	#define glCopyPathNV(...) gl::CopyPathNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void CopyPathNV (GLuint resultPath, GLuint srcPath, const char* file, int line);
	static PFNGLCOPYPATHNVPROC gl_CopyPathNV;

	#define glCoverFillPathInstancedNV(...) gl::CoverFillPathInstancedNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void CoverFillPathInstancedNV (GLsizei numPaths, GLenum pathNameType, const void *paths, GLuint pathBase, GLenum coverMode, GLenum transformType, const GLfloat *transformValues, const char* file, int line);
	static PFNGLCOVERFILLPATHINSTANCEDNVPROC gl_CoverFillPathInstancedNV;

	#define glCoverFillPathNV(...) gl::CoverFillPathNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void CoverFillPathNV (GLuint path, GLenum coverMode, const char* file, int line);
	static PFNGLCOVERFILLPATHNVPROC gl_CoverFillPathNV;

	#define glCoverStrokePathInstancedNV(...) gl::CoverStrokePathInstancedNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void CoverStrokePathInstancedNV (GLsizei numPaths, GLenum pathNameType, const void *paths, GLuint pathBase, GLenum coverMode, GLenum transformType, const GLfloat *transformValues, const char* file, int line);
	static PFNGLCOVERSTROKEPATHINSTANCEDNVPROC gl_CoverStrokePathInstancedNV;

	#define glCoverStrokePathNV(...) gl::CoverStrokePathNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void CoverStrokePathNV (GLuint path, GLenum coverMode, const char* file, int line);
	static PFNGLCOVERSTROKEPATHNVPROC gl_CoverStrokePathNV;

	#define glDeletePathsNV(...) gl::DeletePathsNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void DeletePathsNV (GLuint path, GLsizei range, const char* file, int line);
	static PFNGLDELETEPATHSNVPROC gl_DeletePathsNV;

	#define glGenPathsNV(...) gl::GenPathsNV( __VA_ARGS__, __FILE__,__LINE__ )
	static GLuint GenPathsNV (GLsizei range, const char* file, int line);
	static PFNGLGENPATHSNVPROC gl_GenPathsNV;

	#define glGetPathCommandsNV(...) gl::GetPathCommandsNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetPathCommandsNV (GLuint path, GLubyte *commands, const char* file, int line);
	static PFNGLGETPATHCOMMANDSNVPROC gl_GetPathCommandsNV;

	#define glGetPathCoordsNV(...) gl::GetPathCoordsNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetPathCoordsNV (GLuint path, GLfloat *coords, const char* file, int line);
	static PFNGLGETPATHCOORDSNVPROC gl_GetPathCoordsNV;

	#define glGetPathDashArrayNV(...) gl::GetPathDashArrayNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetPathDashArrayNV (GLuint path, GLfloat *dashArray, const char* file, int line);
	static PFNGLGETPATHDASHARRAYNVPROC gl_GetPathDashArrayNV;

	#define glGetPathLengthNV(...) gl::GetPathLengthNV( __VA_ARGS__, __FILE__,__LINE__ )
	static GLfloat GetPathLengthNV (GLuint path, GLsizei startSegment, GLsizei numSegments, const char* file, int line);
	static PFNGLGETPATHLENGTHNVPROC gl_GetPathLengthNV;

	#define glGetPathMetricRangeNV(...) gl::GetPathMetricRangeNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetPathMetricRangeNV (GLbitfield metricQueryMask, GLuint firstPathName, GLsizei numPaths, GLsizei stride, GLfloat *metrics, const char* file, int line);
	static PFNGLGETPATHMETRICRANGENVPROC gl_GetPathMetricRangeNV;

	#define glGetPathMetricsNV(...) gl::GetPathMetricsNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetPathMetricsNV (GLbitfield metricQueryMask, GLsizei numPaths, GLenum pathNameType, const void *paths, GLuint pathBase, GLsizei stride, GLfloat *metrics, const char* file, int line);
	static PFNGLGETPATHMETRICSNVPROC gl_GetPathMetricsNV;

	#define glGetPathParameterfvNV(...) gl::GetPathParameterfvNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetPathParameterfvNV (GLuint path, GLenum pname, GLfloat *value, const char* file, int line);
	static PFNGLGETPATHPARAMETERFVNVPROC gl_GetPathParameterfvNV;

	#define glGetPathParameterivNV(...) gl::GetPathParameterivNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetPathParameterivNV (GLuint path, GLenum pname, GLint *value, const char* file, int line);
	static PFNGLGETPATHPARAMETERIVNVPROC gl_GetPathParameterivNV;

	#define glGetPathSpacingNV(...) gl::GetPathSpacingNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetPathSpacingNV (GLenum pathListMode, GLsizei numPaths, GLenum pathNameType, const void *paths, GLuint pathBase, GLfloat advanceScale, GLfloat kerningScale, GLenum transformType, GLfloat *returnedSpacing, const char* file, int line);
	static PFNGLGETPATHSPACINGNVPROC gl_GetPathSpacingNV;

	#define glGetProgramResourcefvNV(...) gl::GetProgramResourcefvNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetProgramResourcefvNV (GLuint program, GLenum programInterface, GLuint index, GLsizei propCount, const GLenum *props, GLsizei bufSize, GLsizei *length, GLfloat *params, const char* file, int line);
	static PFNGLGETPROGRAMRESOURCEFVNVPROC gl_GetProgramResourcefvNV;

	#define glInterpolatePathsNV(...) gl::InterpolatePathsNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void InterpolatePathsNV (GLuint resultPath, GLuint pathA, GLuint pathB, GLfloat weight, const char* file, int line);
	static PFNGLINTERPOLATEPATHSNVPROC gl_InterpolatePathsNV;

	#define glIsPathNV(...) gl::IsPathNV( __VA_ARGS__, __FILE__,__LINE__ )
	static GLboolean IsPathNV (GLuint path, const char* file, int line);
	static PFNGLISPATHNVPROC gl_IsPathNV;

	#define glIsPointInFillPathNV(...) gl::IsPointInFillPathNV( __VA_ARGS__, __FILE__,__LINE__ )
	static GLboolean IsPointInFillPathNV (GLuint path, GLuint mask, GLfloat x, GLfloat y, const char* file, int line);
	static PFNGLISPOINTINFILLPATHNVPROC gl_IsPointInFillPathNV;

	#define glIsPointInStrokePathNV(...) gl::IsPointInStrokePathNV( __VA_ARGS__, __FILE__,__LINE__ )
	static GLboolean IsPointInStrokePathNV (GLuint path, GLfloat x, GLfloat y, const char* file, int line);
	static PFNGLISPOINTINSTROKEPATHNVPROC gl_IsPointInStrokePathNV;

	#define glMatrixLoad3x2fNV(...) gl::MatrixLoad3x2fNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void MatrixLoad3x2fNV (GLenum matrixMode, const GLfloat *m, const char* file, int line);
	static PFNGLMATRIXLOAD3X2FNVPROC gl_MatrixLoad3x2fNV;

	#define glMatrixLoad3x3fNV(...) gl::MatrixLoad3x3fNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void MatrixLoad3x3fNV (GLenum matrixMode, const GLfloat *m, const char* file, int line);
	static PFNGLMATRIXLOAD3X3FNVPROC gl_MatrixLoad3x3fNV;

	#define glMatrixLoadTranspose3x3fNV(...) gl::MatrixLoadTranspose3x3fNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void MatrixLoadTranspose3x3fNV (GLenum matrixMode, const GLfloat *m, const char* file, int line);
	static PFNGLMATRIXLOADTRANSPOSE3X3FNVPROC gl_MatrixLoadTranspose3x3fNV;

	#define glMatrixMult3x2fNV(...) gl::MatrixMult3x2fNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void MatrixMult3x2fNV (GLenum matrixMode, const GLfloat *m, const char* file, int line);
	static PFNGLMATRIXMULT3X2FNVPROC gl_MatrixMult3x2fNV;

	#define glMatrixMult3x3fNV(...) gl::MatrixMult3x3fNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void MatrixMult3x3fNV (GLenum matrixMode, const GLfloat *m, const char* file, int line);
	static PFNGLMATRIXMULT3X3FNVPROC gl_MatrixMult3x3fNV;

	#define glMatrixMultTranspose3x3fNV(...) gl::MatrixMultTranspose3x3fNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void MatrixMultTranspose3x3fNV (GLenum matrixMode, const GLfloat *m, const char* file, int line);
	static PFNGLMATRIXMULTTRANSPOSE3X3FNVPROC gl_MatrixMultTranspose3x3fNV;

	#define glPathCommandsNV(...) gl::PathCommandsNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void PathCommandsNV (GLuint path, GLsizei numCommands, const GLubyte *commands, GLsizei numCoords, GLenum coordType, const void *coords, const char* file, int line);
	static PFNGLPATHCOMMANDSNVPROC gl_PathCommandsNV;

	#define glPathCoordsNV(...) gl::PathCoordsNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void PathCoordsNV (GLuint path, GLsizei numCoords, GLenum coordType, const void *coords, const char* file, int line);
	static PFNGLPATHCOORDSNVPROC gl_PathCoordsNV;

	#define glPathCoverDepthFuncNV(...) gl::PathCoverDepthFuncNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void PathCoverDepthFuncNV (GLenum func, const char* file, int line);
	static PFNGLPATHCOVERDEPTHFUNCNVPROC gl_PathCoverDepthFuncNV;

	#define glPathDashArrayNV(...) gl::PathDashArrayNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void PathDashArrayNV (GLuint path, GLsizei dashCount, const GLfloat *dashArray, const char* file, int line);
	static PFNGLPATHDASHARRAYNVPROC gl_PathDashArrayNV;

	#define glPathGlyphIndexArrayNV(...) gl::PathGlyphIndexArrayNV( __VA_ARGS__, __FILE__,__LINE__ )
	static GLenum PathGlyphIndexArrayNV (GLuint firstPathName, GLenum fontTarget, const void *fontName, GLbitfield fontStyle, GLuint firstGlyphIndex, GLsizei numGlyphs, GLuint pathParameterTemplate, GLfloat emScale, const char* file, int line);
	static PFNGLPATHGLYPHINDEXARRAYNVPROC gl_PathGlyphIndexArrayNV;

	#define glPathGlyphIndexRangeNV(...) gl::PathGlyphIndexRangeNV( __VA_ARGS__, __FILE__,__LINE__ )
	static GLenum PathGlyphIndexRangeNV (GLenum fontTarget, const void *fontName, GLbitfield fontStyle, GLuint pathParameterTemplate, GLfloat emScale, GLuint baseAndCount[2], const char* file, int line);
	static PFNGLPATHGLYPHINDEXRANGENVPROC gl_PathGlyphIndexRangeNV;

	#define glPathGlyphRangeNV(...) gl::PathGlyphRangeNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void PathGlyphRangeNV (GLuint firstPathName, GLenum fontTarget, const void *fontName, GLbitfield fontStyle, GLuint firstGlyph, GLsizei numGlyphs, GLenum handleMissingGlyphs, GLuint pathParameterTemplate, GLfloat emScale, const char* file, int line);
	static PFNGLPATHGLYPHRANGENVPROC gl_PathGlyphRangeNV;

	#define glPathGlyphsNV(...) gl::PathGlyphsNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void PathGlyphsNV (GLuint firstPathName, GLenum fontTarget, const void *fontName, GLbitfield fontStyle, GLsizei numGlyphs, GLenum type, const void *charcodes, GLenum handleMissingGlyphs, GLuint pathParameterTemplate, GLfloat emScale, const char* file, int line);
	static PFNGLPATHGLYPHSNVPROC gl_PathGlyphsNV;

	#define glPathMemoryGlyphIndexArrayNV(...) gl::PathMemoryGlyphIndexArrayNV( __VA_ARGS__, __FILE__,__LINE__ )
	static GLenum PathMemoryGlyphIndexArrayNV (GLuint firstPathName, GLenum fontTarget, GLsizeiptr fontSize, const void *fontData, GLsizei faceIndex, GLuint firstGlyphIndex, GLsizei numGlyphs, GLuint pathParameterTemplate, GLfloat emScale, const char* file, int line);
	static PFNGLPATHMEMORYGLYPHINDEXARRAYNVPROC gl_PathMemoryGlyphIndexArrayNV;

	#define glPathParameterfNV(...) gl::PathParameterfNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void PathParameterfNV (GLuint path, GLenum pname, GLfloat value, const char* file, int line);
	static PFNGLPATHPARAMETERFNVPROC gl_PathParameterfNV;

	#define glPathParameterfvNV(...) gl::PathParameterfvNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void PathParameterfvNV (GLuint path, GLenum pname, const GLfloat *value, const char* file, int line);
	static PFNGLPATHPARAMETERFVNVPROC gl_PathParameterfvNV;

	#define glPathParameteriNV(...) gl::PathParameteriNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void PathParameteriNV (GLuint path, GLenum pname, GLint value, const char* file, int line);
	static PFNGLPATHPARAMETERINVPROC gl_PathParameteriNV;

	#define glPathParameterivNV(...) gl::PathParameterivNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void PathParameterivNV (GLuint path, GLenum pname, const GLint *value, const char* file, int line);
	static PFNGLPATHPARAMETERIVNVPROC gl_PathParameterivNV;

	#define glPathStencilDepthOffsetNV(...) gl::PathStencilDepthOffsetNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void PathStencilDepthOffsetNV (GLfloat factor, GLfloat units, const char* file, int line);
	static PFNGLPATHSTENCILDEPTHOFFSETNVPROC gl_PathStencilDepthOffsetNV;

	#define glPathStencilFuncNV(...) gl::PathStencilFuncNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void PathStencilFuncNV (GLenum func, GLint ref, GLuint mask, const char* file, int line);
	static PFNGLPATHSTENCILFUNCNVPROC gl_PathStencilFuncNV;

	#define glPathStringNV(...) gl::PathStringNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void PathStringNV (GLuint path, GLenum format, GLsizei length, const void *pathString, const char* file, int line);
	static PFNGLPATHSTRINGNVPROC gl_PathStringNV;

	#define glPathSubCommandsNV(...) gl::PathSubCommandsNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void PathSubCommandsNV (GLuint path, GLsizei commandStart, GLsizei commandsToDelete, GLsizei numCommands, const GLubyte *commands, GLsizei numCoords, GLenum coordType, const void *coords, const char* file, int line);
	static PFNGLPATHSUBCOMMANDSNVPROC gl_PathSubCommandsNV;

	#define glPathSubCoordsNV(...) gl::PathSubCoordsNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void PathSubCoordsNV (GLuint path, GLsizei coordStart, GLsizei numCoords, GLenum coordType, const void *coords, const char* file, int line);
	static PFNGLPATHSUBCOORDSNVPROC gl_PathSubCoordsNV;

	#define glPointAlongPathNV(...) gl::PointAlongPathNV( __VA_ARGS__, __FILE__,__LINE__ )
	static GLboolean PointAlongPathNV (GLuint path, GLsizei startSegment, GLsizei numSegments, GLfloat distance, GLfloat *x, GLfloat *y, GLfloat *tangentX, GLfloat *tangentY, const char* file, int line);
	static PFNGLPOINTALONGPATHNVPROC gl_PointAlongPathNV;

	#define glProgramPathFragmentInputGenNV(...) gl::ProgramPathFragmentInputGenNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramPathFragmentInputGenNV (GLuint program, GLint location, GLenum genMode, GLint components, const GLfloat *coeffs, const char* file, int line);
	static PFNGLPROGRAMPATHFRAGMENTINPUTGENNVPROC gl_ProgramPathFragmentInputGenNV;

	#define glStencilFillPathInstancedNV(...) gl::StencilFillPathInstancedNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void StencilFillPathInstancedNV (GLsizei numPaths, GLenum pathNameType, const void *paths, GLuint pathBase, GLenum fillMode, GLuint mask, GLenum transformType, const GLfloat *transformValues, const char* file, int line);
	static PFNGLSTENCILFILLPATHINSTANCEDNVPROC gl_StencilFillPathInstancedNV;

	#define glStencilFillPathNV(...) gl::StencilFillPathNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void StencilFillPathNV (GLuint path, GLenum fillMode, GLuint mask, const char* file, int line);
	static PFNGLSTENCILFILLPATHNVPROC gl_StencilFillPathNV;

	#define glStencilStrokePathInstancedNV(...) gl::StencilStrokePathInstancedNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void StencilStrokePathInstancedNV (GLsizei numPaths, GLenum pathNameType, const void *paths, GLuint pathBase, GLint reference, GLuint mask, GLenum transformType, const GLfloat *transformValues, const char* file, int line);
	static PFNGLSTENCILSTROKEPATHINSTANCEDNVPROC gl_StencilStrokePathInstancedNV;

	#define glStencilStrokePathNV(...) gl::StencilStrokePathNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void StencilStrokePathNV (GLuint path, GLint reference, GLuint mask, const char* file, int line);
	static PFNGLSTENCILSTROKEPATHNVPROC gl_StencilStrokePathNV;

	#define glStencilThenCoverFillPathInstancedNV(...) gl::StencilThenCoverFillPathInstancedNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void StencilThenCoverFillPathInstancedNV (GLsizei numPaths, GLenum pathNameType, const void *paths, GLuint pathBase, GLenum fillMode, GLuint mask, GLenum coverMode, GLenum transformType, const GLfloat *transformValues, const char* file, int line);
	static PFNGLSTENCILTHENCOVERFILLPATHINSTANCEDNVPROC gl_StencilThenCoverFillPathInstancedNV;

	#define glStencilThenCoverFillPathNV(...) gl::StencilThenCoverFillPathNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void StencilThenCoverFillPathNV (GLuint path, GLenum fillMode, GLuint mask, GLenum coverMode, const char* file, int line);
	static PFNGLSTENCILTHENCOVERFILLPATHNVPROC gl_StencilThenCoverFillPathNV;

	#define glStencilThenCoverStrokePathInstancedNV(...) gl::StencilThenCoverStrokePathInstancedNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void StencilThenCoverStrokePathInstancedNV (GLsizei numPaths, GLenum pathNameType, const void *paths, GLuint pathBase, GLint reference, GLuint mask, GLenum coverMode, GLenum transformType, const GLfloat *transformValues, const char* file, int line);
	static PFNGLSTENCILTHENCOVERSTROKEPATHINSTANCEDNVPROC gl_StencilThenCoverStrokePathInstancedNV;

	#define glStencilThenCoverStrokePathNV(...) gl::StencilThenCoverStrokePathNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void StencilThenCoverStrokePathNV (GLuint path, GLint reference, GLuint mask, GLenum coverMode, const char* file, int line);
	static PFNGLSTENCILTHENCOVERSTROKEPATHNVPROC gl_StencilThenCoverStrokePathNV;

	#define glTransformPathNV(...) gl::TransformPathNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void TransformPathNV (GLuint resultPath, GLuint srcPath, GLenum transformType, const GLfloat *transformValues, const char* file, int line);
	static PFNGLTRANSFORMPATHNVPROC gl_TransformPathNV;

	#define glWeightPathsNV(...) gl::WeightPathsNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void WeightPathsNV (GLuint resultPath, GLsizei numPaths, const GLuint *paths, const GLfloat *weights, const char* file, int line);
	static PFNGLWEIGHTPATHSNVPROC gl_WeightPathsNV;

	// GL_NV_polygon_mode
	#define glPolygonModeNV(...) gl::PolygonModeNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void PolygonModeNV (GLenum face, GLenum mode, const char* file, int line);
	static PFNGLPOLYGONMODENVPROC gl_PolygonModeNV;

	// GL_NV_read_buffer
	#define glReadBufferNV(...) gl::ReadBufferNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void ReadBufferNV (GLenum mode, const char* file, int line);
	static PFNGLREADBUFFERNVPROC gl_ReadBufferNV;

	// GL_NV_sample_locations
	#define glFramebufferSampleLocationsfvNV(...) gl::FramebufferSampleLocationsfvNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void FramebufferSampleLocationsfvNV (GLenum target, GLuint start, GLsizei count, const GLfloat *v, const char* file, int line);
	static PFNGLFRAMEBUFFERSAMPLELOCATIONSFVNVPROC gl_FramebufferSampleLocationsfvNV;

	#define glNamedFramebufferSampleLocationsfvNV(...) gl::NamedFramebufferSampleLocationsfvNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void NamedFramebufferSampleLocationsfvNV (GLuint framebuffer, GLuint start, GLsizei count, const GLfloat *v, const char* file, int line);
	static PFNGLNAMEDFRAMEBUFFERSAMPLELOCATIONSFVNVPROC gl_NamedFramebufferSampleLocationsfvNV;

	#define glResolveDepthValuesNV() gl::ResolveDepthValuesNV( __FILE__,__LINE__ )
	static void ResolveDepthValuesNV (const char* file, int line);
	static PFNGLRESOLVEDEPTHVALUESNVPROC gl_ResolveDepthValuesNV;

	// GL_NV_viewport_array
	#define glDepthRangeArrayfvNV(...) gl::DepthRangeArrayfvNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void DepthRangeArrayfvNV (GLuint first, GLsizei count, const GLfloat *v, const char* file, int line);
	static PFNGLDEPTHRANGEARRAYFVNVPROC gl_DepthRangeArrayfvNV;

	#define glDepthRangeIndexedfNV(...) gl::DepthRangeIndexedfNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void DepthRangeIndexedfNV (GLuint index, GLfloat n, GLfloat f, const char* file, int line);
	static PFNGLDEPTHRANGEINDEXEDFNVPROC gl_DepthRangeIndexedfNV;

	#define glDisableiNV(...) gl::DisableiNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void DisableiNV (GLenum target, GLuint index, const char* file, int line);
	static PFNGLDISABLEINVPROC gl_DisableiNV;

	#define glEnableiNV(...) gl::EnableiNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void EnableiNV (GLenum target, GLuint index, const char* file, int line);
	static PFNGLENABLEINVPROC gl_EnableiNV;

	#define glGetFloati_vNV(...) gl::GetFloati_vNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetFloati_vNV (GLenum target, GLuint index, GLfloat *data, const char* file, int line);
	static PFNGLGETFLOATI_VNVPROC gl_GetFloati_vNV;

	#define glIsEnablediNV(...) gl::IsEnablediNV( __VA_ARGS__, __FILE__,__LINE__ )
	static GLboolean IsEnablediNV (GLenum target, GLuint index, const char* file, int line);
	static PFNGLISENABLEDINVPROC gl_IsEnablediNV;

	#define glScissorArrayvNV(...) gl::ScissorArrayvNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void ScissorArrayvNV (GLuint first, GLsizei count, const GLint *v, const char* file, int line);
	static PFNGLSCISSORARRAYVNVPROC gl_ScissorArrayvNV;

	#define glScissorIndexedNV(...) gl::ScissorIndexedNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void ScissorIndexedNV (GLuint index, GLint left, GLint bottom, GLsizei width, GLsizei height, const char* file, int line);
	static PFNGLSCISSORINDEXEDNVPROC gl_ScissorIndexedNV;

	#define glScissorIndexedvNV(...) gl::ScissorIndexedvNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void ScissorIndexedvNV (GLuint index, const GLint *v, const char* file, int line);
	static PFNGLSCISSORINDEXEDVNVPROC gl_ScissorIndexedvNV;

	#define glViewportArrayvNV(...) gl::ViewportArrayvNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void ViewportArrayvNV (GLuint first, GLsizei count, const GLfloat *v, const char* file, int line);
	static PFNGLVIEWPORTARRAYVNVPROC gl_ViewportArrayvNV;

	#define glViewportIndexedfNV(...) gl::ViewportIndexedfNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void ViewportIndexedfNV (GLuint index, GLfloat x, GLfloat y, GLfloat w, GLfloat h, const char* file, int line);
	static PFNGLVIEWPORTINDEXEDFNVPROC gl_ViewportIndexedfNV;

	#define glViewportIndexedfvNV(...) gl::ViewportIndexedfvNV( __VA_ARGS__, __FILE__,__LINE__ )
	static void ViewportIndexedfvNV (GLuint index, const GLfloat *v, const char* file, int line);
	static PFNGLVIEWPORTINDEXEDFVNVPROC gl_ViewportIndexedfvNV;

	// GL_OES_EGL_image
	#define glEGLImageTargetRenderbufferStorageOES(...) gl::EGLImageTargetRenderbufferStorageOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void EGLImageTargetRenderbufferStorageOES (GLenum target, GLeglImageOES image, const char* file, int line);
	static PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC gl_EGLImageTargetRenderbufferStorageOES;

	#define glEGLImageTargetTexture2DOES(...) gl::EGLImageTargetTexture2DOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void EGLImageTargetTexture2DOES (GLenum target, GLeglImageOES image, const char* file, int line);
	static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC gl_EGLImageTargetTexture2DOES;

	// GL_OES_copy_image
	#define glCopyImageSubDataOES(...) gl::CopyImageSubDataOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void CopyImageSubDataOES (GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth, const char* file, int line);
	static PFNGLCOPYIMAGESUBDATAOESPROC gl_CopyImageSubDataOES;

	// GL_OES_draw_buffers_indexed
	#define glBlendEquationSeparateiOES(...) gl::BlendEquationSeparateiOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void BlendEquationSeparateiOES (GLuint buf, GLenum modeRGB, GLenum modeAlpha, const char* file, int line);
	static PFNGLBLENDEQUATIONSEPARATEIOESPROC gl_BlendEquationSeparateiOES;

	#define glBlendEquationiOES(...) gl::BlendEquationiOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void BlendEquationiOES (GLuint buf, GLenum mode, const char* file, int line);
	static PFNGLBLENDEQUATIONIOESPROC gl_BlendEquationiOES;

	#define glBlendFuncSeparateiOES(...) gl::BlendFuncSeparateiOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void BlendFuncSeparateiOES (GLuint buf, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha, const char* file, int line);
	static PFNGLBLENDFUNCSEPARATEIOESPROC gl_BlendFuncSeparateiOES;

	#define glBlendFunciOES(...) gl::BlendFunciOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void BlendFunciOES (GLuint buf, GLenum src, GLenum dst, const char* file, int line);
	static PFNGLBLENDFUNCIOESPROC gl_BlendFunciOES;

	#define glColorMaskiOES(...) gl::ColorMaskiOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void ColorMaskiOES (GLuint index, GLboolean r, GLboolean g, GLboolean b, GLboolean a, const char* file, int line);
	static PFNGLCOLORMASKIOESPROC gl_ColorMaskiOES;

	#define glDisableiOES(...) gl::DisableiOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void DisableiOES (GLenum target, GLuint index, const char* file, int line);
	static PFNGLDISABLEIOESPROC gl_DisableiOES;

	#define glEnableiOES(...) gl::EnableiOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void EnableiOES (GLenum target, GLuint index, const char* file, int line);
	static PFNGLENABLEIOESPROC gl_EnableiOES;

	#define glIsEnablediOES(...) gl::IsEnablediOES( __VA_ARGS__, __FILE__,__LINE__ )
	static GLboolean IsEnablediOES (GLenum target, GLuint index, const char* file, int line);
	static PFNGLISENABLEDIOESPROC gl_IsEnablediOES;

	// GL_OES_draw_elements_base_vertex
	#define glDrawElementsBaseVertexOES(...) gl::DrawElementsBaseVertexOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void DrawElementsBaseVertexOES (GLenum mode, GLsizei count, GLenum type, const void *indices, GLint basevertex, const char* file, int line);
	static PFNGLDRAWELEMENTSBASEVERTEXOESPROC gl_DrawElementsBaseVertexOES;

	#define glDrawElementsInstancedBaseVertexOES(...) gl::DrawElementsInstancedBaseVertexOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void DrawElementsInstancedBaseVertexOES (GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex, const char* file, int line);
	static PFNGLDRAWELEMENTSINSTANCEDBASEVERTEXOESPROC gl_DrawElementsInstancedBaseVertexOES;

	#define glDrawRangeElementsBaseVertexOES(...) gl::DrawRangeElementsBaseVertexOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void DrawRangeElementsBaseVertexOES (GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const void *indices, GLint basevertex, const char* file, int line);
	static PFNGLDRAWRANGEELEMENTSBASEVERTEXOESPROC gl_DrawRangeElementsBaseVertexOES;

	#define glMultiDrawElementsBaseVertexOES(...) gl::MultiDrawElementsBaseVertexOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void MultiDrawElementsBaseVertexOES (GLenum mode, const GLsizei *count, GLenum type, const void *const*indices, GLsizei primcount, const GLint *basevertex, const char* file, int line);
	static PFNGLMULTIDRAWELEMENTSBASEVERTEXOESPROC gl_MultiDrawElementsBaseVertexOES;

	// GL_OES_geometry_shader
	#define glFramebufferTextureOES(...) gl::FramebufferTextureOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void FramebufferTextureOES (GLenum target, GLenum attachment, GLuint texture, GLint level, const char* file, int line);
	static PFNGLFRAMEBUFFERTEXTUREOESPROC gl_FramebufferTextureOES;

	// GL_OES_get_program_binary
	#define glGetProgramBinaryOES(...) gl::GetProgramBinaryOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetProgramBinaryOES (GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, void *binary, const char* file, int line);
	static PFNGLGETPROGRAMBINARYOESPROC gl_GetProgramBinaryOES;

	#define glProgramBinaryOES(...) gl::ProgramBinaryOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void ProgramBinaryOES (GLuint program, GLenum binaryFormat, const void *binary, GLint length, const char* file, int line);
	static PFNGLPROGRAMBINARYOESPROC gl_ProgramBinaryOES;

	// GL_OES_mapbuffer
	#define glGetBufferPointervOES(...) gl::GetBufferPointervOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetBufferPointervOES (GLenum target, GLenum pname, void **params, const char* file, int line);
	static PFNGLGETBUFFERPOINTERVOESPROC gl_GetBufferPointervOES;

	#define glMapBufferOES(...) gl::MapBufferOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void MapBufferOES (GLenum target, GLenum access, const char* file, int line);
	static PFNGLMAPBUFFEROESPROC gl_MapBufferOES;

	#define glUnmapBufferOES(...) gl::UnmapBufferOES( __VA_ARGS__, __FILE__,__LINE__ )
	static GLboolean UnmapBufferOES (GLenum target, const char* file, int line);
	static PFNGLUNMAPBUFFEROESPROC gl_UnmapBufferOES;

	// GL_OES_primitive_bounding_box
	#define glPrimitiveBoundingBoxOES(...) gl::PrimitiveBoundingBoxOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void PrimitiveBoundingBoxOES (GLfloat minX, GLfloat minY, GLfloat minZ, GLfloat minW, GLfloat maxX, GLfloat maxY, GLfloat maxZ, GLfloat maxW, const char* file, int line);
	static PFNGLPRIMITIVEBOUNDINGBOXOESPROC gl_PrimitiveBoundingBoxOES;

	// GL_OES_sample_shading
	#define glMinSampleShadingOES(...) gl::MinSampleShadingOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void MinSampleShadingOES (GLfloat value, const char* file, int line);
	static PFNGLMINSAMPLESHADINGOESPROC gl_MinSampleShadingOES;

	// GL_OES_tessellation_shader
	#define glPatchParameteriOES(...) gl::PatchParameteriOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void PatchParameteriOES (GLenum pname, GLint value, const char* file, int line);
	static PFNGLPATCHPARAMETERIOESPROC gl_PatchParameteriOES;

	// GL_OES_texture_3D
	#define glCompressedTexImage3DOES(...) gl::CompressedTexImage3DOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void CompressedTexImage3DOES (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const void *data, const char* file, int line);
	static PFNGLCOMPRESSEDTEXIMAGE3DOESPROC gl_CompressedTexImage3DOES;

	#define glCompressedTexSubImage3DOES(...) gl::CompressedTexSubImage3DOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void CompressedTexSubImage3DOES (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const void *data, const char* file, int line);
	static PFNGLCOMPRESSEDTEXSUBIMAGE3DOESPROC gl_CompressedTexSubImage3DOES;

	#define glCopyTexSubImage3DOES(...) gl::CopyTexSubImage3DOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void CopyTexSubImage3DOES (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height, const char* file, int line);
	static PFNGLCOPYTEXSUBIMAGE3DOESPROC gl_CopyTexSubImage3DOES;

	#define glFramebufferTexture3DOES(...) gl::FramebufferTexture3DOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void FramebufferTexture3DOES (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset, const char* file, int line);
	static PFNGLFRAMEBUFFERTEXTURE3DOESPROC gl_FramebufferTexture3DOES;

	#define glTexImage3DOES(...) gl::TexImage3DOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void TexImage3DOES (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels, const char* file, int line);
	static PFNGLTEXIMAGE3DOESPROC gl_TexImage3DOES;

	#define glTexSubImage3DOES(...) gl::TexSubImage3DOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void TexSubImage3DOES (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels, const char* file, int line);
	static PFNGLTEXSUBIMAGE3DOESPROC gl_TexSubImage3DOES;

	// GL_OES_texture_border_clamp
	#define glGetSamplerParameterIivOES(...) gl::GetSamplerParameterIivOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetSamplerParameterIivOES (GLuint sampler, GLenum pname, GLint *params, const char* file, int line);
	static PFNGLGETSAMPLERPARAMETERIIVOESPROC gl_GetSamplerParameterIivOES;

	#define glGetSamplerParameterIuivOES(...) gl::GetSamplerParameterIuivOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetSamplerParameterIuivOES (GLuint sampler, GLenum pname, GLuint *params, const char* file, int line);
	static PFNGLGETSAMPLERPARAMETERIUIVOESPROC gl_GetSamplerParameterIuivOES;

	#define glGetTexParameterIivOES(...) gl::GetTexParameterIivOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetTexParameterIivOES (GLenum target, GLenum pname, GLint *params, const char* file, int line);
	static PFNGLGETTEXPARAMETERIIVOESPROC gl_GetTexParameterIivOES;

	#define glGetTexParameterIuivOES(...) gl::GetTexParameterIuivOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetTexParameterIuivOES (GLenum target, GLenum pname, GLuint *params, const char* file, int line);
	static PFNGLGETTEXPARAMETERIUIVOESPROC gl_GetTexParameterIuivOES;

	#define glSamplerParameterIivOES(...) gl::SamplerParameterIivOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void SamplerParameterIivOES (GLuint sampler, GLenum pname, const GLint *param, const char* file, int line);
	static PFNGLSAMPLERPARAMETERIIVOESPROC gl_SamplerParameterIivOES;

	#define glSamplerParameterIuivOES(...) gl::SamplerParameterIuivOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void SamplerParameterIuivOES (GLuint sampler, GLenum pname, const GLuint *param, const char* file, int line);
	static PFNGLSAMPLERPARAMETERIUIVOESPROC gl_SamplerParameterIuivOES;

	#define glTexParameterIivOES(...) gl::TexParameterIivOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void TexParameterIivOES (GLenum target, GLenum pname, const GLint *params, const char* file, int line);
	static PFNGLTEXPARAMETERIIVOESPROC gl_TexParameterIivOES;

	#define glTexParameterIuivOES(...) gl::TexParameterIuivOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void TexParameterIuivOES (GLenum target, GLenum pname, const GLuint *params, const char* file, int line);
	static PFNGLTEXPARAMETERIUIVOESPROC gl_TexParameterIuivOES;

	// GL_OES_texture_buffer
	#define glTexBufferOES(...) gl::TexBufferOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void TexBufferOES (GLenum target, GLenum internalformat, GLuint buffer, const char* file, int line);
	static PFNGLTEXBUFFEROESPROC gl_TexBufferOES;

	#define glTexBufferRangeOES(...) gl::TexBufferRangeOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void TexBufferRangeOES (GLenum target, GLenum internalformat, GLuint buffer, GLintptr offset, GLsizeiptr size, const char* file, int line);
	static PFNGLTEXBUFFERRANGEOESPROC gl_TexBufferRangeOES;

	// GL_OES_texture_storage_multisample_2d_array
	#define glTexStorage3DMultisampleOES(...) gl::TexStorage3DMultisampleOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void TexStorage3DMultisampleOES (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLboolean fixedsamplelocations, const char* file, int line);
	static PFNGLTEXSTORAGE3DMULTISAMPLEOESPROC gl_TexStorage3DMultisampleOES;

	// GL_OES_texture_view
	#define glTextureViewOES(...) gl::TextureViewOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void TextureViewOES (GLuint texture, GLenum target, GLuint origtexture, GLenum internalformat, GLuint minlevel, GLuint numlevels, GLuint minlayer, GLuint numlayers, const char* file, int line);
	static PFNGLTEXTUREVIEWOESPROC gl_TextureViewOES;

	// GL_OES_vertex_array_object
	#define glBindVertexArrayOES(...) gl::BindVertexArrayOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void BindVertexArrayOES (GLuint array, const char* file, int line);
	static PFNGLBINDVERTEXARRAYOESPROC gl_BindVertexArrayOES;

	#define glDeleteVertexArraysOES(...) gl::DeleteVertexArraysOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void DeleteVertexArraysOES (GLsizei n, const GLuint *arrays, const char* file, int line);
	static PFNGLDELETEVERTEXARRAYSOESPROC gl_DeleteVertexArraysOES;

	#define glGenVertexArraysOES(...) gl::GenVertexArraysOES( __VA_ARGS__, __FILE__,__LINE__ )
	static void GenVertexArraysOES (GLsizei n, GLuint *arrays, const char* file, int line);
	static PFNGLGENVERTEXARRAYSOESPROC gl_GenVertexArraysOES;

	#define glIsVertexArrayOES(...) gl::IsVertexArrayOES( __VA_ARGS__, __FILE__,__LINE__ )
	static GLboolean IsVertexArrayOES (GLuint array, const char* file, int line);
	static PFNGLISVERTEXARRAYOESPROC gl_IsVertexArrayOES;

	// GL_OVR_multiview
	#define glFramebufferTextureMultiviewOVR(...) gl::FramebufferTextureMultiviewOVR( __VA_ARGS__, __FILE__,__LINE__ )
	static void FramebufferTextureMultiviewOVR (GLenum target, GLenum attachment, GLuint texture, GLint level, GLint baseViewIndex, GLsizei numViews, const char* file, int line);
	static PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC gl_FramebufferTextureMultiviewOVR;

	// GL_OVR_multiview_multisampled_render_to_texture
	#define glFramebufferTextureMultisampleMultiviewOVR(...) gl::FramebufferTextureMultisampleMultiviewOVR( __VA_ARGS__, __FILE__,__LINE__ )
	static void FramebufferTextureMultisampleMultiviewOVR (GLenum target, GLenum attachment, GLuint texture, GLint level, GLsizei samples, GLint baseViewIndex, GLsizei numViews, const char* file, int line);
	static PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC gl_FramebufferTextureMultisampleMultiviewOVR;

	// GL_QCOM_alpha_test
	#define glAlphaFuncQCOM(...) gl::AlphaFuncQCOM( __VA_ARGS__, __FILE__,__LINE__ )
	static void AlphaFuncQCOM (GLenum func, GLclampf ref, const char* file, int line);
	static PFNGLALPHAFUNCQCOMPROC gl_AlphaFuncQCOM;

	// GL_QCOM_driver_control
	#define glDisableDriverControlQCOM(...) gl::DisableDriverControlQCOM( __VA_ARGS__, __FILE__,__LINE__ )
	static void DisableDriverControlQCOM (GLuint driverControl, const char* file, int line);
	static PFNGLDISABLEDRIVERCONTROLQCOMPROC gl_DisableDriverControlQCOM;

	#define glEnableDriverControlQCOM(...) gl::EnableDriverControlQCOM( __VA_ARGS__, __FILE__,__LINE__ )
	static void EnableDriverControlQCOM (GLuint driverControl, const char* file, int line);
	static PFNGLENABLEDRIVERCONTROLQCOMPROC gl_EnableDriverControlQCOM;

	#define glGetDriverControlStringQCOM(...) gl::GetDriverControlStringQCOM( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetDriverControlStringQCOM (GLuint driverControl, GLsizei bufSize, GLsizei *length, GLchar *driverControlString, const char* file, int line);
	static PFNGLGETDRIVERCONTROLSTRINGQCOMPROC gl_GetDriverControlStringQCOM;

	#define glGetDriverControlsQCOM(...) gl::GetDriverControlsQCOM( __VA_ARGS__, __FILE__,__LINE__ )
	static void GetDriverControlsQCOM (GLint *num, GLsizei size, GLuint *driverControls, const char* file, int line);
	static PFNGLGETDRIVERCONTROLSQCOMPROC gl_GetDriverControlsQCOM;

	// GL_QCOM_extended_get
	#define glExtGetBufferPointervQCOM(...) gl::ExtGetBufferPointervQCOM( __VA_ARGS__, __FILE__,__LINE__ )
	static void ExtGetBufferPointervQCOM (GLenum target, void **params, const char* file, int line);
	static PFNGLEXTGETBUFFERPOINTERVQCOMPROC gl_ExtGetBufferPointervQCOM;

	#define glExtGetBuffersQCOM(...) gl::ExtGetBuffersQCOM( __VA_ARGS__, __FILE__,__LINE__ )
	static void ExtGetBuffersQCOM (GLuint *buffers, GLint maxBuffers, GLint *numBuffers, const char* file, int line);
	static PFNGLEXTGETBUFFERSQCOMPROC gl_ExtGetBuffersQCOM;

	#define glExtGetFramebuffersQCOM(...) gl::ExtGetFramebuffersQCOM( __VA_ARGS__, __FILE__,__LINE__ )
	static void ExtGetFramebuffersQCOM (GLuint *framebuffers, GLint maxFramebuffers, GLint *numFramebuffers, const char* file, int line);
	static PFNGLEXTGETFRAMEBUFFERSQCOMPROC gl_ExtGetFramebuffersQCOM;

	#define glExtGetRenderbuffersQCOM(...) gl::ExtGetRenderbuffersQCOM( __VA_ARGS__, __FILE__,__LINE__ )
	static void ExtGetRenderbuffersQCOM (GLuint *renderbuffers, GLint maxRenderbuffers, GLint *numRenderbuffers, const char* file, int line);
	static PFNGLEXTGETRENDERBUFFERSQCOMPROC gl_ExtGetRenderbuffersQCOM;

	#define glExtGetTexLevelParameterivQCOM(...) gl::ExtGetTexLevelParameterivQCOM( __VA_ARGS__, __FILE__,__LINE__ )
	static void ExtGetTexLevelParameterivQCOM (GLuint texture, GLenum face, GLint level, GLenum pname, GLint *params, const char* file, int line);
	static PFNGLEXTGETTEXLEVELPARAMETERIVQCOMPROC gl_ExtGetTexLevelParameterivQCOM;

	#define glExtGetTexSubImageQCOM(...) gl::ExtGetTexSubImageQCOM( __VA_ARGS__, __FILE__,__LINE__ )
	static void ExtGetTexSubImageQCOM (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, void *texels, const char* file, int line);
	static PFNGLEXTGETTEXSUBIMAGEQCOMPROC gl_ExtGetTexSubImageQCOM;

	#define glExtGetTexturesQCOM(...) gl::ExtGetTexturesQCOM( __VA_ARGS__, __FILE__,__LINE__ )
	static void ExtGetTexturesQCOM (GLuint *textures, GLint maxTextures, GLint *numTextures, const char* file, int line);
	static PFNGLEXTGETTEXTURESQCOMPROC gl_ExtGetTexturesQCOM;

	#define glExtTexObjectStateOverrideiQCOM(...) gl::ExtTexObjectStateOverrideiQCOM( __VA_ARGS__, __FILE__,__LINE__ )
	static void ExtTexObjectStateOverrideiQCOM (GLenum target, GLenum pname, GLint param, const char* file, int line);
	static PFNGLEXTTEXOBJECTSTATEOVERRIDEIQCOMPROC gl_ExtTexObjectStateOverrideiQCOM;

	// GL_QCOM_extended_get2
	#define glExtGetProgramBinarySourceQCOM(...) gl::ExtGetProgramBinarySourceQCOM( __VA_ARGS__, __FILE__,__LINE__ )
	static void ExtGetProgramBinarySourceQCOM (GLuint program, GLenum shadertype, GLchar *source, GLint *length, const char* file, int line);
	static PFNGLEXTGETPROGRAMBINARYSOURCEQCOMPROC gl_ExtGetProgramBinarySourceQCOM;

	#define glExtGetProgramsQCOM(...) gl::ExtGetProgramsQCOM( __VA_ARGS__, __FILE__,__LINE__ )
	static void ExtGetProgramsQCOM (GLuint *programs, GLint maxPrograms, GLint *numPrograms, const char* file, int line);
	static PFNGLEXTGETPROGRAMSQCOMPROC gl_ExtGetProgramsQCOM;

	#define glExtGetShadersQCOM(...) gl::ExtGetShadersQCOM( __VA_ARGS__, __FILE__,__LINE__ )
	static void ExtGetShadersQCOM (GLuint *shaders, GLint maxShaders, GLint *numShaders, const char* file, int line);
	static PFNGLEXTGETSHADERSQCOMPROC gl_ExtGetShadersQCOM;

	#define glExtIsProgramBinaryQCOM(...) gl::ExtIsProgramBinaryQCOM( __VA_ARGS__, __FILE__,__LINE__ )
	static GLboolean ExtIsProgramBinaryQCOM (GLuint program, const char* file, int line);
	static PFNGLEXTISPROGRAMBINARYQCOMPROC gl_ExtIsProgramBinaryQCOM;

	// GL_QCOM_tiled_rendering
	#define glEndTilingQCOM(...) gl::EndTilingQCOM( __VA_ARGS__, __FILE__,__LINE__ )
	static void EndTilingQCOM (GLbitfield preserveMask, const char* file, int line);
	static PFNGLENDTILINGQCOMPROC gl_EndTilingQCOM;

	#define glStartTilingQCOM(...) gl::StartTilingQCOM( __VA_ARGS__, __FILE__,__LINE__ )
	static void StartTilingQCOM (GLuint x, GLuint y, GLuint width, GLuint height, GLbitfield preserveMask, const char* file, int line);
	static PFNGLSTARTTILINGQCOMPROC gl_StartTilingQCOM;

}; // end of class gl

}; // namespace Debugger

using namespace Debugger;

#endif // __NO_RENDERER__ - __GLES2__

#endif // __GLES2_DEBUGGER_INCLUDE_H__

