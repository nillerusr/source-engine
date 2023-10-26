//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
//===========================================================================//

#include "cbase.h"
#include "materialsystem/IMaterialProxy.h"
#include "materialsystem/IMaterial.h"
#include "materialsystem/IMaterialVar.h"
#include "FunctionProxy.h"
#include <KeyValues.h>
#include "mathlib/VMatrix.h"

#include "imaterialproxydict.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class C_FuncConveyor : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_FuncConveyor, C_BaseEntity );
	DECLARE_CLIENTCLASS();

	C_FuncConveyor();

	float GetConveyorSpeed() { return m_flConveyorSpeed; }

private:
	float m_flConveyorSpeed;
};


IMPLEMENT_CLIENTCLASS_DT( C_FuncConveyor, DT_FuncConveyor, CFuncConveyor )
	RecvPropFloat( RECVINFO( m_flConveyorSpeed ) ),
END_RECV_TABLE()


C_FuncConveyor::C_FuncConveyor()
{
	m_flConveyorSpeed = 0.0;
}


class CConveyorMaterialProxy : public IMaterialProxy
{
public:
	CConveyorMaterialProxy();
	virtual ~CConveyorMaterialProxy();

	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pC_BaseEntity );
	virtual void Release( void ) { delete this; }
	virtual IMaterial *GetMaterial();

private:
	C_BaseEntity *BindArgToEntity( void *pArg );

	IMaterialVar *m_pTextureScrollVar;
};

CConveyorMaterialProxy::CConveyorMaterialProxy()
{
	m_pTextureScrollVar = NULL;
}

CConveyorMaterialProxy::~CConveyorMaterialProxy()
{
}


bool CConveyorMaterialProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	char const* pScrollVarName = pKeyValues->GetString( "textureScrollVar" );
	if( !pScrollVarName )
		return false;

	bool foundVar;
	m_pTextureScrollVar = pMaterial->FindVar( pScrollVarName, &foundVar, false );
	if( !foundVar )
		return false;

	return true;
}

C_BaseEntity *CConveyorMaterialProxy::BindArgToEntity( void *pArg )
{
	IClientRenderable *pRend = (IClientRenderable *)pArg;
	return pRend->GetIClientUnknown()->GetBaseEntity();
}

void CConveyorMaterialProxy::OnBind( void *pC_BaseEntity )
{
	if( !pC_BaseEntity )
		return;

	C_BaseEntity *pEntity = BindArgToEntity( pC_BaseEntity );

	if ( !pEntity )
		return;

	C_FuncConveyor *pConveyor = dynamic_cast<C_FuncConveyor*>(pEntity);

	if ( !pConveyor )
		return;

	if ( !m_pTextureScrollVar )
	{
		return;
	}

	float flConveyorSpeed	= pConveyor->GetConveyorSpeed();
	float flRate			= abs( flConveyorSpeed ) / 128.0;
	float flAngle			= (flConveyorSpeed >= 0) ? 180 : 0;

	float sOffset = gpGlobals->curtime * cos( flAngle * ( M_PI / 180.0f ) ) * flRate;
	float tOffset = gpGlobals->curtime * sin( flAngle * ( M_PI / 180.0f ) ) * flRate;
	
	// make sure that we are positive
	if( sOffset < 0.0f )
	{
		sOffset += 1.0f + -( int )sOffset;
	}
	if( tOffset < 0.0f )
	{
		tOffset += 1.0f + -( int )tOffset;
	}
			    
	// make sure that we are in a [0,1] range
	sOffset = sOffset - ( int )sOffset;
	tOffset = tOffset - ( int )tOffset;
	
	if (m_pTextureScrollVar->GetType() == MATERIAL_VAR_TYPE_MATRIX)
	{
		VMatrix mat;
		MatrixBuildTranslation( mat, sOffset, tOffset, 0.0f );
		m_pTextureScrollVar->SetMatrixValue( mat );
	}
	else
	{
		m_pTextureScrollVar->SetVecValue( sOffset, tOffset, 0.0f );
	}
}

IMaterial *CConveyorMaterialProxy::GetMaterial()
{
	return m_pTextureScrollVar ? m_pTextureScrollVar->GetOwningMaterial() : NULL;
}

EXPOSE_MATERIAL_PROXY( CConveyorMaterialProxy, ConveyorScroll );
