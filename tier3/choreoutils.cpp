//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Helper methods + classes for file access
//
//===========================================================================//

#include "tier3/choreoutils.h"
#include "tier3/tier3.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "studio.h"
#include "../game/shared/choreoscene.h"
#include "../game/shared/choreoevent.h"
#include "tier1/KeyValues.h"
#include "bone_setup.h"
#include "soundchars.h"


//-----------------------------------------------------------------------------
// Find sequence by name 
//-----------------------------------------------------------------------------
static int LookupSequence( CStudioHdr *pStudioHdr, const char *pSequenceName )
{
	for ( int i = 0; i < pStudioHdr->GetNumSeq(); i++ )
	{
		if ( !Q_stricmp( pSequenceName, pStudioHdr->pSeqdesc( i ).pszLabel() ) )
			return i;
	}
	return -1;
}


//-----------------------------------------------------------------------------
// Returns sequence flags 
//-----------------------------------------------------------------------------
static int GetSequenceFlags( CStudioHdr *pStudioHdr, int nSequence )
{
	if ( !pStudioHdr || nSequence < 0 || nSequence >= pStudioHdr->GetNumSeq() )
		return 0;
	mstudioseqdesc_t &seqdesc = pStudioHdr->pSeqdesc( nSequence );
	return seqdesc.flags;
}


//-----------------------------------------------------------------------------
// Does a sequence loop? 
//-----------------------------------------------------------------------------
static bool DoesSequenceLoop( CStudioHdr *pStudioHdr, int nSequence )
{
	int nFlags = GetSequenceFlags( pStudioHdr, nSequence );
	bool bLooping = ( nFlags & STUDIO_LOOPING ) ? true : false;
	return bLooping;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool AutoAddGestureKeys( CChoreoEvent *e, CStudioHdr *pStudioHdr, float *pPoseParameters, bool bCheckOnly )
{
	int iSequence = LookupSequence( pStudioHdr, e->GetParameters() );
	if ( iSequence < 0 )
		return false;

	KeyValues *pSeqKeyValues = new KeyValues( "" );
	if ( !pSeqKeyValues->LoadFromBuffer( pStudioHdr->pszName(), Studio_GetKeyValueText( pStudioHdr, iSequence ) ) )
	{
		pSeqKeyValues->deleteThis();
		return false;
	}

	// Do we have a build point section?
	KeyValues *pKVAllFaceposer = pSeqKeyValues->FindKey("faceposer");
	if ( !pKVAllFaceposer )
	{
		pSeqKeyValues->deleteThis();
		return false;
	}

	int nMaxFrame = Studio_MaxFrame( pStudioHdr, iSequence, pPoseParameters ) - 1;

	// Start grabbing the sounds and slotting them in
	KeyValues *pkvFaceposer;
	char szStartLoop[CEventAbsoluteTag::MAX_EVENTTAG_LENGTH] = { "loop" };
	char szEndLoop[CEventAbsoluteTag::MAX_EVENTTAG_LENGTH] = { "end" };
	char szEntry[CEventAbsoluteTag::MAX_EVENTTAG_LENGTH] = { "apex" };
	char szExit[CEventAbsoluteTag::MAX_EVENTTAG_LENGTH] = { "end" };

	for ( pkvFaceposer = pKVAllFaceposer->GetFirstSubKey(); pkvFaceposer; pkvFaceposer = pkvFaceposer->GetNextKey() )
	{
		if ( !Q_stricmp( pkvFaceposer->GetName(), "startloop" ) )
		{
			Q_strncpy( szStartLoop, pkvFaceposer->GetString(), sizeof(szStartLoop) );
			continue;
		}
		
		if ( !Q_stricmp( pkvFaceposer->GetName(), "endloop" ) )
		{
			Q_strncpy( szEndLoop, pkvFaceposer->GetString(), sizeof(szEndLoop) );
			continue;
		}
		
		if ( !Q_stricmp( pkvFaceposer->GetName(), "entrytag" ) )
		{
			Q_strncpy( szEntry, pkvFaceposer->GetString(), sizeof(szEntry) );
			continue;
		}
		
		if ( !Q_stricmp( pkvFaceposer->GetName(), "exittag" ) )
		{
			Q_strncpy( szExit, pkvFaceposer->GetString(), sizeof(szExit) );
			continue;
		}
		
		if ( !Q_stricmp( pkvFaceposer->GetName(), "tags" ) )
		{
			if ( nMaxFrame <= 0 )
				continue;

			KeyValues *pkvTags;
			for ( pkvTags = pkvFaceposer->GetFirstSubKey(); pkvTags; pkvTags = pkvTags->GetNextKey() )
			{
				float flPercentage = (float)pkvTags->GetInt() / nMaxFrame;

				CEventAbsoluteTag *ptag = e->FindAbsoluteTag( CChoreoEvent::ORIGINAL, pkvTags->GetName() );
				if (ptag)
				{
					// reposition tag
					ptag->SetPercentage( flPercentage );
				}
				else
				{
					e->AddAbsoluteTag( CChoreoEvent::ORIGINAL, pkvTags->GetName(), flPercentage );
					e->AddAbsoluteTag( CChoreoEvent::PLAYBACK, pkvTags->GetName(), flPercentage );
				}
				// lock the original tags so they can't be edited
				ptag = e->FindAbsoluteTag( CChoreoEvent::ORIGINAL, pkvTags->GetName() );
				Assert( ptag );
				ptag->SetLocked( true );
			}
			e->VerifyTagOrder();
			e->PreventTagOverlap();
			continue;
		}
	}

	// FIXME: lookup linear tags in sequence data
	{
		CEventAbsoluteTag *ptag;
		ptag = e->FindAbsoluteTag( CChoreoEvent::ORIGINAL, szStartLoop );
		if (ptag)
		{
			ptag->SetLinear( true );
		}
		ptag = e->FindAbsoluteTag( CChoreoEvent::PLAYBACK, szStartLoop );
		if (ptag)
		{
			ptag->SetLinear( true );
		}
		ptag = e->FindAbsoluteTag( CChoreoEvent::ORIGINAL, szEndLoop );
		if (ptag)
		{
			ptag->SetLinear( true );
		}
		ptag = e->FindAbsoluteTag( CChoreoEvent::PLAYBACK, szEndLoop );
		if (ptag)
		{
			ptag->SetLinear( true );
		}

		ptag = e->FindAbsoluteTag( CChoreoEvent::ORIGINAL, szEntry );
		if (ptag)
		{
			ptag->SetEntry( true );
		}
		ptag = e->FindAbsoluteTag( CChoreoEvent::PLAYBACK, szEntry );
		if (ptag)
		{
			ptag->SetEntry( true );
		}
		ptag = e->FindAbsoluteTag( CChoreoEvent::ORIGINAL, szExit );
		if (ptag)
		{
			ptag->SetExit( true );
		}
		ptag = e->FindAbsoluteTag( CChoreoEvent::PLAYBACK, szExit );
		if (ptag)
		{
			ptag->SetExit( true );
		}
	}

	pSeqKeyValues->deleteThis();
	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool UpdateGestureLength( CChoreoEvent *e, CStudioHdr *pStudioHdr, float *pPoseParameters, bool bCheckOnly )
{
	Assert( e );
	if ( !e )
		return false;

	if ( e->GetType() != CChoreoEvent::GESTURE )
		return false;

	int iSequence = LookupSequence( pStudioHdr, e->GetParameters() );
	if ( iSequence < 0 )
		return false;

	bool bChanged = false;
	float flSeqDuration = Studio_Duration( pStudioHdr, iSequence, pPoseParameters );
	float flCurDuration;
	e->GetGestureSequenceDuration( flCurDuration );
	if ( flSeqDuration != 0.0f && flSeqDuration != flCurDuration )
	{
		bChanged = true;
		if ( !bCheckOnly )
		{
			e->SetGestureSequenceDuration( flSeqDuration );
		}
	}

	return bChanged;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool UpdateSequenceLength( CChoreoEvent *e, CStudioHdr *pStudioHdr, float *pPoseParameters, bool bCheckOnly, bool bVerbose )
{
	Assert( e );
	if ( !e )
		return false;

	if ( e->GetType() != CChoreoEvent::SEQUENCE )
	{
		if ( bVerbose )
		{
			ConMsg( "UpdateSequenceLength:  called on non-SEQUENCE event %s\n", e->GetName() );
		}
		return false;
	}

	int iSequence = LookupSequence( pStudioHdr, e->GetParameters() );
	if ( iSequence < 0 )
		return false;

	bool bChanged = false;
	bool bLooping = DoesSequenceLoop( pStudioHdr, iSequence );
	float flSeqDuration = Studio_Duration( pStudioHdr, iSequence, pPoseParameters );

	if ( bLooping )
	{
		if ( e->IsFixedLength() )
		{
			if ( bCheckOnly )
				return true;
	
			if ( bVerbose )
			{
				ConMsg( "UpdateSequenceLength:  %s is looping, removing fixed length flag\n", e->GetName() );
			}
			bChanged = true;
		}
		e->SetFixedLength( false );

		if ( !e->HasEndTime() )
		{
			if ( bCheckOnly )
				return true;

			if ( bVerbose )
			{
				ConMsg( "CheckSequenceLength:  %s is looping, setting default end time\n", e->GetName() );
			}
			e->SetEndTime( e->GetStartTime() + flSeqDuration );
			bChanged = true;
		}

		return bChanged;
	}

	if ( !e->IsFixedLength() )
	{
		if ( bCheckOnly )
			return true;

		if ( bVerbose )
		{
			ConMsg( "CheckSequenceLength:  %s is fixed length, removing looping flag\n", e->GetName() );
		}
		bChanged = true;
	}
	e->SetFixedLength( true );

	if ( e->HasEndTime() )
	{
		float dt = e->GetDuration();
		if ( fabs( dt - flSeqDuration ) > 0.01f )
		{
			if ( bCheckOnly )
				return true;
			if ( bVerbose )
			{
				ConMsg( "CheckSequenceLength:  %s has wrong duration, changing length from %f to %f seconds\n",
					e->GetName(), dt, flSeqDuration );
			}
			bChanged = true;
		}
	}
	else
	{
		if ( bCheckOnly )
			return true;
		if ( bVerbose )
		{
			ConMsg( "CheckSequenceLength:  %s has wrong duration, changing length to %f seconds\n",
				e->GetName(), flSeqDuration );
		}
		bChanged = true;
	}

	if ( !bCheckOnly )
	{
		e->SetEndTime( e->GetStartTime() + flSeqDuration );
	}

	return bChanged;
}


//-----------------------------------------------------------------------------
// Finds sound files associated with events
//-----------------------------------------------------------------------------
const char *GetSoundForEvent( CChoreoEvent *pEvent, CStudioHdr *pStudioHdr )
{
	const char *pSoundName = pEvent->GetParameters();
	if ( Q_stristr( pSoundName, ".wav" ) )
		return PSkipSoundChars( pSoundName );

	const char *pFileName = g_pSoundEmitterSystem->GetWavFileForSound( pSoundName, ( pStudioHdr && pStudioHdr->IsValid() ) ? pStudioHdr->pszName() : NULL );
	return PSkipSoundChars( pFileName );
}
