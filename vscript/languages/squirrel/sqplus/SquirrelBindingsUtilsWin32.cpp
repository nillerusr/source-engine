#include "sqplus.h"

//#include "SquirrelObject.h"
//#include "SquirrelVM.h"
#include "SquirrelBindingsUtilsWin32.h"
#if defined(VSCRIPT_DLL_EXPORT) || defined(VSQUIRREL_TEST)
#include "memdbgon.h"
#endif

SQInteger refcounted_release_hook(SQUserPointer p, SQInteger size)
{
	IUnknown *pRC = (IUnknown*)p;
	pRC->Release();
	return 0;
}

static BOOL __CreateRefCountedInstance(HSQUIRRELVM v,const SQChar *classname,IUnknown *pRC,SQRELEASEHOOK hook)
{
	if(!CreateNativeClassInstance(v,classname,pRC,hook)) return FALSE;
	return TRUE;
}

int construct_RefCounted(IUnknown *p)
{
	sq_setinstanceup(SquirrelVM::GetVMPtr(),1,p);
	sq_setreleasehook(SquirrelVM::GetVMPtr(),1,refcounted_release_hook);
	return 1;
}


BOOL CreateRefCountedInstance(HSQUIRRELVM v,const SQChar *classname,IUnknown *pRC)
{
	return __CreateRefCountedInstance(v,classname,pRC,refcounted_release_hook);
}
