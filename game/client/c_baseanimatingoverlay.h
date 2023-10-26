//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#ifndef C_BASEANIMATINGOVERLAY_H
#define C_BASEANIMATINGOVERLAY_H
#pragma once

#include "c_baseanimating.h"
#include "toolframework/itoolframework.h"

// For shared code.
#define CBaseAnimatingOverlay C_BaseAnimatingOverlay


class C_BaseAnimatingOverlay : public C_BaseAnimating
{
	DECLARE_CLASS( C_BaseAnimatingOverlay, C_BaseAnimating );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();
	DECLARE_INTERPOLATION();

	// Inherited from C_BaseAnimating
public:
	virtual void	AccumulateLayers( IBoneSetup &boneSetup, Vector pos[], Quaternion q[], float currentTime );
	virtual void	DoAnimationEvents( CStudioHdr *pStudioHdr );
	virtual void	GetRenderBounds( Vector& theMins, Vector& theMaxs );
	virtual CStudioHdr *OnNewModel();

	virtual bool	Interpolate( float flCurrentTime );

public:
	enum
	{
		MAX_OVERLAYS = 15,
	};

	C_BaseAnimatingOverlay();
	CAnimationLayer* GetAnimOverlay( int i );
	void			SetNumAnimOverlays( int num );	// This makes sure there is space for this # of layers.
	int				GetNumAnimOverlays() const;
	void			SetOverlayPrevEventCycle( int nSlot, float flValue );

	void			CheckInterpChanges( void );
	void			CheckForLayerPhysicsInvalidate( void );

private:
	void CheckForLayerChanges( CStudioHdr *hdr, float currentTime );

	CUtlVector < CAnimationLayer >	m_AnimOverlay;
	CUtlVector < CInterpolatedVar< CAnimationLayer > >	m_iv_AnimOverlay;

	float m_flOverlayPrevEventCycle[ MAX_OVERLAYS ];

	C_BaseAnimatingOverlay( const C_BaseAnimatingOverlay & ); // not defined, not accessible

	friend void ResizeAnimationLayerCallback( void *pStruct, int offsetToUtlVector, int len );
};


inline void C_BaseAnimatingOverlay::SetOverlayPrevEventCycle( int nSlot, float flValue )
{
	m_flOverlayPrevEventCycle[nSlot] = flValue;
}

EXTERN_RECV_TABLE(DT_BaseAnimatingOverlay);


#endif // C_BASEANIMATINGOVERLAY_H




