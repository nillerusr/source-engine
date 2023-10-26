//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Material proxy to get the cycle from a CBaseAnimateing derived entity.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
// identifier was truncated to '255' characters in the debug information
//#pragma warning(disable: 4786)

#include "ProxyEntity.h"
#include "materialsystem/IMaterialVar.h"
#include "imaterialproxydict.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"


class CCycleMaterialProxy: public CEntityMaterialProxy
{
public:
	CCycleMaterialProxy()
	{
		m_pMaterial = NULL;
		m_pResult = NULL;
		m_bEaseIn = false;
		m_bEaseOut = false;
		m_fStart = 0.0f;
		m_fEnd = 1.0f;
	}
	virtual ~CCycleMaterialProxy()
	{
	}
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues )
	{
		m_pMaterial = pMaterial;

		const char *pResult = pKeyValues->GetString( "resultVar", NULL  );
		if ( !pResult )
			return false;

		bool found;
		m_pResult = m_pMaterial->FindVar( pResult, &found );
		if ( !found )
		{
			m_pResult = NULL;
			return false;
		}

		if ( !Q_stricmp( pResult, "$alpha" ) )
		{
			pMaterial->SetMaterialVarFlag( MATERIAL_VAR_ALPHA_MODIFIED_BY_PROXY, true );
		}

		pResult = pKeyValues->GetString( "easein", NULL );
		if( pResult && Q_atoi( pResult ) != 0 )
		{
			m_bEaseIn = true;
		}

		pResult = pKeyValues->GetString( "easeout", NULL );
		if( pResult && Q_atoi( pResult ) != 0 )
		{
			m_bEaseOut = true;
		}

		pResult = pKeyValues->GetString( "start", NULL );
		if( pResult )
		{
			m_fStart = Q_atof( pResult );
		}

		pResult = pKeyValues->GetString( "end", NULL );
		if( pResult )
		{
			m_fEnd = Q_atof( pResult );
		}

		return true;
	}
	virtual void OnBind( C_BaseEntity *pC_BaseEntity )
	{
		C_BaseAnimating *pBaseAnimating = pC_BaseEntity ? pC_BaseEntity->GetBaseAnimating() : NULL;
		if ( pBaseAnimating )
		{
			float fCycle = pBaseAnimating->GetCycle();
			float f = RemapValClamped( fCycle, m_fStart, m_fEnd, 0.0f, 1.0f );
			if ( m_bEaseIn && m_bEaseOut )
			{
				f = SimpleSpline( f );
			}
			else if ( m_bEaseIn )
			{
				f = sin( M_PI * f * 0.5f );
			}
			else if ( m_bEaseOut )
			{
				f = 1.0f - sin( M_PI * f * 0.5f + 0.5f * M_PI );
			}
			
			MaterialVarType_t resultType;
			int vecSize;
			ComputeResultType( resultType, vecSize );

			switch( resultType )
			{
			case MATERIAL_VAR_TYPE_VECTOR:
				{
					Vector4D vec( f, f, f, f );
					m_pResult->SetVecValue( vec.Base(), vecSize );
				}
				break;

			case MATERIAL_VAR_TYPE_FLOAT:
			case MATERIAL_VAR_TYPE_INT:
			default:
				m_pResult->SetFloatValue( f );
				break;
			}
		}
	}
	virtual IMaterial *GetMaterial()
	{
		return m_pMaterial;
	}

	void ComputeResultType( MaterialVarType_t& resultType, int& vecSize )
	{
		vecSize = 1;
		resultType = m_pResult->GetType();
		if (resultType == MATERIAL_VAR_TYPE_VECTOR)
		{
			vecSize = m_pResult->VectorSize();
		}
	}

protected:
	IMaterial *m_pMaterial;
	IMaterialVar *m_pResult;
	bool m_bEaseIn;
	bool m_bEaseOut;
	float m_fStart;
	float m_fEnd;
};

EXPOSE_MATERIAL_PROXY( CCycleMaterialProxy, Cycle );
