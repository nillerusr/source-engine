//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef DMELOGEDITPANEL_H
#define DMELOGEDITPANEL_H

#ifdef _WIN32
#pragma once
#endif


#include "vgui_controls/Frame.h"
#include "matsys_controls/curveeditorpanel.h"
#include "datamodel/dmehandle.h"
#include "movieobjects/timeutils.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeLog;

namespace vgui
{
	class ComboBox;
}

//-----------------------------------------------------------------------------
//
// Curve editor for float DmeLogs
//
//-----------------------------------------------------------------------------
class CDmeLogEditPanel : public CCurveEditorPanel
{
	DECLARE_CLASS_SIMPLE( CDmeLogEditPanel, CCurveEditorPanel );

public:
	enum LogField_t
	{
		FIELD_X = 0x1,
		FIELD_Y = 0x2,
		FIELD_Z = 0x4,
		FIELD_W = 0x8,

		FIELD_R = 0x1,
		FIELD_G = 0x2,
		FIELD_B = 0x4,
		FIELD_A = 0x8,

		FIELD_ALL = 0xF,
	};


	// constructor
	CDmeLogEditPanel( vgui::Panel *pParent, const char *pName );
	~CDmeLogEditPanel();

	// Sets the log to edit
	void SetDmeLog( CDmeLog *pLog );
	void SetMask( int nMask );

	// Sets the time range on the view in ms
	void SetTimeRange( DmeTime_t startTime, DmeTime_t endTime );

	// Sets the vertical range on the view
	void SetVerticalRange( float flMin, float flMax );

protected:
	// Control points + values...
	virtual int FindOrAddControlPoint( float flIn, float flTolerance, float flOut );
	virtual int FindControlPoint( float flIn, float flTolerance );
	virtual int ModifyControlPoint( int nPoint, float flIn, float flOut );
	virtual void RemoveControlPoint( int nPoint );
	virtual float GetValue( float flIn );
	virtual int ControlPointCount();
	virtual void GetControlPoint( int nPoint, float *pIn, float *pOut );

private:
	// Converts normalized values to int time
	DmeTime_t NormalizedToTime( float flIn );
	DmeTime_t NormalizedToDuration( float flDuration );
	float TimeToNormalized( DmeTime_t time );
	float NormalizedToValue( float flValue );
	float ValueToNormalized( float flNormalized );

	template< class T > int FindOrAddKey( DmeTime_t time, DmeTime_t tolerance, int nComps, float flValue );
	template< class T > int ModifyKey( int nPoint, DmeTime_t initialTime, DmeTime_t time, int nComps, float flValue );

	CDmeHandle<CDmeLog> m_hLog;
	int m_LogFieldMask;
	int m_nFieldIndex;
	DmeTime_t m_minTime;
	DmeTime_t m_maxTime;
	float m_flMinVertical;
	float m_flMaxVertical;
};


//-----------------------------------------------------------------------------
// Finds or adds a key
//-----------------------------------------------------------------------------
template< class T > 
int CDmeLogEditPanel::FindOrAddKey( DmeTime_t time, DmeTime_t tolerance, int nComps, float flValue )
{
	T vec = CastElement< CDmeTypedLog<T> >( m_hLog )->GetValue( time );
	for ( int i = 0; i < nComps; ++i )
	{
		if ( m_LogFieldMask & (1 << i) )
		{
			vec[i] = flValue;
		}
	}
 	return CastElement< CDmeTypedLog<T> >( m_hLog )->FindOrAddKey( time, tolerance, vec );
}


//-----------------------------------------------------------------------------
// Modifies an existing key
//-----------------------------------------------------------------------------
template< class T > 
int CDmeLogEditPanel::ModifyKey( int nPoint, DmeTime_t initialTime, DmeTime_t time, int nComps, float flValue )
{
	T vec = CastElement< CDmeTypedLog<T> >( m_hLog )->GetValue( initialTime );
	for ( int i = 0; i < nComps; ++i )
	{
		if ( m_LogFieldMask & (1 << i) )
		{
			vec[i] = flValue;
		}
	}
  	RemoveControlPoint( nPoint );
	return CastElement< CDmeTypedLog<T> >( m_hLog )->FindOrAddKey( time, DmeTime_t( 0 ), vec );
}

//-----------------------------------------------------------------------------
// Purpose: Main app window
//-----------------------------------------------------------------------------
class CDmeLogEditFrame : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CDmeLogEditFrame, vgui::Frame );

public:
	CDmeLogEditFrame( vgui::Panel *pParent, const char *pTitle );
	~CDmeLogEditFrame();

	// Inherited from Frame
	virtual void OnCommand( const char *pCommand );

	// Purpose: Activate the dialog
	// the message "LogEdited" will be sent if ok was hit
	// Pass in a message to add as a subkey to the DmeSelected message
	void DoModal( CDmeLog *pLog, DmeTime_t startTime, DmeTime_t endTime, KeyValues *pContextKeyValues = NULL );

private:
	MESSAGE_FUNC( OnTextChanged, "TextChanged" );

	void CleanUpMessage();

	CDmeLogEditPanel *m_pCurveEditor;
	vgui::Button *m_pOkButton;
	vgui::Button *m_pCancelButton;
	vgui::ComboBox *m_pFilter;
	KeyValues *m_pContextKeyValues;
};


#endif // DMELOGEDITPANEL_H