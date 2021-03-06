/*
 * Copyright (c) [2004-2013] Novell, Inc.
 *
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, contact Novell, Inc.
 *
 * To contact Novell about this file by physical or electronic mail, you may
 * find current contact information at www.novell.com.
 */


#include <pwd.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <libxml/tree.h>
#include <string>
#include <boost/thread.hpp>

#include "snapper/Log.h"
#include "snapper/AppUtil.h"


namespace snapper
{
    using namespace std;


    // Use a pointer to avoid a global destructor. Otherwise the Snapper
    // destructor in Factory.cc can be called after when logging does not work
    // anymore. TODO: nicer solution.
    string* filename = new string("/var/log/snapper.log");

    LogDo log_do = NULL;
    LogQuery log_query = NULL;


    void
    setLogDo(LogDo new_log_do)
    {
	log_do = new_log_do;
    }


    void
    setLogQuery(LogQuery new_log_query)
    {
	log_query = new_log_query;
    }


    static void
    simple_log_do(LogLevel level, const string& component, const char* file, int line,
		  const char* func, const string& text)
    {
	static const char* ln[4] = { "DEB", "MIL", "WAR", "ERR" };

	string prefix = sformat("%s %s libsnapper(%d) %s(%s):%d", datetime(time(0), false, true).c_str(),
				ln[level], getpid(), file, func, line);

	static boost::mutex mutex;

	boost::lock_guard<boost::mutex> lock(mutex);

	FILE* f = fopen(filename->c_str(), "ae");
	if (f)
	{
	    string tmp = text;

	    string::size_type pos1 = 0;

	    while (true)
	    {
		string::size_type pos2 = tmp.find('\n', pos1);

		if (pos2 != string::npos || pos1 != tmp.length())
		    fprintf(f, "%s - %s\n", prefix.c_str(), tmp.substr(pos1, pos2 - pos1).c_str());

		if (pos2 == string::npos)
		    break;

		pos1 = pos2 + 1;
	    }

	    fclose(f);
	}
    }


    static bool
    simple_log_query(LogLevel level, const string& component)
    {
	return level != DEBUG;
    }


    void
    callLogDo(LogLevel level, const string& component, const char* file, int line,
	      const char* func, const string& text)
    {
	if (log_do)
	    (log_do)(level, component, file, line, func, text);
	else
	    simple_log_do(level, component, file, line, func, text);
    }


    bool
    callLogQuery(LogLevel level, const string& component)
    {
	if (log_query)
	    return (log_query)(level, component);
	else
	    return simple_log_query(level, component);
    }


    void
    xml_error_func(void* ctx, const char* msg, ...)
    {
    }

    xmlGenericErrorFunc xml_error_func_ptr = &xml_error_func;


    void
    initDefaultLogger()
    {
	delete filename;
	filename = new string("/var/log/snapper.log");

	if (geteuid())
	{
	    struct passwd pwd;
	    struct passwd* result;

	    long bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
	    char buf[bufsize];

	    if (getpwuid_r(geteuid(), &pwd, buf, bufsize, &result) == 0 && result == &pwd)
	    {
		memset(pwd.pw_passwd, 0, strlen(pwd.pw_passwd));

		delete filename;
		filename = new string(string(pwd.pw_dir) + "/.snapper.log");
	    }
	}

	log_do = NULL;
	log_query = NULL;

	initGenericErrorDefaultFunc(&xml_error_func_ptr);
    }

}
