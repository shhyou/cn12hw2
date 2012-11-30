#include <errno.h>
#include <netdb.h>

#include <cstdarg>
#include <cstring>

#include <string>
#include <vector>

#define _LOG_C
#include "log.h"

using std::string;
using std::vector;

string stktrace() {
	string res;
	for (vector<string>::iterator it=callstk.begin(); it!=callstk.end(); it++) {
		res += *it;
		if (it+1 != callstk.end()) res += "!";
	}
	return res;
}

string strerrmsg(const char *fmt, va_list args, const char *err_str) {
	static char buf[4096];
	vsprintf(buf, fmt, args);
	return stktrace() + ": " + buf + " (" + err_str + ")";
}

logprint_t::logprint_t(const char *_type, FILE *_output) : type(_type), output(_output) {}
void logprint_t::operator()(const char *fmt, ...) const {
	static char buf[64];
	va_list args;
	time_t tm = time(NULL);

	va_start(args, fmt);
	strftime(buf, sizeof(buf), "%Y%m%d %a %H:%M:%S", localtime(&tm));

	fprintf(output, "[%s] ", buf);
	fprintf(output, "[%s] ", type);
	fprintf(output, "Stack trace: %s", stktrace().c_str());
	fprintf(output, "\n");
	vfprintf(output, fmt, args);
	fprintf(output, "\n");

	va_end(args);
}

log_t::log_t(const char* t, const char* s) : print("info", stdout), eprint("err ", stderr) {
	callstk.push_back(t);
	callstk.back() += "::";
	callstk.back() += s;
}

log_t::log_t(const char* s) : print("info", stdout), eprint("err ", stderr) {
	callstk.push_back(s);
}

log_t::~log_t() { callstk.pop_back(); }

string log_t::trace() const { return stktrace(); }

string log_t::errmsg(const char *fmt, ...) const {
	va_list args;
	va_start(args, fmt);
	string res(strerrmsg(fmt, args, strerror(errno)));
	va_end(args);
	return res;
}

string log_t::herrmsg(const char *fmt, ...) const {
	va_list args;
	va_start(args, fmt);
	string res(strerrmsg(fmt, args, hstrerror(h_errno)));
	va_end(args);
	return res;
}

