#ifndef C_NPC_Wilson_H
#define C_NPC_Wilson_H
#ifdef _WIN32
#pragma once
#endif

#include "c_ai_basenpc.h"
#include "flashlighteffect.h"
#include "glow_overlay.h"
#include "view.h"
#include "c_pixel_visibility.h"
#include "beam_shared.h"
#include "c_rope.h"

// Based on env_lightglow code
class C_TurretGlowOverlay : public CGlowOverlay
{
public:

	virtual void CalcSpriteColorAndSize( float flDot, CGlowSprite *pSprite, float *flHorzSize, float *flVertSize, Vector *vColor )
	{
		*flHorzSize = pSprite->m_flHorzSize;
		*flVertSize = pSprite->m_flVertSize;
		
		Vector viewDir = ( CurrentViewOrigin() - m_vecOrigin );
		float distToViewer = VectorNormalize( viewDir );

		float fade;

		// See if we're in the outer fade distance range
		if ( m_nOuterMaxDist > m_nMaxDist && distToViewer > m_nMaxDist )
		{
			fade = RemapValClamped( distToViewer, m_nMaxDist, m_nOuterMaxDist, 1.0f, 0.0f );
		}
		else
		{
			fade = RemapValClamped( distToViewer, m_nMinDist, m_nMaxDist, 0.0f, 1.0f );
		}
		
		*vColor = pSprite->m_vColor * fade * m_flGlowObstructionScale;
	}

	void SetOrigin( const Vector &origin ) { m_vecOrigin = origin; }
	
	void SetFadeDistances( int minDist, int maxDist, int outerMaxDist )
	{
		m_nMinDist = minDist;
		m_nMaxDist = maxDist;
		m_nOuterMaxDist = outerMaxDist;
	}

protected:

	Vector	m_vecOrigin;
	int		m_nMinDist;
	int		m_nMaxDist;
	int		m_nOuterMaxDist;
};

class C_AI_TurretBase : public C_AI_BaseNPC
{
public:
	DECLARE_CLASS( C_AI_TurretBase, C_AI_BaseNPC );

	C_AI_TurretBase();
	~C_AI_TurretBase();

	void OnDataChanged( DataUpdateType_t type );
	void ClientThink( void );
	void Simulate( void );

	virtual void UpdateGlow( Vector &vVector, Vector &vecForward ) = 0;

	virtual C_RopeKeyframe *CreateRope( int iStartAttach, int iEndAttach ) = 0;

	int m_iLightAttachment = -1;

	bool m_bEyeLightEnabled;

	float m_EyeLightColor[3];

	int m_iLastEyeLightBrightness;
	int m_iEyeLightBrightness;

	float m_flEyeLightBrightnessScale;
	CTurretLightEffect *m_EyeLight;

	float m_flRange;
	float m_flFOV;

	int m_iRopeStartAttachments[4];
	int m_iRopeEndAttachments[4];
	C_RopeKeyframe *m_pRopes[4];

	bool m_bClosedIdle;
};

class C_NPC_Wilson : public C_AI_TurretBase
{
public:
	DECLARE_CLASS( C_NPC_Wilson, C_AI_TurretBase );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_NPC_Wilson();
	~C_NPC_Wilson();

	void OnDataChanged( DataUpdateType_t type );
	void Simulate( void );
	void ClientThink( void );

	void UpdateGlow( Vector &vVector, Vector &vecForward );
	C_RopeKeyframe *CreateRope( int iStartAttach, int iEndAttach );

	void AddViseme( Emphasized_Phoneme *classes, float emphasis_intensity, int phoneme, float scale, bool newexpression );

	C_TurretGlowOverlay m_Glow;

	float	m_flTalkGlow;
};

class C_NPC_Arbeit_FloorTurret : public C_AI_TurretBase
{
public:
	DECLARE_CLASS( C_NPC_Arbeit_FloorTurret, C_AI_TurretBase );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_NPC_Arbeit_FloorTurret();
	~C_NPC_Arbeit_FloorTurret();

	void OnDataChanged( DataUpdateType_t type );
	void Activate( void );
	void Simulate( void );

	void UpdateGlow( Vector &vVector, Vector &vecForward ) {}
	C_RopeKeyframe *CreateRope( int iStartAttach, int iEndAttach );

	bool m_bLaser;

	C_Beam *m_pLaser;

	bool m_bGooTurret;
	bool m_bCitizenTurret;
};


#endif // C_NPC_Wilson_H