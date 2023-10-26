// testSqPlus.cpp
// Created by John Schultz 9/5/2005
// Free for any use.

#include <stdarg.h> 
#include <stdio.h>
#include "sqplus.h"

using namespace SqPlus;

struct CTestObj {
  float x;
  float y;
  float z;
  CTestObj() {
    x = y = z = 0.f;
  }
  void update(float t) {
    x += t;
  } // update
  void print(void) {
    SQChar buff[256];
    scsprintf(buff,_T("x: %f\n"),x);
//    OutputDebugString(buff);
    SCPUTS(buff);
  } // print
};

_DECL_CLASS(TestObj);

_IMPL_NATIVE_CONSTRUCTION(TestObj,CTestObj);

_MEMBER_FUNCTION_IMPL(TestObj,constructor) {
  CTestObj * newv = NULL;
  StackHandler sa(v);
  int nparams = sa.GetParamCount();
  newv = new CTestObj();
  return construct_TestObj(newv);
}

_MEMBER_FUNCTION_IMPL(TestObj,_set) {
  StackHandler sa(v);
  _CHECK_SELF(CTestObj,TestObj);
  const SQChar *s = sa.GetString(2);
  int index = s?s[0]:sa.GetInt(2);
  switch(index) {
  case 0: case 'x': case 'r':
    return sa.Return(self->x = sa.GetFloat(3));
    break;
  case 1: case 'y': case 'g':
    return sa.Return(self->y = sa.GetFloat(3));
    break;
  case 2: case 'z': case 'b':
    return sa.Return(self->z = sa.GetFloat(3));
    break;
  } // switch
  return SQ_ERROR;
}

_MEMBER_FUNCTION_IMPL(TestObj,_get) {
  StackHandler sa(v);
  _CHECK_SELF(CTestObj,TestObj);
  const SQChar *s = sa.GetString(2);
  if(s && (s[1] != 0)) return SQ_ERROR;
  int index = s && (s[1] == 0)?s[0]:sa.GetInt(2);
  switch(index) {
  case 0: case 'x': case 'r': return sa.Return(self->x); break;
  case 1: case 'y': case 'g':	return sa.Return(self->y); break;
  case 2: case 'z': case 'b': return sa.Return(self->z); break;
  } // switch
  return SQ_ERROR;
}

_MEMBER_FUNCTION_IMPL(TestObj,update) {
  StackHandler sa(v);
  _CHECK_SELF(CTestObj,TestObj);
  SQObjectType type = (SQObjectType)sa.GetType(2);
  if (type == OT_FLOAT || type == OT_INTEGER) {
    float t = sa.GetFloat(2);
    self->update(t);
  } else {
    SQChar buff[256];
    scsprintf(buff,_T("Invalid type for CTestObj::update(float): type %d\n"),type);
//    OutputDebugString(buff);
    SCPUTS(buff);
  } // if
  return SQ_OK;;
}

_MEMBER_FUNCTION_IMPL(TestObj,print) {
  StackHandler sa(v);
  _CHECK_SELF(CTestObj,TestObj);
  SQChar buff[256];
  scsprintf(buff,_T("x: %f y: %f z: %f\n"),self->x,self->y,self->z);
//  OutputDebugString(buff);
  SCPUTS(buff);
//  return sa.ThrowError(_SC("Error initializing the device"));
  return SQ_OK;;
}

_MEMBER_FUNCTION_IMPL(TestObj,_print) {
  _CHECK_SELF(CTestObj,TestObj);
  SCPUTS(_T("_print: "));
  return __TestObj_print(v);
}

_BEGIN_CLASS(TestObj)
_MEMBER_FUNCTION(TestObj,constructor,1,_T("x")) // x = instance ('self/this' not yet created), no arguments.
_MEMBER_FUNCTION(TestObj,_set,3,_T("xs|n"))     // x = instance, string, or int/float, as .x, .y, .z, or [0], [1], [2].
_MEMBER_FUNCTION(TestObj,_get,2,_T("xs|n"))     // x = instance, string, or int/float, as .x, .y, .z, or [0], [1], [2].
_MEMBER_FUNCTION(TestObj,update,2,_T("xn"))     // x = instance (this), n = int or float.
_MEMBER_FUNCTION(TestObj,print,1,_T("x"))       // x = instance (this).
_MEMBER_FUNCTION(TestObj,_print,1,_T("x"))      // x = instance (this).
_END_CLASS(TestObj)

#ifdef SQUNICODE 
#define scvprintf vwprintf 
#else 
#define scvprintf vprintf 
#endif 

void printfunc(HSQUIRRELVM v,const SQChar *s,...) { 
  va_list arglist;
  va_start(arglist, s);
  scvprintf(s, arglist);
  va_end(arglist);
}

int testFunc(HSQUIRRELVM v) {
  StackHandler sa(v);
  int paramCount = sa.GetParamCount();
  scprintf(_T("testFunc: numParams[%d]\n"),paramCount);
  for (int i=1; i <= paramCount; i++) {
    scprintf(_T("param[%d]: "),i);
    switch(sa.GetType(i)) {
    case OT_TABLE:   scprintf(_T("OT_TABLE[0x%x]\n"),sa.GetObjectHandle(i)); break;
    case OT_INTEGER: scprintf(_T("OT_INTEGER[%d]\n"),sa.GetInt(i));    break;
    case OT_FLOAT:   scprintf(_T("OT_FLOAT[%f]\n"),sa.GetFloat(i));    break;
    case OT_STRING:  scprintf(_T("OT_STRING[%s]\n"),sa.GetString(i));  break;
    default:
      scprintf(_T("TYPEID[%d]\n"),sa.GetType(i));
    } // switch
  } // for
  return SQ_OK;
} // testFunc

// === BEGIN User Pointer version ===
#if 0

int setVarFunc2(HSQUIRRELVM v) {
  StackHandler sa(v);
  if (sa.GetType(1) == OT_TABLE) {
    HSQOBJECT htable = sa.GetObjectHandle(1);
    SquirrelObject table(htable);
    const SQChar * el = sa.GetString(2);
    SquirrelObject upValMapPtr  = table.GetValue(_T("_uvp"));
    SquirrelObject upValMapType = table.GetValue(_T("_uvt"));
    if (!upValMapType.Exists(el)) {
      return SQ_ERROR;
    } // if
    int vType = upValMapType.GetInt(el);
    switch (vType) {
    case TypeInfo<int>::TypeID: {
      int * val = (int *)upValMapPtr.GetUserPointer(el);
      if (val) {
        *val = sa.GetInt(3);
        return sa.Return(*val);
      } else {
        return sa.Return(-1);
      } // if
    } // case
    case TypeInfo<float>::TypeID: {
      float * val = (float *)upValMapPtr.GetUserPointer(el);
      if (val) {
        *val = sa.GetFloat(3);
        return sa.Return(*val);
      } else {
        return sa.Return(-1);
      } // if
    } // case
    case TypeInfo<bool>::TypeID: {
      bool * val = (bool *)upValMapPtr.GetUserPointer(el);
      if (val) {
        *val = sa.GetBool(3) ? true : false;
        return sa.Return(*val);
      } else {
        return sa.Return(-1);
      } // if
    } // case
    } // switch
  } // if
  return SQ_ERROR;
} // setVarFunc2

int getVarFunc2(HSQUIRRELVM v) {
  StackHandler sa(v);
  if (sa.GetType(1) == OT_TABLE) {
    HSQOBJECT htable = sa.GetObjectHandle(1);
    SquirrelObject table(htable);
    int type = sa.GetType(2);
    const SQChar * el = sa.GetString(2);
    SquirrelObject upValMap     = table.GetValue(_T("_uvp"));
    SquirrelObject upValMapType = table.GetValue(_T("_uvt"));
    if (!upValMapType.Exists(el)) {
      return SQ_ERROR;
    } // if
    int vType = upValMapType.GetInt(el);
    switch (vType) {
    case TypeInfo<int>::TypeID: {
      int * val = (int *)upValMap.GetUserPointer(el);
      if (val) {
        return sa.Return(*val);
      } else {
        return sa.Return(-1);
      } // if
    } // case
    case TypeInfo<float>::TypeID: {
      float * val = (float *)upValMap.GetUserPointer(el);
      if (val) {
        return sa.Return(*val);
      } else {
        return sa.Return(-1);
      } // if
    } // case
    case TypeInfo<bool>::TypeID: {
      bool * val = (bool *)upValMap.GetUserPointer(el);
      if (val) {
        return sa.Return(*val);
      } else {
        return sa.Return(-1);
      } // if
    } // case
    } // switch
  } // if
  return SQ_ERROR;
} // getVarFunc2

template<typename T>
void bindVariable2(SquirrelObject & so,T & var,const SQChar * scriptVarName) {
  SquirrelObject __upValMapPtr;
  SquirrelObject __upValMapType;

  if (so.Exists(_T("_uvp"))) {
    __upValMapPtr  = so.GetValue(_T("_uvp"));
    __upValMapType = so.GetValue(_T("_uvt"));
  } else {
    __upValMapPtr  = SquirrelVM::CreateTable();
    __upValMapType = SquirrelVM::CreateTable();
  } // if

  int varType = TypeInfo<T>::TypeID;
  __upValMapPtr.SetUserPointer(scriptVarName,&var);
  __upValMapType.SetValue(scriptVarName,varType);

  so.SetValue(_T("_uvp"),__upValMapPtr);
  so.SetValue(_T("_uvt"),__upValMapType);

  SquirrelObject delegate = so.GetDelegate();
  if (!delegate.Exists(_T("_set"))) {
    delegate = SquirrelVM::CreateTable();
    SquirrelVM::CreateFunction(delegate,"_set",setVarFunc2,_T("sn|b")); // String var name = number(int or float) or bool.
    SquirrelVM::CreateFunction(delegate,"_get",getVarFunc2,_T("s"));    // String var name.
    so.SetDelegate(delegate);
  } // if

} // bindVariable2
#endif

// === BEGIN Old, initial test versions. ===

int setIntFunc(HSQUIRRELVM v) {
  StackHandler sa(v);
  if (sa.GetType(1) == OT_TABLE) {
    HSQOBJECT htable = sa.GetObjectHandle(1);
    SquirrelObject table(htable);
    const SQChar * el = sa.GetString(2);
    SquirrelObject upValMap = table.GetValue(_T("upValMap"));
    int * val = (int *)upValMap.GetUserPointer(el);
    if (val) {
      *val = sa.GetInt(3);
      return sa.Return(*val);
    } else {
      return sa.Return(-1);
    } // if
  } // if
  return SQ_ERROR;
} // setIntFunc

int getIntFunc(HSQUIRRELVM v) {
  StackHandler sa(v);
  if (sa.GetType(1) == OT_TABLE) {
    HSQOBJECT htable = sa.GetObjectHandle(1);
    SquirrelObject table(htable);
    int type = sa.GetType(2);
    const SQChar * el = sa.GetString(2);
    SquirrelObject upValMap = table.GetValue(_T("upValMap"));
    int * val = (int *)upValMap.GetUserPointer(el);
    if (val) {
      return sa.Return(*val);
    } else {
      return sa.Return(-1);
    } // if
  } // if
  return SQ_ERROR;
} // getIntFunc

// === END Old, initial tests versions. ===

_DECL_STATIC_NAMESPACE(GB); // Globals

_MEMBER_FUNCTION_IMPL(GB,Update) {
  StackHandler sa(v);
  scprintf(_T("GB.Update()\n"));
  return sa.Return(true);
}

enum {TEST_CONST=123};

_BEGIN_NAMESPACE(GB)
_MEMBER_FUNCTION(GB,Update,0,0)
_BEGIN_NAMESPACE_CONSTANTS(GB)
_CONSTANT_IMPL(TEST_CONST,OT_INTEGER)
_END_NAMESPACE(GB,NULL)

#if 0
int getVarName(HSQUIRRELVM v) {
  StackHandler sa(v);
  const SQChar * varName = sq_getlocal(v,1,0);
  return sa.Return(varName);
} // getVarName
#endif

void newtest(void) {
  scprintf(_T("NewTest\n"));
}

SQChar * newtestR1(const SQChar * inString) {
  scprintf(_T("NewTestR1: %s\n"),inString);
  return _T("Returned String");
}

struct NewTestObj {
  ScriptStringVar64 s1;
  ScriptStringVar32 s2;
  int pad;
  int val;
  int c1;
  ScriptStringVar8 c2; // 8 char plus null (max string is 8 printable chars).
  NewTestObj() : val(777) {
    s1 = _T("s1");
    s2 = _T("s2");
    c1 = 996;
    c2 = _T("It's a 997"); // Prints: "It's a 9", as only 8 chars in static buffer (plus null).
  }
  void newtest(void) {
    scprintf(_T("NewTest: %d\n"),val);
  }
  SQChar * newtestR1(const SQChar * inString) {
    scprintf(_T("NewTestR1: Member var val is %d, function arg is %s\n"),val,inString);
    return _T("Returned String");
  }

  int multiArgs(HSQUIRRELVM v) {
    StackHandler sa(v);
    int paramCount = sa.GetParamCount();
    int p1 = sa.GetInt(2);
    int p2 = sa.GetInt(3);
    int p3 = sa.GetInt(4);
    return 0;
  } // multiArgs

  int _set(HSQUIRRELVM v) {
    StackHandler sa(v);
    int paramCount = sa.GetParamCount();
    const SQChar * el = sa.GetString(2);
    val = sa.GetInt(3);
    return sa.Return(val);
//    return setInstanceVarFunc(v);
  }

  int _get(HSQUIRRELVM v) {
    StackHandler sa(v);
    int paramCount = sa.GetParamCount();
//    return getInstanceVarFunc(v);
    return sa.Return(val);
  }

};

struct CustomTestObj {
  ScriptStringVar128 name;
  int val;
  bool state;
  CustomTestObj() : val(0), state(false) { name = _T("empty"); }
  CustomTestObj(const SQChar * _name,int _val,bool _state) : val(_val), state(_state) {
    name = _name;
  }
  static int construct(HSQUIRRELVM v) {
    StackHandler sa(v);
    int paramCount = sa.GetParamCount();
    if (paramCount == 1) {
      return PostConstruct(v,new CustomTestObj(),release);
    } if (paramCount == 4) {
      return PostConstruct(v,new CustomTestObj(sa.GetString(2),sa.GetInt(3),sa.GetBool(4)?true:false),release);
    } // if
    return sq_throwerror(v,_T("Invalid Constructor arguments"));
  } // construct

  SQ_DECLARE_RELEASE(CustomTestObj)

  // Member function that handles variable types.
  int varArgTypes(HSQUIRRELVM v) {
    StackHandler sa(v);
    int paramCount = sa.GetParamCount();
    if (sa.GetType(2) == OT_INTEGER) {
      val = sa.GetInt(2);
    } // if
    if (sa.GetType(2) == OT_STRING) {
      name = sa.GetString(2);
    } // if
    if (sa.GetType(3) == OT_INTEGER) {
      val = sa.GetInt(3);
    } // if
    if (sa.GetType(3) == OT_STRING) {
      name = sa.GetString(3);
    } // if
    return 0;
  } // varArgTypes

  // Member function that handles variable types and has variable return types+count.
  int varArgTypesAndCount(HSQUIRRELVM v) {
    StackHandler sa(v);
    int paramCount = sa.GetParamCount();
    SQObjectType type1 = (SQObjectType)sa.GetType(1); // Always OT_INSTANCE
    SQObjectType type2 = (SQObjectType)sa.GetType(2);
    SQObjectType type3 = (SQObjectType)sa.GetType(3);
    SQObjectType type4 = (SQObjectType)sa.GetType(4);
    int returnCount = 0;
    if (paramCount == 3) {
      sq_pushinteger(v,val);
      returnCount = 1;
    } else if (paramCount == 4) {
      sq_pushinteger(v,val);
      sq_pushstring(v,name,-1);
      returnCount = 2;
    } // if
    return returnCount;
  } //

  int noArgsVariableReturn(HSQUIRRELVM v) {
    if (val == 123) {
      val++;
      return 0; // This will print (null).
    } else if (val == 124) {
      sq_pushinteger(v,val); // Will return int:124.
      val++;
      return 1;
    } else if (val == 125) {
      sq_pushinteger(v,val);
      name = _T("Case 125");
      sq_pushstring(v,name,-1);
      val = 123; // reset
      return 2;
    } // if
    return 0;
  } // noArgsVariableReturn

  // Registered with func() instead of funcVarArgs(): fixed (single) return type.
  const SQChar * variableArgsFixedReturnType(HSQUIRRELVM v) {
    StackHandler sa(v);
    int paramCount = sa.GetParamCount();
    SQObjectType type1 = (SQObjectType)sa.GetType(1); // Always OT_INSTANCE
    SQObjectType type2 = (SQObjectType)sa.GetType(2);
    SQObjectType type3 = (SQObjectType)sa.GetType(3);
    if (paramCount == 1) {
      return _T("No Args");
    } else if (paramCount == 2) {
      return _T("One Arg");
    } else if (paramCount == 3) {
      return _T("Two Args");
    } // if
    return _T("More than two args");
  } // variableArgsFixedReturnType

  void manyArgs(int i,float f,bool b,const SQChar * s) {
    scprintf(_T("i: %d, f: %f, b: %s, s: %s\n"),i,f,b?_T("true"):_T("false"),s);
  } // manyArgs

  float manyArgsR1(int i,float f,bool b,const SQChar * s)  {
    manyArgs(i,f,b,s);
    return i+f;
  } // manyArgsR1

};

#if 0
struct Base {
  int val1;
  ScriptStringVar16 nameBase;
  Base() : nameBase(_T("Base")) {
    val1 = 123;
  }
  void funcBase(void) {
    scprintf(_T("Val1: %d, Name: %s\n"),val1,nameBase.s);
  }
};

struct Derived {
  int val2;
  ScriptStringVar16 nameDerived;
  Derived() : nameDerived(_T("Derived")), val2(456) {
    val2 = 456;
  }
  void funcDerived(void) {
    scprintf(_T("Val2: %d, Name: %s\n"),val2,nameDerived.s);
  }
};
#endif

//SQ_DECLARE_CLASS(NewTestObj);

class TestSqPlus {
public:
  void init(void) {
    SquirrelVM::Init();
//    sq_setprintfunc(SquirrelVM::GetVMPtr(),printfunc); //sets the print function.
    _INIT_CLASS(TestObj);
    _INIT_STATIC_NAMESPACE(GB);
  } // if

  TestSqPlus() {
    init();

    try {
      HSQUIRRELVM v = SquirrelVM::GetVMPtr();

      // === BEGIN Global Function binding tests ===

      // Implemented as SquirrelVM::CreateFunction(rootTable,func,name,typeMask);
      SquirrelVM::CreateFunctionGlobal(&testFunc,_T("testFunc0"));
      SquirrelVM::CreateFunctionGlobal(&testFunc,_T("testFuncN"),_T("n"));
      SquirrelVM::CreateFunctionGlobal(&testFunc,_T("testFuncS"),_T("s"));

      // === Register Standard Functions ===

      RegisterGlobal(v,&newtest,_T("test"));
      RegisterGlobal(v,&newtestR1,_T("testR1"));

      // === Register Member Functions to existing classes (as opposed to instances of classes) ===

      NewTestObj t1,t2,t3;
      t1.val = 123;
      t2.val = 456;
      t3.val = 789;
      RegisterGlobal(v,t1,&NewTestObj::newtest,_T("testObj_newtest1"));
      RegisterGlobal(v,t2,&NewTestObj::newtest,_T("testObj_newtest2")); // Register newtest() again with different name and object pointer.
      SquirrelObject tr = SquirrelVM::GetRootTable(); // Can be any object supporting closures (functions).
      Register(v,tr.GetObjectHandle(),t3,&NewTestObj::newtestR1,_T("testObj_newtestR1")); // Return value version.

      // === END Global Function binding tests ===

      // === BEGIN Class Instance tests ===

#if 0
      SQClassDef<NewTestObj> sqClass(_T("NewTestObj"));
      sqClass.func(&NewTestObj::newtestR1,_T("newtestR1"));
      sqClass.var(&NewTestObj::val,_T("val"));
      sqClass.var(&NewTestObj::s1,_T("s1"));
      sqClass.var(&NewTestObj::s2,_T("s2"));
      sqClass.var(&NewTestObj::c1,_T("c1"),VAR_ACCESS_READ_ONLY);
      sqClass.var(&NewTestObj::c2,_T("c2"),VAR_ACCESS_READ_ONLY);
      sqClass.funcVarArgs(&NewTestObj::multiArgs,_T("multiArgs"));
#else
      SQClassDef<NewTestObj>(_T("NewTestObj")).
        func(&NewTestObj::newtestR1,_T("newtestR1")).
        var(&NewTestObj::val,_T("val")).
        var(&NewTestObj::s1,_T("s1")).
        var(&NewTestObj::s2,_T("s2")).
        var(&NewTestObj::c1,_T("c1"),VAR_ACCESS_READ_ONLY).
        var(&NewTestObj::c2,_T("c2"),VAR_ACCESS_READ_ONLY).
        funcVarArgs(&NewTestObj::multiArgs,_T("multiArgs"));
#endif

      SQClassDef<CustomTestObj> customClass(_T("CustomTestObj"));
      customClass.staticFuncVarArgs(&CustomTestObj::construct,_T("constructor"),_T("snb"));            // string, number, bool (all types must match).
      customClass.funcVarArgs(&CustomTestObj::varArgTypes,_T("varArgTypes"),_T("s|ns|ns|ns|n"));       // string or number + string or number.
      customClass.funcVarArgs(&CustomTestObj::varArgTypesAndCount,_T("varArgTypesAndCount"),_T("*"));  // "*"): no type or count checking.
      customClass.funcVarArgs(&CustomTestObj::noArgsVariableReturn,_T("noArgsVariableReturn"));        // No type string means no arguments allowed.
      customClass.func(&CustomTestObj::variableArgsFixedReturnType,_T("variableArgsFixedReturnType")); // Variables args, fixed return type.
      customClass.func(&CustomTestObj::manyArgs,_T("manyArgs"));                                       // Many args, type checked.
      customClass.func(&CustomTestObj::manyArgsR1,_T("manyArgsR1"));                                   // Many args, type checked, one return value.

// Old macro-based method. Must use SQ_DECLARE_CLASS() macro above.
#if 0
      SquirrelObject newClass = SQ_REGISTER_CLASS(NewTestObj);
      SQ_REGISTER_INSTANCE(newClass,NewTestObj,newtestR1);
// Currently, can use either automatic variable handling OR manual handling (but not both at once).
#if 1
      SQ_REGISTER_INSTANCE_VARIABLE(newClass,NewTestObj,val); // _set/_get will be defined to use automatic methods for val.
#else
      SQ_REGISTER_INSTANCE_VARARGS(newClass,NewTestObj,_set); // _set is now defined and won't be overridden.
      SQ_REGISTER_INSTANCE_VARARGS(newClass,NewTestObj,_get);
      SQ_REGISTER_INSTANCE_VARIABLE(newClass,NewTestObj,val); // Access will be through _set/_get member functions above, not automatic methods.
#endif

#if 1
      // With an HSQUIRRELVM argument, can handle variable args, and when registered this way can return multiple values.
      SQ_REGISTER_INSTANCE_VARARGS(newClass,NewTestObj,multiArgs);
#else
      // With an HSQUIRRELVM argument, can handle variable args, but can only return one value (defined by the function definition).
      SQ_REGISTER_INSTANCE(newClass,NewTestObj,multiArgs); 
#endif
#endif

#if 1
      SquirrelObject testReg0 = SquirrelVM::CompileBuffer(_T(" co <- CustomTestObj(\"hello\",123,true); co.varArgTypes(\"str\",123,123,\"str\"); co.varArgTypes(123,\"str\",\"str\",123); "));
      SquirrelVM::RunScript(testReg0);

      SquirrelObject testReg0a = SquirrelVM::CompileBuffer(_T(" print(co.varArgTypesAndCount(1,true)); print(co.varArgTypesAndCount(2,false,3.)); print(\"\\n\"); "));
      SquirrelVM::RunScript(testReg0a);

      SquirrelObject testReg0b = SquirrelVM::CompileBuffer(_T(" print(co.noArgsVariableReturn()); print(co.noArgsVariableReturn()); print(co.noArgsVariableReturn()); print(\"\\n\"); "));
      SquirrelVM::RunScript(testReg0b);

      SquirrelObject testReg0c = SquirrelVM::CompileBuffer(_T(" print(co.variableArgsFixedReturnType(1)); print(co.variableArgsFixedReturnType(1,2)); print(co.variableArgsFixedReturnType(1,2,3)); print(\"\\n\"); "));
      SquirrelVM::RunScript(testReg0c);

      SquirrelObject testReg0d = SquirrelVM::CompileBuffer(_T(" co.manyArgs(111,222.2,true,\"Hello\"); print(co.manyArgsR1(333,444.3,false,\"World\")); print(\"\\n\"); "));
      SquirrelVM::RunScript(testReg0d);
#endif

// Inheriting from an existing base class in this way is not currently supported.
// Requires either a Squirrel language/behavior change, or extra code at the interface
// layer to allocate memory and call constructor of parent class (and perhaps
// handle constructor args) store pointer in UserData, and handle proper variable access
// (get correct class/struct pointer) and member function calls (store class
// type/id in function up-var and search/hash for actual 'this' pointer for function call).
#if 0
      SQClassDef<Base>(_T("Base")).
        var(&Base::nameBase,_T("nameBase")).
        func(&Base::funcBase,_T("funcBase"));

      SQClassDef<Derived>("Derived",_T("Base")).
        var(&Derived::nameDerived,_T("nameDerived")).
        func(&Derived::funcDerived,_T("funcDerived"));

//      SquirrelObject testBaseDerived = SquirrelVM::CompileBuffer(_T(" local base = Base(); print(base.nameBase); local derived = Derived(); print(\"NameBase: \"+derived.nameBase+\" NameDerived: \"+derived.nameDerived); derived.funcBase(); "));
      SquirrelObject testBaseDerived = SquirrelVM::CompileBuffer(_T(" local derived = Derived(); print(\"NameBase: \"+derived.nameBase+\" NameDerived: \"+derived.nameDerived); derived.funcBase(); "));
      SquirrelVM::RunScript(testBaseDerived);
#endif

#if 1
      SquirrelObject testReg1a = SquirrelVM::CompileBuffer(_T(" co <- CustomTestObj(\"hello\",123,true); co.noArgsVariableReturn(); local t = NewTestObj(); print(\"C1: \"+t.c1); print(\"C2: \"+t.c2); // t.c1 = 123; "));
      SquirrelVM::RunScript(testReg1a);

      // Constant test (read only var). Var can change on C++ side, but not on script side.
      try {
        SquirrelObject testRegConstant = SquirrelVM::CompileBuffer(_T(" local t = NewTestObj(); t.c1 = 123; "));
        SquirrelVM::RunScript(testRegConstant);
      } // try
      catch (SquirrelError & e) {
        SQChar buff[256];
        scsprintf(buff,_T("Error: %s, %s\n"),e.desc,_T("Squirrel::TestConstant"));
        //      OutputDebugString(buff);
        SCPUTS(buff);
      } // catch

      SquirrelObject testReg1 = SquirrelVM::CompileBuffer(_T(" local t = NewTestObj(); t.newtestR1(\"Hello\"); t.val = 789; print(t.val); print(t.s1); print(t.s2); t.s1 = \"New S1\"; print(t.s1); "));
      SquirrelVM::RunScript(testReg1);

      SquirrelObject testReg2 = SquirrelVM::CompileBuffer(_T(" local t = NewTestObj(); t.val = 789; print(t.val); t.val = 876; print(t.val); t.multiArgs(1,2,3); t.multiArgs(1,2,3,4); "));
      SquirrelVM::RunScript(testReg2);
      SquirrelObject testReg3 = SquirrelVM::CompileBuffer(_T(" test(); local rv = testR1(\"Hello\"); print(rv); "));
      SquirrelVM::RunScript(testReg3);     
      SquirrelObject testReg4 = SquirrelVM::CompileBuffer(_T(" print(\"\\nMembers:\"); testObj_newtest1(); testObj_newtest2(); print(testObj_newtestR1(\"Hello Again\")); "));
      SquirrelVM::RunScript(testReg4);

      SquirrelObject defCallFunc = SquirrelVM::CompileBuffer(_T(" function callMe(var) { print(\"I was called by: \"+var); return 123; }"));
      SquirrelVM::RunScript(defCallFunc);

      SquirrelObject root = SquirrelVM::GetRootTable();

      // Get a function from the root table and call it.
      SquirrelFunction<int> callFunc(_T("callMe"));
      int ival = callFunc(_T("Squirrel"));
      scprintf(_T("IVal: %d\n"),ival);
      ival = 0;
      // Get a function from any table.
      SquirrelFunction<int> callFunc2(root.GetObjectHandle(),_T("callMe"));
      ival = callFunc(456); // Argument count is checked; type is not.

      // === END Class Instance tests ===

      // === BEGIN macro-only class-registrated tests ===
//      SquirrelVM::CreateFunctionGlobal(_T("getName",getVarName,"*")); // * = any type.
//      SquirrelObject main = SquirrelVM::CompileBuffer(_T("local testObj = TestObj(); testObj.print(); testObj.update(\"ab\"); testObj.print()"));
//      SquirrelObject main = SquirrelVM::CompileBuffer(_T("local LF = \"\\n\"; local testObj = TestObj(); testObj.print(); testObj.update(1.5); testObj.print(); testObj.y += 10.; testObj.z = -1.; print(testObj.y+LF); print(\"Array: \"+testObj[0]+LF); print(testObj); print(LF); "));
//      SquirrelObject main = SquirrelVM::CompileBuffer(_T("local testObj = TestObj(); testObj.z = -1.; print(testObj); testFunc(); testFunc0(); testFuncN(1.); testFuncS(\"Hello\"); "));
//      SquirrelObject main = SquirrelVM::CompileBuffer(_T("testFunc0(); testFuncN(1); testFuncN(1.23); testFuncS(\"Hello\");"));

      SquirrelObject main = SquirrelVM::CompileBuffer(_T("table1 <- {key1=\"keyVal\",key2 = 123};\n if (\"key1\" in table1)\n print(\"Sq: Found it\");\n else\n print(\"Sq: Not found\");"));
      SquirrelVM::RunScript(main);
      SquirrelObject table1 = root.GetValue(_T("table1"));
      if (table1.Exists(_T("key1"))) {
        scprintf(_T("C++: Found it.\n"));
      } else {
        scprintf(_T("C++: Did not find it.\n"));
      } // if

      // === BEGIN Simple variable binding tests ===

      int iVar = 777;
      float fVar = 88.99f;
      bool bVar = true;
      BindVariable(root,&iVar,_T("iVar"));
      BindVariable(root,&fVar,_T("fVar"));
      BindVariable(root,&bVar,_T("bVar"));

      static ScriptStringVar128 testString;
      scsprintf(testString,_T("This is a test string"));
      BindVariable(root,&testString,_T("testString"));

      // === END Simple variable binding tests ===

      // === BEGIN Array Tests ===

      SquirrelObject array = SquirrelVM::CreateArray(10);
      int i;
      for (i = 0; i < 10; i++) array.SetValue(i,i);
      array.ArrayAppend(123);          // int
      array.ArrayAppend(true);         // bool (must use bool and not SQBool (SQBool is treated as INT by compiler).
      array.ArrayAppend(false);        // bool (must use bool and not SQBool (SQBool is treated as INT by compiler).
      array.ArrayAppend(123.456f);     // float
      array.ArrayAppend(_T("string")); // string
      array.ArrayAppend(456);          // Will be popped and thrown away (below).

      // Pop 3 items from array:
      array.ArrayPop(SQFalse);                 // Don't retrieve the popped value (int:123).
      SquirrelObject so1 = array.ArrayPop();   // Retrieve the popped value.
      const SQChar * val1 = so1.ToString();    // Get string.
      float val2 = array.ArrayPop().ToFloat(); // Pop and get float.
      scprintf(_T("[Popped values] Val1: %s, Val2: %f\n"),val1,val2);

      int startIndex = array.Len();
      array.ArrayExtend(10); // Implemented as: ArrayResize(Len()+amount).
      for (i = startIndex; i < array.Len(); i++) array.SetValue(i,i*10);
      root.SetValue(_T("array"),array);

      SquirrelObject arrayr = array.Clone(); // Get a copy as opposed to another reference.
      arrayr.ArrayReverse();
      root.SetValue(_T("arrayr"),arrayr);

      // === END Array Tests ===

//      SquirrelObject test = SquirrelVM::CompileBuffer(_T(" print(iVar); print(fVar); print(bVar); iVar += 1; fVar += 100.; bVar = false; print(iVar); print(fVar); print(bVar); xVar = 1; ")); // Test for xVar error.
//      SquirrelObject test = SquirrelVM::CompileBuffer(_T(" print(iVar); print(fVar); print(bVar); iVar += 1; fVar += 100.; bVar = false; print(iVar); print(fVar); print(bVar); print(testString); testString = \"New string value\"; print(testString);"));
      SquirrelObject define_printArray = SquirrelVM::CompileBuffer(_T(" function printArray(name,array) { print(name+\".len() = \"+array.len()); foreach(i, v in array) if (v != null) { if (typeof v == \"bool\") v = v ? \"true\" : \"false\"; print(\"[\"+i+\"]: \"+v); } } "));
      SquirrelVM::RunScript(define_printArray);
      SquirrelObject test = SquirrelVM::CompileBuffer(_T(" printArray(\"array\",array); printArray(\"arrayr\",arrayr); "));

//      SquirrelObject test = SquirrelVM::CompileBuffer(_T(" iVar = 1; print(iVar); "));
//      SquirrelObject test = SquirrelVM::CompileBuffer(_T(" print(\"GB:\"+GB.TEST_CONST); GB.TEST_CONST += 1; GB.Update(); print(\"GB2:\"+GB.TEST_CONST); \n"));

      SquirrelVM::RunScript(test);
#endif

    } // try
    catch (SquirrelError & e) {
//      SquirrelVM::DumpStack();
      SQChar buff[256];
      scsprintf(buff,_T("Error: %s, %s\n"),e.desc,_T("Squirrel::TestObj"));
//      OutputDebugString(buff);
      SCPUTS(buff);
    } // catch

  }

  ~TestSqPlus() {
    SquirrelVM::Shutdown();
  }

};

void doTest(void) {
  TestSqPlus testSqPlus;
} // doTest

int main(int argc,char * argv[]) {

  // Run twice to make sure cleanup/shutdown works OK.
  SCPUTS(_T("Start Pass 1\n"));
  doTest();
#if 0
  SCPUTS(_T("Start Pass 2\n"));
  doTest();
#endif
  SCPUTS(_T("Done.\n"));

  scprintf(_T("Press RETURN to exit."));
  getchar();

	return 0;
}

