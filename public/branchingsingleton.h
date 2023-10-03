//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Macros for defining branching singletons.
//
//			A branching singleton defines a singleton class within another class, and subclasses
//			of the outer class can automatically expand on that singleton at their node in the
//			class branching tree with the confidence that changes will be reflected in all
//			subclasses.
//
//			The primary reason to have a branching singleton is to centralize management code
//			without being tied explicitly to one interface. The interface can possibly change
//			vastly as it gets passed down the tree to the point where all the original functions
//			are stubs and the interface uses an entirely different set of functions.
//
// $NoKeywords: $
//=============================================================================//

#ifndef BRANCHINGSINGLETON_H
#define BRANCHINGSINGLETON_H
#ifdef _WIN32
#pragma once
#endif


#define START_BRANCHING_SINGLETON_DEFINITION_NOBASE( classname ) class classname

#define START_BRANCHING_SINGLETON_DEFINITION( classname ) class classname : public Base##classname

#define _END_BRANCHING_SINGLETON_DEFINITION( classname );\
	static classname *Get_##classname##_Static( void )\
	{\
		static classname s_Singleton;\
		return &s_Singleton;\
	}\
	\
	virtual Root##classname *Get_##classname##( void )\
	{\
		return Get_##classname##_Static();\
	}\
	typedef classname Base##classname;

#define END_BRANCHING_SINGLETON_DEFINITION( classname ) _END_BRANCHING_SINGLETON_DEFINITION( classname )

#define END_BRANCHING_SINGLETON_DEFINITION_NOBASE( classname );\
	typedef classname Root##classname;\
	_END_BRANCHING_SINGLETON_DEFINITION( classname );

#endif //#ifndef BRANCHINGSINGLETON_H
