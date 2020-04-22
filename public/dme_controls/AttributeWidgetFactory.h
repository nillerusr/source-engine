//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef ATTRIBUTEWIDGETFACTORY_H
#define ATTRIBUTEWIDGETFACTORY_H

#ifdef _WIN32
#pragma once
#endif

#include "tier0/platform.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmElement;
class CMovieDoc;
class IDmNotify;
class CDmeEditorAttributeInfo;
class CDmeEditorTypeDictionary;
class CDmAttribute;

namespace vgui
{
	class EditablePanel;
	class Panel;
}


//-----------------------------------------------------------------------------
// Info about the attribute being edited, and how the editor should look
//-----------------------------------------------------------------------------
struct AttributeWidgetInfo_t
{
	AttributeWidgetInfo_t()
	{
		m_nArrayIndex = -1;
	}

	CDmElement *m_pElement;
	const char *m_pAttributeName;
	int m_nArrayIndex;
	CDmeEditorTypeDictionary *m_pEditorTypeDictionary;
	CDmeEditorAttributeInfo *m_pEditorInfo;

	IDmNotify *m_pNotify;
	bool m_bAutoApply;
	bool m_bShowMemoryUsage;
};


//-----------------------------------------------------------------------------
// Interface used to create an attribute widget
//-----------------------------------------------------------------------------
class IAttributeWidgetFactory
{
public:
	virtual vgui::Panel *Create( vgui::Panel *pParent, const AttributeWidgetInfo_t &info ) = 0;
};


//-----------------------------------------------------------------------------
// Templatized class used to create widget factories
//-----------------------------------------------------------------------------
class IAttributeWidgetFactoryList
{
public:
	// Returns a named widget factory
	virtual IAttributeWidgetFactory *GetWidgetFactory( const char *pWidgetName ) = 0;

	// Returns a factory used to create widget for the attribute passed in
	virtual IAttributeWidgetFactory *GetWidgetFactory( CDmElement *object, CDmAttribute *pAttribute, CDmeEditorTypeDictionary *pTypeDictionary	) = 0;

	// Returns a factory used to create widgets for entries in an attribute array
	virtual IAttributeWidgetFactory *GetArrayWidgetFactory( CDmElement *object, CDmAttribute *pAttribute, CDmeEditorTypeDictionary *pTypeDictionary ) = 0;

	// Applies changes to a widget
	virtual void ApplyChanges( vgui::Panel *pWidget, vgui::Panel *pSender = NULL ) = 0;

	// Refreshes a widget when attributes change
	virtual void Refresh( vgui::Panel *pWidget, vgui::Panel *pSender = NULL ) = 0;
};

extern IAttributeWidgetFactoryList *attributewidgetfactorylist;


#endif // ATTRIBUTEWIDGETFACTORY_H