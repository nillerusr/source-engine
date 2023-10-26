/*
	see copyright notice in sqrdbg.h
*/
#include <windows.h>
#include <winsock.h>
#include <squirrel.h>
#include <stdlib.h>
#include <stdio.h>
#include <sqstdblob.h>
#include <sqstdio.h>
#include <sqstdaux.h>
#include "sqrdbg.h"
#include "sqdbgserver.h"

#pragma comment (lib ,"Ws2_32.lib")

#ifdef _UNICODE
#define scfprintf fwprintf
#define scvprintf vwprintf
#else
#define scfprintf fprintf
#define scvprintf vprintf
#endif


void printfunc(HSQUIRRELVM v,const SQChar *s,...)
{
	va_list vl;
	va_start(vl, s);
	scvprintf( s, vl);
	va_end(vl);
}

void PrintError(HSQUIRRELVM v)
{
	const SQChar *err;
	sq_getlasterror(v);
	if(SQ_SUCCEEDED(sq_getstring(v,-1,&err))) {
		scprintf(_SC("SQDBG error : %s"),err);
	}else {
		scprintf(_SC("SQDBG error"),err);
	}
	sq_pop(v,1);
}

int main(int argc, char *argv[])
{
	if(argc < 2){
		scprintf(_SC("SQDBG error : no file specified"));
		return -1;
	}

	HSQUIRRELVM v = sq_open(1024);
	sqstd_seterrorhandlers(v);

	//!! INITIALIZES THE DEBUGGER ON THE TCP PORT 1234
	//!! ENABLES AUTOUPDATE
	HSQREMOTEDBG rdbg = sq_rdbg_init(v,1234,SQTrue);
	if(rdbg) {

		//!! ENABLES DEBUG INFO GENERATION(for the compiler)
		sq_enabledebuginfo(v,SQTrue);

		sq_setprintfunc(v,printfunc);

		//!! SUSPENDS THE APP UNTIL THE DEBUGGER CLIENT CONNECTS
		if(SQ_SUCCEEDED(sq_rdbg_waitforconnections(rdbg))) {
			scprintf(_SC("connected\n"));

			const SQChar *fname=NULL;
#ifdef _UNICODE
			SQChar sTemp[256];
			mbstowcs(sTemp,argv[1],(int)strlen(argv[1])+1);
			fname=sTemp;
#else
			fname=argv[1];
#endif
			//!!REGISTERS STANDARDS LIBS
			sq_pushroottable(v);
			sqstd_register_bloblib(v);
			sqstd_register_iolib(v);
			//!!EXECUTE A SCTIPT
			if(SQ_FAILED(sqstd_dofile(v,fname,SQFalse,SQTrue))) {
				PrintError(v);
			}
		}
		//!! CLEANUP
		sq_rdbg_shutdown(rdbg);
	}
	else {
		PrintError(v);
	}
	sq_close(v);
	return 0;
}
