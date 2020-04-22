//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Contains all world state--the main game database
//
// $Revision: $
// $NoKeywords: $
//===========================================================================//

#include "worldmanager.h"
#include "legion.h"
#include "heightfield.h"
#include "rendermanager.h"

//-----------------------------------------------------------------------------
// Singleton accessor
//-----------------------------------------------------------------------------
static CWorldManager s_WorldManager;
extern CWorldManager *g_pWorldManager = &s_WorldManager;


//-----------------------------------------------------------------------------
// ConVars
//-----------------------------------------------------------------------------
static ConVar cam_forwardspeed( "cam_forwardspeed", "100", FCVAR_CHEAT, "Sets the camera forward speed" );
static ConVar cam_backwardspeed( "cam_backwardspeed", "100", FCVAR_CHEAT, "Sets the camera backward speed" );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
CWorldManager::CWorldManager()
{
	m_pHeightField = NULL;
}

CWorldManager::~CWorldManager()
{
	Assert( m_pHeightField == NULL );
}


//-----------------------------------------------------------------------------
// Level init, shutdown
//-----------------------------------------------------------------------------
LevelRetVal_t CWorldManager::LevelInit( bool bFirstCall )
{
	if ( !bFirstCall )
		return FINISHED;

	Assert( !m_pHeightField );
	m_pHeightField = new CHeightField( 6, 6, 4 );
	if ( !m_pHeightField->LoadHeightFromFile( "maps/testheight.psd" ) )
		return FAILED;

	CreateEntities();
	SetInitialLocalPlayerPosition();
	return FINISHED;
}

LevelRetVal_t CWorldManager::LevelShutdown( bool bFirstCall )
{
	if ( !bFirstCall )
		return FINISHED;

	DestroyEntities();

	if ( m_pHeightField )
	{
		delete m_pHeightField;
		m_pHeightField = NULL;
	}
	return FINISHED;
}


//-----------------------------------------------------------------------------
// Create/ destroy entities
//-----------------------------------------------------------------------------
void CWorldManager::CreateEntities()
{
	m_PlayerEntity.m_pCameraProperty = g_pRenderManager->CreateCameraProperty();
}

void CWorldManager::DestroyEntities()
{
	g_pRenderManager->DestroyCameraProperty( m_PlayerEntity.m_pCameraProperty );
}


//-----------------------------------------------------------------------------
// Gets the camera to world matrix
//-----------------------------------------------------------------------------
CPlayerEntity* CWorldManager::GetLocalPlayer()
{
	return &m_PlayerEntity;
}


//-----------------------------------------------------------------------------
// Sets the initial camera position
//-----------------------------------------------------------------------------
void CWorldManager::SetInitialLocalPlayerPosition()
{
	float flDistance = 1024.0;
	Vector vecCameraDirection( 1.0f, 1.0f, -0.5f );
	VectorNormalize( vecCameraDirection );

	VectorMA( Vector( 512, 512, 0 ), -flDistance, vecCameraDirection, m_PlayerEntity.m_pCameraProperty->m_Origin );

	QAngle angles;
	VectorAngles( vecCameraDirection, m_PlayerEntity.m_pCameraProperty->m_Angles );

}


//-----------------------------------------------------------------------------
// Draws the UI
//-----------------------------------------------------------------------------
void CWorldManager::DrawWorld()
{
	m_pHeightField->Draw( );
}


//-----------------------------------------------------------------------------
// Commands
//-----------------------------------------------------------------------------
void CWorldManager::ForwardStart( const CCommand &args )
{
	CCameraProperty *pCamera = m_PlayerEntity.m_pCameraProperty;
	
	Vector vecForward;
	pCamera->GetForward( &vecForward );
	
	VectorMA( pCamera->m_Velocity, cam_forwardspeed.GetFloat(), vecForward, pCamera->m_Velocity );
}

void CWorldManager::ForwardStop( const CCommand &args )
{
	CCameraProperty *pCamera = m_PlayerEntity.m_pCameraProperty;

	Vector vecForward;
	pCamera->GetForward( &vecForward );

	VectorMA( pCamera->m_Velocity, -cam_forwardspeed.GetFloat(), vecForward, pCamera->m_Velocity );
}

void CWorldManager::BackwardStart( const CCommand &args )
{
	CCameraProperty *pCamera = m_PlayerEntity.m_pCameraProperty;

	Vector vecForward;
	pCamera->GetForward( &vecForward );

	VectorMA( pCamera->m_Velocity, -cam_backwardspeed.GetFloat(), vecForward, pCamera->m_Velocity );
}

void CWorldManager::BackwardStop( const CCommand &args )
{
	CCameraProperty *pCamera = m_PlayerEntity.m_pCameraProperty;

	Vector vecForward;
	pCamera->GetForward( &vecForward );

	VectorMA( pCamera->m_Velocity, cam_backwardspeed.GetFloat(), vecForward, pCamera->m_Velocity );
}