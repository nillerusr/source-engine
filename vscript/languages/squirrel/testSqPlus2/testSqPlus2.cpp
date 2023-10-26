// testSqPlus2.cpp
// Created by John Schultz 9/21/2005
// Free for any use.

// Step through the code with a debugger (setting breakpoints in functions)
// to get an idea how everything works.

#include <stdarg.h> 
#include <stdio.h>
#if 0
#include <string>
//#define SQPLUS_SUPPORT_STD_STRING
#define SQPLUS_SUPPORT_SQ_STD_STRING
#endif
//#define SQPLUS_CONST_OPT
#include "sqplus.h"

using namespace SqPlus;

void scprintfunc(HSQUIRRELVM v,const SQChar *s,...) { 
  static SQChar temp[2048];
  va_list vl;
  va_start(vl,s);
  scvsprintf( temp,s,vl);
  SCPUTS(temp);
  va_end(vl);
}

void newtest(void) {
  scprintf(_T("NewTest\n"));
}

SQChar * newtestR1(const SQChar * inString) {
  scprintf(_T("NewTestR1: %s\n"),inString);
  return _T("Returned String");
}

struct Vector3 {
  static float staticVar;
  float x,y,z;
  Vector3() {
    x = 1.f;
    y = 2.f;
    z = 3.f;
  }
  Vector3(float _x,float _y,float _z) : x(_x), y(_y), z(_z) {}
  ~Vector3() {
    scprintf(_T("~Vector()\n"));
  }
  Vector3 Inc(Vector3 & v) {
    x += v.x;
    y += v.y;
    z += v.z;
    return *this;
  } // Inc
  Vector3 operator+(Vector3 & v) {
    return Vector3(x+v.x,y+v.y,z+v.z);
  }
};


float Vector3::staticVar = 898.434f;

#if 0 // It may be possible to make this method work in the future. If so, the DECLARE_INSTANCE_FUNCS() macro
      // would not be needed. The issue is duplicate compiler matching for const SQChar * and Push():
      // Push(const SQChar * &) and Push(const SQChar *) both match.
      // The typeid() compiler function may not be portable to other compilers.
#include <typeinfo.h>
template<typename TYPE>
inline const SQChar * GetTypeName(const TYPE & n)            { return typeid(TYPE).name(); }
template<typename TYPE>
inline void Push(HSQUIRRELVM v,const TYPE & value)           { CreateCopyInstance(GetTypeName(value),value); }
template<typename TYPE>
inline bool	Match(TypeWrapper<TYPE &>,HSQUIRRELVM v,int idx) { return  GetInstance<TYPE>(v,idx) != NULL; }
template<typename TYPE>
inline TYPE & Get(TypeWrapper<TYPE &>,HSQUIRRELVM v,int idx) { return *GetInstance<TYPE>(v,idx); }
#endif

DECLARE_INSTANCE_TYPE(Vector3)

Vector3 Add2(Vector3 & a,Vector3 & b) {
  Vector3 c;
  c.x = a.x + b.x;
  c.y = a.y + b.y;
  c.z = a.z + b.z;
  return c;
} // Add2

int Add(HSQUIRRELVM v) {
//  StackHandler sa(v);
  Vector3 * self = GetInstance<Vector3,true>(v,1);
  Vector3 * arg  = GetInstance<Vector3,true>(v,2);
//  SquirrelObject so = sa.GetObjectHandle(1);
#if 0
  SQUserPointer type=0;
  so.GetTypeTag(&type);
  SQUserPointer reqType = ClassType<Vector3>::type();
  if (type != reqType) {
    throw SquirrelError(_T("Invalid class type"));
  } // if
#endif
//  Vector3 * self = (Vector3 *)so.GetInstanceUP(ClassType<Vector3>::type());
//  if (!self) throw SquirrelError(_T("Invalid class type"));
  Vector3 tv;
  tv.x = arg->x + self->x;
  tv.y = arg->y + self->y;
  tv.z = arg->z + self->z;
  return ReturnCopy(v,tv);
}

struct NewTestObj {
  ScriptStringVar64 s1;
  ScriptStringVar32 s2;
  bool b;
  int val;
  int c1;
  ScriptStringVar8 c2; // 8 char plus null (max string is 8 printable chars).
  NewTestObj() : val(777) {
    s1 = _T("s1=s1");
    s2 = _T("s2=s2");
    c1 = 996;
    c2 = _T("It's a 997"); // Prints: "It's a 9", as only 8 chars in static buffer (plus null).
  }

  NewTestObj(const SQChar * _s1,int _val,bool _b) {
    s1  = _s1;
    val = _val;
    b   = _b;
    s2 = _T("s2=s2");
    c1 = 993;
    c2 = _T("It's a 998"); // Prints: "It's a 9", as only 8 chars in static buffer (plus null).
  }

  static int construct(HSQUIRRELVM v) {
//    StackHandler sa(v);
//    SquirrelObject so = sa.GetObjectHandle(1);
    return PostConstruct<NewTestObj>(v,new NewTestObj(),release);
  } // construct

  SQ_DECLARE_RELEASE(NewTestObj) // Required when using a custom constructor.

  void newtest(void) {
    scprintf(_T("NewTest: %d\n"),val);
  }
  SQChar * newtestR1(const SQChar * inString) {
    scprintf(_T("NewTestR1: Member var val is %d, function arg is %s\n"),val,inString);
    return _T("Returned String");
  }

  int multiArgs(HSQUIRRELVM v) {
    StackHandler sa(v);
    SquirrelObject so = sa.GetObjectHandle(1);
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
  }

  int _get(HSQUIRRELVM v) {
    StackHandler sa(v);
    int paramCount = sa.GetParamCount();
    return sa.Return(val);
  }

};

// Using global functions to construct and release classes.
int releaseNewTestObj(SQUserPointer up,SQInteger size) {
  SQ_DELETE_CLASS(NewTestObj);
} // releaseNewTestObj

int constructNewTestObj(HSQUIRRELVM v) {
  StackHandler sa(v);
  int paramCount = sa.GetParamCount();
  if (paramCount == 1) {
    return PostConstruct<NewTestObj>(v,new NewTestObj(),releaseNewTestObj);
  } else if (paramCount == 4) {
    return PostConstruct<NewTestObj>(v,new NewTestObj(sa.GetString(2),sa.GetInt(3),sa.GetBool(4)?true:false),releaseNewTestObj);
  } // if
  return sq_throwerror(v,_T("Invalid Constructor arguments"));
} // constructNewTestObj

// Using fixed args with auto-marshaling. Note that the HSQUIRRELVM must be last in the argument list (and must be present to send to PostConstruct).
// SquirrelVM::GetVMPtr() could also be used with PostConstruct(): no HSQUIRRELVM argument would be required.
int constructNewTestObjFixedArgs(const SQChar * s,int val,bool b,HSQUIRRELVM v) {
  StackHandler sa(v);
  int paramCount = sa.GetParamCount();
  return PostConstruct<NewTestObj>(v,new NewTestObj(s,val,b),releaseNewTestObj);
} // constructNewTestObj

// Will be registered in a class namespace.
void globalFunc(const SQChar * s,int val) {
  scprintf(_T("globalFunc: s: %s val: %d\n"),s,val);
} // globalFunc

class GlobalClass {
public:
  void func(const SQChar * s,int val) {
    scprintf(_T("globalClassFunc: s: %s val: %d\n"),s,val);
  } // func
} globalClass;

struct CustomTestObj {
  ScriptStringVar128 name;
  int val;
  bool state;
  CustomTestObj() : val(0), state(false) { name = _T("empty"); }
  CustomTestObj(const SQChar * _name,int _val,bool _state) : val(_val), state(_state) {
    name = _name;
  }

  // Custom variable argument constructor
  static int construct(HSQUIRRELVM v) {
    StackHandler sa(v);
    int paramCount = sa.GetParamCount();
    if (paramCount == 1) {
      return PostConstruct<CustomTestObj>(v,new CustomTestObj(),release);
    } if (paramCount == 4) {
      return PostConstruct<CustomTestObj>(v,new CustomTestObj(sa.GetString(2),sa.GetInt(3),sa.GetBool(4)?true:false),release);
    } // if
    return sq_throwerror(v,_T("Invalid Constructor arguments"));
  } // construct

  SQ_DECLARE_RELEASE(CustomTestObj) // Required when using a custom constructor.

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
//    return sa.ThrowError(_T("varArgTypes() error"));
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

// === Standard (non member) function ===
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

int globalVar = 5551234;

class Creature {
  int health;
public:
  enum {MaxHealth=100};
  Creature() : health(MaxHealth) {}
  int GetMaxHealth(void) {
    return MaxHealth;
  }
  int GetHealth(void) {
    return health;
  }
  void SetHealth(int newHealth) {
    health = newHealth;
  }
};

DECLARE_INSTANCE_TYPE(Creature)

// === BEGIN Class Instance Test ===

class PlayerManager {
public:
  struct Player {
    ScriptStringVar64 name;
    void printName(void) {
      scprintf(_T("Player.name = %s\n"),name.s);
    }
  };
  Player playerVar; // Will be accessed directly.
  Player players[2];
  Player * GetPlayer(int player) { // Must return pointer: a returned reference will behave the same as return by value.
    return &players[player];
  }
  PlayerManager() {
    players[0].name = _T("Player1");
    players[1].name = _T("Player2");
    playerVar.name  = _T("PlayerVar");
  }
} playerManager;

DECLARE_INSTANCE_TYPE(PlayerManager)
DECLARE_INSTANCE_TYPE(PlayerManager::Player)

PlayerManager * getPlayerManager(void) { // Must return pointer: a returned reference will behave the same as return by value.
  return &playerManager;
}

// Example from forum post question:
class STestScripts {}; // Proxy class
class TestScripts {
public:
  int Var_ToBind1,Var_ToBind2;

  void InitScript1(void) {
    Var_ToBind1 = 808;
    RegisterGlobal(*this,&TestScripts::Test1,_T("Test1"));
    RegisterGlobal(*this,&TestScripts::Test2,_T("Test2"));
    BindVariable(&Var_ToBind1,_T("Var_ToBind1"));
  } // InitScript1

  void InitScript2(void) {
    Var_ToBind2 = 909;
    SQClassDef<STestScripts>(_T("STestScripts")).
      staticFunc(*this,&TestScripts::Test1,_T("Test1")).
      staticFunc(*this,&TestScripts::Test2,_T("Test2")).
      staticVar(&Var_ToBind2,_T("Var_ToBind2"));
  } // InitScript2

  void Test1(void) {
    scprintf(_T("Test1 called.\n"));
  }
  void Test2(void) {
    scprintf(_T("Test2 called.\n"));
  }
} testScripts;

// From forum questions

#if 1

template<typename T>
struct Point {
  Point() {}
  Point(T X, T Y) : X(X), Y(Y) {}
  T X, Y;
};

template<typename T>
struct Box {
  Box() {}
  Box(Point<T> UpperLeft, Point<T> LowerRight) : UpperLeft(UpperLeft), LowerRight(LowerRight) {}
  Point<T> UpperLeft, LowerRight;
  void print(void) {
    scprintf(_T("UL.X %f UL.Y %f LR.X %f LR.Y %f\n"),UpperLeft.X,UpperLeft.Y,LowerRight.X,LowerRight.Y);
  }
};

template<typename T>
struct Window {
  int id;
  Box<T> box;
};

typedef Point<float> Pointf;
typedef Box<float> Boxf;
typedef Window<float> Windowf;

#else

struct Pointf {
  float X,Y;
  Pointf() {}
  Pointf(float _X, float _Y) : X(_X), Y(_Y) {}
};

struct Boxf {
  Pointf UpperLeft,LowerRight;
  Boxf() {}
  Boxf(Pointf _UpperLeft,Pointf _LowerRight) : UpperLeft(_UpperLeft), LowerRight(_LowerRight) {}
  void print(void) {
    scprintf(_T("UL.X %f UL.Y %f LR.X %f LR.Y %f\n"),UpperLeft.X,UpperLeft.Y,LowerRight.X,LowerRight.Y);
  }
};

struct Windowf {
  int id;
  Boxf box;
};

#endif

DECLARE_INSTANCE_TYPE(Pointf)
DECLARE_INSTANCE_TYPE(Boxf)
DECLARE_INSTANCE_TYPE(Windowf)

int constructPointf(float X,float Y,HSQUIRRELVM v) {
  return PostConstruct<Pointf>(v,new Pointf(X,Y),ReleaseClassPtr<Pointf>::release);
} // constructPointf

// Must pass by reference or pointer (not copy)
int constructBoxf(Pointf & UpperLeft,Pointf & LowerRight,HSQUIRRELVM v) {
  return PostConstruct<Boxf>(v,new Boxf(UpperLeft,LowerRight),ReleaseClassPtr<Boxf>::release);
} // constructBoxf

struct WindowHolder {
  static Windowf * currentWindow;
  static Windowf * getWindow(void) {
    return currentWindow;
  } // getWindow
};
Windowf * WindowHolder::currentWindow = 0;

// From forum post: compiler works OK.
void testCompiler(void) {
  SquirrelObject test = SquirrelVM::CompileBuffer(_T("\
    local SceneManager = getSceneManager() ; \n\
\n\
    SceneManager.AddScene(\"Scene1\") ; \n\
    SceneManager.AddScene(\"Scene4\") ; \n\
    SceneManager.ActivateScene(\"Scene1\") ; \n\
  "));
  SquirrelVM::RunScript(test);
}

void testPointfBoxf(void) {

//  testCompiler();

  SQClassDef<Pointf>(_T("Pointf")).
    staticFunc(constructPointf,_T("constructor")).
    var(&Pointf::X,_T("X")).
    var(&Pointf::Y,_T("Y"));

  SQClassDef<Boxf>(_T("Boxf")).
    staticFunc(constructBoxf,_T("constructor")).
    func(&Boxf::print,_T("print")).
    var(&Boxf::UpperLeft,_T("UpperLeft")).
    var(&Boxf::LowerRight,_T("LowerRight"));

  SQClassDef<Windowf>(_T("Windowf")).
    var(&Windowf::id,_T("Id")).
    var(&Windowf::box,_T("Box"));

  RegisterGlobal(WindowHolder::getWindow,_T("getWindow"));
  Windowf myWindow;
  myWindow.id = 42;
  myWindow.box = Boxf(Pointf(1.f,2.f),Pointf(3.f,4.f));
  WindowHolder::currentWindow = &myWindow;

  // The createWindow() function below creates a new instance on the root table.
  // The instance data is a pointer to the C/C++ instance, and will not be freed
  // or otherwise managed.

  SquirrelObject test = SquirrelVM::CompileBuffer(_T("\
    local MyWindow = Windowf(); \n\
    MyWindow.Box = Boxf(Pointf(11.,22.),Pointf(33.,44.)); \n\
    print(MyWindow.Box.LowerRight.Y); \n\
    MyWindow.Box.LowerRight.Y += 1.; \n\
    local MyWindow2 = Windowf(); \n\
    MyWindow2 = MyWindow; \n\
    print(MyWindow2.Box.LowerRight.Y); \n\
    local MyBox = Boxf(Pointf(10.,20.),Pointf(30.,40.)); \n\
    MyBox.UpperLeft = Pointf(1000.,1000.); \n\
    MyBox.UpperLeft.X = 5000. \n\
    print(MyBox.UpperLeft.X) \n\
    print(MyBox.UpperLeft.Y) \n\
    MyWindow2.Box = MyBox; \n\
    MyWindow2.Box.print(); \n\
    MyWindow2 = getWindow(); \n\
    print(\"MyWindow2: \"+MyWindow2.Id); \n\
    MyWindow2.Box.print(); \n\
    function createWindow(name,instance) { \n\
      ::rawset(name,instance); \n\
    } \n\
  "));
  SquirrelVM::RunScript(test);

  Windowf window = myWindow;
  window.id = 54;
  window.box.UpperLeft.X  += 1;
  window.box.UpperLeft.Y  += 1;
  window.box.LowerRight.X += 1;
  window.box.LowerRight.Y += 1;
  // Create a new Window instance "NewWindow" on the root table.
  SquirrelFunction<void>(_T("createWindow"))(_T("NewWindow"),&window);

  SquirrelObject test2 = SquirrelVM::CompileBuffer(_T("\
    print(\"NewWindow: \"+NewWindow.Id); \n\
    NewWindow.Box.print(); \n\
  "));
  SquirrelVM::RunScript(test2);

} // testPointfBoxf

// Example debug hook: called back during script execution.
SQInteger debug_hook(HSQUIRRELVM v) {
  SQUserPointer up;
  int event_type,line;
  const SQChar *src,*func;
  sq_getinteger(v,2,&event_type);
  sq_getstring(v,3,&src);
  sq_getinteger(v,4,&line);
  sq_getstring(v,5,&func);
  sq_getuserpointer(v,-1,&up);
  return 0;
} // debug_hook

// You can add functions/vars here, as well as bind globals to be accessed through this class as shown in the NameSpace example.
// If the class is instantiated in script, the instance is "locked", preventing accidental changes to elements.
// Thus using an instance as the namespace can be a better design for development.
// If variables/constants are bound to the class and/or non-static/non-global functions, the class must be instantiated before use.
struct NamespaceClass {
};

// === END Class Instance Test ===

class TestBase {
public:
  int x;
  TestBase() : x(0) {
    printf("Constructing TestBase[0x%x]\n",(size_t)this);
  }
  void print(void) {
    printf("TestBase[0x%x], x[%d]\n",(size_t)this,x);
  }
};

DECLARE_INSTANCE_TYPE(TestBase)

class TestDerivedCPP : public TestBase {
public:
  int y;
  TestDerivedCPP() {
    x = 121;
  }
};

typedef void (TestDerivedCPP::*TestDerivedCPP_print)(void);

void testInhertianceCase(void) {

  SQClassDef<TestBase>(_T("TestBase")).
    var(&TestBase::x,_T("x")).
    func(&TestBase::print,_T("print"));

  SQClassDef<TestDerivedCPP>(_T("TestDerivedCPP")).
    func((TestDerivedCPP_print)&TestDerivedCPP::print,_T("print"));

  // Note that the constructor definition and call below is not required for this example.
  // (The C/C++ constructor will be called automatically).

  SquirrelObject testInheritance2 = SquirrelVM::CompileBuffer(_T("\
    class TestDerived extends TestBase { \n\
      function print() {                 \n\
        ::TestBase.print();              \n\
        ::print(\"Derived: \"+x);        \n\
      }                                  \n\
      constructor() {                    \n\
        TestBase.constructor();          \n\
      }                                  \n\
    }                                    \n\
    local a = TestDerived();             \n\
    local b = TestDerived();             \n\
    a.x = 1;                             \n\
    b.x = 2;                             \n\
    print(\"a.x = \"+a.x);               \n\
    print(\"b.x = \"+b.x);               \n\
    a.print();                           \n\
    b.print();                           \n\
    local c = TestDerivedCPP();          \n\
    c.print();                           \n\
  "));
  SquirrelVM::RunScript(testInheritance2);
}

// === BEGIN from a forum post by jkleinecke. 8/23/06 jcs ===

namespace Scripting {
  class ScriptEntity {
  public:
    ScriptEntity() {
      Bind();
    }
    static void Bind() {
      SqPlus::SQClassDef<ScriptEntity>(_T("ScriptEntity")).
        var(&ScriptEntity::m_strName,_T("name"));
    } // Bind
    SqPlus::ScriptStringVar64 m_strName;
  };
}

DECLARE_INSTANCE_TYPE_NAME(Scripting::ScriptEntity,ScriptEntity)

void testScriptingTypeName(void) {
  try {
    Scripting::ScriptEntity entity ;
    SqPlus::BindVariable(&entity,_T("instance"));
    SquirrelObject sqObj = SquirrelVM::CompileBuffer(_T("instance.name = \"Testing an instance variable bind: member assignment.\"; print(instance.name);"));
    SquirrelVM::RunScript(sqObj);
  } // try
  catch (SquirrelError e) {
    scprintf(_T("testScriptingTypeName: %s\n"),e.desc);
  } // catch
}

// === END from a forum post by jkleinecke. 8/23/06 jcs ===


// === BEGIN Interface Test ===

class PureInterface {
public:
  virtual void pureFunc1(void)=0;
  virtual void pureFunc2(void)=0;
};

class MyImp : public PureInterface {
public:
  PureInterface * getInterface(void) { return (PureInterface *)this; }
  void pureFunc1(void) {
    scprintf(_T("PureFunc1 called [0x%p].\n"),this);
  }
  void pureFunc2(void) {
    scprintf(_T("PureFunc2 called [0x%p].\n"),this);
  }
};

class InterfaceHolder {
public:
  PureInterface * theInterface;
  void setInterface(PureInterface * pureInterface) {
    theInterface = pureInterface;
  }
  PureInterface * getInterface(void) {
    return theInterface;
  }
};

DECLARE_INSTANCE_TYPE(PureInterface)
DECLARE_INSTANCE_TYPE(MyImp)
DECLARE_INSTANCE_TYPE(InterfaceHolder)

void testPureVirtualInterface(void) {
  SQClassDefNoConstructor<PureInterface>(_T("PureInterface")).
    func(&PureInterface::pureFunc1,_T("pureFunc1")).
    func(&PureInterface::pureFunc2,_T("pureFunc2"));

  SQClassDef<InterfaceHolder>(_T("InterfaceHolder")).
    func(&InterfaceHolder::setInterface,_T("setInterface")).
    func(&InterfaceHolder::getInterface,_T("getInterface"));

  SQClassDef<MyImp>(_T("MyImp")).
    func(&MyImp::getInterface,_T("getInterface"));

  MyImp myImp;

  SquirrelObject test = SquirrelVM::CompileBuffer(_T("ih <- InterfaceHolder();"));
  SquirrelVM::RunScript(test);

  SquirrelObject root = SquirrelVM::GetRootTable();
  SquirrelObject ih = root.GetValue(_T("ih"));
  InterfaceHolder * ihp = (InterfaceHolder * )ih.GetInstanceUP(ClassType<InterfaceHolder>::type());
  ihp->setInterface(&myImp);

  test = SquirrelVM::CompileBuffer(_T("\
                                      ih.getInterface().pureFunc1(); \n\
                                      ih.getInterface().pureFunc2(); \n\
                                      ihp <- ih.getInterface(); \n\
                                      ihp.pureFunc1(); \n\
                                      ihp.pureFunc2(); \n\
                                      myIh <- MyImp(); \n\
                                      ih.setInterface(myIh.getInterface()); \n\
                                      ih.getInterface().pureFunc1(); \n\
                                      ih.getInterface().pureFunc2(); \n\
                                      "));
   SquirrelVM::RunScript(test);

} // testPureVirtualInterface

// === END Interface Test ===

void testSquirrelObjectSetGet(void) {
  // We can pass in arguments:
  //   by value ('true' arg, required for constant float, int, etc., or when a copy is desired),
  //   by reference (data will be copied to SquirrelObject and memory managed),
  //   by pointer (no data copying: pointer is used directly in SquirrelObject; the memory will not be managed).

  SquirrelObject tc(5.678f); // constant argument is passed by value (even though declaration is by ref: const & results in by-value in this case), memory will be allocated and managed for the copy.
  float valc = tc.Get<float>();

  scprintf(_T("Valc is: %f\n"),valc);

  float val = 1.234f;
  SquirrelObject t(val); // val is passed by reference, memory will be allocated, and the value copied once.

  float val2 = t.Get<float>();

  scprintf(_T("Val2 is: %f\n"),val2);

  if (1) {
    SquirrelObject v(Vector3(1.f,2.f,3.f)); // Pass in by reference: will be copied once, with memory for new copy managed by Squirrel.

    Vector3 * pv = v.Get<Vector3 *>();
    scprintf(_T("Vector3 is: %f %f %f\n"),pv->x,pv->y,pv->z);
    pv->z += 1.f;

    if (1) {
      SquirrelObject v2p(pv); // This is a pointer to v's instance (passed in by pointer: see SquirrelObject.h). 
                              // A new Squirrel Instance will be created, but the C++ instance pointer will not get freed when v2p goes out of scope (release hook will be null).
      pv = v2p.Get<Vector3 *>();
      scprintf(_T("Vector3 is: %f %f %f\n"),pv->x,pv->y,pv->z);
    } // if

  } // if

  scprintf(_T("Vector3() instance has been released.\n\n"));

} // testSquirrelObjectSetGet


#define SQDBG_DEBUG_HOOK _T("_sqdebughook_")

class TestSqPlus {
public:

  enum {SQ_ENUM_TEST=1234321};

  void init(void) {
    SquirrelVM::Init();
    HSQUIRRELVM _v = SquirrelVM::GetVMPtr();

#if 1
    sq_pushregistrytable(_v);
    sq_pushstring(_v,SQDBG_DEBUG_HOOK,-1);
    sq_pushuserpointer(_v,this);
    sq_newclosure(_v,debug_hook,1);
    sq_createslot(_v,-3);
//    sq_pop(_v,1);

//    sq_pushregistrytable(_v);
    sq_pushstring(_v,SQDBG_DEBUG_HOOK,-1);
    sq_rawget(_v,-2);
    sq_setdebughook(_v);
    sq_pop(_v,1);

#endif

    sq_enabledebuginfo(_v,SQTrue);

  } // init

  TestSqPlus() {
    init();

    testPureVirtualInterface();

    testScriptingTypeName();

    try {
      HSQUIRRELVM v = SquirrelVM::GetVMPtr();
      SquirrelObject root = SquirrelVM::GetRootTable();

      testPointfBoxf();

      // Example from forum question:
      testScripts.InitScript1();
      testScripts.InitScript2();
      SquirrelObject testScriptBinding = SquirrelVM::CompileBuffer(_T("\
        local testScripts = STestScripts(); \n\
        testScripts.Test1(); \n\
        testScripts.Test2(); \n\
        print(testScripts.Var_ToBind2); \n\
        Test1(); \n\
        Test2(); \n\
        print(Var_ToBind1); \n\
      "));
      SquirrelVM::RunScript(testScriptBinding);

      // === BEGIN Global Function binding tests ===

      // Implemented as SquirrelVM::CreateFunction(rootTable,func,name,typeMask). CreateFunctionGlobal() binds a standard SQFUNCTION (stack args).
      SquirrelVM::CreateFunctionGlobal(testFunc,_T("testFunc0"));
      SquirrelVM::CreateFunctionGlobal(testFunc,_T("testFuncN"),_T("n"));
      SquirrelVM::CreateFunctionGlobal(testFunc,_T("testFuncS"),_T("s"));
#if 0
      SquirrelObject testStandardFuncs = SquirrelVM::CompileBuffer(_T(" testFunc0(); testFuncN(1.); testFuncS(\"Hello\"); "));
      SquirrelVM::RunScript(testStandardFuncs);
#endif
      // === Register Standard Functions using template system (function will be directly called with argument auto-marshaling) ===

      RegisterGlobal(v,newtest,_T("test"));
      RegisterGlobal(v,newtestR1,_T("testR1"));

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

      // === BEGIN Namespace examples ===

      // Create a namespace using a table.
      SquirrelObject nameSpaceTable = SquirrelVM::CreateTable();
      root.SetValue(_T("Namespace1"),nameSpaceTable);
      Register(v,nameSpaceTable.GetObjectHandle(),globalFunc,_T("namespaceFunc"));

      // Create a namespace using a class. If an instance is created from the class, using the instance will prevent accidental changes to the instance members.
      // Using the class/instance form also allows extra information to be added to the proxy class, if desired (such as vars/funcs).
      // NOTE: If any variables/static-variables/constants are registered to the class, it must be instantiated before use.
      SQClassDef<NamespaceClass>(_T("Namespace2")).
        staticFunc(globalFunc,_T("namespaceFunc"));

      SquirrelObject testNameSpace = SquirrelVM::CompileBuffer(_T("\
        Namespace1.namespaceFunc(\"Hello Namespace1 (table),\",321); \n\
        Namespace2.namespaceFunc(\"Hello Namespace2 (class),\",654); \n\
        local Namespace3 = Namespace2(); \n\
        Namespace3.namespaceFunc(\"Hello Namespace3 (instance),\",987); \n\
      "));

      SquirrelVM::RunScript(testNameSpace);

      // === END Namespace examples ===

      // === BEGIN Class Instance tests ===

      // Example showing two methods for registration.
#if 0
      SQClassDef<NewTestObj> sqClass(_T("NewTestObj"));
      sqClass.func(NewTestObj::newtestR1,_T("newtestR1"));
      sqClass.var(&NewTestObj::val,_T("val"));
      sqClass.var(&NewTestObj::s1,_T("s1"));
      sqClass.var(&NewTestObj::s2,_T("s2"));
      sqClass.var(&NewTestObj::c1,_T("c1"),VAR_ACCESS_READ_ONLY);
      sqClass.var(&NewTestObj::c2,_T("c2"),VAR_ACCESS_READ_ONLY);
      sqClass.funcVarArgs(NewTestObj::multiArgs,_T("multiArgs"));
#else
      SQClassDef<NewTestObj>(_T("NewTestObj")).                         // If a special constructor+destructor are not needed, the auto-generated versions can be used.
                                                                    // Example methods for custom constructors:
        staticFuncVarArgs(constructNewTestObj,_T("constructor"),_T("*")).   // Using a global constructor: useful in cases where a custom constructor/destructor are required and the original class is not to be modified.
//        staticFunc(constructNewTestObjFixedArgs,_T("constructor")).   // Using a global constructor: useful in cases where a custom constructor/destructor are required and the original class is not to be modified.      
//        staticFuncVarArgs(NewTestObj::construct,_T("constructor")).   // Using a static member constructor.
        staticFunc(globalFunc,_T("globalFunc")).                        // Any global function can be registered in a class namespace (no 'this' pointer will be passed to the function).
        staticFunc(globalClass,&GlobalClass::func,_T("globalClassFunc")).
        func(&NewTestObj::newtestR1,_T("newtestR1")).
        var(&NewTestObj::val,_T("val")).
        var(&NewTestObj::s1,_T("s1")).
        var(&NewTestObj::s2,_T("s2")).
        var(&NewTestObj::c1,_T("c1"),VAR_ACCESS_READ_ONLY).
        var(&NewTestObj::c2,_T("c2"),VAR_ACCESS_READ_ONLY).
        funcVarArgs(&NewTestObj::multiArgs,_T("multiArgs"));

#define SQ_10 10
#define SQ_E 2.71828182845904523536f
#define SQ_PI 3.14159265358979323846264338327950288f
#define SQ_CONST_STRING _T("A constant string")
      const int intConstant     = 7;
      const float floatConstant = 8.765f;
      const bool boolConstant   = true;
#if 1
      SQClassDef<Vector3>(_T("Vector3")).
        var(&Vector3::x,_T("x")).
        var(&Vector3::y,_T("y")).
        var(&Vector3::z,_T("z")).
        func(&Vector3::Inc,_T("Inc")).
        func(&Vector3::operator+,_T("_add")).
        staticFunc(&Add2,_T("Add2")).
        staticFuncVarArgs(&Add,_T("Add")).
#if 1
        staticVar(&Vector3::staticVar,_T("staticVar")).
#else
        staticVar(&Vector3::staticVar,_T("staticVar"),VAR_ACCESS_READ_ONLY).
#endif
        staticVar(&globalVar,_T("globalVar")).
        constant(SQ_10,_T("SQ_10")).
        constant(SQ_E,_T("SQ_E")).
        constant(SQ_PI,_T("SQ_PI")).
        constant(SQ_CONST_STRING,_T("SQ_CONST_STRING")).
        enumInt(SQ_ENUM_TEST,_T("SQ_ENUM_TEST")).
        constant(intConstant,_T("intConstant")).
        constant(floatConstant,_T("floatConstant")).
        constant(true,_T("boolTrue")).
        constant(false,_T("boolFalse")).
        constant(boolConstant,_T("boolConstant"));
#endif

#endif

      testSquirrelObjectSetGet(); // Uses Vector3().

      BindConstant(SQ_PI*2,_T("SQ_PI_2"));
      BindConstant(SQ_10*2,_T("SQ_10_2"));
      BindConstant(_T("Global String"),_T("GLOBAL_STRING"));

      SquirrelObject testStaticVars = SquirrelVM::CompileBuffer(_T(" local v = Vector3(); local v2 = Vector3(); local v3 = v+v2; v3 += v2; print(\"v3.x: \"+v3.x); print(\"Vector3::staticVar: \"+v.staticVar+\" Vector3::globalVar: \"+v.globalVar); v.staticVar = 0; "));
      SquirrelVM::RunScript(testStaticVars);

      SquirrelObject testConstants0 = SquirrelVM::CompileBuffer(_T(" print(\"SQ_PI*2: \"+SQ_PI_2+\" SQ_10_2: \"+SQ_10_2+\" GLOBAL_STRING: \"+GLOBAL_STRING); "));
      SquirrelVM::RunScript(testConstants0);

      SquirrelObject testConstants1 = SquirrelVM::CompileBuffer(_T("local v = Vector3(); print(\"SQ_10: \"+v.SQ_10+\" SQ_E: \"+v.SQ_E+\" SQ_PI: \"+v.SQ_PI+\" SQ_CONST_STRING: \"+v.SQ_CONST_STRING+\" SQ_ENUM_TEST: \"+v.SQ_ENUM_TEST);" ));
      SquirrelVM::RunScript(testConstants1);
      SquirrelObject testConstants2 = SquirrelVM::CompileBuffer(_T("local v = Vector3(); print(\"intConstant: \"+v.intConstant+\" floatConstant: \"+v.floatConstant+\" boolTrue: \"+(v.boolTrue?\"True\":\"False\")+\" boolFalse: \"+(v.boolFalse?\"True\":\"False\")+\" boolConstant: \"+(v.boolConstant?\"True\":\"False\"));" ));
      SquirrelVM::RunScript(testConstants2);

      SquirrelObject scriptedBase = SquirrelVM::CompileBuffer(_T(" class ScriptedBase { sbval = 5551212; function multiArgs(a,...) { print(\"SBase: \"+a+val); } \n } \n ")); // Note val does not exist in base.
      SquirrelVM::RunScript(scriptedBase);

      // === BEGIN Instance Test ===

      SQClassDef<PlayerManager::Player>(_T("PlayerManager::Player")).
        func(&PlayerManager::Player::printName,_T("printName")).
        var(&PlayerManager::Player::name,_T("name"));

      SQClassDef<PlayerManager>(_T("PlayerManager")).
        func(&PlayerManager::GetPlayer,_T("GetPlayer")).
        var(&PlayerManager::playerVar,_T("playerVar"));

      RegisterGlobal(getPlayerManager,_T("getPlayerManager"));
      BindVariable(&playerManager,_T("playerManagerVar"));

      SquirrelObject testGetInstance = SquirrelVM::CompileBuffer(_T("\
        local PlayerManager = getPlayerManager(); \n\
        local oPlayer = PlayerManager.GetPlayer(0); \n\
        print(typeof oPlayer); \n\
        oPlayer.printName(); \n\
        PlayerManager.playerVar.printName(); \n\
        print(PlayerManager.playerVar.name); \n\
        oPlayer = PlayerManager.playerVar; \n\
        oPlayer.name = \"New_Name1\"; \n\
        playerManagerVar.playerVar.printName(); \n\
        oPlayer.name = \"New_Name2\"; \n\
      "));
      SquirrelVM::RunScript(testGetInstance);
      scprintf(_T("playerManager.playerVar.name: %s\n"),playerManager.playerVar.name.s);

      // === END Instance Test ===

      // === BEGIN example from forum post ===

      SQClassDef<Creature>(_T("Creature")).
        func(&Creature::GetMaxHealth,_T("GetMaxHealth")).
        func(&Creature::GetHealth,_T("GetHealth")).
        func(&Creature::SetHealth,_T("SetHealth"));

      SquirrelObject testClass = SquirrelVM::CompileBuffer( _T("function HealthPotionUse(Target) { \n\
                                                               local curHealth = Target.GetHealth(); \n\
                                                               local maxHealth = Target.GetMaxHealth(); \n\
                                                               if ((maxHealth - curHealth) > 15) { \n\
                                                                 curHealth += 15; \n\
                                                               } else { \n\
                                                                 curHealth = maxHealth; \n\
                                                               } \n\
                                                               Target.SetHealth(curHealth); \n\
                                                               print(typeof Target); \n\
                                                               return Target; \n\
                                                             }"));
      Creature frodo;
      frodo.SetHealth(frodo.GetMaxHealth()/2);

      SquirrelVM::RunScript(testClass);

      Creature newFrodo = SquirrelFunction<Creature &>(_T("HealthPotionUse"))(frodo); // Pass by value and return a copy (Must return by reference due to template system design).
      SquirrelFunction<void>(_T("HealthPotionUse"))(&frodo);                          // Pass the address to directly modify frodo.
      scprintf(_T("Frodo's health: %d %d\n"),frodo.GetHealth(),newFrodo.GetHealth());

      // === END example from forum post ===

#ifdef SQ_USE_CLASS_INHERITANCE
      // Base class constructors, if registered, must use this form: static int construct(HSQUIRRELVM v).
//      SQClassDef<CustomTestObj> customClass(_T("CustomTestObj"));
      SQClassDef<CustomTestObj> customClass(_T("CustomTestObj"),_T("ScriptedBase"));
//      SQClassDef<CustomTestObj> customClass(_T("CustomTestObj"),_T("NewTestObj"));
      customClass.staticFuncVarArgs(&CustomTestObj::construct,_T("constructor"),_T("*"));                  // MUST use this form (or no args) if CustomTestObj will be used as a base class.
                                                                                                   // Using the "*" form will allow a single constructor to be used for all cases.
//      customClass.staticFuncVarArgs(CustomTestObj::construct,_T("constructor"));                     // (this form is also OK if used as a base class)
      customClass.funcVarArgs(&CustomTestObj::varArgTypesAndCount,_T("multiArgs"),_T("*"));                // "*": no type or count checking.
      customClass.funcVarArgs(&CustomTestObj::varArgTypesAndCount,_T("varArgTypesAndCount"),_T("*"));      // "*": no type or count checking.
#else
      SQClassDef<CustomTestObj> customClass(_T("CustomTestObj"));
      customClass.staticFuncVarArgs(&CustomTestObj::construct,_T("constructor"),_T("snb"));                // string, number, bool (all types must match).
      customClass.funcVarArgs(&CustomTestObj::varArgTypesAndCount,_T("varArgTypesAndCount"),_T("*"));      // "*": no type or count checking.
#endif
      customClass.funcVarArgs(&CustomTestObj::varArgTypes,_T("varArgTypes"),_T("s|ns|ns|ns|n"));           // string or number + string or number.
      customClass.funcVarArgs(&CustomTestObj::noArgsVariableReturn,_T("noArgsVariableReturn"));        // No type string means no arguments allowed.
      customClass.func(&CustomTestObj::variableArgsFixedReturnType,_T("variableArgsFixedReturnType")); // Variables args, fixed return type.
      customClass.func(&CustomTestObj::manyArgs,_T("manyArgs"));                                       // Many args, type checked.
      customClass.func(&CustomTestObj::manyArgsR1,_T("manyArgsR1"));                                   // Many args, type checked, one return value.

#ifdef SQ_USE_CLASS_INHERITANCE
//      SquirrelObject testInheritance = SquirrelVM::CompileBuffer(_T(" class Derived extends NewTestObj { s1 = 123; \n constructor() { NewTestObj.constructor(this); }; function getParentS2() return s2; \n }; \n local t = Derived(); \n print(\"DerS2: \"+t.getParentS2()); t.multiArgs(); //t.newtestR1(\"R1in\"); "));
//      SquirrelObject testInheritance = SquirrelVM::CompileBuffer(_T(" local t = CustomTestObj(\"sa\",321,true); \n t.val = 444; print(t.val); t.variableArgsFixedReturnType(4,5.5); t.multiArgs(1,2,3); t.newtestR1(\"R1in\"); "));
//      SquirrelObject testInheritance = SquirrelVM::CompileBuffer(_T(" class Derived extends CustomTestObj { val = 888; \n function func(a) print(a+dVal);\n } \n local x = Derived(); print(\"x.val \"+x.val); local t = CustomTestObj(\"sa\",321,true); \n t.val = 444; print(t.val); t.variableArgsFixedReturnType(4,5.5); t.multiArgs(1,2,3); t.newtestR1(\"R1in\"); "));
      SquirrelObject testInheritance = SquirrelVM::CompileBuffer(_T(" class Derived extends CustomTestObj { val = 888; \n function multiArgs(a,...) { print(a+val); \n print(sbval); ::CustomTestObj.multiArgs(4); ::ScriptedBase.multiArgs(5,6,7); \n }\n } \n local x = Derived(); print(\"x.val \"+x.val); x.multiArgs(1,2,3); "));
//      SquirrelObject testInheritance = SquirrelVM::CompileBuffer(_T(" class Derived extends CustomTestObj { val = 888; \n function multiArgs(a,...) print(a+val);\n } \n local x = Derived(); print(\"x.val \"+x.val); x.multiArgs(1,2,3); //local t = CustomTestObj(); \n t.val = 444; print(t.val); t.variableArgsFixedReturnType(4,5.5); t.multiArgs(1,2,3); t.newtestR1(\"R1in\"); "));
      printf("=== BEGIN INHERITANCE ===\n");
      testInhertianceCase();
      SquirrelVM::RunScript(testInheritance);
      printf("===  END INHERITANCE  ===\n");
#endif

      SquirrelObject testRegV = SquirrelVM::CompileBuffer(_T(" local vec = Vector3(); print(vec.x); vec = vec.Add(vec); print(vec.x); vec = vec.Add(vec); print(vec.x); vec = vec.Add2(vec,vec); print(vec.x); local v2 = Vector3(); vec = v2.Inc(vec); print(vec.x); print(v2.x); "));
      SquirrelVM::RunScript(testRegV);


#ifdef SQ_USE_CLASS_INHERITANCE
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

      SquirrelObject testReg1a = SquirrelVM::CompileBuffer(_T(" co <- CustomTestObj(\"hello\",123,true); co.noArgsVariableReturn(); local t = NewTestObj(\"S1in\",369,true); print(\"C1: \"+t.c1); print(\"C2: \"+t.c2); // t.c1 = 123; "));
      SquirrelVM::RunScript(testReg1a);

      // Constant test (read only var). Var can change on C++ side, but not on script side.
      try {
        SquirrelObject testRegConstant = SquirrelVM::CompileBuffer(_T(" local t = NewTestObj(); t.c1 = 123; "));
        SquirrelVM::RunScript(testRegConstant);
      } // try
      catch (SquirrelError & e) {
        scprintf(_T("Error: %s, %s\n"),e.desc,_T("Squirrel::TestConstant"));
      } // catch

      SquirrelObject testReg1 = SquirrelVM::CompileBuffer(_T(" local t = NewTestObj(); t.newtestR1(\"Hello\"); t.val = 789; print(t.val); print(t.s1); print(t.s2); t.s1 = \"New S1\"; print(t.s1); "));
      SquirrelVM::RunScript(testReg1);

      SquirrelObject testReg2 = SquirrelVM::CompileBuffer(_T(" local t = NewTestObj(); t.val = 789; print(t.val); t.val = 876; print(t.val); t.multiArgs(1,2,3); t.multiArgs(1,2,3,4); t.globalFunc(\"Hola\",5150,false); t.globalClassFunc(\"Bueno\",5151,true); "));
      SquirrelVM::RunScript(testReg2);
      SquirrelObject testReg3 = SquirrelVM::CompileBuffer(_T(" test(); local rv = testR1(\"Hello\"); print(rv); "));
      SquirrelVM::RunScript(testReg3);     
      SquirrelObject testReg4 = SquirrelVM::CompileBuffer(_T(" print(\"\\nMembers:\"); testObj_newtest1(); testObj_newtest2(); print(testObj_newtestR1(\"Hello Again\")); "));
      SquirrelVM::RunScript(testReg4);

      SquirrelObject defCallFunc = SquirrelVM::CompileBuffer(_T(" function callMe(var) { print(\"I was called by: \"+var); return 123; }"));
      SquirrelVM::RunScript(defCallFunc);

      SquirrelObject defCallFuncStrRet = SquirrelVM::CompileBuffer(_T(" function callMeStrRet(var) { print(\"I was called by: \"+var); return var; }"));
      SquirrelVM::RunScript(defCallFuncStrRet);

      SquirrelFunction<void>(_T("callMe"))(_T("Squirrel 1"));

      // Get a function from the root table and call it.
#if 1
      SquirrelFunction<int> callFunc(_T("callMe"));
      int ival = callFunc(_T("Squirrel"));
      scprintf(_T("IVal: %d\n"),ival);

      SquirrelFunction<const SQChar *> callFuncStrRet(_T("callMeStrRet"));
      const SQChar * sval = callFuncStrRet(_T("Squirrel StrRet"));
      scprintf(_T("SVal: %s\n"),sval);
#endif
      ival = 0;
      // Get a function from any table.
      SquirrelFunction<int> callFunc2(root.GetObjectHandle(),_T("callMe"));
      ival = callFunc(456); // Argument count is checked; type is not.

      // === END Class Instance tests ===

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
      array.ArrayAppend((SQUserPointer)0);

      // Pop 3 items from array:
      array.ArrayPop(SQFalse);                 // Don't retrieve the popped value (int:123).
      SquirrelObject so1 = array.ArrayPop();   // Retrieve the popped value.
      const SQChar * val1 = so1.ToString();      // Get string.
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

      SquirrelObject define_printArray = SquirrelVM::CompileBuffer(_T(" function printArray(name,array) { print(name+\".len() = \"+array.len()); foreach(i, v in array) if (v != null) { if (typeof v == \"bool\") v = v ? \"true\" : \"false\"; print(\"[\"+i+\"]: \"+v); } } "));
      SquirrelVM::RunScript(define_printArray);
      SquirrelObject test = SquirrelVM::CompileBuffer(_T(" printArray(\"array\",array); printArray(\"arrayr\",arrayr); "));

      SquirrelVM::RunScript(test);
#endif
    } // try
    catch (SquirrelError & e) {
      scprintf(_T("Error: %s, in %s\n"),e.desc,_T("TestSqPlus"));
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

