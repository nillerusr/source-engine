/*
Copyright (C) 2022 nillerusr

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of 
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <unwind.h>
#include <android/log.h>
#include "tier0/dbg.h"
#include <stdlib.h>
#include <inttypes.h>

struct sigaction old_sa;

#define IN_LIBGCC2 1 // means we want to define __cxxabiv1::__cxa_demangle
namespace __cxxabiv1
{
	extern "C"
	{
		#include "demangle/cp-demangle.c"
	}
}

#define Log(msg) __android_log_print(ANDROID_LOG_DEBUG, "SRCENG", "%s", msg); DebugLogger()->Write(msg);

void printPC(void *pc)
{
	char message[4096];

	const char* symbol = "unknown";

	Dl_info info = { 0 };
	const char *fname = "unknown";

	dladdr(pc, &info);
	if( info.dli_fname )
		fname = info.dli_fname;

	if( info.dli_sname )
		symbol = info.dli_sname;

	int status = 0;
	char *demangled = __cxxabiv1::__cxa_demangle(symbol, 0, 0, &status);

	if( NULL != demangled && 0 == status )
		symbol = demangled;

	uintptr_t relative_addr = (uintptr_t)pc - (uintptr_t)info.dli_fbase;

	snprintf( message, sizeof(message), "0x%" PRIXPTR ":\t%s (base=0x%" PRIXPTR ", %s)\n", relative_addr, symbol, (uintptr_t)info.dli_fbase, fname );
	Log(message);
}

_Unwind_Reason_Code UnwindBacktraceWithSkippingCallback(struct _Unwind_Context* unwind_context, void* state_voidp)
{
	uintptr_t pc = _Unwind_GetIP(unwind_context);
	printPC((void*)pc);

	return _URC_NO_REASON;
}

static void CrashHandler( int sig, siginfo_t *si, void *uc)
{
	char message[4096], symbol[256];
	int len, line, logfd, i = 0;

	Log(">>> crash report begin\n");

	snprintf(message, sizeof(message), "Signal=%d, errno=%d, code=%d, addr=0x%" PRIXPTR "\n", sig, si->si_errno, si->si_code, (uintptr_t)si->si_addr);
	Log(message);

	const ucontext_t* signal_ucontext = (ucontext_t*)uc;
	const mcontext_t* signal_mcontext = &(signal_ucontext->uc_mcontext);

	_Unwind_Backtrace(UnwindBacktraceWithSkippingCallback, NULL);

	Log(">>> crash report end\n");

	if (old_sa.sa_sigaction)
		(*old_sa.sa_sigaction)(sig, si, uc);
	else if(old_sa.sa_handler)
		(*old_sa.sa_handler)(sig);
}

void InitCrashHandler()
{
	struct sigaction act;
	act.sa_sigaction = CrashHandler;
	act.sa_flags = SA_SIGINFO | SA_ONSTACK;
	sigaction(SIGSEGV, &act, &old_sa);
	sigaction(SIGABRT, &act, &old_sa);
	sigaction(SIGBUS, &act, &old_sa);
	sigaction(SIGFPE, &act, &old_sa);
	sigaction(SIGTRAP, &act, &old_sa);
}
