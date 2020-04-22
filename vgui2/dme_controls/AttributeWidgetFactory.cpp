//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "dme_controls/AttributeWidgetFactory.h"
#include "tier1/utldict.h"
#include "tier1/KeyValues.h"
#include "movieobjects/dmeeditortypedictionary.h"
#include "dme_controls/AttributeTextEntry.h"
#include "dme_controls/AttributeFilePickerPanel.h"
#include "dme_controls/AttributeBoolChoicePanel.h"
#include "dme_controls/AttributeIntChoicePanel.h"
#include "dme_controls/AttributeStringChoicePanel.h"
#include "dme_controls/AttributeElementPanel.h"
#include "dme_controls/AttributeElementPickerPanel.h"
#include "dme_controls/AttributeMDLPickerPanel.h"
#include "dme_controls/AttributeSequencePickerPanel.h"
#include "dme_controls/AttributeSoundPickerPanel.h"
#include "dme_controls/AttributeAssetPickerPanel.h"
#include "dme_controls/AttributeShaderPickerPanel.h"
#include "dme_controls/AttributeSurfacePropertyPickerPanel.h"
#include "dme_controls/AttributeDetailTypePickerPanel.h"
#include "dme_controls/AttributeColorPickerPanel.h"
#include "dme_controls/AttributeInterpolatorChoicePanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Forward declaration
//-----------------------------------------------------------------------------
class CAttributeWidgetFactoryList;


using namespace vgui;


//-----------------------------------------------------------------------------
// CAttributeWidgetFactoryList class definition
//-----------------------------------------------------------------------------
class CAttributeWidgetFactoryList : public IAttributeWidgetFactoryList
{
public:
	// Inherited from IAttributeWidgetFactoryList
	virtual IAttributeWidgetFactory *GetWidgetFactory( const char *pWidgetName );
	virtual IAttributeWidgetFactory *GetWidgetFactory( CDmElement *object, CDmAttribute *pAttribute, CDmeEditorTypeDictionary *pTypeDictionary );
	virtual IAttributeWidgetFactory *GetArrayWidgetFactory( CDmElement *object, CDmAttribute *pAttribute, CDmeEditorTypeDictionary *pTypeDictionary );
	virtual void ApplyChanges( vgui::Panel *pWidget, vgui::Panel *pSender = NULL );
	virtual void Refresh( vgui::Panel *pWidget, vgui::Panel *pSender = NULL );

	// Adds a widget to the factory
	void AddWidgetFactory( IAttributeWidgetFactory *pFactory, const char *pWidgetName );

	// Finds a widget factory by name
	IAttributeWidgetFactory *FindWidgetFactory( const char *pWidgetName );

	// Creates a widget using editor attribute info
//	vgui::Panel *CreateWidget( vgui::Panel *parent, CDmElement *obj, INotifyUI *pNotify, CDmeEditorAttributeInfo *pWidgetInfo, bool bAutoApply );

private:
	CUtlDict< IAttributeWidgetFactory*, unsigned short > m_Factories;
};


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CAttributeWidgetFactoryList *g_pWidgetFactoryFactoryList;
IAttributeWidgetFactoryList *attributewidgetfactorylist;

CAttributeWidgetFactoryList *GetWidgetFactoryManager()
{
	if ( !g_pWidgetFactoryFactoryList )
	{
		g_pWidgetFactoryFactoryList = new CAttributeWidgetFactoryList;
		attributewidgetfactorylist = g_pWidgetFactoryFactoryList;
	}
	return g_pWidgetFactoryFactoryList;
}


//-----------------------------------------------------------------------------
// Standard implementation of a widget factory
//-----------------------------------------------------------------------------
template < class T >
class CAttributeWidgetFactory : public IAttributeWidgetFactory
{
public:
	CAttributeWidgetFactory( const char *pWidgetName )
	{
		GetWidgetFactoryManager()->AddWidgetFactory( this, pWidgetName );
	}

	// Backward compat
	virtual vgui::Panel *Create( vgui::Panel *pParent, const AttributeWidgetInfo_t &info )
	{
		CBaseAttributePanel *newPanel = new T( pParent, info );
		if ( newPanel )
		{
			newPanel->PostConstructor();
		}
		return newPanel;
	}
};


//-----------------------------------------------------------------------------
// create all the AttributeWidgetFactorys
//-----------------------------------------------------------------------------

// An Attribute Widget Factory for: text entry
static CAttributeWidgetFactory<CAttributeTextPanel> g_AttributeTextWidgetFactory( "text" );

// An Attribute Widget Factory for: picking files
static CAttributeWidgetFactory<CAttributeDmeFilePickerPanel> g_AttributeFilePickerWidgetFactory( "filepicker" );

// An Attribute Widget Factory for: choosing integers
static CAttributeWidgetFactory<CAttributeBoolChoicePanel> g_AttributeBoolChoiceWidgetFactory( "boolchoice" );

// An Attribute Widget Factory for: choosing integers
static CAttributeWidgetFactory<CAttributeIntChoicePanel> g_AttributeIntChoiceWidgetFactory( "intchoice" );

// An Attribute Widget Factory for: choosing strings
static CAttributeWidgetFactory<CAttributeStringChoicePanel> g_AttributeStringChoiceWidgetFactory( "stringchoice" );

// An Attribute Widget Factory for: elements
static CAttributeWidgetFactory<CAttributeElementPanel> g_AttributeElementWidgetFactory( "element" );

// An Attribute Widget Factory for: picking elements
static CAttributeWidgetFactory<CAttributeElementPickerPanel> g_AttributeElementPickerWidgetFactory( "elementchoice" );

// An Attribute Widget Factory for: picking MDLs
static CAttributeWidgetFactory<CAttributeMDLPickerPanel> g_AttributeMDLPickerWidgetFactory( "mdlpicker" );

// An Attribute Widget Factory for: picking sequences
static CAttributeWidgetFactory<CAttributeSequencePickerPanel> g_AttributeSequencePickerWidgetFactory( "sequencepicker" );

// An Attribute Widget Factory for: picking sounds
static CAttributeWidgetFactory<CAttributeSoundPickerPanel> g_AttributeSoundPickerWidgetFactory( "soundpicker" );

// An Attribute Widget Factory for: picking bsps
static CAttributeWidgetFactory<CAttributeBspPickerPanel> g_AttributeBspPickerWidgetFactory( "bsppicker" );

// An Attribute Widget Factory for: picking vmts
static CAttributeWidgetFactory<CAttributeVmtPickerPanel> g_AttributeVmtPickerWidgetFactory( "vmtpicker" );

// An Attribute Widget Factory for: picking vtfs
static CAttributeWidgetFactory<CAttributeVtfPickerPanel> g_AttributeVtfPickerWidgetFactory( "vtfpicker" );

// An Attribute Widget Factory for: picking tgas
static CAttributeWidgetFactory<CAttributeTgaFilePickerPanel> g_AttributeTgaPickerWidgetFactory( "tgapicker" );

// An Attribute Widget Factory for: picking shaders
static CAttributeWidgetFactory<CAttributeShaderPickerPanel> g_AttributeShaderPickerWidgetFactory( "shaderpicker" );

// An Attribute Widget Factory for: picking surface properties
static CAttributeWidgetFactory<CAttributeSurfacePropertyPickerPanel> g_AttributeSurfacePropertyPickerWidgetFactory( "surfacepropertypicker" );

// An Attribute Widget Factory for: picking surface properties
static CAttributeWidgetFactory<CAttributeColorPickerPanel> g_AttributeColorPickerWidgetFactory( "colorpicker" );

// An Attribute Widget Factory for: picking avis
static CAttributeWidgetFactory<CAttributeAviFilePickerPanel> g_AttributeAviPickerWidgetFactory( "avipicker" );

// An Attribute Widget Factory for: picking sht
static CAttributeWidgetFactory<CAttributeShtFilePickerPanel> g_AttributeShtPickerWidgetFactory( "shtpicker" );

// An Attribute Widget Factory for: picking detail types
static CAttributeWidgetFactory<CAttributeDetailTypePickerPanel> g_AttributeDetailTypePickerWidgetFactory( "detailtypepicker" );

// An Attribute Widget Factory for: picking color correction lookup files
static CAttributeWidgetFactory<CAttributeRawFilePickerPanel> g_AttributeRawPickerWidgetFactory( "rawpicker" );

// An Attribute Widget Factory for: choosing interpolator types (left and right)
static CAttributeWidgetFactory<CAttributeInterpolatorChoicePanel> g_AttributeInterpolatorChoiceWidgetFactory( "interpolatorchoice" );


//-----------------------------------------------------------------------------
// Name-based widget factories
//-----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// g_AttributeWidgetFactories
// Purpose: a mapping of all attribute types to AttributeWidgetFactories

struct DefaultAttributeFactoryEntry_t
{
	int							attributeType;
	IAttributeWidgetFactory		*factory;
};

static DefaultAttributeFactoryEntry_t g_AttributeWidgetFactories[] =
{

	{ AT_UNKNOWN,		NULL },

	{ AT_ELEMENT,		&g_AttributeElementWidgetFactory },
	{ AT_INT,			&g_AttributeTextWidgetFactory },
	{ AT_FLOAT,			&g_AttributeTextWidgetFactory },
	{ AT_BOOL,			&g_AttributeTextWidgetFactory },
	{ AT_STRING,		&g_AttributeTextWidgetFactory },
	{ AT_VOID,			&g_AttributeTextWidgetFactory },
	{ AT_OBJECTID,		&g_AttributeTextWidgetFactory },
	{ AT_COLOR,			&g_AttributeColorPickerWidgetFactory },
	{ AT_VECTOR2,		&g_AttributeTextWidgetFactory },
	{ AT_VECTOR3,		&g_AttributeTextWidgetFactory },
	{ AT_VECTOR4,		&g_AttributeTextWidgetFactory },
	{ AT_QANGLE,		&g_AttributeTextWidgetFactory },
	{ AT_QUATERNION,	&g_AttributeTextWidgetFactory },
	{ AT_VMATRIX,		&g_AttributeTextWidgetFactory },

	{ AT_ELEMENT_ARRAY,		&g_AttributeTextWidgetFactory },
	{ AT_INT_ARRAY,			&g_AttributeTextWidgetFactory },
	{ AT_FLOAT_ARRAY,		&g_AttributeTextWidgetFactory },
	{ AT_BOOL_ARRAY,		&g_AttributeTextWidgetFactory },
	{ AT_STRING_ARRAY,		&g_AttributeTextWidgetFactory },
	{ AT_VOID_ARRAY,		&g_AttributeTextWidgetFactory },
	{ AT_ELEMENT_ARRAY,		&g_AttributeTextWidgetFactory },
	{ AT_OBJECTID_ARRAY,	&g_AttributeTextWidgetFactory },
	{ AT_COLOR_ARRAY,		&g_AttributeColorPickerWidgetFactory },
	{ AT_VECTOR2_ARRAY,		&g_AttributeTextWidgetFactory },
	{ AT_VECTOR3_ARRAY,		&g_AttributeTextWidgetFactory },
	{ AT_VECTOR4_ARRAY,		&g_AttributeTextWidgetFactory },
	{ AT_QANGLE_ARRAY,		&g_AttributeTextWidgetFactory },
	{ AT_QUATERNION_ARRAY,	&g_AttributeTextWidgetFactory },
	{ AT_VMATRIX_ARRAY,		&g_AttributeTextWidgetFactory },

};


//-----------------------------------------------------------------------------
//
// CAttributeWidgetFactoryList
//
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Adds a widget to the factory
//-----------------------------------------------------------------------------
void CAttributeWidgetFactoryList::AddWidgetFactory( IAttributeWidgetFactory *pFactory, const char *pWidgetName )
{
	m_Factories.Insert( pWidgetName, pFactory );
}


//-----------------------------------------------------------------------------
// Finds a widget factory by name
//-----------------------------------------------------------------------------
IAttributeWidgetFactory *CAttributeWidgetFactoryList::FindWidgetFactory( const char *pWidgetName )
{
	unsigned short i = m_Factories.Find( pWidgetName );
	if ( i != m_Factories.InvalidIndex() )
		return m_Factories[i];
	return NULL;
}

	
//-----------------------------------------------------------------------------
// Returns a factory requested by name
//-----------------------------------------------------------------------------
IAttributeWidgetFactory *CAttributeWidgetFactoryList::GetWidgetFactory( const char *pWidgetName )
{
	return FindWidgetFactory( pWidgetName );
}


//-----------------------------------------------------------------------------
// Returns a factory used to create widget for the attribute passed in
//-----------------------------------------------------------------------------
IAttributeWidgetFactory *CAttributeWidgetFactoryList::GetWidgetFactory( CDmElement *object, 
	CDmAttribute *pAttribute, CDmeEditorTypeDictionary *pTypeDictionary )
{
	if ( !object )
		return NULL;

	DmAttributeType_t attributeType = pAttribute->GetType();
	IAttributeWidgetFactory *pFactory = g_AttributeWidgetFactories[ attributeType ].factory;

	// Override behavior with editor info, if it exists
	if ( pTypeDictionary )
	{
		const char *pAttributeName = pAttribute->GetName();
		CDmeEditorAttributeInfo *pEditorInfo = pTypeDictionary->GetAttributeInfo( object, pAttributeName );
		if ( pEditorInfo )
		{
			if ( !pEditorInfo->m_bIsVisible )
				return NULL;

			if ( pEditorInfo->GetWidgetName() )
			{
				IAttributeWidgetFactory *pOverriddenFactory = g_pWidgetFactoryFactoryList->FindWidgetFactory( pEditorInfo->GetWidgetName() );
				if ( pOverriddenFactory )
				{
					pFactory = pOverriddenFactory;
				}
			}
		}
	}
	return pFactory;
}


//-----------------------------------------------------------------------------
// Returns a factory used to create widgets for entries in an attribute array
//-----------------------------------------------------------------------------
IAttributeWidgetFactory *CAttributeWidgetFactoryList::GetArrayWidgetFactory( CDmElement *object,
	CDmAttribute *pAttribute, CDmeEditorTypeDictionary *pTypeDictionary )
{
	if ( !object )
		return NULL;

	DmAttributeType_t attributeType = ArrayTypeToValueType( pAttribute->GetType() );
	IAttributeWidgetFactory *pFactory = g_AttributeWidgetFactories[ attributeType ].factory;

	// Override behavior with editor info, if it exists
	if ( pTypeDictionary )
	{
		CDmeEditorAttributeInfo *pEditorInfo = pTypeDictionary->GetAttributeArrayInfo( object, pAttribute->GetName() );
		if ( pEditorInfo )
		{
			if ( !pEditorInfo->m_bIsVisible )
				return NULL;

			if ( pEditorInfo->GetWidgetName() )
			{
				IAttributeWidgetFactory *pOverriddenFactory = g_pWidgetFactoryFactoryList->FindWidgetFactory( pEditorInfo->GetWidgetName() );
				if ( pOverriddenFactory )
				{
					pFactory = pOverriddenFactory;
				}
			}
		}
	}

	return pFactory;
}

	
//-----------------------------------------------------------------------------
// Applies changes to a widget
//-----------------------------------------------------------------------------
void CAttributeWidgetFactoryList::ApplyChanges( vgui::Panel *pWidget, vgui::Panel *pSender )
{
	CBaseAttributePanel *pPanel = dynamic_cast< CBaseAttributePanel *>( pWidget );
	if ( pPanel && pPanel->GetDirty() )
	{
		Assert( !pPanel->IsAutoApply() );
		vgui::ipanel()->SendMessage( pWidget->GetVPanel(), new KeyValues( "ApplyChanges" ), pSender ? pSender->GetVPanel() : NULL );
	}
}


//-----------------------------------------------------------------------------
// Refreshes a widget when attributes change
//-----------------------------------------------------------------------------
void CAttributeWidgetFactoryList::Refresh( vgui::Panel *pWidget, vgui::Panel *pSender )
{
	if ( pWidget )
	{
		vgui::ipanel()->SendMessage( pWidget->GetVPanel(), new KeyValues( "Refresh" ), pSender ? pWidget->GetVPanel() : NULL );
	}
}

