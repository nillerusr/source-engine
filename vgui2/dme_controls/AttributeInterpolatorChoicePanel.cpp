//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "dme_controls/AttributeInterpolatorChoicePanel.h"
#include "tier1/KeyValues.h"
#include "vgui_controls/ComboBox.h"
#include "datamodel/dmelement.h"
#include "movieobjects/dmeeditortypedictionary.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "dme_controls/inotifyui.h"
#include "interpolatortypes.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


using namespace vgui;

//-----------------------------------------------------------------------------
//
// Constructor
//
//-----------------------------------------------------------------------------
CAttributeInterpolatorChoicePanel::CAttributeInterpolatorChoicePanel( vgui::Panel *parent, const AttributeWidgetInfo_t &info ) :
	BaseClass( parent, info )
{
}


//-----------------------------------------------------------------------------
// Derived classes can re-implement this to fill the combo box however they like
//-----------------------------------------------------------------------------
void CAttributeInterpolatorChoicePanel::PopulateComboBoxes( vgui::ComboBox *pComboBox[ 2 ] )
{
	pComboBox[ 0 ]->DeleteAllItems();
	pComboBox[ 1 ]->DeleteAllItems();

	// Fill in the choices
	int c = NUM_INTERPOLATE_TYPES;
	for ( int i = 0; i < c; ++i )
	{
		KeyValues *kv = new KeyValues( "entry" );
		kv->SetInt( "value", i );
		pComboBox[ 0 ]->AddItem( Interpolator_NameForInterpolator( i, true ) , kv );

		kv = new KeyValues( "entry" );
		kv->SetInt( "value", i );
		pComboBox[ 1 ]->AddItem( Interpolator_NameForInterpolator( i, true ) , kv );
	}
}


//-----------------------------------------------------------------------------
// Sets the attribute based on the combo box
//-----------------------------------------------------------------------------
void CAttributeInterpolatorChoicePanel::SetAttributeFromComboBoxes( vgui::ComboBox *pComboBox[ 2 ], KeyValues *pKeyValues[ 2 ] )
{
	int nOldValue = GetAttributeValue<int>();
	int nValueLeft = pKeyValues[ 0 ]->GetInt( "value", 0 );
	int nValueRight= pKeyValues[ 1 ]->GetInt( "value" , 0 );

	int nValue = MAKE_CURVE_TYPE( nValueLeft, nValueRight );

	// No change
	if ( nOldValue == nValue )
		return;

	CElementTreeUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, GetNotify(), "Set Attribute Value", "Set Attribute Value" );
	SetAttributeValue( nValue );
}


//-----------------------------------------------------------------------------
// Sets the combo box from the attribute 
//-----------------------------------------------------------------------------
void CAttributeInterpolatorChoicePanel::SetComboBoxesFromAttribute( vgui::ComboBox *pComboBox[ 2 ] )
{
	int nValue	= GetAttributeValue<int>();

	// Decompose
	int leftPart	= GET_LEFT_CURVE( nValue );
	int rightPart	= GET_RIGHT_CURVE( nValue );

	pComboBox[ 0 ]->SetText( Interpolator_NameForInterpolator( leftPart, true ) );
	pComboBox[ 1 ]->SetText( Interpolator_NameForInterpolator( rightPart, true ) );
}
