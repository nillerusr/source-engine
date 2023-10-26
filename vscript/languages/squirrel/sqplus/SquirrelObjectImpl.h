// This is part of squirrelobject.h that removes circular dependency between sqplus.h and squirrelobject.h: it depends on both

#ifndef _SQUIRREL_OBJECT_IMPL_H_
#define _SQUIRREL_OBJECT_IMPL_H_

// === BEGIN code suggestion from the Wiki ===
// Get any bound type from this SquirrelObject. Note that Squirrel's handling of references and pointers still holds here.
template<typename _ty>
inline _ty SquirrelObject::Get(void) {
	sq_pushobject(SquirrelVM::_VM,GetObjectHandle());
	_ty val = SqPlus::Get(SqPlus::TypeWrapper<_ty>(),SquirrelVM::_VM,-1);
	sq_poptop(SquirrelVM::_VM);
	return val;
}

// Set any bound type to this SquirrelObject. Note that Squirrel's handling of references and pointers still holds here.
template<typename _ty>
inline SquirrelObject SquirrelObject::SetByValue(_ty val) { // classes/structs should be passed by ref (below) to avoid an extra copy.
	SqPlus::Push(SquirrelVM::_VM,val);
	AttachToStackObject(-1);
	sq_poptop(SquirrelVM::_VM);
	return *this;
}

// Set any bound type to this SquirrelObject. Note that Squirrel's handling of references and pointers still holds here.
template<typename _ty>
inline SquirrelObject SquirrelObject::Set(_ty & val) {
	SqPlus::Push(SquirrelVM::_VM,val);
	AttachToStackObject(-1);
	sq_poptop(SquirrelVM::_VM);
	return *this;
}

// === END code suggestion from the Wiki ===

#endif