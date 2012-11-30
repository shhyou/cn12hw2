#ifndef _LOG_H
#define _LOG_H

#include <ctime>
#include <cstdio>
#include <cstdarg>

#include <string>
#include <vector>

#include <typeinfo>

#ifndef _LOG_C
extern std::vector<std::string> callstk;
#else
std::vector<std::string> callstk;
#endif

/* __logc is for logging function in a class/struct; it includes type signatures */
#define __log log_t logger(__func__)
#define __logc log_t logger(typeid(*this).name(), __func__)

struct logprint_t {
	const char *type;
	FILE * const output;
	logprint_t(const char *_type, FILE *_output);
	void operator()(const char *fmt, ...) const;
};

/*************************************************************************
 * log_t::trace   returnes a string of current stack
 * log_t::print   is for printing usual log (to stdout)
 * log_t::eprint  prints message to stderr
 * log_t::errmsg  produces a string with designated message, system error
 *                message and stack trace
 * log_t::herrmsg is similar to log_t::errmsg except that strerror is
 *                replaced by hstrerror (and errno by h_errno)
 ************************************************************************/

struct log_t {
	log_t(const char* function);
	log_t(const char* type, const char* function);
	~log_t();
	logprint_t print;
	logprint_t eprint;
	std::string trace() const;
	std::string errmsg(const char* fmt, ...) const;
	std::string herrmsg(const char* fmt, ...) const;
};

#endif

