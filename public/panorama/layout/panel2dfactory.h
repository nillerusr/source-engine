//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANEL2DFACTORY_H
#define PANEL2DFACTORY_H

#ifdef _WIN32
#pragma once
#endif

#include "utlsymbol.h"
#include "tier1/utlrbtree.h"
#include "panorama/panoramasymbol.h"


namespace panorama
{
class IUIPanel;
class IUIPanelClient;
class IUIEngine;
class CPanel2DFactory;

#ifdef DBGFLAG_VALIDATE
void ValidatePanel2DFactory( CValidator &validator );
#endif

void RegisterPanelFactoriesWithEngine( IUIEngine *pEngine );

//-----------------------------------------------------------------------------
// Purpose: Used by DECLARE_PANEL2D_FACTORY macro to create a linked list of instancing functions
//-----------------------------------------------------------------------------
typedef panorama::IUIPanelClient *(*PANEL2DCREATEFUNC)(const char *pchName, panorama::IUIPanel *parent);
typedef CPanoramaSymbol( *PANELSYMBOLFUNC )();
class CPanel2DFactory
{
public:
	CPanel2DFactory( CPanoramaSymbol *pSym, char const *pchLayoutName, PANEL2DCREATEFUNC func, void *pChrisBoydBS, PANELSYMBOLFUNC symParentFunc );

	static bool BRegisteredLocalName( CUtlSymbol symName );
	CPanoramaSymbol BaseClassSymbol()  { return m_funcSymParent(); }

private:
	friend class CUIEngine;
	IUIPanelClient *CreatePanelInternal( const char *pchID, panorama::IUIPanel *parent );

	PANELSYMBOLFUNC m_funcSymParent;

	PANEL2DCREATEFUNC m_funcCreate;
};


// This is the macro which implements creation of each type of panel. It creates a function which instances an object of the specified type
// It them hooks that function up to the helper list so that objects can create the elements by name, with no header file dependency, etc.
// Params:	className = name of class to create
//			layoutName = name used in the layout file
#define REGISTER_PANEL2D_FACTORY( className, layoutName )								\
	panorama::CPanoramaSymbol className::m_symbol;			\
	static panorama::IUIPanelClient *Create_##layoutName( const char *pchID, panorama::IUIPanel *parent )			\
	{																					\
		return new className( ToPanel2D(parent), pchID );											\
	};																					\
	namespace panorama { className *g_##layoutName##LinkerHack = NULL;	}									\
	panorama::CPanel2DFactory g_##layoutName##_Helper( &className::m_symbol, #layoutName, Create_##layoutName, panorama::g_##layoutName##LinkerHack, &className::BaseClass::GetPanelSymbol );	

// Can be used to register a name so it can be used as a top level panel in XML, but can't be created through a layout file
#define REGISTER_PANEL2D( className, layoutName )																\
	panorama::CPanoramaSymbol className::m_symbol;			\
	namespace panorama { className *g_##layoutName##LinkerHack = NULL; }										\
	panorama::CPanel2DFactory g_##layoutName##_Helper( &className::m_symbol, #layoutName, NULL, panorama::g_##layoutName##LinkerHack, &className::BaseClass::GetPanelSymbol );


// Can be used to reserve a panel name w/o a panel class
#define REGISTER_PANEL_NAME( layoutName )											\
	panorama::CPanoramaSymbol k_symPanel##layoutName;							\
	panorama::CPanoramaSymbol Get_##layoutName##_Symbol() { return k_symPanel##layoutName; }			\
	panorama::CPanel2DFactory g_##layoutName##_Helper( &k_symPanel##layoutName, #layoutName, NULL, NULL, &Get_##layoutName##_Symbol );



#define DECLARE_PANEL2D( className, baseClassName )							\
public:																		\
	typedef baseClassName BaseClass;										\
	typedef className ThisClass;											\
	static panorama::CPanoramaSymbol m_symbol;										\
																	\
	static panorama::CPanoramaSymbol GetPanelSymbol() { return className::m_symbol; }		\
	virtual panorama::CPanoramaSymbol GetPanelType() const { return className::m_symbol; } \
private:


#define DECLARE_PANEL2D_NO_BASE( className )								\
public:																		\
	typedef className ThisClass;											\
	typedef className BaseClass;											\
	static panorama::CPanoramaSymbol m_symbol;										\
																	\
	static panorama::CPanoramaSymbol GetPanelSymbol() { return className::m_symbol; }		\
	virtual panorama::CPanoramaSymbol GetPanelType() const { return className::m_symbol; } \
private:

} // namespace panorama

inline uint32 HashItem( const panorama::CPanoramaSymbol &item )
{
	return ::HashItem( (UtlSymId_t)item );
}


#endif // PANEL2DFACTORY_H