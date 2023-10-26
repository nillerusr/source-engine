//====== Copyright © 1996-2009, Valve Corporation, All rights reserved. =======
//
// Purpose: Projects a texture into the world (like the flashlight)
//
//=============================================================================

#include "cbase.h"
#include "shareddefs.h"
#include "env_projectedtexture.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( env_projectedtexture, CEnvProjectedTexture );

BEGIN_DATADESC( CEnvProjectedTexture )
	DEFINE_FIELD( m_hTargetEntity, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bState, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bSimpleProjection, FIELD_BOOLEAN ),
	DEFINE_KEYFIELD( m_flLightFOV, FIELD_FLOAT, "lightfov" ),
	DEFINE_KEYFIELD( m_bEnableShadows, FIELD_BOOLEAN, "enableshadows" ),
	DEFINE_KEYFIELD( m_bSimpleProjection, FIELD_BOOLEAN, "simpleprojection" ),
	DEFINE_KEYFIELD( m_bLightOnlyTarget, FIELD_BOOLEAN, "lightonlytarget" ),
	DEFINE_KEYFIELD( m_bLightWorld, FIELD_BOOLEAN, "lightworld" ),
	DEFINE_KEYFIELD( m_bCameraSpace, FIELD_BOOLEAN, "cameraspace" ),
	DEFINE_KEYFIELD( m_flAmbient, FIELD_FLOAT, "ambient" ),
	DEFINE_AUTO_ARRAY_KEYFIELD( m_SpotlightTextureName, FIELD_CHARACTER, "texturename" ),
	DEFINE_KEYFIELD( m_nSpotlightTextureFrame, FIELD_INTEGER, "textureframe" ),
	DEFINE_KEYFIELD( m_flNearZ, FIELD_FLOAT, "nearz" ),
	DEFINE_KEYFIELD( m_flFarZ, FIELD_FLOAT, "farz" ),
	DEFINE_KEYFIELD( m_nShadowQuality, FIELD_INTEGER, "shadowquality" ),
	DEFINE_KEYFIELD( m_flBrightnessScale, FIELD_FLOAT, "brightnessscale" ),
	DEFINE_FIELD( m_LightColor, FIELD_COLOR32 ), 
	DEFINE_KEYFIELD( m_flColorTransitionTime, FIELD_FLOAT, "colortransitiontime" ),
	DEFINE_KEYFIELD( m_flProjectionSize, FIELD_FLOAT, "projection_size" ),
	DEFINE_KEYFIELD( m_flRotation, FIELD_FLOAT, "projection_rotation" ),

	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOn", InputTurnOn ),
	DEFINE_INPUTFUNC( FIELD_VOID, "TurnOff", InputTurnOff ),
	DEFINE_INPUTFUNC( FIELD_VOID, "AlwaysUpdateOn", InputAlwaysUpdateOn ),
	DEFINE_INPUTFUNC( FIELD_VOID, "AlwaysUpdateOff", InputAlwaysUpdateOff ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "FOV", InputSetFOV ),
	DEFINE_INPUTFUNC( FIELD_EHANDLE, "Target", InputSetTarget ),
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "CameraSpace", InputSetCameraSpace ),
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "LightOnlyTarget", InputSetLightOnlyTarget ),
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "LightWorld", InputSetLightWorld ),
	DEFINE_INPUTFUNC( FIELD_BOOLEAN, "EnableShadows", InputSetEnableShadows ),
	DEFINE_INPUTFUNC( FIELD_COLOR32, "LightColor", InputSetLightColor ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "Ambient", InputSetAmbient ),
	DEFINE_INPUTFUNC( FIELD_STRING, "SpotlightTexture", InputSetSpotlightTexture ),
	DEFINE_THINKFUNC( InitialThink ),
END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CEnvProjectedTexture, DT_EnvProjectedTexture )
	SendPropEHandle( SENDINFO( m_hTargetEntity ) ),
	SendPropBool( SENDINFO( m_bState ) ),
	SendPropBool( SENDINFO( m_bAlwaysUpdate ) ),
	SendPropFloat( SENDINFO( m_flLightFOV ) ),
	SendPropBool( SENDINFO( m_bEnableShadows ) ),
	SendPropBool( SENDINFO( m_bSimpleProjection ) ),
	SendPropBool( SENDINFO( m_bLightOnlyTarget ) ),
	SendPropBool( SENDINFO( m_bLightWorld ) ),
	SendPropBool( SENDINFO( m_bCameraSpace ) ),
	SendPropFloat( SENDINFO( m_flBrightnessScale ) ),
	SendPropInt( SENDINFO ( m_LightColor ),	32, SPROP_UNSIGNED, SendProxy_Color32ToInt32 ),
	SendPropFloat( SENDINFO( m_flColorTransitionTime ) ),
	SendPropFloat( SENDINFO( m_flAmbient ) ),
	SendPropString( SENDINFO( m_SpotlightTextureName ) ),
	SendPropInt( SENDINFO( m_nSpotlightTextureFrame ) ),
	SendPropFloat( SENDINFO( m_flNearZ ), 16, SPROP_ROUNDDOWN, 0.0f,  500.0f ),
	SendPropFloat( SENDINFO( m_flFarZ ),  18, SPROP_ROUNDDOWN, 0.0f, 1500.0f ),
	SendPropInt( SENDINFO( m_nShadowQuality ), 1, SPROP_UNSIGNED ),  // Just one bit for now
	SendPropFloat( SENDINFO( m_flProjectionSize ) ),
	SendPropFloat( SENDINFO( m_flRotation ) ),
END_SEND_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CEnvProjectedTexture::CEnvProjectedTexture( void )
{
	m_bState = true;
	m_bAlwaysUpdate = false;
	m_flLightFOV = 45.0f;
	m_bEnableShadows = false;
	m_bSimpleProjection = false;
	m_bLightOnlyTarget = false;
	m_bLightWorld = true;
	m_bCameraSpace = false;

	Q_strcpy( m_SpotlightTextureName.GetForModify(), "effects/flashlight_border" );
	Q_strcpy( m_SpotlightTextureName.GetForModify(), "effects/flashlight001" );

	m_nSpotlightTextureFrame = 0;
	m_flBrightnessScale = 1.0f;
	m_LightColor.Init( 255, 255, 255, 255 );
	m_flColorTransitionTime = 0.5f;
	m_flAmbient = 0.0f;
	m_flNearZ = 4.0f;
	m_flFarZ = 750.0f;
	m_nShadowQuality = 0;
	m_flProjectionSize = 500.0f;
	m_flRotation = 0.0f;
}

void UTIL_ColorStringToLinearFloatColor( Vector &color, const char *pString )
{
	float tmp[4];
	UTIL_StringToFloatArray( tmp, 4, pString );
	if( tmp[3] <= 0.0f )
	{
		tmp[3] = 255.0f;
	}
	tmp[3] *= ( 1.0f / 255.0f );
	color.x = tmp[0] * ( 1.0f / 255.0f ) * tmp[3];
	color.y = tmp[1] * ( 1.0f / 255.0f ) * tmp[3];
	color.z = tmp[2] * ( 1.0f / 255.0f ) * tmp[3];
}

bool CEnvProjectedTexture::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( FStrEq( szKeyName, "lightcolor" ) )
	{
		float tmp[4];
		UTIL_StringToFloatArray( tmp, 4, szValue );

		m_LightColor.SetR( tmp[0] );
		m_LightColor.SetG( tmp[1] );
		m_LightColor.SetB( tmp[2] );
		m_LightColor.SetA( tmp[3] );
	}
	else if ( FStrEq( szKeyName, "texturename" ) )
	{
#if defined( _X360 )
		if ( Q_strcmp( szValue, "effects/flashlight001" ) == 0 )
		{
			// Use this as the default for Xbox
			Q_strcpy( m_SpotlightTextureName.GetForModify(), "effects/flashlight_border" );
		}
		else
		{
			Q_strcpy( m_SpotlightTextureName.GetForModify(), szValue );
		}
#else
		Q_strcpy( m_SpotlightTextureName.GetForModify(), szValue );
#endif
	}
	else
	{
		return BaseClass::KeyValue( szKeyName, szValue );
	}

	return true;
}

bool CEnvProjectedTexture::GetKeyValue( const char *szKeyName, char *szValue, int iMaxLen )
{
	if ( FStrEq( szKeyName, "lightcolor" ) )
	{
		Q_snprintf( szValue, iMaxLen, "%d %d %d %d", m_LightColor.GetR(), m_LightColor.GetG(), m_LightColor.GetB(), m_LightColor.GetA() );
		return true;
	}
	else if ( FStrEq( szKeyName, "texturename" ) )
	{
		Q_snprintf( szValue, iMaxLen, "%s", m_SpotlightTextureName.Get() );
		return true;
	}
	return BaseClass::GetKeyValue( szKeyName, szValue, iMaxLen );
}

void CEnvProjectedTexture::InputTurnOn( inputdata_t &inputdata )
{
	m_bState = true;
}

void CEnvProjectedTexture::InputTurnOff( inputdata_t &inputdata )
{
	m_bState = false;
}

void CEnvProjectedTexture::InputAlwaysUpdateOn( inputdata_t &inputdata )
{
	m_bAlwaysUpdate = true;
}

void CEnvProjectedTexture::InputAlwaysUpdateOff( inputdata_t &inputdata )
{
	m_bAlwaysUpdate = false;
}

void CEnvProjectedTexture::InputSetFOV( inputdata_t &inputdata )
{
	m_flLightFOV = inputdata.value.Float();
}

void CEnvProjectedTexture::InputSetTarget( inputdata_t &inputdata )
{
	m_hTargetEntity = inputdata.value.Entity();
}

void CEnvProjectedTexture::InputSetCameraSpace( inputdata_t &inputdata )
{
	m_bCameraSpace = inputdata.value.Bool();
}

void CEnvProjectedTexture::InputSetLightOnlyTarget( inputdata_t &inputdata )
{
	m_bLightOnlyTarget = inputdata.value.Bool();
}

void CEnvProjectedTexture::InputSetLightWorld( inputdata_t &inputdata )
{
	m_bLightWorld = inputdata.value.Bool();
}

void CEnvProjectedTexture::InputSetEnableShadows( inputdata_t &inputdata )
{
	m_bEnableShadows = inputdata.value.Bool();
}

void CEnvProjectedTexture::InputSetLightColor( inputdata_t &inputdata )
{
	m_LightColor = inputdata.value.Color32();
}

void CEnvProjectedTexture::InputSetAmbient( inputdata_t &inputdata )
{
	m_flAmbient = inputdata.value.Float();
}

void CEnvProjectedTexture::InputSetSpotlightTexture( inputdata_t &inputdata )
{
	Q_strcpy( m_SpotlightTextureName.GetForModify(), inputdata.value.String() );
}

void CEnvProjectedTexture::Activate( void )
{
	m_bState = ( ( GetSpawnFlags() & ENV_PROJECTEDTEXTURE_STARTON ) != 0 );
	m_bAlwaysUpdate = ( ( GetSpawnFlags() & ENV_PROJECTEDTEXTURE_ALWAYSUPDATE ) != 0 );

	SetThink( &CEnvProjectedTexture::InitialThink );
	SetNextThink( gpGlobals->curtime + 0.1f );

	BaseClass::Activate();
}

void CEnvProjectedTexture::InitialThink( void )
{
	m_hTargetEntity = gEntList.FindEntityByName( NULL, m_target );
}

int CEnvProjectedTexture::UpdateTransmitState()
{
	return SetTransmitState( FL_EDICT_ALWAYS );
}


// Console command for creating env_projectedtexture entities
void CC_CreateFlashlight( const CCommand &args )
{
	CBasePlayer *pPlayer = UTIL_GetCommandClient();
	if( !pPlayer )
		return;

	QAngle angles = pPlayer->EyeAngles();
	Vector origin = pPlayer->EyePosition();		

	CEnvProjectedTexture *pFlashlight = dynamic_cast< CEnvProjectedTexture * >( CreateEntityByName("env_projectedtexture") );
	if( args.ArgC() > 1 )
	{
		pFlashlight->SetName( AllocPooledString( args[1] ) );
	}

	pFlashlight->Teleport( &origin, &angles, NULL );

}
static ConCommand create_flashlight("create_flashlight", CC_CreateFlashlight, 0, FCVAR_CHEAT);
