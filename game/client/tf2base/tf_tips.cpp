//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: Rich Presence support
//
//=====================================================================================//

#include "cbase.h"
#include "tf_tips.h"
#include "tier3/tier3.h"
#include <vgui/ILocalize.h>
#include "cdll_util.h"
#include "fmtstr.h"

//-----------------------------------------------------------------------------
// Purpose: constructor
//-----------------------------------------------------------------------------
CTFTips::CTFTips() : CAutoGameSystem( "CTFTips" )
{
	Q_memset( m_iTipCount, 0, sizeof( m_iTipCount ) );
	m_iTipCountAll = 0;
	m_iCurrentClassTip = 0;
	m_bInited = false;
}

//-----------------------------------------------------------------------------
// Purpose: Initializer
//-----------------------------------------------------------------------------
bool CTFTips::Init()
{
	if ( !m_bInited )
	{
		// count how many tips there are for each class and in total
		m_iTipCountAll = 0;
		for ( int iClass = TF_FIRST_NORMAL_CLASS; iClass <= TF_LAST_NORMAL_CLASS; iClass++ )
		{
			// tip count per class is stored in resource file
			wchar_t *wzTipCount = g_pVGuiLocalize->Find( CFmtStr( "Tip_%d_Count", iClass ) );
			int iClassTipCount = wzTipCount ? _wtoi( wzTipCount ) : 0;
			m_iTipCount[iClass] = iClassTipCount;
			m_iTipCountAll += iClassTipCount;
		}
		m_bInited = true;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Returns a random tip, selected from tips for all classes
//-----------------------------------------------------------------------------
const wchar_t *CTFTips::GetRandomTip()
{
	Init();

	// pick a random tip
	int iTip = RandomInt( 0, m_iTipCountAll-1 );
	// walk through each class until we find the class this tip lands in
	for ( int iClass = TF_FIRST_NORMAL_CLASS; iClass <= TF_LAST_NORMAL_CLASS; iClass++ )
	{
		Assert( iTip >= 0 );
		int iClassTipCount = m_iTipCount[iClass]; 
		if ( iTip < iClassTipCount )
		{
			// return the tip
			return GetTip( iClass, iTip+1 );
		}
		iTip -= iClassTipCount;
	}
	Assert( false );	// shouldn't hit this
	return L"";
}

//-----------------------------------------------------------------------------
// Purpose: Returns the next tip for specified class
//-----------------------------------------------------------------------------
const wchar_t *CTFTips::GetNextClassTip( int iClass )
{
	// OK to call this function with TF_CLASS_UNDEFINED or TF_CLASS_RANDOM, just return a random tip for any class in that case
	if ( iClass < TF_FIRST_NORMAL_CLASS || iClass > TF_LAST_NORMAL_CLASS )
		return GetRandomTip();

	int iClassTipCount = m_iTipCount[iClass];
	Assert( 0 != iClassTipCount );
	if ( 0 == iClassTipCount )
		return L"";
	// wrap the tip index to the valid range for this class
	if ( m_iCurrentClassTip >= iClassTipCount )
	{
		m_iCurrentClassTip %= iClassTipCount;
	}

	// return the tip
	const wchar_t *wzTip = GetTip( iClass, m_iCurrentClassTip+1 );
	m_iCurrentClassTip++;

	return wzTip;
}

//-----------------------------------------------------------------------------
// Purpose: Returns specified tip index for specified class
//-----------------------------------------------------------------------------
const wchar_t *CTFTips::GetTip( int iClass, int iTip )
{
	static wchar_t wzTip[512] = L"";

	// get the tip
	const wchar_t *wzFmt = g_pVGuiLocalize->Find( CFmtStr( "#Tip_%d_%d", iClass, iTip ) );
	// replace any commands with their bound keys
	UTIL_ReplaceKeyBindings( wzFmt, 0, wzTip, sizeof( wzTip ) );

	return wzTip;
}

// global instance
CTFTips g_TFTips;