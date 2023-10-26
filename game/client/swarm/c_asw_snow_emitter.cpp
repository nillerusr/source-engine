#include "cbase.h"
#include "c_asw_snow_emitter.h"
#include "c_asw_snow_volume.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CASWSnowEmitter::CASWSnowEmitter( const char *pDebugName ) : CASWGenericEmitter( pDebugName )
{
	m_hLastMarine = NULL;
}

CSmartPtr<CASWSnowEmitter> CASWSnowEmitter::Create( const char *pDebugName )
{
	CASWSnowEmitter *pRet = new CASWSnowEmitter( pDebugName );
	pRet->SetDynamicallyAllocated( true );
	return pRet;
}

// check this particle is being spawned inside a snow volume
ASWParticle*	CASWSnowEmitter::AddASWParticle( PMaterialHandle hMaterial, const Vector &vOrigin, float flDieTime, unsigned char uchSize )
{
	if (g_ASWSnowVolumes.Count() <= 0 || !g_ASWSnowVolumes[0])
		return NULL;

	if (!C_ASW_Snow_Volume::IsPointInSnowVolume(vOrigin))
		return NULL;

	return BaseClass::AddASWParticle(hMaterial, vOrigin, flDieTime, uchSize);
}