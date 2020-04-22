//========= Copyright Valve Corporation, All rights reserved. ============//
//
// NOTE: This is a cut-and-paste hack job to get animation set construction
// working from a commandline tool. It came from tools/ifm/createsfmanimation.cpp
// This file needs to die almost immediately + be replaced with a better solution
// that can be used both by the sfm + sfmgen.
//
//=============================================================================

#include "sfmobjects/sfmanimationsetutils.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmeclip.h"
#include "movieobjects/dmetrackgroup.h"
#include "movieobjects/dmetrack.h"
#include "movieobjects/dmecamera.h"
#include "movieobjects/dmetimeselection.h"
#include "movieobjects/dmeanimationset.h"
#include "movieobjects/dmegamemodel.h"
#include "sfmobjects/flexcontrolbuilder.h"
#include "tier3/tier3.h"
#include "bone_setup.h"
#include "vstdlib/random.h"
#include "tier1/KeyValues.h"
#include "filesystem.h"
#include "movieobjects/timeutils.h"


#define ANIMATION_SET_DEFAULT_GROUP_MAPPING_FILE	"cfg/SFM_DefaultAnimationGroups.txt"
#define STANDARD_CHANNEL_TRACK_GROUP	"channelTrackGroup"
#define STANDARD_ANIMATIONSET_CHANNELS_TRACK	"animSetEditorChannels"
#define CLIP_PREROLL_TIME DmeTime_t( 5.0f )
#define CLIP_POSTROLL_TIME DmeTime_t( 5.0f )


//-----------------------------------------------------------------------------
// Creates channels clip for the animation set
//-----------------------------------------------------------------------------
static CDmeChannelsClip* CreateChannelsClip( CDmeAnimationSet *pAnimationSet, CDmeFilmClip *pOwnerClip )
{
	CDmeTrackGroup *pTrackGroup = pOwnerClip->FindOrAddTrackGroup( "channelTrackGroup" );
	if ( !pTrackGroup )
	{
		Assert( 0 );
		return NULL;
	}

	CDmeTrack *pAnimSetEditorTrack = pTrackGroup->FindOrAddTrack( "animSetEditorChannels", DMECLIP_CHANNEL );
	Assert( pAnimSetEditorTrack );

	CDmeChannelsClip *pChannelsClip = CreateElement< CDmeChannelsClip >( pAnimationSet->GetName(), pAnimationSet->GetFileId() );
	pAnimSetEditorTrack->AddClip( pChannelsClip );

	DmeTime_t childMediaTime = pOwnerClip->GetStartInChildMediaTime();
	pChannelsClip->SetStartTime( childMediaTime - CLIP_PREROLL_TIME );
	DmeTime_t childMediaDuration = pOwnerClip->ToChildMediaDuration( pOwnerClip->GetDuration() );
	pChannelsClip->SetDuration( childMediaDuration + CLIP_PREROLL_TIME + CLIP_POSTROLL_TIME );
	return pChannelsClip;
}


//-----------------------------------------------------------------------------
// Creates a constant valued log
//-----------------------------------------------------------------------------
template < class T >
CDmeChannel *CreateConstantValuedLog( CDmeChannelsClip *channelsClip, const char *basename, const char *pName, CDmElement *pToElement, const char *pToAttr, const T &value )
{
	char name[ 256 ];
	Q_snprintf( name, sizeof( name ), "%s_%s channel", basename, pName );

	CDmeChannel *pChannel = CreateElement< CDmeChannel >( name, channelsClip->GetFileId() );
	pChannel->SetMode( CM_PLAY );
	pChannel->CreateLog( CDmAttributeInfo< T >::AttributeType() );
	pChannel->SetOutput( pToElement, pToAttr );
	pChannel->GetLog()->SetValueThreshold( 0.0f );

	((CDmeTypedLog< T > *)pChannel->GetLog())->InsertKey( DmeTime_t( 0 ), value );

	channelsClip->m_Channels.AddToTail( pChannel );

	return pChannel;
}


//-----------------------------------------------------------------------------
// Create channels for transform data
//-----------------------------------------------------------------------------
static void CreateTransformChannels( CDmeTransform *pTransform, const char *pBaseName, int bi, CDmeChannelsClip *pChannelsClip )
{
	char name[ 256 ];

	// create, connect and cache bonePos channel
	Q_snprintf( name, sizeof( name ), "%s_bonePos channel %d", pBaseName, bi );
	CDmeChannel *pPosChannel = CreateElement< CDmeChannel >( name, pChannelsClip->GetFileId() );
	pPosChannel->SetMode( CM_PLAY );
	pPosChannel->CreateLog( AT_VECTOR3 );
	pPosChannel->SetOutput( pTransform, "position" );
	pPosChannel->GetLog()->SetValueThreshold( 0.0f );
	pChannelsClip->m_Channels.AddToTail( pPosChannel );

	// create, connect and cache boneRot channel
	Q_snprintf( name, sizeof( name ), "%s_boneRot channel %d", pBaseName, bi );
	CDmeChannel *pRotChannel = CreateElement< CDmeChannel >( name, pChannelsClip->GetFileId() );
	pRotChannel->SetMode( CM_PLAY );
	pRotChannel->CreateLog( AT_QUATERNION );
	pRotChannel->SetOutput( pTransform, "orientation" );
	pRotChannel->GetLog()->SetValueThreshold( 0.0f );
	pChannelsClip->m_Channels.AddToTail( pRotChannel );
}

static void CreateAnimationLogs( CDmeChannelsClip *channelsClip, CDmeGameModel *pModel, studiohdr_t *pStudioHdr, const char *basename, int sequence, float flStartTime, float flDuration, float flTimeStep = 0.015f )
{
	Assert( pModel );
	Assert( pStudioHdr );

	CStudioHdr hdr( pStudioHdr, g_pMDLCache );

	if ( sequence >= hdr.GetNumSeq() )
	{
		sequence = 0;
	}

	int numbones = hdr.numbones();

	// make room for bones
	CUtlVector< CDmeDag* > dags;
	CUtlVector< CDmeChannel * > poschannels;
	CUtlVector< CDmeChannel * > rotchannels;

	dags.EnsureCapacity( numbones );
	poschannels.EnsureCapacity( numbones );
	rotchannels.EnsureCapacity( numbones );

	Vector pos[ MAXSTUDIOBONES ];
	Quaternion q[ MAXSTUDIOBONES ];

	float poseparameter[ MAXSTUDIOPOSEPARAM ];
	for ( int pp = 0; pp < MAXSTUDIOPOSEPARAM; ++pp )
	{
		poseparameter[ pp ] = 0.0f;
	}

	float flSequenceDuration = Studio_Duration( &hdr, sequence, poseparameter );
	mstudioseqdesc_t &seqdesc = hdr.pSeqdesc( sequence );

	bool created = false;

	for ( float t = flStartTime; t <= flStartTime + flDuration; t += flTimeStep )
	{
		int bi;

		if ( t > flStartTime + flDuration )
			t = flStartTime + flDuration;

		float flCycle = t / flSequenceDuration;

		if ( seqdesc.flags & STUDIO_LOOPING )
		{
			flCycle = flCycle - (int)flCycle;
			if (flCycle < 0) flCycle += 1;
		}
		else
		{
			flCycle = max( 0.f, min( flCycle, 0.9999f ) );
		}

		if ( !created )
		{
			created = true;

			// create, connect and cache each bone's pos and rot channels
			for ( bi = 0; bi < numbones; ++bi )
			{
				int nCount = channelsClip->m_Channels.Count();

				CDmeTransform *pTransform = pModel->GetBone( bi );
				CreateTransformChannels( pTransform, basename, bi, channelsClip );

				CDmeChannel *pPosChannel = channelsClip->m_Channels[ nCount ];
				CDmeChannel *pRotChannel = channelsClip->m_Channels[ nCount+1 ];
				poschannels.AddToTail( pPosChannel );
				rotchannels.AddToTail( pRotChannel );
			}
		}

		// Set up skeleton
		IBoneSetup boneSetup( &hdr, BONE_USED_BY_ANYTHING, poseparameter );
		boneSetup.InitPose( pos, q );
		boneSetup.AccumulatePose( pos, q, sequence, flCycle, 1.0f, t, NULL );

		// Copy bones into recording logs
		for ( bi = 0 ; bi < numbones; ++bi )
		{
			((CDmeVector3Log *)poschannels[ bi ]->GetLog())->InsertKey( DmeTime_t( t ), pos[ bi ] );
			((CDmeQuaternionLog *)rotchannels[ bi ]->GetLog())->InsertKey( DmeTime_t( t ), q[ bi ] );
		}
	}
}



static CDmeChannelsClip *FindChannelsClipTargetingDmeGameModel( CDmeFilmClip *pClip, CDmeGameModel *pGameModel )
{
	uint nBoneCount = pGameModel->NumBones();
	CDmeTransform *pGameModelTransform = pGameModel->GetTransform();

	int gc = pClip->GetTrackGroupCount();
	for ( int i = 0; i < gc; ++i )
	{
		CDmeTrackGroup *pTrackGroup = pClip->GetTrackGroup( i );
		DMETRACKGROUP_FOREACH_CLIP_TYPE_START( CDmeChannelsClip, pTrackGroup, pTrack, pChannelsClip )

		if ( FindChannelTargetingElement( pChannelsClip, pGameModel ) )
			return pChannelsClip;

		if ( FindChannelTargetingElement( pChannelsClip, pGameModelTransform ) )
			return pChannelsClip;

		for ( uint j = 0; j < nBoneCount; ++j )
		{
			if ( FindChannelTargetingElement( pChannelsClip, pGameModel->GetBone( j ) ) )
				return pChannelsClip;
		}

		DMETRACKGROUP_FOREACH_CLIP_TYPE_END()
	}

	return NULL;
}


static void RetimeLogData( CDmeChannelsClip *pSrcChannelsClip, CDmeChannelsClip *pDstChannelsClip, CDmeLog *pLog )
{
	float srcScale = pSrcChannelsClip->GetTimeScale();
	float dstScale = pDstChannelsClip->GetTimeScale();
	DmeTime_t srcStart = pSrcChannelsClip->GetStartTime();
	DmeTime_t dstStart = pDstChannelsClip->GetStartTime();
	DmeTime_t srcOffset = pSrcChannelsClip->GetTimeOffset();
	DmeTime_t dstOffset = pDstChannelsClip->GetTimeOffset();
	srcOffset -= srcStart;
	dstOffset -= dstStart;
	if ( srcScale != dstScale || srcOffset != dstOffset )
	{
		// for speed, I pulled out the math converting out of one timeframe into another:
		// t = (t/f0-o0+s0 -s1+o1)*f1
		//	 = t * f1/f0 + f1 * (o1-o0-s1+s0)
		float scale = dstScale / srcScale;
		DmeTime_t offset = dstScale * ( dstOffset - srcOffset );
		int nKeys = pLog->GetKeyCount();
		for ( int i = 0; i < nKeys; ++i )
		{
			DmeTime_t keyTime = pLog->GetKeyTime( i );
			keyTime = keyTime * scale + offset;
			pLog->SetKeyTime( i, keyTime );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Once bones have been setup and flex channels moved, the only things left should be:
//  a channel logging the model's "visibility" state
//  a channel logging the model's "sequence"
//  a channel loggint the model's "viewtarget" position
//-----------------------------------------------------------------------------
static void TransferRemainingChannels( CDmeFilmClip *shot, CDmeChannelsClip *destClip, CDmeChannelsClip *srcClip )
{
	if ( srcClip == destClip )
		return;

	int channelsCount = srcClip->m_Channels.Count();
	for ( int i = 0; i < channelsCount; ++i )
	{
		// Remove channel from channels clip
		CDmeChannel *channel = srcClip->m_Channels[ i ];
		Assert( channel );
		if ( !channel )
			continue;

		Msg( "Transferring '%s'\n", channel->GetName() );

		destClip->m_Channels.AddToTail( channel );
		channel->SetMode( CM_PLAY );

		// Transfer the logs over to the
		CDmeLog *log = channel->GetLog();
		if ( log )
		{
			RetimeLogData( srcClip, destClip, log );
		}
	}

	srcClip->m_Channels.RemoveAll();

	// Now find the track which contains the srcClip and remove the srcClip from the track
	for ( DmAttributeReferenceIterator_t it = g_pDataModel->FirstAttributeReferencingElement( srcClip->GetHandle() );
		it != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID;
		it = g_pDataModel->NextAttributeReferencingElement( it ) )
	{
		CDmAttribute *attr = g_pDataModel->GetAttribute( it );
		Assert( attr );
		CDmElement *element = attr->GetOwner();
		Assert( element );
		if ( !element )
			continue;

		CDmeTrack *t = CastElement< CDmeTrack >( element );
		if ( !t )
			continue;

		t->RemoveClip( srcClip );
		g_pDataModel->DestroyElement( srcClip->GetHandle() );
		break;
	}
}

static void SetupBoneTransform( CDmeFilmClip *shot, CDmeChannelsClip *srcChannelsClip, CDmeChannelsClip *channelsClip, 
	CDmElement *control, CDmeGameModel *gameModel, const char *basename, studiohdr_t *hdr, int bonenum, const char *boneName, bool bAttachToGameRecording )
{
	const char *channelNames[] = { "position", "orientation" };
	const char *valueNames[] = { "valuePosition", "valueOrientation" };
	const char *suffix[] = { "Pos", "Rot" };

	DmAttributeType_t channelTypes[] = { AT_VECTOR3, AT_QUATERNION };
	int i;

	CDmeTransform *pBoneTxForm = gameModel->GetBone( bonenum );

	for ( i = 0; i < 2 ; ++i )
	{
		char szName[ 512 ];
		Q_snprintf( szName, sizeof( szName ), "%s_bone%s %d", basename, suffix[ i ], bonenum );

		CDmeChannel *pAttachChannel = NULL;
		if ( srcChannelsClip )
		{
			pAttachChannel = FindChannelTargetingElement( srcChannelsClip, pBoneTxForm, channelNames[ i ] );
		}

		if ( !pAttachChannel )
		{
			// Create one
			pAttachChannel = CreateElement< CDmeChannel >( szName, channelsClip->GetFileId() );
			Assert( pAttachChannel );
			pAttachChannel->SetOutput( pBoneTxForm, channelNames[ i ], 0 );
		}

		if ( !pAttachChannel )
			continue;

		if ( bAttachToGameRecording && srcChannelsClip )
		{
			// Remove channel from channels clip
			int idx = srcChannelsClip->m_Channels.Find( pAttachChannel->GetHandle() );
			if ( idx != srcChannelsClip->m_Channels.InvalidIndex() )
			{
				srcChannelsClip->m_Channels.Remove( idx );
			}
			channelsClip->m_Channels.AddToTail( pAttachChannel );
		}

		control->SetValue( channelNames[ i ], pAttachChannel );
		control->AddAttribute( valueNames[ i ], channelTypes[ i ] );

		CDmeLog *pOriginalLog = pAttachChannel->GetLog();

		pAttachChannel->SetMode( CM_PLAY );
		pAttachChannel->SetInput( control, valueNames[ i ] );

		// Transfer the logs over to the
		if ( bAttachToGameRecording && pOriginalLog && srcChannelsClip )
		{
			CDmeLog *pNewLog = pAttachChannel->GetLog();
			if ( pNewLog != pOriginalLog )
			{
				pAttachChannel->SetLog( pOriginalLog );
				g_pDataModel->DestroyElement( pNewLog->GetHandle() );
			}

			DmeTime_t tLogToGlobal[ 2 ];

			Assert(0);
			// NOTE: Fix the next 2 lines to look like createsfmanimation.cpp
			DmeTime_t curtime = DMETIME_ZERO; //doc->GetTime();
			DmeTime_t cmt = DMETIME_ZERO; //doc->ToCurrentMediaTime( curtime, false );
			DmeTime_t channelscliptime = shot->ToChildMediaTime( cmt, false );

			DmeTime_t logtime = channelsClip->ToChildMediaTime( channelscliptime, false );

			tLogToGlobal[ 0 ] = curtime - logtime;

			DmeTime_t attachlogtime = srcChannelsClip->ToChildMediaTime( channelscliptime, false );

			tLogToGlobal[ 1 ] = curtime - attachlogtime;

			DmeTime_t offset = tLogToGlobal[ 1 ] - tLogToGlobal[ 0 ];

			if ( DMETIME_ZERO != offset )
			{
				int c = pOriginalLog->GetKeyCount();
				for ( int iLog = 0; iLog < c; ++iLog )
				{
					DmeTime_t keyTime = pOriginalLog->GetKeyTime( iLog );
					keyTime += offset;
					pOriginalLog->SetKeyTime( iLog, keyTime );
				}
			}
			continue;
		}

		if ( pOriginalLog )
		{
			pOriginalLog->ClearKeys();
		}

		CDmeLog *log = pAttachChannel->GetLog();
		if ( !log )
		{
			log = pAttachChannel->CreateLog( channelTypes[ i ] );
		}

		log->SetValueThreshold( 0.0f );
		if ( bAttachToGameRecording )
		{
			Vector pos;
			Quaternion rot;

			matrix3x4_t matrix;
			pBoneTxForm->GetTransform( matrix );
			MatrixAngles( matrix, rot, pos );

			if ( i == 0 )
			{
				((CDmeTypedLog< Vector > *)log)->SetKey( DMETIME_ZERO, pos );
			}
			else
			{
				((CDmeTypedLog< Quaternion > *)log)->SetKey( DMETIME_ZERO, rot );
			}
			continue;
		}

		CStudioHdr studiohdr( hdr, g_pMDLCache );

		Vector pos[ MAXSTUDIOBONES ];
		Quaternion q[ MAXSTUDIOBONES ];
		float poseparameter[ MAXSTUDIOPOSEPARAM ];
		for ( int pp = 0; pp < MAXSTUDIOPOSEPARAM; ++pp )
		{
			poseparameter[ pp ] = 0.0f;
		}

		// Set up skeleton
		IBoneSetup boneSetup( &studiohdr, BONE_USED_BY_ANYTHING, poseparameter );
		boneSetup.InitPose( pos, q );
		boneSetup.AccumulatePose( pos, q, 0, 0.0f, 1.0f, 0.0f, NULL );

		if ( i == 0 )
		{
			((CDmeTypedLog< Vector > *)log)->SetKey( DMETIME_ZERO, pos[ bonenum ] );
			pBoneTxForm->SetPosition(  pos[ bonenum ]);
		}
		else
		{
			((CDmeTypedLog< Quaternion > *)log)->SetKey( DMETIME_ZERO, q[ bonenum ] );
			pBoneTxForm->SetOrientation( q[ bonenum ] );
		}
	}
}


//-----------------------------------------------------------------------------
// Sets up the root transform
//-----------------------------------------------------------------------------
static void SetupRootTransform( CDmeFilmClip *shot, CDmeChannelsClip *srcChannelsClip, 
	CDmeChannelsClip *channelsClip, CDmElement *control, CDmeGameModel *gameModel, const char *basename, bool bAttachToGameRecording )
{
	char *channelNames[] = { "position", "orientation" };
	char *valueNames[] = { "valuePosition", "valueOrientation" };
	DmAttributeType_t channelTypes[] = { AT_VECTOR3, AT_QUATERNION };
	const char *suffix[]			= { "Pos", "Rot" };
	DmAttributeType_t logType[]		= { AT_VECTOR3, AT_QUATERNION };

	int i;
	for ( i = 0; i < 2 ; ++i )
	{
		char szName[ 512 ];
		Q_snprintf( szName, sizeof( szName ), "%s_root%s channel", basename, suffix[ i ] );

		CDmeChannel *pAttachChannel = NULL;
		if ( srcChannelsClip )
		{
			pAttachChannel = FindChannelTargetingElement( srcChannelsClip, gameModel->GetTransform(), channelNames[ i ] );
		}

		if ( !pAttachChannel )
		{
			// Create one
			pAttachChannel = CreateElement< CDmeChannel >( szName, channelsClip->GetFileId() );
			Assert( pAttachChannel );
			pAttachChannel->SetOutput( gameModel->GetTransform(), channelNames[ i ], 0 );
		}

		if ( bAttachToGameRecording && srcChannelsClip )
		{
			// Remove channel from channels clip
			int idx = srcChannelsClip->m_Channels.Find( pAttachChannel->GetHandle() );
			if ( idx != srcChannelsClip->m_Channels.InvalidIndex() )
			{
				srcChannelsClip->m_Channels.Remove( idx );
			}
			channelsClip->m_Channels.AddToTail( pAttachChannel );
		}

		control->SetValue( channelNames[ i ], pAttachChannel );
		control->AddAttribute( valueNames[ i ], channelTypes[ i ] );

		CDmeLog *pOriginalLog = pAttachChannel->GetLog();

		pAttachChannel->SetMode( CM_PLAY );
		pAttachChannel->SetInput( control, valueNames[ i ] );

		if ( bAttachToGameRecording && pOriginalLog && srcChannelsClip )
		{
			CDmeLog *pNewLog = pAttachChannel->GetLog();
			if ( pNewLog != pOriginalLog )
			{
				pAttachChannel->SetLog( pOriginalLog );
				g_pDataModel->DestroyElement( pNewLog->GetHandle() );
			}

			RetimeLogData( srcChannelsClip, channelsClip, pOriginalLog );
		}
		else
		{
			Assert( !pOriginalLog );
			CDmeLog *log = pAttachChannel->GetLog();
			if ( !log )
			{
				log = pAttachChannel->CreateLog( logType[ i ] );
			}

			log->SetValueThreshold( 0.0f );

			Vector vecPos;
			Quaternion qOrientation;

			matrix3x4_t txform;
			gameModel->GetTransform()->GetTransform( txform );

			MatrixAngles( txform, qOrientation, vecPos );

			if ( i == 0 )
			{
				((CDmeTypedLog< Vector > *)log)->SetKey( DMETIME_ZERO, vecPos );
			}
			else
			{
				((CDmeTypedLog< Quaternion > *)log)->SetKey( DMETIME_ZERO, qOrientation );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Creates preset groups for new animation sets
//-----------------------------------------------------------------------------
static bool ShouldRandomize( const char *name )
{
	if ( !Q_stricmp( name, "eyes_updown" ) )
		return false;
	if ( !Q_stricmp( name, "eyes_rightleft" ) )
		return false;
	if ( !Q_stricmp( name, "lip_bite" ) )
		return false;
	if ( !Q_stricmp( name, "blink" ) )
		return false;
	if ( Q_stristr( name, "sneer" ) )
		return false;
	return true;
}

static void CreateProceduralPreset( CDmePresetGroup *pPresetGroup, const char *pPresetName, const CDmaElementArray< CDmElement > &controls, bool bIdentity, float flForceValue = 0.5f )
{
	CDmePreset *pPreset = pPresetGroup->FindOrAddPreset( pPresetName );

	int c = controls.Count();
	for ( int i = 0; i < c ; ++i )
	{
		CDmElement *pControl = controls[ i ];

		// Setting values on transforms doesn't make sense right now
		if ( pControl->GetValue<bool>( "transform" ) )
			continue;

		bool bIsCombo = pControl->GetValue< bool >( "combo" );
		bool bIsMulti = pControl->GetValue< bool >( "multi" );
		bool bRandomize = ShouldRandomize( pControl->GetName() );
		if ( !bIdentity && !bRandomize )
			continue;

		CDmElement *pControlValue = pPreset->FindOrAddControlValue( pControl->GetName() ); 

		if ( !bIdentity )
		{
			pControlValue->SetValue< float >( "value", RandomFloat( 0.0f, 1.0f ) );
			if ( bIsCombo )
			{
				pControlValue->SetValue< float >( "balance", RandomFloat( 0.25f, 0.75f ) );
			}
			if ( bIsMulti )
			{
				pControlValue->SetValue< float >( "multilevel", RandomFloat( 0.0f, 1.0f ) );
			}
		}
		else
		{
			pControlValue->SetValue< float >( "value", flForceValue );
			if ( bIsCombo )
			{
				pControlValue->SetValue< float >( "balance", 0.5f );
			}
			if ( bIsMulti )
			{
				pControlValue->SetValue< float >( "multilevel", flForceValue );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Creates preset groups for new animation sets
//-----------------------------------------------------------------------------
static void CreatePresetGroups( CDmeAnimationSet *pAnimationSet, const char *pModelName )
{
	CDmaElementArray< CDmElement > &controls = pAnimationSet->GetControls();

	// Now create some presets
	CDmePresetGroup *pProceduralPresets = pAnimationSet->FindOrAddPresetGroup( "procedural" );
	pProceduralPresets->m_bIsReadOnly = true;
	pProceduralPresets->FindOrAddPreset( "Default" );
	CreateProceduralPreset( pProceduralPresets, "Zero", controls, true, 0.0f );
	CreateProceduralPreset( pProceduralPresets, "Half", controls, true, 0.5f );
	CreateProceduralPreset( pProceduralPresets, "One", controls, true, 1.0f );

	// Add just one fake one for now
	CreateProceduralPreset( pProceduralPresets, "Random", controls, false );

	// These are the truly procedural ones...
	pAnimationSet->EnsureProceduralPresets();

	// Also load the model-specific presets
	g_pModelPresetGroupMgr->ApplyModelPresets( pModelName, pAnimationSet );
}


//-----------------------------------------------------------------------------
// Destroys existing group mappings
//-----------------------------------------------------------------------------
static void RemoveExistingGroupMappings( CDmeAnimationSet *pAnimationSet )
{
	CDmaElementArray<> &groups = pAnimationSet->GetSelectionGroups();
	int nCount = groups.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmElement *pGroup = groups[i];
		groups.Set( i, NULL );
		DestroyElement( pGroup );
	}
	groups.RemoveAll();
}


void LoadDefaultGroupMappings( CUtlDict< CUtlString, int > &defaultGroupMapping, CUtlVector< CUtlString >& defaultGroupOrdering )
{
	defaultGroupMapping.RemoveAll();
	defaultGroupOrdering.RemoveAll();

	KeyValues *pGroupFile = new KeyValues( "groupFile" );
	if ( !pGroupFile )
		return;

	if ( !pGroupFile->LoadFromFile( g_pFullFileSystem, ANIMATION_SET_DEFAULT_GROUP_MAPPING_FILE, "GAME" ) )
	{
		pGroupFile->deleteThis();
		return;
	}

	// Fill in defaults
	for ( KeyValues *sub = pGroupFile->GetFirstSubKey(); sub; sub = sub->GetNextKey() )
	{
		const char *pGroupName = sub->GetName();
		if ( !pGroupName )
		{
			Warning( "%s is malformed\n", ANIMATION_SET_DEFAULT_GROUP_MAPPING_FILE );
			continue;
		}

		int i = defaultGroupOrdering.AddToTail();
		defaultGroupOrdering[i] = pGroupName;

		for ( KeyValues *pControl = sub->GetFirstSubKey(); pControl; pControl = pControl->GetNextKey() )
		{
			Assert( !Q_stricmp( pControl->GetName(), "control" ) );
			CUtlString controlName = pControl->GetString();
			defaultGroupMapping.Insert( controlName, pGroupName );
		}
	}

	pGroupFile->deleteThis();
}

CDmElement *FindOrAddDefaultGroupForControls( const char *pGroupName, CDmaElementArray< CDmElement > &groups, DmFileId_t fileid )
{
	// Now see if this group exists in the array
	int c = groups.Count();
	for ( int i = 0; i < c; ++i )
	{
		CDmElement *pGroup = groups[ i ];
		if ( !Q_stricmp( pGroup->GetName(), pGroupName ) )
			return pGroup;
	}

	CDmElement *pGroup = CreateElement< CDmElement >( pGroupName, fileid );
	pGroup->AddAttribute( "selectedControls", AT_STRING_ARRAY );
	groups.AddToTail( pGroup );
	return pGroup;
}

//-----------------------------------------------------------------------------
// Build group mappings
//-----------------------------------------------------------------------------
static void BuildGroupMappings( CDmeAnimationSet *pAnimationSet )
{
	RemoveExistingGroupMappings( pAnimationSet );

	// Maps flex controls to first level "groups" by flex controller name
	CUtlDict< CUtlString, int > defaultGroupMapping;
	CUtlVector< CUtlString > defaultGroupOrdering;

	LoadDefaultGroupMappings( defaultGroupMapping, defaultGroupOrdering );

	// Create the default groups in order
	CDmaElementArray<> &groups = pAnimationSet->GetSelectionGroups();
	int nCount = defaultGroupOrdering.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		const char *pGroupName = (const char *)defaultGroupOrdering[ i ];
		if ( !Q_stricmp( pGroupName, "IGNORE" ) )
			continue;

		CDmElement *pGroup = CreateElement< CDmElement >( pGroupName, pAnimationSet->GetFileId() );

		// Fill in members
		pGroup->AddAttribute( "selectedControls", AT_STRING_ARRAY );
		groups.AddToTail( pGroup );
	}

	// Populate the groups with the controls
	CDmaElementArray<> &controls = pAnimationSet->GetControls();
	nCount = controls.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		const char *pGroupName = "Unknown";
		const char *pControlName = controls[ i ]->GetName();

		// Find the default if there is one
		int idx = defaultGroupMapping.Find( pControlName );
		if ( idx != defaultGroupMapping.InvalidIndex() )
		{
			pGroupName = defaultGroupMapping[ idx ];
		}
		else if ( Q_stristr( pControlName, "root" ) || Q_stristr( pControlName, "Valve" ) )
		{
			pGroupName = "Root";
		}

		if ( !Q_stricmp( pGroupName, "IGNORE" ) )
			continue;

		CDmElement *pGroup = FindOrAddDefaultGroupForControls( pGroupName, groups, pAnimationSet->GetFileId() );

		// Fill in members
		CDmrStringArray selectedControls( pGroup, "selectedControls" );
		Assert( selectedControls.IsValid() );
		if ( selectedControls.IsValid() )
		{
			selectedControls.AddToTail( pControlName );
		}
	}
}

void AddIllumPositionAttribute( CDmeGameModel *pGameModel )
{
	studiohdr_t *pHdr = pGameModel->GetStudioHdr();
	if ( !pHdr )
		return;

	if ( pHdr->IllumPositionAttachmentIndex() > 0 )
		return; // don't add attr if model already has illumposition attachment

	CDmAttribute *pAttr = pGameModel->AddAttributeElement< CDmeDag >( "illumPositionDag" );
	Assert( pAttr );
	if ( !pAttr )
		return;

	Assert( pGameModel->GetChildCount() > 0 );
	pAttr->SetValue( pGameModel->GetChild( 0 ) );
}

//-----------------------------------------------------------------------------
// Creates an animation set
//-----------------------------------------------------------------------------
CDmeAnimationSet *CreateAnimationSet( CDmeFilmClip *pMovie, CDmeFilmClip *pShot, 
	CDmeGameModel *pGameModel, const char *pAnimationSetName, int nSequenceToUse, bool bAttachToGameRecording )
{
	CDmeAnimationSet *pAnimationSet = CreateElement< CDmeAnimationSet >( pAnimationSetName, pMovie->GetFileId() );
	Assert( pAnimationSet );

	studiohdr_t *hdr = pGameModel->GetStudioHdr();

	// Associate this animation set with a specific game model
	// FIXME: Should the game model refer back to this set?
	pAnimationSet->SetValue( "gameModel", pGameModel );

	CDmeChannelsClip* pChannelsClip = CreateChannelsClip( pAnimationSet, pShot );

	// Does everything associated with building facial controls on a model
	CFlexControlBuilder builder;
	builder.CreateAnimationSetControls( pMovie, pAnimationSet, pGameModel, pShot, pChannelsClip, bAttachToGameRecording );

	// Create animation data if there wasn't any already in the model
	if ( !bAttachToGameRecording )
	{
		CreateConstantValuedLog( pChannelsClip, pAnimationSetName, "skin", pGameModel, "skin", (int)0 );
		CreateConstantValuedLog( pChannelsClip, pAnimationSetName, "body", pGameModel, "body", (int)0 );
		CreateConstantValuedLog( pChannelsClip, pAnimationSetName, "sequence", pGameModel, "sequence", (int)0 );

		CreateAnimationLogs( pChannelsClip, pGameModel, hdr, pAnimationSetName, nSequenceToUse, 0.0f, 1.0f, 0.05f );
	}

	CDmeChannelsClip *srcChannelsClip = FindChannelsClipTargetingDmeGameModel( pShot, pGameModel );
	CDmaElementArray<> &controls = pAnimationSet->GetControls();

	// First the root transform
	{
		const char *ctrlName = "rootTransform";

		// Add the control to the controls group
		CDmElement *ctrl = CreateElement< CDmElement >( ctrlName, pMovie->GetFileId() );
		Assert( ctrl );
		ctrl->SetValue< bool >( "transform", true );
		controls.AddToTail( ctrl );
		SetupRootTransform( pShot, srcChannelsClip, pChannelsClip, ctrl, pGameModel, pAnimationSetName, bAttachToGameRecording );
	}

	// Now add the bone transforms as well
	{
		int numbones = hdr->numbones;
		for ( int b = 0; b < numbones; ++b )
		{
			mstudiobone_t *bone = hdr->pBone( b );
			const char *name = bone->pszName();

			// Add the control to the controls group
			CDmElement *ctrl = CreateElement< CDmElement >( name, pMovie->GetFileId() );
			Assert( ctrl );
			ctrl->SetValue< bool >( "transform", true );
			controls.AddToTail( ctrl );
			SetupBoneTransform( pShot, srcChannelsClip, pChannelsClip, ctrl, pGameModel, pAnimationSetName, hdr, b, name, bAttachToGameRecording );
		}
	}

	// Now copy all remaining logs, and retime them, over to the animation set channels clip...
	if ( srcChannelsClip )
	{
		TransferRemainingChannels( pShot, pChannelsClip, srcChannelsClip );
	}

	// Create default preset groups for the animation set
	CreatePresetGroups( pAnimationSet, pGameModel->GetModelName() );
	
	// Builds the preset groups displayed in the upper left of the animation set panel
	BuildGroupMappings( pAnimationSet );

	pShot->AddAnimationSet( pAnimationSet );

	AddIllumPositionAttribute( pGameModel );

	return pAnimationSet;
}
