//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#ifndef CURVEEDITORHELPERS_H
#define CURVEEDITORHELPERS_H
#ifdef _WIN32
#pragma once
#endif

#include "mxtk/mx.h"

struct CExpressionSample;

template< class T >
class CCurveEditorHelper
{
public:
	CCurveEditorHelper( T *outer );

	int GetBestCurveTypeForSelectedSamples( bool reflect );
	int	CountSelected( bool reflect );
	void ChangeCurveType( bool forward, bool shiftdown, bool altdown );
	void SetCurveTypeForSelectedSamples( bool reflect, int curvetype );
	void SetCurveTypeForSample( int curvetype, CExpressionSample *sample );
	void ToggleHoldTypeForSelectedSamples( bool reflect );
	void ToggleHoldTypeForSample( CExpressionSample *sample );

	bool HelperHandleEvent( mxEvent *event );

private:
	T			*GetOuter();

private:

	T			*m_pOuter;
};

template< class T >
CCurveEditorHelper<T>::CCurveEditorHelper( T *pOuter ) :
	m_pOuter( pOuter )
{
	Assert( pOuter );
}

template< class T >
T *CCurveEditorHelper<T>::GetOuter()
{
	return m_pOuter;
}

template< class T >
int CCurveEditorHelper<T>::GetBestCurveTypeForSelectedSamples( bool reflect )
{
	int numSelected = CountSelected( reflect );
	if ( !numSelected )
		return CURVE_DEFAULT;

	CUtlMap< int, int >	counts( 0, 0, DefLessFunc( int ) );

	CUtlVector< T * > workList;
	GetOuter()->GetWorkList( reflect, workList );

	for ( int w = 0; w < workList.Count(); ++w )
	{
		int numSamples = workList[ w ]->NumSamples();
		if ( !numSamples )
			continue;

		for ( int i = numSamples - 1; i >= 0 ; i-- )
		{
			CExpressionSample *sample = workList[ w ]->GetSample( i );
			if ( !sample->selected )
				continue;

			int curveType = sample->GetCurveType();
			int idx = counts.Find( curveType );
			if ( idx == counts.InvalidIndex() )
			{
				idx = counts.Insert( curveType, 0 );
			}

			counts[ idx ]++;
		}
	}

	int maxType = CURVE_DEFAULT;
	int maxCount = -1;

	for ( int i = counts.FirstInorder(); i != counts.InvalidIndex(); i = counts.NextInorder( i ) )
	{
		if ( counts[ i ] > maxType )
		{
			maxCount = counts[ i ];
			maxType = counts.Key( i );
		}
	}

	return maxType;
}

template< class T >
int CCurveEditorHelper<T>::CountSelected( bool reflect )
{
	int numSelected = 0;

	CUtlVector< T * > workList;
	GetOuter()->GetWorkList( reflect, workList );

	for ( int w = 0; w < workList.Count(); ++w )
	{
		int numSamples = workList[ w ]->NumSamples();
		if ( !numSamples )
			continue;

		for ( int i = 0 ; i < numSamples; ++i )
		{
			CExpressionSample *sample = workList[ w ]->GetSample( i );
			if ( !sample || !sample->selected )
				continue;

			++numSelected;
		}
	}

	return numSelected;
}

template< class T >
void CCurveEditorHelper<T>::ChangeCurveType( bool forward, bool shiftdown, bool altdown )
{
	// If holding ctrl and shift, only do inbound
	bool inbound = shiftdown;
	// if holding ctrl, shift + alt, do both inbound and outbound
	bool outbound = !shiftdown || altdown;
	// if holding ctrl + alt, do outbound
	// if holding just ctrl, do outbound

	int numSelected = CountSelected( false );
	if ( !numSelected )
		return;

	int curveType = GetBestCurveTypeForSelectedSamples( false );

	int sides[ 2 ];
	Interpolator_CurveInterpolatorsForType( curveType, sides[ 0 ], sides[ 1 ] );

	int dir = forward ? 1 : -1;
	for ( int i = 0; i < 2; ++i )
	{
		if ( i == 0 && !inbound )
			continue;
		if ( i == 1 && !outbound )
			continue;

		sides[ i ] += dir;
		if ( sides[ i ] < 0 )
		{
			sides[ i ] = NUM_INTERPOLATE_TYPES - 1;
		}
		else if ( sides[ i ] >= NUM_INTERPOLATE_TYPES )
		{
			sides[ i ] = INTERPOLATE_DEFAULT;
		}
	}

	curveType = MAKE_CURVE_TYPE( sides[ 0 ], sides[ 1 ] );
	SetCurveTypeForSelectedSamples( false, curveType );
}

template< class T >
void CCurveEditorHelper<T>::SetCurveTypeForSelectedSamples( bool reflect, int curvetype )
{
	int numSelected = CountSelected( reflect );
	if ( !numSelected )
		return;

	GetOuter()->PreDataChanged( "Set curve type" );

	CUtlVector< T * > workList;
	GetOuter()->GetWorkList( reflect, workList );

	for ( int w = 0; w < workList.Count(); ++w )
	{
		int numSamples = workList[ w ]->NumSamples();

		for ( int i = 0 ; i < numSamples; ++i )
		{
			CExpressionSample *sample = workList[ w ]->GetSample( i );
			if ( !sample->selected )
				continue;

			sample->SetCurveType( curvetype );
		}
	}

	GetOuter()->PostDataChanged( "Set curve type" );
}

template< class T >
void CCurveEditorHelper<T>::SetCurveTypeForSample( int curvetype, CExpressionSample *sample )
{
	GetOuter()->PreDataChanged( "Set curve type" );

	sample->SetCurveType( curvetype );

	GetOuter()->PostDataChanged( "Set curve type" );
}

template< class T >
void CCurveEditorHelper<T>::ToggleHoldTypeForSelectedSamples( bool reflect )
{
	int numSelected = CountSelected( reflect );
	if ( !numSelected )
		return;

	GetOuter()->PreDataChanged( "Set hold out value" );

	CUtlVector< T * > workList;
	GetOuter()->GetWorkList( reflect, workList );

	for ( int w = 0; w < workList.Count(); ++w )
	{
		int numSamples = workList[ w ]->NumSamples();

		int newValue = -1;

		for ( int i = 0 ; i < numSamples; ++i )
		{
			CExpressionSample *sample = workList[ w ]->GetSample( i );
			if ( !sample->selected )
				continue;

			// First one controls setting
			int l, r;
			Interpolator_CurveInterpolatorsForType( sample->GetCurveType(), l, r );

			if ( newValue == -1 )
			{
				newValue = ( r == INTERPOLATE_HOLD ) ? 0 : 1;
			}

			int newCurveType = MAKE_CURVE_TYPE( l, newValue == 1 ? INTERPOLATE_HOLD : l );
			sample->SetCurveType( newCurveType );
		}
	}

	GetOuter()->PostDataChanged( "Set hold out value" );
}

template< class T >
void CCurveEditorHelper<T>::ToggleHoldTypeForSample( CExpressionSample *sample )
{
	GetOuter()->PreDataChanged( "Set hold out value" );

	int l, r;
	Interpolator_CurveInterpolatorsForType( sample->GetCurveType(), l, r );

	if ( r == INTERPOLATE_HOLD )
	{
		r = l;
	}
	else
	{
		r = INTERPOLATE_HOLD;
	}

	int newCurveType = MAKE_CURVE_TYPE( l, r );
	sample->SetCurveType( newCurveType );

	GetOuter()->PostDataChanged( "Set hold out value" );
}

template< class T >
bool CCurveEditorHelper<T>::HelperHandleEvent( mxEvent *event )
{
	bool handled = false;

	switch ( event->event )
	{
	case mxEvent::KeyDown:
		{
			switch ( event->key )
			{
			default:
				// Hotkey pressed
				if ( event->key >= '0' && 
					 event->key <= '9' )
				{
					bool shiftdown = GetAsyncKeyState( VK_SHIFT ) ? true : false;

					handled = true;
					// Get curve type
					int curveType = Interpolator_CurveTypeForHotkey( event->key );
					if ( curveType >= 0 )
					{
						if ( CountSelected( shiftdown ) <= 0 )
						{
							GetOuter()->SetMousePositionForEvent( event );

							CExpressionSample *hover = GetOuter()->GetSampleUnderMouse( event->x, event->y, 0.0f );

							// Deal with highlighted item
							if ( hover )
							{
								SetCurveTypeForSample( curveType, hover );
							}
						}
						else
						{
							SetCurveTypeForSelectedSamples( shiftdown, curveType );
						}
					}
				}
				break;
			case 'H':
				{	
					handled = true;

					bool shiftdown = GetAsyncKeyState( VK_SHIFT ) ? true : false;

					if ( CountSelected( shiftdown ) <= 0 )
					{
						GetOuter()->SetMousePositionForEvent( event );

						CExpressionSample *hover = GetOuter()->GetSampleUnderMouse( event->x, event->y, 0.0f );

						// Deal with highlighted item
						if ( hover )
						{
							ToggleHoldTypeForSample( hover );
						}
					}
					else
					{
						ToggleHoldTypeForSelectedSamples( shiftdown );
					}
				}
				break;
			case VK_UP:
				{
					bool shiftdown = GetAsyncKeyState( VK_SHIFT ) ? true : false;
					bool altdown = GetAsyncKeyState( VK_MENU  ) ? true : false;
					if ( GetAsyncKeyState( VK_CONTROL ) )
					{
						ChangeCurveType( false, shiftdown, altdown );
					}
				}
				break;
			case VK_DOWN:
				{
					bool shiftdown = GetAsyncKeyState( VK_SHIFT ) ? true : false;
					bool altdown = GetAsyncKeyState( VK_MENU  ) ? true : false;
					if ( GetAsyncKeyState( VK_CONTROL ) )
					{
						ChangeCurveType( true, shiftdown, altdown );
					}
				}
				break;
			}
		}
	}

	return handled;
}

#endif // CURVEEDITORHELPERS_H

