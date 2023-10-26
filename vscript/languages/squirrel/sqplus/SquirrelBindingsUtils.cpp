#include "sqplus.h"
#if defined(VSCRIPT_DLL_EXPORT) || defined(VSQUIRREL_TEST)
#include "memdbgon.h"
#endif



BOOL CreateStaticNamespace(HSQUIRRELVM v,ScriptNamespaceDecl *sn)
{
	int n = 0;
	sq_pushroottable(v);
	sq_pushstring(v,sn->name,-1);
	sq_newtable(v);
	const ScriptClassMemberDecl *members = sn->members;
	const ScriptClassMemberDecl *m = NULL;
	while(members[n].name) {
		m = &members[n];
		sq_pushstring(v,m->name,-1);
		sq_newclosure(v,m->func,0);
		sq_setparamscheck(v,m->params,m->typemask);
		sq_setnativeclosurename(v,-1,m->name);
		sq_createslot(v,-3);
		n++;
	}
	const ScriptConstantDecl *consts = sn->constants;
	const ScriptConstantDecl *c = NULL;
	n = 0;
	while(consts[n].name) {
		c = &consts[n];
		sq_pushstring(v,c->name,-1);
		switch(c->type) {
		case OT_STRING: sq_pushstring(v,c->val.s,-1);break;
		case OT_INTEGER: sq_pushinteger(v,c->val.i);break;
		case OT_FLOAT: sq_pushfloat(v,c->val.f);break;
		}
		sq_createslot(v,-3);
		n++;
	}
	if(sn->delegate) {
		const ScriptClassMemberDecl *members = sn->delegate;
		const ScriptClassMemberDecl *m = NULL;
		sq_newtable(v);
		while(members[n].name) {
			m = &members[n];
			sq_pushstring(v,m->name,-1);
			sq_newclosure(v,m->func,0);
			sq_setparamscheck(v,m->params,m->typemask);
			sq_setnativeclosurename(v,-1,m->name);
			sq_createslot(v,-3);
			n++;
		}
		sq_setdelegate(v,-2);
	}
	sq_createslot(v,-3);
	sq_pop(v,1);
	
	return TRUE;
}

BOOL CreateClass(HSQUIRRELVM v,SquirrelClassDecl *cd)
{
	int n = 0;
	int oldtop = sq_gettop(v);
	sq_pushroottable(v);
	sq_pushstring(v,cd->name,-1);
	if(cd->base) {
		sq_pushstring(v,cd->base,-1);
		if(SQ_FAILED(sq_get(v,-3))) { // Make sure the base exists if specified by cd->base name.
			sq_settop(v,oldtop);
			return FALSE;
		}
	}
	if(SQ_FAILED(sq_newclass(v,cd->base?1:0))) { // Will inherit from base class on stack from sq_get() above.
		sq_settop(v,oldtop);
		return FALSE;
	}
//  sq_settypetag(v,-1,(unsigned int)cd);
#ifdef _WIN32
#pragma warning(disable : 4311)
#endif
	sq_settypetag(v,-1,reinterpret_cast<SQUserPointer>(cd));
	const ScriptClassMemberDecl *members = cd->members;
	const ScriptClassMemberDecl *m = NULL;
  if (members) {
    while(members[n].name) {
      m = &members[n];
      sq_pushstring(v,m->name,-1);
      sq_newclosure(v,m->func,0);
      sq_setparamscheck(v,m->params,m->typemask);
      sq_setnativeclosurename(v,-1,m->name);
      sq_createslot(v,-3);
      n++;
    }
  } // if
	sq_createslot(v,-3);
	sq_pop(v,1);
	return TRUE;
}

BOOL CreateNativeClassInstance(HSQUIRRELVM v,const SQChar *classname,SQUserPointer ud,SQRELEASEHOOK hook)
{
	int oldtop = sq_gettop(v);
	sq_pushroottable(v);
	sq_pushstring(v,classname,-1);
	if(SQ_FAILED(sq_rawget(v,-2))){ // Get the class (created with sq_newclass()).
		sq_settop(v,oldtop);
		return FALSE;
	}
	//sq_pushroottable(v);
	if(SQ_FAILED(sq_createinstance(v,-1))) {
		sq_settop(v,oldtop);
		return FALSE;
	}
	sq_remove(v,-3); //removes the root table
	sq_remove(v,-2); //removes the class
	if(SQ_FAILED(sq_setinstanceup(v,-1,ud))) {
		sq_settop(v,oldtop);
		return FALSE;
	}
	sq_setreleasehook(v,-1,hook);
	return TRUE;
}
