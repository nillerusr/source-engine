//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Contains a bunch of information about editor types
// Editor types are arbitrary
//
// $NoKeywords: $
//
//=============================================================================//

#include "movieobjects/dmeeditortypedictionary.h"
#include "datamodel/dmelementfactoryhelper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose DmeEditorAttributeInfo to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeEditorAttributeInfo, CDmeEditorAttributeInfo );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeEditorAttributeInfo::OnConstruction()
{
	m_Widget.Init( this, "widget" );
	m_bIsVisible.Init( this, "isVisible" );
	m_bIsReadOnly.Init( this, "isReadOnly" );
	m_ArrayEntries.Init( this, "arrayEntries" );
	m_bHideType.Init( this, "hideType" );
	m_bHideValue.Init( this, "hideValue" );
	m_Help.Init( this, "help" );

	m_bIsVisible = true;
	m_bIsReadOnly = false;
}

void CDmeEditorAttributeInfo::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Returns the attribute name this info is associated with
//-----------------------------------------------------------------------------
const char *CDmeEditorAttributeInfo::GetAttributeName() const
{
	return GetName();
}

	
//-----------------------------------------------------------------------------
// Returns the widget name
//-----------------------------------------------------------------------------
const char *CDmeEditorAttributeInfo::GetWidgetName() const
{
	return m_Widget.Get();
}

	
//-----------------------------------------------------------------------------
// Returns the info for a entry in an attribute array, if this attribute is an array type
//-----------------------------------------------------------------------------
CDmeEditorAttributeInfo *CDmeEditorAttributeInfo::GetArrayInfo()
{
	return m_ArrayEntries;
}


//-----------------------------------------------------------------------------
// Sets the info for an entry in an attribute array
//-----------------------------------------------------------------------------
void CDmeEditorAttributeInfo::SetArrayInfo( CDmeEditorAttributeInfo *pInfo )
{
	m_ArrayEntries = pInfo;
}

	
//-----------------------------------------------------------------------------
//
// CDmeEditorChoicesInfo, Base class for configuration for choices
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Expose DmeEditorChoicesInfo to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeEditorChoicesInfo, CDmeEditorChoicesInfo );

    
//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeEditorChoicesInfo::OnConstruction()
{
	m_Choices.Init( this, "choices" );
	m_ChoiceType.Init( this, "choicetype" );
}

void CDmeEditorChoicesInfo::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Sets/gets choice type
//-----------------------------------------------------------------------------
void CDmeEditorChoicesInfo::SetChoiceType( const char *pChoiceType )
{
	m_ChoiceType = pChoiceType;
}

const char *CDmeEditorChoicesInfo::GetChoiceType() const
{
	return m_ChoiceType;
}

bool CDmeEditorChoicesInfo::HasChoiceType() const
{
	return m_ChoiceType.Length() > 0;
}


//-----------------------------------------------------------------------------
// Gets the choices
//-----------------------------------------------------------------------------
int CDmeEditorChoicesInfo::GetChoiceCount() const
{
	return m_Choices.Count();
}

CDmElement *CDmeEditorChoicesInfo::CreateChoice( const char *pChoiceString )
{
	CDmElement *pChoice = CreateElement< CDmElement >( "", GetFileId() );
	m_Choices.AddToTail( pChoice );
	pChoice->SetValue<CUtlString>( "string", pChoiceString );
	return pChoice;
}

const char *CDmeEditorChoicesInfo::GetChoiceString( int nIndex ) const
{
	Assert( ( nIndex < GetChoiceCount() ) && ( nIndex >= 0 ) );
	CDmElement *pChoice = m_Choices[nIndex];
	if ( !pChoice )
		return "";

	return pChoice->GetValueString( "string" );
}


//-----------------------------------------------------------------------------
// Expose DmeEditorType class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeEditorType, CDmeEditorType );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeEditorType::OnConstruction()
{
}

void CDmeEditorType::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Computes the actual attribute name stored in the type
//-----------------------------------------------------------------------------
const char *CDmeEditorType::GetActualAttributeName( const char *pAttributeName )
{
	// Fixup the names of the attribute info for the 3 standard fields (name, type, id)
	if ( !V_stricmp( "name", pAttributeName ) )
		return "__name";

	if ( !V_stricmp( "id", pAttributeName ) )
		return "__id";

	if ( !V_stricmp( "type", pAttributeName ) )
		return "__type";

	return pAttributeName;
}


//-----------------------------------------------------------------------------
// Returns the editor info associated with an editor type
//-----------------------------------------------------------------------------
void CDmeEditorType::AddAttributeInfo( const char *pAttributeName, CDmeEditorAttributeInfo *pInfo )
{
	pAttributeName = GetActualAttributeName( pAttributeName );
	SetValue( pAttributeName, pInfo ); 
}


//-----------------------------------------------------------------------------
// Removes a editor type associated with a particular attribute
//-----------------------------------------------------------------------------
void CDmeEditorType::RemoveAttributeInfo( const char *pAttributeName )
{
	pAttributeName = GetActualAttributeName( pAttributeName );
	if ( HasAttribute( pAttributeName ) )
	{
		RemoveAttribute( pAttributeName ); 
	}
}

	
//-----------------------------------------------------------------------------
// Returns the editor info associated with an editor type
//-----------------------------------------------------------------------------
CDmeEditorAttributeInfo *CDmeEditorType::GetAttributeInfo( const char *pAttributeName )
{
	pAttributeName = GetActualAttributeName( pAttributeName );
	if ( !HasAttribute( pAttributeName ) )
		return NULL;

	return GetValueElement< CDmeEditorAttributeInfo >( pAttributeName );
}


//-----------------------------------------------------------------------------
// Returns the editor info associated with a single entry in an attribute array
//-----------------------------------------------------------------------------
CDmeEditorAttributeInfo *CDmeEditorType::GetAttributeArrayInfo( const char *pAttributeName )
{
	CDmeEditorAttributeInfo *pAttributeInfo = GetAttributeInfo( pAttributeName );
	if ( !pAttributeInfo )
		return NULL;

	return pAttributeInfo->GetArrayInfo();
}


//-----------------------------------------------------------------------------
// Expose DmeEditorTypeDictionary class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeEditorTypeDictionary, CDmeEditorTypeDictionary );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeEditorTypeDictionary::OnConstruction()
{
}

void CDmeEditorTypeDictionary::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeEditorTypeDictionary::AddEditorType( CDmeEditorType *pEditorType )
{
	const char *pEditorTypeName = pEditorType->GetName();
	if ( HasAttribute( pEditorTypeName ) )
	{
		Warning( "Editor type %s is already defined! Ignoring...\n", pEditorTypeName );
		return;
	}

	SetValue( pEditorTypeName, pEditorType->GetHandle() );
}

void CDmeEditorTypeDictionary::AddEditorTypesFromFile( const char *pFileName, const char *pPathID )
{
}


//-----------------------------------------------------------------------------
// Returns the editor type to use with an element
//-----------------------------------------------------------------------------
CDmeEditorType *CDmeEditorTypeDictionary::GetEditorType( CDmElement *pElement )
{
	if ( !pElement )
		return NULL;

	const char *pEditorTypeName = NULL;
	if ( pElement->HasAttribute( "editorType" ) )
	{
		pEditorTypeName = pElement->GetValueString( "editorType" );
	}

	if ( !pEditorTypeName || !pEditorTypeName[0] )
	{
		// Try to use the type name as an editor
		pEditorTypeName = pElement->GetTypeString();
	}

	if ( !pEditorTypeName || !pEditorTypeName[0] )
		return NULL;

	if ( !HasAttribute( pEditorTypeName ) )
		return NULL;

	return GetValueElement< CDmeEditorType >( pEditorTypeName );
}


//-----------------------------------------------------------------------------
// Returns the editor info associated with an editor type
//-----------------------------------------------------------------------------
CDmeEditorAttributeInfo *CDmeEditorTypeDictionary::GetAttributeInfo( CDmElement *pElement, const char *pAttributeName )
{
	CDmeEditorType *pEditorType = GetEditorType( pElement );
	if ( !pEditorType )
		return NULL;
	return pEditorType->GetAttributeInfo( pAttributeName );
}


//-----------------------------------------------------------------------------
// Returns the editor info associated with an editor type
//-----------------------------------------------------------------------------
CDmeEditorAttributeInfo *CDmeEditorTypeDictionary::GetAttributeArrayInfo( CDmElement *pElement, const char *pAttributeName )
{
	CDmeEditorType *pEditorType = GetEditorType( pElement );
	if ( !pEditorType )
		return NULL;
	return pEditorType->GetAttributeArrayInfo( pAttributeName );
}
