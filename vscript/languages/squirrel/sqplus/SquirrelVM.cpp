#include <stdio.h>
#include <stdarg.h>

#define _DEBUG_DUMP

#include "sqplus.h"

#include <sqstdio.h>
#include <sqstdmath.h>
#include <sqstdstring.h>
#include <sqstdaux.h>
#include <sqstdblob.h>
#if defined(VSCRIPT_DLL_EXPORT) || defined(VSQUIRREL_TEST)
#include "memdbgon.h"
#endif

#ifdef _MSC_VER
#define STRLEN(n) _tcslen(n)
#else
#define STRLEN(n) strlen(n)
#endif

HSQUIRRELVM SquirrelVM::_VM = NULL;
int SquirrelVM::_CallState = -1;
SquirrelObject * SquirrelVM::_root = NULL;

SquirrelError::SquirrelError() 
{
	const SQChar *s;
	sq_getlasterror(SquirrelVM::_VM);
	sq_getstring(SquirrelVM::_VM,-1,&s);
	if(s) {
		desc = s;
	}
	else {
		desc = _T("unknown error");
	}
}

void SquirrelVM::Init()
{
	_VM = sq_open(1024);
	sq_setprintfunc(_VM,SquirrelVM::PrintFunc);
	sq_pushroottable(_VM);
	sqstd_register_iolib(_VM);
	sqstd_register_bloblib(_VM);
	sqstd_register_mathlib(_VM);
	sqstd_register_stringlib(_VM);
	sqstd_seterrorhandlers(_VM);
  _root = new SquirrelObject();
	_root->AttachToStackObject(-1);
	sq_pop(_VM,1);
	//TODO error handler, compiler error handler
}

BOOL SquirrelVM::Update()
{
	//update remote debugger
	return TRUE;
}

void SquirrelVM::Cleanup()
{
	//cleans the root table
	sq_pushnull(_VM);
	sq_setroottable(_VM);
}

void SquirrelVM::Shutdown()
{
  if (_VM) {
    Cleanup();
#if 0
    sq_release(_VM,&_root->_o);
    sq_resetobject(&_root->_o);
#endif
    delete _root;
    _root = NULL;
    HSQUIRRELVM v = _VM;
    _VM = NULL;
    sq_close(v);
  } // if
}

void SquirrelVM::PrintFunc(HSQUIRRELVM v,const SQChar* s,...)
{
	static SQChar temp[2048];
	va_list vl;
	va_start(vl, s);
	scvsprintf( temp,s, vl);
	SCPUTS(temp);
	va_end(vl);
}

SquirrelObject SquirrelVM::CompileScript(const SQChar *s)
{
#define MAX_EXPANDED_PATH 1023
	SquirrelObject ret;
	if(SQ_SUCCEEDED(sqstd_loadfile(_VM,s,1))) {
		ret.AttachToStackObject(-1);
		sq_pop(_VM,1);
		return ret;
	}
	throw SquirrelError();
}

SquirrelObject SquirrelVM::CompileBuffer(const SQChar *s,const SQChar * debugInfo)
{
	SquirrelObject ret;
	if(SQ_SUCCEEDED(sq_compilebuffer(_VM,s,(int)STRLEN(s)*sizeof(SQChar),debugInfo,1))) {
		ret.AttachToStackObject(-1);
		sq_pop(_VM,1);
		return ret;
	}
	throw SquirrelError();
}

SquirrelObject SquirrelVM::RunScript(const SquirrelObject &o,SquirrelObject *_this)
{
	SquirrelObject ret;
	sq_pushobject(_VM,o._o);
	if(_this) {
		sq_pushobject(_VM,_this->_o);
	}
	else {
		sq_pushroottable(_VM);
	}
	if(SQ_SUCCEEDED(sq_call(_VM,1,SQTrue,SQ_CALL_RAISE_ERROR))) {
		ret.AttachToStackObject(-1);
		sq_pop(_VM,1);
		return ret;
	}
	sq_pop(_VM,1);
	throw SquirrelError();
	
}


BOOL SquirrelVM::BeginCall(const SquirrelObject &func)
{
	if(_CallState != -1)
		return FALSE;
	_CallState = 1;
	sq_pushobject(_VM,func._o);
	sq_pushroottable(_VM);
	return TRUE;
}

BOOL SquirrelVM::BeginCall(const SquirrelObject &func,SquirrelObject &_this)
{
	if(_CallState != -1)
		throw SquirrelError(_T("call already initialized"));
	_CallState = 1;
	sq_pushobject(_VM,func._o);
	sq_pushobject(_VM,_this._o);
	return TRUE;
}

#define _CHECK_CALL_STATE \
	if(_CallState == -1) \
		throw SquirrelError(_T("call not initialized"));

void SquirrelVM::PushParam(const SquirrelObject &o)
{
	_CHECK_CALL_STATE
	sq_pushobject(_VM,o._o);
	_CallState++;
}

void SquirrelVM::PushParam(const SQChar *s)
{
	_CHECK_CALL_STATE
	sq_pushstring(_VM,s,-1);
	_CallState++;
}

void SquirrelVM::PushParam(SQInteger n)
{
	_CHECK_CALL_STATE
	sq_pushinteger(_VM,n);
	_CallState++;
}

void SquirrelVM::PushParam(SQFloat f)
{
	_CHECK_CALL_STATE
	sq_pushfloat(_VM,f);
	_CallState++;
}

void SquirrelVM::PushParamNull()
{
	_CHECK_CALL_STATE
	sq_pushnull(_VM);
	_CallState++;
}

void SquirrelVM::PushParam(SQUserPointer up)
{
	_CHECK_CALL_STATE
	sq_pushuserpointer(_VM,up);
	_CallState++;
}

SquirrelObject SquirrelVM::EndCall()
{
	SquirrelObject ret;
	if(_CallState >= 0) { 
		int oldtop = sq_gettop(_VM);
		int nparams = _CallState;
		_CallState = -1;
		if(SQ_SUCCEEDED(sq_call(_VM,nparams,SQTrue,SQ_CALL_RAISE_ERROR))) {
			ret.AttachToStackObject(-1);
			sq_pop(_VM,2);
		}else {
			sq_settop(_VM,oldtop-(nparams+1));
			throw SquirrelError();
		}
		
	}
	return ret;
}

SquirrelObject SquirrelVM::CreateInstance(SquirrelObject &oclass)
{
	SquirrelObject ret;
	int oldtop = sq_gettop(_VM);
	sq_pushobject(_VM,oclass._o);
	if(SQ_FAILED(sq_createinstance(_VM,-1))) {
		sq_settop(_VM,oldtop);
		throw SquirrelError();
	}
	ret.AttachToStackObject(-1);
	sq_pop(_VM,2);
	return ret;
}

SquirrelObject SquirrelVM::CreateTable()
{
	SquirrelObject ret;
	sq_newtable(_VM);
	ret.AttachToStackObject(-1);
	sq_pop(_VM,1);
	return ret;
}

SquirrelObject SquirrelVM::CreateString(const SQChar *s)
{
	SquirrelObject ret;
	sq_pushstring(_VM,s,-1);
	ret.AttachToStackObject(-1);
	sq_pop(_VM,1);
	return ret;
}


SquirrelObject SquirrelVM::CreateArray(int size)
{
	SquirrelObject ret;
	sq_newarray(_VM,size);
	ret.AttachToStackObject(-1);
	sq_pop(_VM,1);
	return ret;
}

SquirrelObject SquirrelVM::CreateFunction(SQFUNCTION func)
{
	SquirrelObject ret;
	sq_newclosure(_VM,func,0);
	ret.AttachToStackObject(-1);
	sq_pop(_VM,1);
	return ret;
}

SquirrelObject SquirrelVM::CreateUserData(int size) {
  SquirrelObject ret;
  sq_newuserdata(_VM,size);
  ret.AttachToStackObject(-1);
  sq_pop(_VM,1);
  return ret;
}

const SquirrelObject &SquirrelVM::GetRootTable()
{
	return *_root;
}

void SquirrelVM::PushRootTable(void) {
  sq_pushroottable(_VM);
} // SquirrelVM::PushRootTable

// Creates a function in the table or class currently on the stack.
//void CreateFunction(HSQUIRRELVM v,const SQChar * scriptFuncName,SQFUNCTION func,int numParams=0,const SQChar * typeMask=0) {
SquirrelObject SquirrelVM::CreateFunction(SQFUNCTION func,const SQChar * scriptFuncName,const SQChar * typeMask) {
  sq_pushstring(_VM,scriptFuncName,-1);
  sq_newclosure(_VM,func,0);
  SquirrelObject ret;
  ret.AttachToStackObject(-1);
  SQChar tm[64];
  SQChar * ptm = tm;
  int numParams = SQ_MATCHTYPEMASKSTRING;
  if (typeMask) {
    if (typeMask[0] == '*') {
      ptm       = 0; // Variable args: don't check parameters.
      numParams = 0; // Clear SQ_MATCHTYPEMASKSTRING (does not mean match 0 params. See sq_setparamscheck()).
    } else {
      if (SCSNPRINTF(tm,sizeof(tm),_T("t|y|x%s"),typeMask) < 0) {
//        sq_throwerror(_VM,_T("CreateFunction: typeMask string too long."));
        throw SquirrelError(_T("CreateFunction: typeMask string too long."));
      } // if
    } // if
  } else { // <TODO> Need to check object type on stack: table, class, instance, etc.
    SCSNPRINTF(tm,sizeof(tm),_T("%s"),_T("t|y|x")); // table, class, instance.
//    tm[0] = 't';
//    tm[1] = 0;
  } // if
#if 0
  sq_setparamscheck(_VM,numParams+1,ptm); // Parameters are table+args (thus, the +1).
#else
  if (ptm) {
    sq_setparamscheck(_VM,numParams,ptm); // Determine arg count from type string.
  } // if
#endif
#ifdef _DEBUG
  sq_setnativeclosurename(_VM,-1,scriptFuncName); // For debugging only.
#endif
  sq_createslot(_VM,-3); // Create slot in table or class (assigning function to slot at scriptNameFunc).
  return ret;
} // SquirrelVM::CreateFunction

SquirrelObject SquirrelVM::CreateFunction(SquirrelObject & so,SQFUNCTION func,const SQChar * scriptFuncName,const SQChar * typeMask) {
  PushObject(so);
  SquirrelObject ret = CreateFunction(func,scriptFuncName,typeMask);
  Pop(1);
  return ret;
} // SquirrelVM::CreateFunction

// Create a Global function on the root table.
//void CreateFunctionGlobal(HSQUIRRELVM v,const SQChar * scriptFuncName,SQFUNCTION func,int numParams=0,const SQChar * typeMask=0) {
SquirrelObject SquirrelVM::CreateFunctionGlobal(SQFUNCTION func,const SQChar * scriptFuncName,const SQChar * typeMask) {
  PushRootTable(); // Push root table.
  //  CreateFunction(scriptFuncName,func,numParams,typeMask);
  SquirrelObject ret = CreateFunction(func,scriptFuncName,typeMask);
  Pop(1);         // Pop root table.
  return ret;
} // SquirrelVM::CreateFunctionGlobal
