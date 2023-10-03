#ifndef _SQUIRREL_VM_H_
#define _SQUIRREL_VM_H_

struct SquirrelError {
	SquirrelError();
	SquirrelError(const SQChar* s):desc(s){}
	const SQChar *desc;
};

struct SquirrelVMSys {
  HSQUIRRELVM _VM;
  SquirrelObject * _root;
};

class SquirrelVM
{
	friend class SquirrelObject;
	friend struct SquirrelError;
public:
	static void Init();
	static BOOL IsInitialized(){return _VM == NULL?FALSE:TRUE;}
	static void Shutdown();
	static void Cleanup();
	static BOOL Update(); //debugger and maybe GC later
	static SquirrelObject CompileScript(const SQChar *s);
	static SquirrelObject CompileBuffer(const SQChar *s,const SQChar * debugInfo=_T("console_buffer"));
	static SquirrelObject RunScript(const SquirrelObject &o,SquirrelObject *_this = NULL);
	static void PrintFunc(HSQUIRRELVM v,const SQChar* s,...);
	static BOOL BeginCall(const SquirrelObject &func);
	static BOOL BeginCall(const SquirrelObject &func,SquirrelObject &_this);
	static void PushParam(const SquirrelObject &o);
	static void PushParam(const SQChar *s);
	static void PushParam(SQInteger n);
	static void PushParam(SQFloat f);
	static void PushParam(SQUserPointer up);
	static void PushParamNull();
	static SquirrelObject EndCall();
	static SquirrelObject CreateString(const SQChar *s);
	static SquirrelObject CreateTable();	
	static SquirrelObject CreateArray(int size);
	static SquirrelObject CreateInstance(SquirrelObject &oclass); // oclass is an existing class. Create an 'instance' (OT_INSTANCE) of oclass.
	static SquirrelObject CreateFunction(SQFUNCTION func);
  static SquirrelObject CreateUserData(int size);

	static const SquirrelObject &GetRootTable();
	static HSQUIRRELVM GetVMPtr() { return _VM; }

#if 0
  static void SetVMPtr(HSQUIRRELVM v) {
    _VM = v;
  } // setVMPtr
#endif

  static void GetVMSys(SquirrelVMSys & vmSys) {
    vmSys._VM   = _VM;
    vmSys._root = _root;
  } // GetVMSys

  static void SetVMSys(const SquirrelVMSys & vmSys) {
    _VM   = vmSys._VM;
    _root = vmSys._root;
  } // SetVMSys

  static void PushValue(INT val) {
    sq_pushinteger(_VM,val);
  } // PushValue
  static void PushValue(FLOAT val) {
    sq_pushfloat(_VM,val);
  } // PushValue
  static void PushValue(bool val) { // Compiler treats SQBool as INT.
    sq_pushbool(_VM,val);
  } // PushValue
  static void PushValue(SQChar * val) {
    sq_pushstring(_VM,val,-1);
  } // PushValue
  static void PushValue(SQUserPointer val) {
    sq_pushuserpointer(_VM,val);
  } // PushValue
  static void PushValue(const SQChar * val) {
    sq_pushstring(_VM,val,-1);
  } // PushValue
  static void PushObject(SquirrelObject & so) {
    sq_pushobject(_VM,so._o);
  } // PushObject
  static void Pop(SQInteger nelemstopop) {
    sq_pop(_VM,nelemstopop);
  } // Pop
  static void PushRootTable(void);
  // Create/bind a function on the table currently on the stack.
  static SquirrelObject CreateFunction(SQFUNCTION func,const SQChar * scriptFuncName,const SQChar * typeMask=0);
  // Create/bind a function on the table so. typeMask: standard Squirrel types plus: no typemask means no args, "*" means any type of args.
  static SquirrelObject CreateFunction(SquirrelObject & so,SQFUNCTION func,const SQChar * scriptFuncName,const SQChar * typeMask=0);
  // Create/bind a function to the root table. typeMask: standard Squirrel types plus: no typemask means no args, "*" means any type of args.
  static SquirrelObject CreateFunctionGlobal(SQFUNCTION func,const SQChar * scriptFuncName,const SQChar * typeMask=0);

private:
	static HSQUIRRELVM _VM;
	static int _CallState;
	static SquirrelObject * _root;
};

template<typename T>
inline BOOL SquirrelObject::ArrayAppend(T item) {
  sq_pushobject(SquirrelVM::_VM,GetObjectHandle());
  SquirrelVM::PushValue(item);
  BOOL res = sq_arrayappend(SquirrelVM::_VM,-2) == SQ_OK;
  sq_pop(SquirrelVM::_VM,1);
  return res;
} // ArrayAppend

#endif //_SQUIRREL_VM_H_

