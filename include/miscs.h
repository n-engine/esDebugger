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
#ifndef __MISCS_INCLUDE_H__
#define __MISCS_INCLUDE_H__

// misc tool given as an example for the test
// you can easily replace String, vector and
// map class with your own with just one define.

#include <stdarg.h>	// va_list, va_start, va_arg, va_end

#include <iostream>	// std::cout
#include <sstream>	// std::stringstream
#include <string>	// std::string
#include <vector>	// std::vector
#include <map>		// std::map

// type
//#define String	std::string 
#define Vector	std::vector
#define Map		std::map

typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef ulong hash_t;

typedef Vector<uint> uint_v;
typedef Vector<ulong> ulong_v;
typedef Vector<hash_t> hash_v;

#define Iterator const_iterator

// ----
class String : public std::string
{
public:
	using std::string::string;

	inline String& operator <<(const char* rhs)
	{
		*this += rhs;
		return *this;
	}

	inline String& operator <<(const String& rhs)
	{
		*this += rhs;
		return *this;
	}

	inline operator const char*() { return (*this).c_str(); }
	inline operator const char*() const { return (*this).c_str(); }
}; // end of class String

typedef Vector<String> string_v;

// ---------------------------------------------------------------------
// hash djb2 from (http://www.cse.yorku.ca/~oz/hash.html)
inline hash_t hash(const char* s)
{
	int c;
	hash_t hh = 5381;
	while (c = *s++) { hh = ((hh << 5) + hh) + c; }
	return hh;
}

// tokenizer -----------------------------------------------------------
bool Explode(string_v& result,const String &data,char delim=' ');

const String format(const char* fmt,...);

// helper --------------------------------------------------------------
#define foreach(p_) \
	/* you must have iter defined as this : type::Iterator iter */ \
	for(iter=p_.begin();iter!=p_.end();++iter)



inline String toString(const int& v)
{
	return String(format("%d", v));
}

inline String toString(const float& v)
{
	return String(format("%g", v));
}

/** */
#define MakeBool(val) (val)?true:false

// log and assertion ---------------------------------------------------

/** 0 == ERROR, 1 == WARNING, 2 == DEBUG */
extern int _debug_level;

#define TRACE_LOG(level,logs_) \
	if ( level <= _debug_level ) \
	std::cout << logs_ << std::endl

#define TRACE_ERROR(logs_)	TRACE_LOG(0,logs_)
#define TRACE_WARNING(logs_)	TRACE_LOG(1,logs_)
#define TRACE_DEBUG(logs_)	TRACE_LOG(2,logs_)
#define TRACE_BUFFER(...)	/** implement your own data dumper */

#if defined(__WIN32__)
	#define TRAP_	__debugbreak();
#else
	#define TRAP_ __builtin_trap();
#endif

/** very simple assert (active all the time) */
#define ne_assert(a) \
 if(!(a)) { ::printf("Assertion occur from: %s line: %d\n", __FILE__, \
 	__LINE__ ); ::fflush(0); TRAP_ }
 
/** wrapper tool */
class Libc
{
public:
	static inline int strlen(const char* a) { return ::strlen(a); }
	static inline const char* strrchr(const char* a, int b) { return ::strrchr(a, b); }
	static inline const char* strrchr(const String& a, int b) { return ::strrchr(a.c_str(), b); }

}; // Libc

class Core
{
public:
	static inline hash_t hash(const char* a) { return ::hash(a); }
}; // Core

#endif // __MISCS_INCLUDE_H__
