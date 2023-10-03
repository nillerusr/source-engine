//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef C_BREAKABLEPROP_H
#define C_BREAKABLEPROP_H
#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_BreakableProp : public C_BaseAnimating
{
	typedef C_BaseAnimating BaseClass;
public:
	DECLARE_CLIENTCLASS();

	C_BreakableProp();

	virtual bool IsProp( void ) const
	{
		return true;
	};

	// Copy fade from another breakable prop
	void CopyFadeFrom( C_BreakableProp *pSource );
	virtual void OnDataChanged( DataUpdateType_t type );

private:
	bool m_bClientPhysics;
};

#endif // C_BREAKABLEPROP_H
