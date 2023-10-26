#include "sqplus.h"
#ifdef _PS3
#undef _STD_USING
#endif
#include <stdio.h>
#if defined(VSCRIPT_DLL_EXPORT) || defined(VSQUIRREL_TEST)
#include "memdbgon.h"
#endif

namespace SqPlus {

static int getVarInfo(StackHandler & sa,VarRefPtr & vr) {
  HSQOBJECT htable = sa.GetObjectHandle(1);
  SquirrelObject table(htable);
#ifdef _DEBUG
  SQObjectType type = (SQObjectType)sa.GetType(2);
#endif
  const SQChar * el = sa.GetString(2);
  ScriptStringVar256 varNameTag;
  getVarNameTag(varNameTag,sizeof(varNameTag),el);
  SQUserPointer data=0;
  if (!table.RawGetUserData(varNameTag,&data)) {
//    throw SquirrelError("getVarInfo: Could not retrieve UserData");
    return sa.ThrowError(_T("getVarInfo: Could not retrieve UserData")); // Results in variable not being found error.
  } // if
  vr = (VarRefPtr)data;
  return SQ_OK;
} // getVarInfo

static int getInstanceVarInfo(StackHandler & sa,VarRefPtr & vr,SQUserPointer & data) {
  HSQOBJECT ho = sa.GetObjectHandle(1);
  SquirrelObject instance(ho);
#ifdef _DEBUG
  SQObjectType type = (SQObjectType)sa.GetType(2);
#endif
  const SQChar * el = sa.GetString(2);
  ScriptStringVar256 varNameTag;
  getVarNameTag(varNameTag,sizeof(varNameTag),el);
  SQUserPointer ivrData=0;
  if (!instance.RawGetUserData(varNameTag,&ivrData)) {
//    throw SquirrelError("getInstanceVarInfo: Could not retrieve UserData");
    return sa.ThrowError(_T("getInstanceVarInfo: Could not retrieve UserData")); // Results in variable not being found error.
  } // if
  vr = (VarRefPtr)ivrData;
  unsigned char * up;
  if (!(vr->access & (VAR_ACCESS_STATIC|VAR_ACCESS_CONSTANT))) {
#ifdef SQ_USE_CLASS_INHERITANCE
    SQUserPointer typetag; instance.GetTypeTag(&typetag);
    if (typetag != vr->instanceType) {
      SquirrelObject typeTable = instance.GetValue(SQ_CLASS_OBJECT_TABLE_NAME);
      up = (unsigned char *)typeTable.GetUserPointer(INT((size_t)vr->instanceType)); // <TODO> 64-bit compatible version.
      if (!up) {
        throw SquirrelError(_T("Invalid Instance Type"));
      } // if
    } else {
      up = (unsigned char *)instance.GetInstanceUP(0);
    } // if
#else
    up = (unsigned char *)instance.GetInstanceUP(0);
#endif
    up += (size_t)vr->offsetOrAddrOrConst;         // Offset
  } else {
    up = (unsigned char *)vr->offsetOrAddrOrConst; // Address
  } // if
  data = up;
  return SQ_OK;
} // getInstanceVarInfo

static int setVar(StackHandler & sa,VarRef * vr,void * data) {
  if (vr->access & (VAR_ACCESS_READ_ONLY|VAR_ACCESS_CONSTANT)) {
    ScriptStringVar256 msg;
    const SQChar * el = sa.GetString(2);
    SCSNPRINTF(msg.s,sizeof(msg),_T("setVar(): Cannot write to constant: %s"),el);
    throw SquirrelError(msg.s);
  } // if
  switch (vr->type) {
  case TypeInfo<INT>::TypeID: {
    INT * val = (INT *)data; // Address
    if (val) {
      *val = sa.GetInt(3);
      return sa.Return(*val);
    } // if
    break;
  } // case
  case TypeInfo<FLOAT>::TypeID: {
    FLOAT * val = (FLOAT *)data; // Address
    if (val) {
      *val = sa.GetFloat(3);
      return sa.Return(*val);
    } // if
    break;
  } // case
  case TypeInfo<bool>::TypeID: {
    bool * val = (bool *)data; // Address
    if (val) {
      *val = sa.GetBool(3) ? true : false;
      return sa.Return(*val);
    } // if
    break;
  } // case
  case VAR_TYPE_INSTANCE: {
    HSQUIRRELVM v = sa.GetVMPtr();
    // vr->copyFunc is the LHS variable type: the RHS var's type is ClassType<>::type() (both point to ClassType<>::copy()).
    // src will be null if the LHS and RHS types don't match.
    SQUserPointer src = sa.GetInstanceUp(3,(SQUserPointer)vr->copyFunc); // Effectively performs: ClassType<>::type() == ClassType<>getCopyFunc().
    if (!src) throw SquirrelError(_T("INSTANCE type assignment mismatch"));
    vr->copyFunc(data,src);
#if 0 // Return an instance on the stack (allocates memory)
    if (!CreateNativeClassInstance(sa.GetVMPtr(),vr->typeName,data,0)) { // data = address
      ScriptStringVar256 msg;
      SCSNPRINTF(msg.s,sizeof(msg),_T("getVar(): Could not create instance: %s"),vr->typeName);
      throw SquirrelError(msg.s);
    } // if
    return 1;
#else // Don't return on stack.
    return 0;
#endif
  }
  case TypeInfo<SQUserPointer>::TypeID: {
    ScriptStringVar256 msg;
    const SQChar * el = sa.GetString(2);
    SCSNPRINTF(msg.s,sizeof(msg),_T("setVar(): Cannot write to an SQUserPointer: %s"),el);
    throw SquirrelError(msg.s);
  } // case
  case TypeInfo<ScriptStringVarBase>::TypeID: {
    ScriptStringVarBase * val = (ScriptStringVarBase *)data; // Address
    if (val) {
      const SQChar * strVal = sa.GetString(3);
      if (strVal) {
        *val = strVal;
        return sa.Return(val->s);
      } // if
    } // if
    break;
  } // case
  } // switch
  return SQ_ERROR;
} // setVar

static int getVar(StackHandler & sa,VarRef * vr,void * data) {
  switch (vr->type) {
  case TypeInfo<INT>::TypeID: {
    if (!(vr->access & VAR_ACCESS_CONSTANT)) {
      INT * val = (INT *)data; // Address
      if (val) {
        return sa.Return(*val);
      } // if
    } else {
      INT * val = (INT *)&data; // Constant value
      return sa.Return(*val);
    } // if
    break;
  } // case
  case TypeInfo<FLOAT>::TypeID: {
    if (!(vr->access & VAR_ACCESS_CONSTANT)) {
      FLOAT * val = (FLOAT *)data; // Address
      if (val) {
        return sa.Return(*val);
      } // if
    } else {
      FLOAT * val = (FLOAT *)&data; // Constant value
      return sa.Return(*val);
    } // if
    break;
  } // case
  case TypeInfo<bool>::TypeID: {
    if (!(vr->access & VAR_ACCESS_CONSTANT)) {
      bool * val = (bool *)data; // Address
      if (val) {
        return sa.Return(*val);
      } // if
    } else {
      bool * val = (bool *)&data; // Constant value
      return sa.Return(*val);
    } // if
    break;
  } // case
  case VAR_TYPE_INSTANCE:
    if (!CreateNativeClassInstance(sa.GetVMPtr(),vr->typeName,data,0)) { // data = address. Allocates memory.
      ScriptStringVar256 msg;
      SCSNPRINTF(msg.s,sizeof(msg),_T("getVar(): Could not create instance: %s"),vr->typeName);
      throw SquirrelError(msg.s);
    } // if
    return 1;
  case TypeInfo<SQUserPointer>::TypeID: {
    return sa.Return(data); // The address of member variable, not the variable itself.
  } // case
  case TypeInfo<ScriptStringVarBase>::TypeID: {
    if (!(vr->access & VAR_ACCESS_CONSTANT)) {
      ScriptStringVarBase * val = (ScriptStringVarBase *)data; // Address
      if (val) {
        return sa.Return(val->s);
      } // if
    } else {
      throw SquirrelError(_T("getVar(): Invalid type+access: 'ScriptStringVarBase' with VAR_ACCESS_CONSTANT (use VAR_ACCESS_READ_ONLY instead)"));
    } // if
    break;
  } // case
  case TypeInfo<const SQChar *>::TypeID: {
    if (!(vr->access & VAR_ACCESS_CONSTANT)) {
      throw SquirrelError(_T("getVar(): Invalid type+access: 'const SQChar *' without VAR_ACCESS_CONSTANT"));
    } else {
      return sa.Return((const SQChar *)data); // Address
    } // if
    break;
  } // case
  } // switch
  return SQ_ERROR;
} // getVar

// === Global Vars ===

SQInteger setVarFunc(HSQUIRRELVM v) {
  StackHandler sa(v);
  if (sa.GetType(1) == OT_TABLE) {
    VarRefPtr vr;
    int res = getVarInfo(sa,vr);
    if (res != SQ_OK) return res;
    return setVar(sa,vr,vr->offsetOrAddrOrConst);
  } // if
  return SQ_ERROR;
} // setVarFunc

SQInteger getVarFunc(HSQUIRRELVM v) {
  StackHandler sa(v);
  if (sa.GetType(1) == OT_TABLE) {
    VarRefPtr vr;
    int res = getVarInfo(sa,vr);
    if (res != SQ_OK) return res;
    return getVar(sa,vr,vr->offsetOrAddrOrConst);
  } // if
  return SQ_ERROR;
} // getVarFunc

// === Instance Vars ===

SQInteger setInstanceVarFunc(HSQUIRRELVM v) {
  StackHandler sa(v);
  if (sa.GetType(1) == OT_INSTANCE) {
    VarRefPtr vr;
    void * data;
    int res = getInstanceVarInfo(sa,vr,data);
    if (res != SQ_OK) return res;
    return setVar(sa,vr,data);
  } // if
  return SQ_ERROR;
} // setInstanceVarFunc

SQInteger getInstanceVarFunc(HSQUIRRELVM v) {
  StackHandler sa(v);
  if (sa.GetType(1) == OT_INSTANCE) {
    VarRefPtr vr;
    void * data;
    int res = getInstanceVarInfo(sa,vr,data);
    if (res != SQ_OK) return res;
    return getVar(sa,vr,data);
  } // if
  return SQ_ERROR;
} // getInstanceVarFunc

// === Classes ===

BOOL CreateClass(HSQUIRRELVM v,SquirrelObject & newClass,SQUserPointer classType,const SQChar * name,const SQChar * baseName) {
  int n = 0;
  int oldtop = sq_gettop(v);
  sq_pushroottable(v);
  sq_pushstring(v,name,-1);
  if (baseName) {
    sq_pushstring(v,baseName,-1);
    if (SQ_FAILED(sq_get(v,-3))) { // Make sure the base exists if specified by baseName.
      sq_settop(v,oldtop);
      return FALSE;
    } // if
  } // if
  if (SQ_FAILED(sq_newclass(v,baseName ? 1 : 0))) { // Will inherit from base class on stack from sq_get() above.
    sq_settop(v,oldtop);
    return FALSE;
  } // if
  newClass.AttachToStackObject(-1);
  sq_settypetag(v,-1,classType);
  sq_createslot(v,-3);
  sq_pop(v,1);
  return TRUE;
} // CreateClass

SquirrelObject RegisterClassType(HSQUIRRELVM v,const SQChar * scriptClassName,SQUserPointer classType,SQFUNCTION constructor) {
  int top = sq_gettop(v);
  SquirrelObject newClass;
  if (CreateClass(v,newClass,classType,scriptClassName)) {
    SquirrelVM::CreateFunction(newClass,constructor,_T("constructor"));
  } // if
  sq_settop(v,top);
  return newClass;
} // RegisterClassType

}; // namespace SqPlus

// sqPlus
