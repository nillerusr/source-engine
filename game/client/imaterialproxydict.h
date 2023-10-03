//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef IMATERIALPROXYDICT_H
#define IMATERIALPROXYDICT_H
#ifdef _WIN32
#pragma once
#endif

class IMaterialProxy;

typedef IMaterialProxy *MaterialProxyFactory_t();

abstract_class IMaterialProxyDict
{
public:
	// This is used to instance a proxy.
	virtual IMaterialProxy *CreateProxy( const char *proxyName ) = 0;
	// virtual destructor
	virtual	~IMaterialProxyDict() {}
	// Used by EXPOSE_MATERIAL_PROXY to insert all proxies into the dictionary.
	virtual void Add( const char *pMaterialProxyName, MaterialProxyFactory_t *pMaterialProxyFactory ) = 0;
};

extern IMaterialProxyDict &GetMaterialProxyDict();

#define EXPOSE_MATERIAL_PROXY( className, proxyName )						\
	static IMaterialProxy *C##className##Factory( void )					\
	{																		\
		return static_cast< IMaterialProxy * >( new className );			\
	};																		\
	class C##proxyName##Foo													\
	{																		\
	public:																	\
		C##proxyName##Foo( void )											\
		{																	\
			GetMaterialProxyDict().Add( #proxyName,							\
				&C##className##Factory );									\
		}																	\
	};																		\
	static C##proxyName##Foo g_C##proxyName##Foo;

#endif // IMATERIALPROXYDICT_H
