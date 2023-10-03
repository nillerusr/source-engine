//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "beam_shared.h"
#include "ai_motor.h"
#include "asw_ai_behavior.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


BEGIN_DATADESC( CAI_ASW_Behavior )
END_DATADESC();


static CUtlSymbolTable	BehaviorSymbols;


CUtlSymbol CAI_ASW_Behavior::GetSymbol( const char *pszText )
{
	return BehaviorSymbols.AddString( pszText );
}


const char *CAI_ASW_Behavior::GetSymbolText( CUtlSymbol &Symbol )
{
	return BehaviorSymbols.String( Symbol );
}


//------------------------------------------------------------------------------
// Purpose: constructor
//------------------------------------------------------------------------------
CAI_ASW_Behavior::CAI_ASW_Behavior( )
{
	m_StatusParm = UTL_INVAL_SYMBOL;
	m_nNumRequiredParms = 0;
	m_flScheduleChance = 100.0f;
	m_flScheduleChanceRate = -1;
	m_flScheduleChanceNextCheck = gpGlobals->curtime;
}


bool CAI_ASW_Behavior::KeyValue( const char *szKeyName, const char *szValue )
{
	if ( V_stricmp( szKeyName, "requires_param" ) == 0 )
	{
		char	*pszTemp = ( char * )stackalloc( strlen( szValue ) + 1 );
		strcpy( pszTemp, szValue );

		while( 1 )
		{
			char	*pszKey, *pszOperator, *pszValue;

			pszKey = strtok( pszTemp, " " );
			pszTemp = NULL;
			pszOperator = strtok( NULL, " " );
			pszValue = strtok( NULL, " ," );

			if ( pszKey == NULL || pszOperator == NULL || pszValue == NULL )
			{
				break;
			}

			m_RequiredParm[ m_nNumRequiredParms ] = GetSymbol( pszKey );
			m_nRequiredParmValue[ m_nNumRequiredParms ] = atoi( pszValue );
			m_nNumRequiredParms++;
		}
#if 0
		strcpy( pszTemp, szValue );
		pszPosKey = pszTemp;
		while( ( *pszPosKey ) != 0 && ( *pszPosKey ) != ' ' && ( *pszPosKey ) != ',' )
		{
			pszPosKey++;
		}

		if ( ( *pszPosKey ) != 0 )
		{
			pszPosValue = pszPosKey;

			while( ( *pszPosValue ) != 0 && ( ( *pszPosValue ) == ' ' || ( *pszPosValue ) == ',' ) )
			{
				pszPosValue++;
			}

			if ( ( *pszPosKey ) != 0 && ( *pszPosValue ) != 0 )
			{
				*pszPosKey = 0;
				m_RequiredParm = pszTemp;
				m_nRequiredParmValue = atoi( pszPosValue );
			}
		}
#endif
		return true;
	}
	else if ( V_stricmp( szKeyName, "status_param" ) == 0 )
	{
		m_StatusParm = GetSymbol( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "schedule_chance" ) == 0 )
	{
		m_flScheduleChance = atof( szValue );
		return true;
	}
	else if ( V_stricmp( szKeyName, "schedule_chance_rate" ) == 0 )
	{
		m_flScheduleChanceRate = atof( szValue );
		return true;
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}


bool CAI_ASW_Behavior::CanSelectSchedule( )
{
	if ( m_nNumRequiredParms != 0 )
	{
		for( int i = 0; i < m_nNumRequiredParms; i++ )
		{
			if ( GetBehaviorParam( m_RequiredParm[ i ] ) != m_nRequiredParmValue[ i ] )
			{
				return false;
			}
		}
	}

	if ( m_flScheduleChance < 100.0f )
	{
		if ( m_flScheduleChanceRate > 0.0f )
		{
			if ( m_flScheduleChanceNextCheck > gpGlobals->curtime )
			{
				return false;
			}
			m_flScheduleChanceNextCheck = gpGlobals->curtime + m_flScheduleChanceRate;
		}
		if ( RandomFloat( 0.0f, 100.0f ) > m_flScheduleChance )
		{
			return false;
		}
	}

	return true;
}


void CAI_ASW_Behavior::GatherConditions( )
{
	BaseClass::GatherConditions();

	GatherCommonConditions();
}


void CAI_ASW_Behavior::GatherConditionsNotActive( )
{
	BaseClass::GatherConditions();

	GatherCommonConditions();
}

AI_BEGIN_CUSTOM_SCHEDULE_PROVIDER( CAI_ASW_Behavior )

	DECLARE_CONDITION( COND_CHARGE_CAN_CHARGE )

AI_END_CUSTOM_SCHEDULE_PROVIDER()


#include "tier0/memdbgoff.h"