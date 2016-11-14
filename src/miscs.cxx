#include "config.h"

#include "miscs.h"

/** 0 == ERROR, 1 == WARNING, 2 == DEBUG */
int _debug_level = 2;

bool Explode(string_v& result,const String &data,char delim)
{
	std::stringstream s;
	s.str(data);
	String entry;

	while (std::getline(s, entry, delim))
	{
		result.push_back(entry);
	}

    return (result.size() >= 1);
}

const String format(const char* fmt, ...)
{
	static char buf_fmt_[8192] = { 0 };
	va_list argptr;
	va_start(argptr, fmt);
	vsnprintf(buf_fmt_, sizeof(buf_fmt_), fmt, argptr);
	va_end(argptr);
	return buf_fmt_;
}