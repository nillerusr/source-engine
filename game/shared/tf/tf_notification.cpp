//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "tf_notification.h"
#include "gcsdk/enumutils.h"
#include "schemainitutils.h"
// memdbgon must be the last include file in a .cpp file!!!
#ifdef GC
IMPLEMENT_CLASS_MEMPOOL( CTFNotification, 1000, UTLMEMORYPOOL_GROW_SLOW );
#endif

#include "tier0/memdbgon.h"

using namespace GCSDK;

#ifdef GC
// See LOCALIZED NOTIFICATIONS note in header
static bool BIsLocalizedNotificationType( CMsgGCNotification_NotificationType eType )
{
	switch ( eType )
	{
		case CMsgGCNotification_NotificationType_NOTIFICATION_SUPPORT_MESSAGE:
			return true;
		case CMsgGCNotification_NotificationType_NOTIFICATION_REPORTED_PLAYER_BANNED:
		case CMsgGCNotification_NotificationType_NOTIFICATION_CUSTOM_STRING:
		case CMsgGCNotification_NotificationType_NOTIFICATION_MM_BAN_DUE_TO_EXCESSIVE_REPORTS:
		case CMsgGCNotification_NotificationType_NOTIFICATION_REPORTED_PLAYER_WAS_BANNED:
		case CMsgGCNotification_NotificationType_NOTIFICATION_NUM_TYPES:
			return false;
		default:
			Assert( !"Unknown notification type" );
			return false;
	}
}

CTFNotification::CTFNotification()
	: m_eLocalizedToLanguage( k_Lang_None )
{
	Obj().set_notification_id( 0 );
	Obj().set_account_id( 0 );
	Obj().set_expiration_time( 0 );
	Obj().set_type( CMsgGCNotification_NotificationType_NOTIFICATION_NUM_TYPES ); // Invalid
	Obj().set_notification_string( "" );
}

CTFNotification::CTFNotification( CMsgGCNotification msg, const char *pUnlocalizedString, ELanguage eLang )
	: m_eLocalizedToLanguage( k_Lang_None )
{
	Obj() = msg;

	if ( BIsLocalizedNotificationType( Obj().type() ) )
	{
		m_strUnlocalizedString = pUnlocalizedString;
		BLocalize( eLang );
	}
	else
	{
		Obj().set_notification_string( pUnlocalizedString );
	}
}

bool CTFNotification::BLocalize( ELanguage eLang )
{
	if ( !BIsLocalizedNotificationType( Obj().type() ) || eLang == m_eLocalizedToLanguage )
	{
		return false;
	}

	m_eLocalizedToLanguage = eLang;
	const char *pLocalized = GGCGameBase()->LocalizeToken( m_strUnlocalizedString.Get(), eLang, false );

	// Try english fallback
	const ELanguage eFallback = k_Lang_English;
	if ( !pLocalized && eLang != eFallback )
	{
		pLocalized = GGCGameBase()->LocalizeToken( m_strUnlocalizedString.Get(), eFallback, false );
		if ( pLocalized )
		{
			EmitError( SPEW_GC,
			           "Notification has localized token \"%s\" that is not available in language %s, falling back to %s.\n",
			           m_strUnlocalizedString.Get(), GetLanguageShortName( eLang ), GetLanguageShortName( eFallback ) );
		}
	}

	if ( !pLocalized )
	{
		EmitError( SPEW_GC, "Notification has localized token \"%s\" that was not found in primary language %s *or* fallback to %s.\n",
		           m_strUnlocalizedString.Get(), GetLanguageShortName( eLang ), GetLanguageShortName( eFallback ) );
		pLocalized = m_strUnlocalizedString.Get();
	}

	Obj().set_notification_string( pLocalized ? pLocalized : m_strUnlocalizedString.Get() );

	return true;
}

bool CTFNotification::BYieldingAddInsertToTransaction( GCSDK::CSQLAccess & sqlAccess )
{
	CSchNotification schNotification;
	WriteToRecord( &schNotification );
	return CSchemaSharedObjectHelper::BYieldingAddInsertToTransaction( sqlAccess, &schNotification );
}

bool CTFNotification::BYieldingAddWriteToTransaction( GCSDK::CSQLAccess & sqlAccess, const CUtlVector< int > &fields )
{
	CSchNotification schNotification;
	WriteToRecord( &schNotification );
	CColumnSet csDatabaseDirty( schNotification.GetPSchema()->GetRecordInfo() );
	csDatabaseDirty.MakeEmpty();
	FOR_EACH_VEC( fields, nField )
	{
		switch ( fields[nField] )
		{
			case CMsgGCNotification::kNotificationIdFieldNumber		: csDatabaseDirty.BAddColumn( CSchNotification::k_iField_ulNotificationID ); break;
			case CMsgGCNotification::kAccountIdFieldNumber			: csDatabaseDirty.BAddColumn( CSchNotification::k_iField_unAccountID ); break;
			case CMsgGCNotification::kExpirationTimeFieldNumber		: csDatabaseDirty.BAddColumn( CSchNotification::k_iField_RTime32Expiration ); break;
			case CMsgGCNotification::kTypeFieldNumber				: csDatabaseDirty.BAddColumn( CSchNotification::k_iField_unNotificationType ); break;
			case CMsgGCNotification::kNotificationStringFieldNumber	: csDatabaseDirty.BAddColumn( CSchNotification::k_iField_VarCharData ); break;
			default:
				Assert( false );
		}
	}
	return CSchemaSharedObjectHelper::BYieldingAddWriteToTransaction( sqlAccess, &schNotification, csDatabaseDirty );
}

bool CTFNotification::BYieldingAddRemoveToTransaction( GCSDK::CSQLAccess & sqlAccess )
{
	CSchNotification schNotification;
	WriteToRecord( &schNotification );
	return CSchemaSharedObjectHelper::BYieldingAddRemoveToTransaction( sqlAccess, &schNotification );
}

void CTFNotification::WriteToRecord( CSchNotification *pNotification ) const
{
	// Important: For localized notifications, the DB value is the localization key (#TF_Some_Thing), but the in-memory
	//            shared object value is localized.  See LOCALIZED NOTIFICATIONS note in header.
	pNotification->m_ulNotificationID	= Obj().notification_id();
	pNotification->m_unAccountID		= Obj().account_id();
	pNotification->m_RTime32Expiration	= Obj().expiration_time();
	pNotification->m_unNotificationType	= Obj().type();
	if ( BIsLocalizedNotificationType( Obj().type() ) )
	{
		WRITE_VAR_CHAR_FIELD_TRUNC( *pNotification, VarCharData, m_strUnlocalizedString.Get() );
	}
	else
	{
		WRITE_VAR_CHAR_FIELD_TRUNC( *pNotification, VarCharData, Obj().notification_string().c_str() );
	}
}

void CTFNotification::ReadFromRecord( const CSchNotification & schNotification )
{
	// Important: For localized notifications, the DB value is the localization key (#TF_Some_Thing), but the in-memory
	//            shared object value is localized.  See LOCALIZED NOTIFICATIONS note in header.
	m_strUnlocalizedString.Clear();
	m_eLocalizedToLanguage = k_Lang_None;

	Obj().set_notification_id( schNotification.m_ulNotificationID );
	Obj().set_account_id( schNotification.m_unAccountID );
	Obj().set_expiration_time( schNotification.m_RTime32Expiration );
	Obj().set_type( ( CMsgGCNotification::NotificationType ) schNotification.m_unNotificationType );
	const char *pchNotificationString = READ_VAR_CHAR_FIELD( schNotification, m_VarCharData );
	Obj().set_notification_string( pchNotificationString );
	if ( BIsLocalizedNotificationType( Obj().type() ) )
	{
		m_strUnlocalizedString = pchNotificationString;
	}
}

#endif
