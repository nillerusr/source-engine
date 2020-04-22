//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "KeyValues.h"
#include "dme_controls/ElementPropertiesTree.h"
#include "datamodel/dmelement.h"

#include "vgui_controls/TextEntry.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/Button.h"
#include "vgui_controls/PanelListPanel.h"

#include "FactoryOverloads.h"

void CFactoryOverloads::AddOverload( 
	char const *attributeName, 
	IAttributeWidgetFactory *newFactory,
	IAttributeElementChoiceList *newChoiceList )
{
	Assert( attributeName );
	Assert( newFactory || newChoiceList );

	if ( !newFactory )
	{
		return;
	}

	Entry_t e;
	e.factory = newFactory;
	e.choices = newChoiceList;

	m_Overloads.Insert( attributeName, e );
}

int CFactoryOverloads::Count()
{
	return m_Overloads.Count();
}

char const *CFactoryOverloads::Name( int index )
{
	return m_Overloads.GetElementName( index );
}

IAttributeWidgetFactory *CFactoryOverloads::Factory( int index )
{
	return m_Overloads[ index ].factory;
}

IAttributeElementChoiceList *CFactoryOverloads::ChoiceList( int index )
{
	return m_Overloads[ index ].choices;
}