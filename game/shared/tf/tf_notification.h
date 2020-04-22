//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Enables sending of notifications (custom messages of various kinds) to the client
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_NOTIFICATION_H
#define TF_NOTIFICATION_H
#ifdef _WIN32
#pragma once
#endif

#include "gcsdk/protobufsharedobject.h"
#include "tf_gcmessages.h"

#ifdef GC
	#include "tf_gc.h"
#endif

//---------------------------------------------------------------------------------
// Purpose: Send a notification to the client
//---------------------------------------------------------------------------------

// LOCALIZED NOTIFICATIONS
//
// Some types of notification are on-the-fly localized. These notifications are stored in the database as unlocalized
// strings (#TF_Foo), but on-the-fly localized when loaded as a shared-object.  Clients only see the final localized
// version. BLocalizeAndMaybeDirty() will update the localization for a newer language.

class CTFNotification : public GCSDK::CProtoBufSharedObject< CMsgGCNotification, k_EEconTypeNotification >
{
public:
	// If using this form, ensure you call BLocalize after filling fields for localized types.

#ifdef GC
	CTFNotification();
	CTFNotification( CMsgGCNotification msg, const char *pUnlocalizedString, ELanguage eLang );
	DECLARE_CLASS_MEMPOOL( CTFNotification );

	// For on-the-fly localized notification types, update the localization to this language now.  If true, object
	// changed.  See LOCALIZED NOTIFICATIONS above.
	//
	// !! Caller is responsible for dirtying and sending network updates if this results in a change.
	bool BLocalize( ELanguage eLang );

	virtual bool BYieldingAddInsertToTransaction( GCSDK::CSQLAccess & sqlAccess );
	virtual bool BYieldingAddWriteToTransaction( GCSDK::CSQLAccess & sqlAccess, const CUtlVector< int > &fields );
	virtual bool BYieldingAddRemoveToTransaction( GCSDK::CSQLAccess & sqlAccess );

	void WriteToRecord( CSchNotification *pNotification ) const;
	void ReadFromRecord( const CSchNotification & pNotification );

private:
	// For notifications that are on-the-fly localized, this holds the localization token (which is stored in the DB),
	// whereas the in-memory object is a localized representation.
	CUtlString m_strUnlocalizedString;
	ELanguage m_eLocalizedToLanguage;
#endif // GC
};

#endif // TF_NOTIFICATION_H
