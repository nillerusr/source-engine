//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//


#include "cbase.h"
#include "tempentity.h"
#include "tempent.h"
#include "c_te_legacytempents.h"


#define NUM_MUZZLE_SPRITES 3


class CHL1TempEnts : public CTempEnts
{
public:
			CHL1TempEnts( void )	{ };
	virtual	~CHL1TempEnts( void )	{ };

public:
	virtual void	Init( void );
	virtual void	LevelInit();
	void	MuzzleFlash( const Vector &pos1, const QAngle &angles, int type, ClientEntityHandle_t hEntity, bool firstPerson = false );

private:
	CHL1TempEnts( const CHL1TempEnts & );

private:
	struct model_t			*m_pSpriteMuzzleFlash[ NUM_MUZZLE_SPRITES ];
};


// Temp entity interface
static CHL1TempEnts g_TempEnts;
// Expose to rest of the client .dll
ITempEnts *tempents = ( ITempEnts * )&g_TempEnts;

void CHL1TempEnts::LevelInit()
{
	CTempEnts::LevelInit();

	m_pSpriteMuzzleFlash[0] = (model_t *)engine->LoadModel( "sprites/muzzleflash1.vmt" );
	m_pSpriteMuzzleFlash[1] = (model_t *)engine->LoadModel( "sprites/muzzleflash2.vmt" );
	m_pSpriteMuzzleFlash[2] = (model_t *)engine->LoadModel( "sprites/muzzleflash3.vmt" );
}

void CHL1TempEnts::Init( void )
{
	CTempEnts::Init();

	m_pSpriteMuzzleFlash[0] = (model_t *)engine->LoadModel( "sprites/muzzleflash1.vmt" );
	m_pSpriteMuzzleFlash[1] = (model_t *)engine->LoadModel( "sprites/muzzleflash2.vmt" );
	m_pSpriteMuzzleFlash[2] = (model_t *)engine->LoadModel( "sprites/muzzleflash3.vmt" );
}


void CHL1TempEnts::MuzzleFlash( const Vector& pos1, const QAngle& angles, int type, ClientEntityHandle_t hEntity, bool firstPerson )
{
	C_LocalTempEntity	*pTemp;
	int					index;
	float				scale;
	int					frameCount;
	QAngle				ang;

	index = type % 10;
	index = index % NUM_MUZZLE_SPRITES;

	scale = ( type / 10 ) * 0.1;
	if ( scale == 0 )
		scale = 0.5;

	frameCount = modelinfo->GetModelFrameCount( m_pSpriteMuzzleFlash[index] );

	// DevMsg( 1,"%d %f\n", index, scale );
	pTemp = TempEntAlloc( pos1, ( model_t * )m_pSpriteMuzzleFlash[index] );
	if (!pTemp)
		return;

	pTemp->SetRenderMode( kRenderTransAdd );
	pTemp->m_nRenderFX = 0;
	pTemp->SetRenderColor( 255, 255, 255, 255 );
	pTemp->m_flSpriteScale = scale;
	pTemp->SetAbsOrigin( pos1 );
	pTemp->die = gpGlobals->curtime + 0.01;
	pTemp->m_flFrame = random->RandomInt( 0, frameCount-1 );
	pTemp->m_flFrameMax = frameCount - 1;

	ang = vec3_angle;
	if (index == 0)
	{
		ang.z = random->RandomInt( 0, 20 );
	}
	else
	{
		ang.z = random->RandomInt( 0, 359 );
	}
	pTemp->SetAbsAngles( ang );
}
