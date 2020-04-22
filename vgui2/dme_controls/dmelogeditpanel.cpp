//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include "dme_controls/dmelogeditpanel.h"
#include "movieobjects/dmelog.h"
#include "vgui_controls/button.h"
#include "vgui_controls/combobox.h"
#include "tier1/KeyValues.h"

using namespace vgui;



//-----------------------------------------------------------------------------
// constructor, destructor
//-----------------------------------------------------------------------------
CDmeLogEditPanel::CDmeLogEditPanel( vgui::Panel *pParent, const char *pName ) : BaseClass( pParent, pName )
{
	SetVisible( false );
	m_flMinVertical = 0;
	m_flMaxVertical = 256;
}

CDmeLogEditPanel::~CDmeLogEditPanel()
{
}


//-----------------------------------------------------------------------------
// Converts normalized values to int time
//-----------------------------------------------------------------------------
DmeTime_t CDmeLogEditPanel::NormalizedToTime( float flIn )
{
	return m_minTime + NormalizedToDuration( flIn );
}

DmeTime_t CDmeLogEditPanel::NormalizedToDuration( float flDuration )
{
	flDuration = clamp( flDuration, 0.0f, 1.0f );
	return flDuration * ( m_maxTime - m_minTime );
}

float CDmeLogEditPanel::TimeToNormalized( DmeTime_t time )
{
	if ( m_maxTime == m_minTime )
		return 0.0f;
	return GetFractionOfTimeBetween( time, m_minTime, m_maxTime, true );
}

float CDmeLogEditPanel::NormalizedToValue( float flValue )
{
	return Lerp( flValue, m_flMinVertical, m_flMaxVertical );
}

float CDmeLogEditPanel::ValueToNormalized( float flNormalized )
{
	if ( m_flMaxVertical == m_flMinVertical )
		return 0.0f;
	return (flNormalized - m_flMinVertical) / ( m_flMaxVertical - m_flMinVertical );
}


//-----------------------------------------------------------------------------
// Control points + values...
//-----------------------------------------------------------------------------
int CDmeLogEditPanel::FindOrAddControlPoint( float flIn, float flTolerance, float flOut )
{
	Assert( m_hLog.Get() );
	DmeTime_t time = NormalizedToTime( flIn );
	DmeTime_t tolerance = ( flTolerance >= 0 ) ? NormalizedToDuration( flTolerance ) : DmeTime_t( 0 );
	float flValue = NormalizedToValue( flOut );

	int nKeyIndex = -1;

	Assert( m_hLog.Get() );
	switch( m_hLog->GetDataType() )
	{
	case AT_BOOL:
 		nKeyIndex = CastElement<CDmeBoolLog >( m_hLog )->FindOrAddKey( time, tolerance, (bool)(flValue >= 0.5f) );
		break;

	case AT_INT:
 		nKeyIndex = CastElement<CDmeIntLog >( m_hLog )->FindOrAddKey( time, tolerance, (int)(flValue + 0.5f) );
		break;

	case AT_FLOAT:
 		nKeyIndex = CastElement<CDmeFloatLog >( m_hLog )->FindOrAddKey( time, tolerance, flValue );
 		break;

	case AT_COLOR:
		{
			Color c = CastElement<CDmeColorLog >( m_hLog )->GetValue( time );
			int nComp = (int)( flValue + 0.5f );
			nComp = clamp( nComp, 0, 255 );
			for ( int i = 0; i < 4; ++i )
			{
				if ( m_LogFieldMask & (1 << i) )
				{
					c[i] = (unsigned char)nComp;
				}
			}
 			nKeyIndex = CastElement<CDmeColorLog >( m_hLog )->FindOrAddKey( time, tolerance, c );
		}
		break;

	case AT_VECTOR2:
		nKeyIndex = FindOrAddKey< Vector2D >( time, tolerance, 2, flValue );
		break;

	case AT_VECTOR3:
		nKeyIndex = FindOrAddKey< Vector >( time, tolerance, 3, flValue );
		break;

	case AT_VECTOR4:
		nKeyIndex = FindOrAddKey< Vector4D >( time, tolerance, 4, flValue );
		break;

	case AT_QANGLE:
		nKeyIndex = FindOrAddKey< QAngle >( time, tolerance, 3, flValue );
		break;

	case AT_QUATERNION:
		nKeyIndex = FindOrAddKey< Quaternion >( time, tolerance, 4, flValue );
		break;
	}
	return nKeyIndex;
}


//-----------------------------------------------------------------------------
// Finds a control point within tolerance
//-----------------------------------------------------------------------------
int CDmeLogEditPanel::FindControlPoint( float flIn, float flTolerance )
{
	Assert( m_hLog.Get() );
	DmeTime_t time = NormalizedToTime( flIn ); 
	DmeTime_t tolerance = NormalizedToDuration( flTolerance ); 
	return m_hLog->FindKeyWithinTolerance( time, tolerance );
}


//-----------------------------------------------------------------------------
// Modifies an existing control point
//-----------------------------------------------------------------------------
int CDmeLogEditPanel::ModifyControlPoint( int nPoint, float flIn, float flOut )
{
	Assert( m_hLog.Get() );
	DmeTime_t time = NormalizedToTime( flIn ); 
	DmeTime_t initialTime = m_hLog->GetKeyTime( nPoint ); 
	float flValue = NormalizedToValue( flOut );

	int nKeyIndex = -1;

	Assert( m_hLog.Get() );
	switch( m_hLog->GetDataType() )
	{
	case AT_BOOL:
		RemoveControlPoint( nPoint );
 		nKeyIndex = CastElement<CDmeBoolLog >( m_hLog )->FindOrAddKey( time, DmeTime_t( 0 ), (bool)(flValue >= 0.5f) );
		break;

	case AT_INT:
		RemoveControlPoint( nPoint );
 		nKeyIndex = CastElement<CDmeIntLog >( m_hLog )->FindOrAddKey( time, DmeTime_t( 0 ), (int)(flValue + 0.5f) );
		break;

	case AT_FLOAT:
 		RemoveControlPoint( nPoint );
		nKeyIndex = CastElement<CDmeFloatLog >( m_hLog )->FindOrAddKey( time, DmeTime_t( 0 ), flValue );
 		break;

	case AT_COLOR:
		{
			Color c = CastElement<CDmeColorLog >( m_hLog )->GetValue( initialTime );
			int nComp = (int)( flValue + 0.5f );
			nComp = clamp( nComp, 0, 255 );
			for ( int i = 0; i < 4; ++i )
			{
				if ( m_LogFieldMask & (1 << i) )
				{
					c[i] = (unsigned char)nComp;
				}
			}
 			RemoveControlPoint( nPoint );
			nKeyIndex = CastElement<CDmeColorLog >( m_hLog )->FindOrAddKey( time, DmeTime_t( 0 ), c );
		}
		break;

	case AT_VECTOR2:
		nKeyIndex = ModifyKey< Vector2D >( nPoint, initialTime, time, 2, flValue );
		break;

	case AT_VECTOR3:
		nKeyIndex = ModifyKey< Vector >( nPoint, initialTime, time, 3, flValue );
		break;

	case AT_VECTOR4:
		nKeyIndex = ModifyKey< Vector4D >( nPoint, initialTime, time, 4, flValue );
		break;

	case AT_QANGLE:
		nKeyIndex = ModifyKey< QAngle >( nPoint, initialTime, time, 3, flValue );
		break;

	case AT_QUATERNION:
		nKeyIndex = ModifyKey< Quaternion >( nPoint, initialTime, time, 4, flValue );
		break;
	}
	return nKeyIndex;
}


//-----------------------------------------------------------------------------
// Removes a single control point
//-----------------------------------------------------------------------------
void CDmeLogEditPanel::RemoveControlPoint( int nPoint )
{
	Assert( m_hLog.Get() );
	m_hLog->RemoveKey( nPoint );
}


//-----------------------------------------------------------------------------
// Gets the interpolated value of the log based on normalized time
//-----------------------------------------------------------------------------
float CDmeLogEditPanel::GetValue( float flIn )
{
	DmeTime_t time = NormalizedToTime( flIn ); 

	float flValue = 0.0f;

	Assert( m_hLog.Get() );
	switch( m_hLog->GetDataType() )
	{
	case AT_BOOL:
 		flValue = CastElement<CDmeBoolLog >( m_hLog )->GetValue( time );
		break;

	case AT_INT:
 		flValue = CastElement<CDmeIntLog >( m_hLog )->GetValue( time );
		break;

	case AT_FLOAT:
 		flValue = CastElement<CDmeFloatLog >( m_hLog )->GetValue( time );
 		break;

	case AT_COLOR:
		{
			Color c = CastElement<CDmeColorLog >( m_hLog )->GetValue( time );
			flValue = c[m_nFieldIndex];
		}
		break;

	case AT_VECTOR2:
 		flValue = CastElement<CDmeVector2Log >( m_hLog )->GetValue( time )[m_nFieldIndex];
		break;

	case AT_VECTOR3:
 		flValue = CastElement<CDmeVector3Log >( m_hLog )->GetValue( time )[m_nFieldIndex];
		break;

	case AT_VECTOR4:
 		flValue = CastElement<CDmeVector2Log >( m_hLog )->GetValue( time )[m_nFieldIndex];
		break;

	case AT_QANGLE:
 		flValue = CastElement<CDmeQAngleLog >( m_hLog )->GetValue( time )[m_nFieldIndex];
		break;

	case AT_QUATERNION:
 		flValue = CastElement<CDmeQuaternionLog >( m_hLog )->GetValue( time )[m_nFieldIndex];
		break;
	}

	return ValueToNormalized( flValue );
}

int CDmeLogEditPanel::ControlPointCount()
{
	Assert( m_hLog.Get() );
	return m_hLog->GetKeyCount( );
}


//-----------------------------------------------------------------------------
// Gets a particular control point's value
//-----------------------------------------------------------------------------
void CDmeLogEditPanel::GetControlPoint( int nPoint, float *pIn, float *pOut )
{
	Assert( m_hLog.Get() );
	DmeTime_t time = m_hLog->GetKeyTime( nPoint );
	*pIn = TimeToNormalized( time );

	float flValue = 0.0f;

	Assert( m_hLog.Get() );
	switch( m_hLog->GetDataType() )
	{
	case AT_BOOL:
 		flValue = CastElement<CDmeBoolLog >( m_hLog )->GetKeyValue( nPoint );
		break;

	case AT_INT:
 		flValue = CastElement<CDmeIntLog >( m_hLog )->GetKeyValue( nPoint );
		break;

	case AT_FLOAT:
 		flValue = CastElement<CDmeFloatLog >( m_hLog )->GetKeyValue( nPoint );
 		break;

	case AT_COLOR:
		{
			Color c = CastElement<CDmeColorLog >( m_hLog )->GetKeyValue( nPoint );
			flValue = c[m_nFieldIndex];
		}
		break;

	case AT_VECTOR2:
 		flValue = CastElement<CDmeVector2Log >( m_hLog )->GetKeyValue( nPoint )[m_nFieldIndex];
		break;

	case AT_VECTOR3:
 		flValue = CastElement<CDmeVector3Log >( m_hLog )->GetKeyValue( nPoint )[m_nFieldIndex];
		break;

	case AT_VECTOR4:
 		flValue = CastElement<CDmeVector2Log >( m_hLog )->GetKeyValue( nPoint )[m_nFieldIndex];
		break;

	case AT_QANGLE:
 		flValue = CastElement<CDmeQAngleLog >( m_hLog )->GetKeyValue( nPoint )[m_nFieldIndex];
		break;

	case AT_QUATERNION:
 		flValue = CastElement<CDmeQuaternionLog >( m_hLog )->GetKeyValue( nPoint )[m_nFieldIndex];
		break;
	}

	*pOut = ValueToNormalized( flValue );
}


//-----------------------------------------------------------------------------
// Sets the log to edit
//-----------------------------------------------------------------------------
void CDmeLogEditPanel::SetDmeLog( CDmeLog *pLog )
{
	bool bValid = pLog && ( pLog->GetDataType() == AT_INT || pLog->GetDataType() == AT_FLOAT || pLog->GetDataType() == AT_COLOR );
	if ( bValid )
	{
		m_hLog = pLog;
	}
	else
	{
		m_minTime.SetSeconds( 0.0f );
		m_maxTime.SetSeconds( 0.0f );
	}
	SetVisible( bValid );
}


void CDmeLogEditPanel::SetMask( int nMask )
{
	m_LogFieldMask = nMask;
	m_nFieldIndex = 0;
	for ( int i = 0; i < 4; ++i )
	{
		if ( m_LogFieldMask & (1 << i) )
		{
			m_nFieldIndex = i;
			break;
		}
	}
}


//-----------------------------------------------------------------------------
// Sets the time range on the view in ms
//-----------------------------------------------------------------------------
void CDmeLogEditPanel::SetTimeRange( DmeTime_t startTime, DmeTime_t endTime )
{
	m_minTime = startTime;
	m_maxTime = endTime;
}


//-----------------------------------------------------------------------------
// Sets the vertical range on the view
//-----------------------------------------------------------------------------
void CDmeLogEditPanel::SetVerticalRange( float flMin, float flMax )
{
	m_flMinVertical = flMin;
	m_flMaxVertical = flMax;
}

	
//-----------------------------------------------------------------------------
//
// Purpose: Modal picker frame
//
//-----------------------------------------------------------------------------
CDmeLogEditFrame::CDmeLogEditFrame( vgui::Panel *pParent, const char *pTitle ) : 
	BaseClass( pParent, "DmeLogEditFrame" )
{
	m_pContextKeyValues = NULL;
	SetDeleteSelfOnClose( true );
	m_pCurveEditor = new CDmeLogEditPanel( this, "DmeLogEditPanel" );
	m_pOkButton = new Button( this, "OkButton", "#GameUI_OK", this, "Ok" );
	m_pCancelButton = new Button( this, "CancelButton", "#GameUI_Cancel", this, "Cancel" );
	m_pFilter = new ComboBox( this, "LogFilter", 5, false );
	SetBlockDragChaining( true );

	LoadControlSettingsAndUserConfig( "resource/dmelogeditframe.res" );

	SetTitle( pTitle, false );
}

CDmeLogEditFrame::~CDmeLogEditFrame()
{
	CleanUpMessage();
}

		    
//-----------------------------------------------------------------------------
// Deletes the message
//-----------------------------------------------------------------------------
void CDmeLogEditFrame::CleanUpMessage()
{
	if ( m_pContextKeyValues )
	{
		m_pContextKeyValues->deleteThis();
		m_pContextKeyValues = NULL;
	}
}


//-----------------------------------------------------------------------------
// Purpose: Called when the combo box changes
//-----------------------------------------------------------------------------
void CDmeLogEditFrame::OnTextChanged( )
{
	KeyValues *pKeyValues = m_pFilter->GetActiveItemUserData();
	int nMask = pKeyValues->GetInt( "Value", CDmeLogEditPanel::FIELD_ALL );
	m_pCurveEditor->SetMask( nMask );
}


//-----------------------------------------------------------------------------
// Purpose: Activate the dialog
//-----------------------------------------------------------------------------
void CDmeLogEditFrame::DoModal( CDmeLog *pLog, DmeTime_t startTime, DmeTime_t endTime, KeyValues *pKeyValues )
{
	CleanUpMessage();
	m_pContextKeyValues = pKeyValues;
	m_pCurveEditor->SetDmeLog( pLog );
	m_pCurveEditor->SetTimeRange( startTime, endTime );

	m_pFilter->SetVisible( true );
	m_pFilter->RemoveAll();

	switch( pLog->GetDataType() )
	{
	case AT_BOOL:
	case AT_INT:
	case AT_FLOAT:
		m_pFilter->SetVisible( false );
		break;

	case AT_COLOR:
		m_pFilter->AddItem( "RGB Channel", new KeyValues( "Mask", "Value", CDmeLogEditPanel::FIELD_R | CDmeLogEditPanel::FIELD_G | CDmeLogEditPanel::FIELD_B ) );
		m_pFilter->AddItem( "Red Channel", new KeyValues( "Mask", "Value", CDmeLogEditPanel::FIELD_R ) );
		m_pFilter->AddItem( "Green Channel", new KeyValues( "Mask", "Value", CDmeLogEditPanel::FIELD_G ) );
		m_pFilter->AddItem( "Blue Channel", new KeyValues( "Mask", "Value", CDmeLogEditPanel::FIELD_B ) );
		m_pFilter->AddItem( "Alpha Channel", new KeyValues( "Mask", "Value", CDmeLogEditPanel::FIELD_A ) );
		break;

	case AT_VECTOR2:
		m_pFilter->AddItem( "X Channel", new KeyValues( "Mask", "Value", CDmeLogEditPanel::FIELD_X ) );
		m_pFilter->AddItem( "Y Channel", new KeyValues( "Mask", "Value", CDmeLogEditPanel::FIELD_Y ) );
		break;

	case AT_VECTOR3:
	case AT_QANGLE:
		m_pFilter->AddItem( "X Channel", new KeyValues( "Mask", "Value", CDmeLogEditPanel::FIELD_X ) );
		m_pFilter->AddItem( "Y Channel", new KeyValues( "Mask", "Value", CDmeLogEditPanel::FIELD_Y ) );
		m_pFilter->AddItem( "Z Channel", new KeyValues( "Mask", "Value", CDmeLogEditPanel::FIELD_Z ) );
		break;

	case AT_VECTOR4:
	case AT_QUATERNION:
		m_pFilter->AddItem( "X Channel", new KeyValues( "Mask", "Value", CDmeLogEditPanel::FIELD_X ) );
		m_pFilter->AddItem( "Y Channel", new KeyValues( "Mask", "Value", CDmeLogEditPanel::FIELD_Y ) );
		m_pFilter->AddItem( "Z Channel", new KeyValues( "Mask", "Value", CDmeLogEditPanel::FIELD_Z ) );
		m_pFilter->AddItem( "W Channel", new KeyValues( "Mask", "Value", CDmeLogEditPanel::FIELD_W ) );
		break;
	}

	if ( m_pFilter->IsVisible() )
	{
		// Will cause the mask to be set
		m_pFilter->ActivateItemByRow( 0 ); 
	}
	else
	{
		m_pCurveEditor->SetMask( CDmeLogEditPanel::FIELD_ALL );
	}
	BaseClass::DoModal();
}


//-----------------------------------------------------------------------------
// On command
//-----------------------------------------------------------------------------
void CDmeLogEditFrame::OnCommand( const char *pCommand )
{
	if ( !Q_stricmp( pCommand, "Ok" ) )
	{
		KeyValues *pActionKeys = new KeyValues( "LogEdited" );
		if ( m_pContextKeyValues )
		{
			pActionKeys->AddSubKey( m_pContextKeyValues );

			// This prevents them from being deleted later
			m_pContextKeyValues = NULL;
		}

		PostActionSignal( pActionKeys );
		CloseModal();
		return;
	}

	if ( !Q_stricmp( pCommand, "Cancel" ) )
	{
		CloseModal();
		return;
	}

	BaseClass::OnCommand( pCommand );
}