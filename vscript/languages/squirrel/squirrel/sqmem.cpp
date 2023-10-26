/*
	see copyright notice in squirrel.h
*/
#include "sqpcheader.h"
#if !(defined(VSCRIPT_DLL_EXPORT) || defined(VSQUIRREL_TEST))
void *sq_vm_malloc(SQUnsignedInteger size){	return malloc(size); }

void *sq_vm_realloc(void *p, SQUnsignedInteger oldsize, SQUnsignedInteger size){ return realloc(p, size); }

void sq_vm_free(void *p, SQUnsignedInteger size){	free(p); }
#endif
