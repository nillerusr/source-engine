//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "movieobjects/dmx_to_vcd.h"
#include "movieobjects/movieobjects.h"
#include "choreoscene.h"
#include "choreoactor.h"
#include "choreochannel.h"
#include "choreoevent.h"
#include "iscenetokenprocessor.h"
#include "characterset.h"


bool ConvertEventToDmx( CChoreoEvent *event, CDmeChannelsClip *clip )
{
	clip->SetName( event->GetName() );
	clip->SetValue( "eventtype", CChoreoEvent::NameForType( event->GetType() ) );

	CDmeTimeFrame *tf = clip->GetTimeFrame();
	Assert( tf );
	if ( tf )
	{
		tf->SetStartTime( DmeTime_t( event->GetStartTime() ) );
		tf->SetDuration( DmeTime_t( event->GetDuration() ) );
	}

	clip->SetValue( "parameters", event->GetParameters() );
	clip->SetValue( "parameters2", event->GetParameters2() );

	// event_ramp is a channel under the event's channels clip
	CDmrElementArray<> array( clip, "channels" ); 
	Assert( array.IsValid() );
	if ( array.IsValid() )
	{
		CDmeChannel *channel = CreateElement< CDmeChannel >( "event_ramp", clip->GetFileId() );
		array.AddToTail( channel );

		// Fill in values..., just log for now
		channel->CreateLog( AT_FLOAT );
		CDmeTypedLog<float> *ramp = static_cast< CDmeTypedLog<float> * >( channel->GetLog() );
		if ( ramp )
		{
			CDmeFloatCurveInfo *pCurveInfo = CreateElement< CDmeFloatCurveInfo >( "floatcurveinfo", clip->GetFileId() );

			pCurveInfo->SetDefaultCurveType( MAKE_CURVE_TYPE( INTERPOLATE_CATMULL_ROM_NORMALIZE, INTERPOLATE_CATMULL_ROM_NORMALIZE ) );
			pCurveInfo->SetRightEdgeTime( DmeTime_t( event->GetDuration() ) );

			pCurveInfo->SetUseEdgeInfo( true );
			pCurveInfo->SetDefaultEdgeZeroValue( 0.0f );
			// Left edge
			pCurveInfo->SetEdgeInfo( 0, 
				event->GetRamp()->IsEdgeActive( true ), 
				event->GetRamp()->GetEdgeZeroValue( true ), 
				event->GetRamp()->GetEdgeCurveType( true ) );
			// Right edge
			pCurveInfo->SetEdgeInfo( 1, 
				event->GetRamp()->IsEdgeActive( false ), 
				event->GetRamp()->GetEdgeZeroValue( false ), 
				event->GetRamp()->GetEdgeCurveType( false ) );

			ramp->SetCurveInfo( pCurveInfo );

			int rampCount = event->GetRampCount();
			for ( int j = 0; j < rampCount; ++j )
			{
				CExpressionSample *sample = event->GetRamp( j );
				ramp->SetKey( DmeTime_t( sample->time ), sample->value, sample->GetCurveType() );
			}
		}
	}

	clip->SetValue< float >( "pitch", event->GetPitch() );
	clip->SetValue< float >( "yaw", event->GetYaw() );

	clip->SetValue< bool >( "resumecondition", event->IsResumeCondition() );
	clip->SetValue< bool >( "lockbodyfacing", event->IsLockBodyFacing() );
	clip->SetValue< float >( "distancetotarget", event->GetDistanceToTarget() );
	clip->SetValue< bool >( "fixedlength", event->IsFixedLength() );
	clip->SetValue< bool >( "forceshortmovement", event->GetForceShortMovement() );
	clip->SetValue< bool >( "active", event->GetActive() );

	// Relative tags
	if ( event->GetNumRelativeTags() > 0 )
	{
		CDmElement *tags = CreateElement< CDmElement >( "relative_tags", clip->GetFileId() );
		if ( tags )
		{
			clip->SetValue< CDmElement >( "tags", tags );

			// Now create arrays for the name and percentages
			CDmrStringArray names( tags, "tagname", true );
			CDmrArray<float> percentages( tags, "tagpercentage", true );
			Assert( names.IsValid() && percentages.IsValid() );

			for ( int t = 0; t < event->GetNumRelativeTags(); t++ )
			{
				CEventRelativeTag *rt = event->GetRelativeTag( t );
				Assert( rt );

				names.AddToTail( rt->GetName() );
				percentages.AddToTail( rt->GetPercentage() );
			}
		}
	}

	// Timing tags
	if ( event->GetNumTimingTags() > 0 )
	{
		CDmElement *tags = CreateElement< CDmElement >( "timing_tags", clip->GetFileId() );
		if ( tags )
		{
			clip->SetValue< CDmElement >( "flextimingtags", tags );

			// Now create arrays for the name and percentages
			CDmrStringArray names( tags, "tagname", true );
			CDmrArray<float> percentages( tags, "tagpercentage", true );
			CDmrArray<bool> lockstates( tags, "lockedstate", true );
			Assert( names.IsValid() && percentages.IsValid() && lockstates.IsValid() );

			for ( int t = 0; t < event->GetNumTimingTags(); t++ )
			{
				CFlexTimingTag *tt = event->GetTimingTag( t );
				Assert( tt );

				names.AddToTail( tt->GetName() );
				percentages.AddToTail( tt->GetPercentage() );
				lockstates.AddToTail( tt->GetLocked() );
			}
		}
	}

	// Abs tags
	int tagtype;
	for ( tagtype = 0; tagtype < CChoreoEvent::NUM_ABS_TAG_TYPES; tagtype++ )
	{
		if ( event->GetNumAbsoluteTags( (CChoreoEvent::AbsTagType)tagtype ) > 0 )
		{
			char sz[ 512 ];
			
			Q_snprintf( sz, sizeof( sz ), "absolutetags %s", CChoreoEvent::NameForAbsoluteTagType( (CChoreoEvent::AbsTagType)tagtype ) );

			CDmElement *tags = CreateElement< CDmElement >( sz, clip->GetFileId() );
			if ( tags )
			{
				clip->SetValue< CDmElement >( sz, tags );

				// Now create arrays for the name and percentages
				CDmrStringArray names( tags, "tagname", true );
				CDmrArray<float> percentages( tags, "tagpercentage", true );
				Assert( names.IsValid() && percentages.IsValid() );
				for ( int t = 0; t < event->GetNumAbsoluteTags( (CChoreoEvent::AbsTagType)tagtype ); t++ )
				{
					CEventAbsoluteTag *abstag = event->GetAbsoluteTag( (CChoreoEvent::AbsTagType)tagtype, t );
					Assert( abstag );

					names.AddToTail( abstag->GetName() );
					percentages.AddToTail( abstag->GetPercentage() );
				}
			}
		}
	}

	// IsUsingRelativeTag
	if ( event->IsUsingRelativeTag() )
	{
		CDmElement *relativeTag = CreateElement< CDmElement >( "relative_tag", clip->GetFileId() );
		if ( relativeTag )
		{
			clip->SetValue< CDmElement >( "relative_tag", relativeTag );

			relativeTag->SetValue( "tagname", event->GetRelativeTagName() );
			relativeTag->SetValue( "tagwav",  event->GetRelativeWavName() );
		}
	}

	switch ( event->GetType() )
	{
	default:
		break;
	case CChoreoEvent::SPEAK:
		{
			CDmElement *speakProperties = CreateElement< CDmElement >( "SPEAK", clip->GetFileId() );
			if ( speakProperties )
			{
				clip->SetValue< CDmElement >( "SPEAK", speakProperties );

				speakProperties->SetValue( "closedcaption_type", CChoreoEvent::NameForCCType( event->GetCloseCaptionType() ) );
				speakProperties->SetValue( "closedcaption_token", event->GetCloseCaptionToken() );
				bool usingCombined = ( event->GetCloseCaptionType() != CChoreoEvent::CC_DISABLED && event->IsUsingCombinedFile() );
				speakProperties->SetValue< bool >( "closedcaption_usingcombinedfile", usingCombined );
				speakProperties->SetValue< bool >( "closedcaption_combinedusesgender", event->IsCombinedUsingGenderToken() );
				speakProperties->SetValue< bool >( "closedcaption_noattenuate", event->IsSuppressingCaptionAttenuation() );
			}
		}
		break;
	case CChoreoEvent::GESTURE:
		{
			CDmElement *gestureProperties = CreateElement< CDmElement >( "GESTURE", clip->GetFileId() );
			if ( gestureProperties )
			{
				clip->SetValue< CDmElement >( "GESTURE", gestureProperties );

				float duration;
				event->GetGestureSequenceDuration( duration );
				gestureProperties->SetValue< float >( "sequenceduration", duration );
			}
		}
		break;
	case CChoreoEvent::FLEXANIMATION:
		{
			// Save flex animation tracks as channels
			CDmrElementArray<> array( clip, "channels" );
			Assert( array.IsValid() );
			if ( array.IsValid() )
			{
				// Now add a DmeChannel for each flex controller
				int numTracks = event->GetNumFlexAnimationTracks();
				for ( int i = 0 ; i < numTracks; ++i )
				{
					CFlexAnimationTrack *track = event->GetFlexAnimationTrack( i );

					CDmeChannel *channel = CreateElement< CDmeChannel >( track->GetFlexControllerName(), clip->GetFileId() );
					array.AddToTail( channel );

					channel->SetValue< bool >( "flexchannel", true );

					channel->SetValue< bool >( "disabled", !track->IsTrackActive() );
					channel->SetValue< bool >( "combo", track->IsComboType() );

					channel->SetValue< float >( "rangemin", track->GetMin() );
					channel->SetValue< float >( "rangemax", track->GetMax() );

					channel->SetValue< bool >( "isbalancechannel", false );

					// Fill in values..., just log for now
					channel->CreateLog( AT_FLOAT );
					CDmeTypedLog<float> *log = static_cast< CDmeTypedLog<float> * >( channel->GetLog() );
					if ( log )
					{
						CDmeFloatCurveInfo *pCurveInfo = CreateElement< CDmeFloatCurveInfo >( "floatcurveinfo", clip->GetFileId() );

						pCurveInfo->SetDefaultCurveType( MAKE_CURVE_TYPE( INTERPOLATE_CATMULL_ROM_NORMALIZEX, INTERPOLATE_CATMULL_ROM_NORMALIZEX ) );
						pCurveInfo->SetRightEdgeTime( DmeTime_t( event->GetDuration() ) );

						pCurveInfo->SetUseEdgeInfo( true );
						pCurveInfo->SetDefaultEdgeZeroValue( 0.0f );
						// Left edge
						pCurveInfo->SetEdgeInfo( 0, 
							track->IsEdgeActive( true ), 
							track->GetEdgeZeroValue( true ), 
							track->GetEdgeCurveType( true ) );
						// Right edge
						pCurveInfo->SetEdgeInfo( 1, 
							track->IsEdgeActive( false ), 
							track->GetEdgeZeroValue( false ), 
							track->GetEdgeCurveType( false ) );

						log->SetCurveInfo( pCurveInfo );

						// Now set up edge properties

						int sampleCount = track->GetNumSamples();
						for ( int j = 0; j < sampleCount; ++j )
						{
							CExpressionSample *sample = track->GetSample( j );                            
							log->SetKey( DmeTime_t( sample->time ), sample->value, sample->GetCurveType() );
						}
					}

					// Right out the stereo "balance" curve
					if ( track->IsComboType() )
					{
						char balanceChannelName[ 512 ];
						Q_snprintf( balanceChannelName, sizeof( balanceChannelName ), "%s_balance", track->GetFlexControllerName() );

						CDmeChannel *balanceChannel = CreateElement< CDmeChannel >( balanceChannelName, clip->GetFileId() );
						array.AddToTail( balanceChannel );

						channel->SetValue( "balanceChannel", balanceChannel );
						
						balanceChannel->SetValue< bool >( "flexchannel", true );

						balanceChannel->SetValue< bool >( "disabled", !track->IsTrackActive() );
						balanceChannel->SetValue< bool >( "isbalancechannel", true );

						balanceChannel->CreateLog( AT_FLOAT );
						CDmeTypedLog< float > *balance = static_cast< CDmeTypedLog< float > * >( balanceChannel->GetLog() );
						if ( balance )
						{
							CDmeFloatCurveInfo *pCurveInfo = CreateElement< CDmeFloatCurveInfo >( "floatcurveinfo", clip->GetFileId() );

							pCurveInfo->SetDefaultCurveType( MAKE_CURVE_TYPE( INTERPOLATE_CATMULL_ROM_NORMALIZEX, INTERPOLATE_CATMULL_ROM_NORMALIZEX ) );
							pCurveInfo->SetRightEdgeTime( DmeTime_t( event->GetDuration() ) );

							pCurveInfo->SetUseEdgeInfo( false );
							pCurveInfo->SetDefaultEdgeZeroValue( 0.5f );

							/*
							// Don't need to support edge properties for balance curves?
							// Left edge
							pCurveInfo->SetEdgeInfo( 0, 
								track->IsEdgeActive( true ), 
								track->GetEdgeZeroValue( true ), 
								track->GetEdgeCurveType( true ) );
							// Right edge
							pCurveInfo->SetEdgeInfo( 1, 
								track->IsEdgeActive( false ), 
								track->GetEdgeZeroValue( false ), 
								track->GetEdgeCurveType( false ) );
							*/

							balance->SetCurveInfo( pCurveInfo );

							// Now set up edge properties

							int sampleCount = track->GetNumSamples( 1 );
							for ( int j = 0; j < sampleCount; ++j )
							{
								CExpressionSample *sample = track->GetSample( j, 1 );
								balance->SetKey( DmeTime_t( sample->time ), sample->value, sample->GetCurveType() );
							}
						}
					}
				}
			}
		}
		break;
	case CChoreoEvent::LOOP:
		{
			CDmElement *loopProperties = CreateElement< CDmElement >( "LOOP", clip->GetFileId() );
			if ( loopProperties )
			{
				clip->SetValue< CDmElement >( "LOOP", loopProperties );

				loopProperties->SetValue< int >( "loopcount", event->GetLoopCount() );
			}
		}
		break;
	}

	return true;
}

bool ConvertSceneToDmx( CChoreoScene *scene, CDmeFilmClip *dmx )
{
	bool bret = true;

	dmx->SetName( scene->GetFilename() );

	CDmeTimeFrame *tf = dmx->GetTimeFrame();
	Assert( tf );
	if ( tf )
	{
		tf->SetDuration( DmeTime_t( scene->FindStopTime() ) );
	}

	CDmElement *scaleSettings = CreateElement< CDmElement >( "scalesettings", dmx->GetFileId() );
	Assert( scaleSettings );
	CDmAttribute *scaleAttribute = dmx->AddAttributeElement< CDmElement >( "scalesettings" );
	Assert( scaleAttribute );
	scaleAttribute->SetValue( scaleSettings->GetHandle() );
	if ( scaleSettings )
	{
		for ( int i = scene->TimeZoomFirst(); i != scene->TimeZoomInvalid(); i = scene->TimeZoomNext( i ) )
		{
			const char *name = scene->TimeZoomName( i );
			int value = scene->GetTimeZoom( name );
		
			scaleSettings->SetValue< int >( name, value );
		}
	}

	CDmeTrackGroup *pTrackGroup = dmx->FindOrAddTrackGroup( VCD_SCENE_RAMP_TRACK_GROUP_NAME );
	CDmeTrack *track = pTrackGroup->FindOrAddTrack( VCD_SCENE_RAMP_TRACK_GROUP_NAME, DMECLIP_CHANNEL );
	Assert( track );

	// Set a CDmeChannel for the scene_ramp
	CDmeChannelsClip *pClip = CreateElement< CDmeChannelsClip >( VCD_SCENE_RAMP_TRACK_GROUP_NAME, dmx->GetFileId() );
	Assert( pClip );
	track->AddClip( pClip );

	int rampCount = scene->GetSceneRampCount();
	if ( rampCount > 0 )
	{
		// scene_ramp is a channels
		CDmrElementArray<> array( pClip, "channels" );
		Assert( array.IsValid() );
		if ( array.IsValid() )
		{																			   
			CDmeChannel *channel = CreateElement< CDmeChannel >( VCD_SCENE_RAMP_TRACK_GROUP_NAME, dmx->GetFileId() );
			array.AddToTail( channel );
	
			// Fill in values..., just log for now
			channel->CreateLog( AT_FLOAT );
			CDmeTypedLog<float> *ramp = static_cast< CDmeTypedLog<float> * >( channel->GetLog() );
			if ( ramp )
			{
				CDmeFloatCurveInfo *pCurveInfo = CreateElement< CDmeFloatCurveInfo >( "floatcurveinfo", dmx->GetFileId() );

				pCurveInfo->SetDefaultCurveType( MAKE_CURVE_TYPE( INTERPOLATE_CATMULL_ROM_NORMALIZE, INTERPOLATE_CATMULL_ROM_NORMALIZE ) );
				pCurveInfo->SetRightEdgeTime( DmeTime_t( scene->FindStopTime() ) );

				pCurveInfo->SetUseEdgeInfo( true );
				pCurveInfo->SetDefaultEdgeZeroValue( 0.0f );
				// Left edge
				pCurveInfo->SetEdgeInfo( 0, 
					scene->GetSceneRamp()->IsEdgeActive( true ), 
					scene->GetSceneRamp()->GetEdgeZeroValue( true ), 
					scene->GetSceneRamp()->GetEdgeCurveType( true ) );
				// Right edge
				pCurveInfo->SetEdgeInfo( 1, 
					scene->GetSceneRamp()->IsEdgeActive( false ), 
					scene->GetSceneRamp()->GetEdgeZeroValue( false ), 
					scene->GetSceneRamp()->GetEdgeCurveType( false ) );

				ramp->SetCurveInfo( pCurveInfo );

				for ( int j = 0; j < rampCount; ++j )
				{
					CExpressionSample *sample = scene->GetSceneRamp( j );
					ramp->SetKey( DmeTime_t( sample->time ), sample->value, sample->GetCurveType() );
				}
			}
		}
	}

	// Walk the actors and channels
	int numActors = scene->GetNumActors();
	for ( int actor = 0; actor < numActors; ++actor )
	{
		CChoreoActor *pActor = scene->GetActor( actor );
		Assert( pActor );
		if ( !pActor )
			continue;

		CDmeTrackGroup *pTrackGroup = dmx->FindOrAddTrackGroup( pActor->GetName() );
		Assert( pTrackGroup );

		pTrackGroup->SetValue< bool >( "isActor", true );
		pTrackGroup->SetValue< bool >( "actorDisabled", !pActor->GetActive() );
		pTrackGroup->SetValue( "actorModel", pActor->GetFacePoserModelName() );

		int numChannels = pActor->GetNumChannels();
		for ( int channel = 0; channel < numChannels; ++channel )
		{
			CChoreoChannel *pChannel = pActor->GetChannel( channel );
			Assert( pChannel );
			if ( !pChannel )
				continue;

			const char *channelName = pChannel->GetName();
			CDmeTrack *track = pTrackGroup->FindOrAddTrack( channelName, DMECLIP_CHANNEL );
			Assert( track );
			if ( !track )
				continue;

			track->SetMute( !pChannel->GetActive() );

			int numEvents = pChannel->GetNumEvents();
			for ( int event = 0; event < numEvents; ++event )
			{
				CChoreoEvent *pEvent = pChannel->GetEvent( event );
				Assert( pEvent );
				if ( !pEvent )
					continue;

				// Set up event
				CDmeChannelsClip *pClip = CreateElement< CDmeChannelsClip >( "", dmx->GetFileId() );
				Assert( pClip );
				track->AddClip( pClip );

				// Fill in data
				bool success = ConvertEventToDmx( pEvent, pClip );
				if ( !success )
				{
					bret = false;
					Assert( 0 );
					break;
				}
    		}
		}
	}

	pTrackGroup = dmx->FindOrAddTrackGroup( VCD_GLOBAL_EVENTS_TRACK_GROUP_NAME );
	Assert( pTrackGroup );

	track = pTrackGroup->FindOrAddTrack( VCD_GLOBAL_EVENTS_TRACK_GROUP_NAME, DMECLIP_CHANNEL );
	Assert( track );

	// Now add global events
	int numEvents = scene->GetNumEvents();
	for ( int event = 0; event < numEvents; ++event )
	{
		CChoreoEvent *pEvent = scene->GetEvent( event );
		if ( !pEvent || pEvent->GetActor() )
			continue;

		// Set up event
		CDmeChannelsClip *pClip = CreateElement< CDmeChannelsClip >( "", dmx->GetFileId() );
		Assert( pClip );

		track->AddClip( pClip );

		// Fill in data
		bool success = ConvertEventToDmx( pEvent, pClip );
		if ( !success )
		{
			bret = false;
			Assert( 0 );
			break;
		}
	}

	dmx->SetValue( "associated_bsp", scene->GetMapname() );
	dmx->SetValue< float >( "fps", scene->GetSceneFPS() );
	dmx->SetValue< bool >( "snap", scene->IsUsingFrameSnap() );

	return bret;
}

void EnsureActorAndChannelForTrack( CChoreoScene *scene, CDmeTrackGroup *pActor, CDmeTrack *pChannel )
{
	const char *actorName = pActor->GetName();
	const char *channelName = pChannel->GetName();

	CChoreoActor *a = scene->FindActor( actorName );
	if ( !a )
	{
		a = scene->AllocActor();
		Assert( a );
		a->SetName( actorName );
		a->SetActive( !pActor->GetValue< bool >( "actorDisabled" ) );
		a->SetFacePoserModelName( pActor->GetValueString( "actorModel" ) );
	}

	CChoreoChannel *c = a->FindChannel( channelName );
	if ( !c )
	{
		c = scene->AllocChannel();
		Assert( c );
		c->SetName( channelName );
		c->SetActor( a );
		c->SetActive( !pChannel->IsMute() );
		a->AddChannel( c );
	}
}

template< class T >
T *FindAttributeInArray( const CDmrElementArray<> &array, const char *elementName )
{
	int c = array.Count();
	for ( int i = 0; i < c; ++i )
	{
		T *element = CastElement< T >( array[ i ] );
		if ( !element )
			continue;

		if ( !Q_stricmp( element->GetName(), elementName ) )
			return element;
	}

	return NULL;
}

bool ConvertDmxToEvent( CChoreoScene *scene, CDmeTrackGroup *pActor, CDmeTrack *pChannel, CDmeChannelsClip *clip, bool globalEvent )
{
	bool bret = true;

	// Allocate choreo event
	CChoreoEvent *event = scene->AllocEvent();
	Assert( event );
	if ( !event )
	{
		bret = false;
		return bret;
	}

	event->SetName( clip->GetName() );
	event->SetType( CChoreoEvent::TypeForName( clip->GetValueString( "eventtype" ) ) );

	if ( !globalEvent )
	{
		EnsureActorAndChannelForTrack( scene, pActor, pChannel );

		const char *actorName = pActor->GetName();
		const char *channelName = pChannel->GetName();

		CChoreoActor *a = scene->FindActor( actorName );
		Assert( a );
		CChoreoChannel *c = a->FindChannel( channelName );
		Assert( c );

		if ( !a || !c )
		{
			bret = false;
			return bret;
		}

		event->SetActor( a );
		event->SetChannel( c );
		c->AddEvent( event );
	}

	// Set timeframe info
	CDmeTimeFrame *tf = clip->GetTimeFrame();
	Assert( tf );
	if ( tf )
	{
		event->SetStartTime( tf->GetStartTime().GetSeconds() );
		float duration =  tf->GetDuration().GetSeconds();
		if ( duration <= 0.0f )
		{
			event->SetEndTime( -1.0f );
		}
		else
		{
			event->SetEndTime( event->GetStartTime() + duration );
		}
	}

	event->SetParameters( clip->GetValueString( "parameters" ) );
	event->SetParameters2( clip->GetValueString( "parameters2" ) );

	const CDmrElementArray<> array( clip, "channels" );
	Assert( array.IsValid() );
	if ( array.IsValid() )
	{
		int c = array.Count();
		for ( int i = 0 ; i < c; ++i )
		{
			CDmeChannel *channel = CastElement< CDmeChannel >( array[i] );
			if ( !channel || Q_stricmp( channel->GetName(), "event_ramp" ) )
				continue;

			CDmeTypedLog< float > *ramp = static_cast< CDmeTypedLog< float > * >( channel->GetLog() );
			if ( !ramp )
				continue;

			bool active[ 2 ];
			float value[ 2 ];
			int curveType[ 2 ];

			ramp->GetEdgeInfo( 0, active[ 0 ], value[ 0 ], curveType[ 0 ] );
			ramp->GetEdgeInfo( 1, active[ 1 ], value[ 1 ], curveType[ 1 ] );

			event->GetRamp()->SetEdgeActive( true, active[ 0 ] ); 
			event->GetRamp()->SetEdgeActive( false, active[ 1 ] ); 

			event->GetRamp()->SetEdgeInfo( true, curveType[ 0 ], value[ 0 ] );
			event->GetRamp()->SetEdgeInfo( false, curveType[ 1 ], value[ 1 ] );

			int rampCount = ramp->GetKeyCount();
			for ( int j = 0; j < rampCount; ++j )
			{
				CExpressionSample *sample = event->AddRamp( ramp->GetKeyTime( j ).GetSeconds(), ramp->GetKeyValue( j ), false );
				sample->SetCurveType( ramp->GetKeyCurveType( j ) );
			}
		}
	}

	event->SetPitch( clip->GetValue< float >( "pitch" ) );
	event->SetYaw( clip->GetValue< float >( "yaw" ) );

	event->SetResumeCondition( clip->GetValue< bool >( "resumecondition" ) );
	event->SetLockBodyFacing( clip->GetValue< bool >( "lockbodyfacing" ) );
	event->SetDistanceToTarget( clip->GetValue< float >( "distancetotarget" ) );
	event->SetFixedLength( clip->GetValue< bool >( "fixedlength" ) );
	event->SetForceShortMovement( clip->GetValue< bool >( "forceshortmovement" ) );
	event->SetActive( clip->GetValue< bool >( "active" ) );

	if ( clip->HasAttribute( "tags" ) )
	{
		CDmElement *tags = clip->GetValueElement< CDmElement >( "tags" );
		if ( tags )
		{
			// Now create arrays for the name and percentages
			const CDmrStringArray names( tags, "tagname" );
			const CDmrArray<float> percentages( tags, "tagpercentage" );
			Assert( names.IsValid() && percentages.IsValid() );
			
			Assert( names.Count() == percentages.Count() );
			for ( int t = 0; t < names.Count(); t++ )
			{
				event->AddRelativeTag( names[t], percentages[t] );
			}
		}
	}

	if ( clip->HasAttribute( "timing_tags" ) )
	{
		CDmElement *tags = clip->GetValueElement< CDmElement >( "timing_tags" );
		if ( tags )
		{
			// Now create arrays for the name and percentages
			const CDmrStringArray names( tags, "tagname" );
			const CDmrArray<float> percentages( tags, "tagpercentage" );
			const CDmrArray<bool> lockstates( tags, "lockedstate" );

			Assert( names.IsValid() && percentages.IsValid() && lockstates.IsValid() );
			Assert( names.Count() == percentages.Count() && names.Count() == lockstates.Count() );

			for ( int t = 0; t < names.Count(); t++ )
			{
				event->AddTimingTag( names[ t ], percentages[ t ], lockstates[ t ] );
			}
		}
	}

	
	// Abs tags
	int tagtype;
	for ( tagtype = 0; tagtype < CChoreoEvent::NUM_ABS_TAG_TYPES; tagtype++ )
	{
		char sz[ 512 ];
		Q_snprintf( sz, sizeof( sz ), "absolutetags %s", CChoreoEvent::NameForAbsoluteTagType( (CChoreoEvent::AbsTagType)tagtype ) );

		if ( clip->HasAttribute( sz ) )
		{
			CDmElement *tags = clip->GetValueElement< CDmElement >( sz );
			if ( tags )
			{
				// Now create arrays for the name and percentages
				const CDmrStringArray names( tags, "tagname" );
				const CDmrArray<float> percentages( tags, "tagpercentage" );

				Assert( names.IsValid() && percentages.IsValid() );
				Assert( names.Count() == percentages.Count() );

				for ( int t = 0; t < names.Count(); t++ )
				{
					event->AddAbsoluteTag( (CChoreoEvent::AbsTagType)tagtype, names[ t ], percentages[ t ] );
				}
			}
		}
	}
	
	if ( clip->HasAttribute( "relative_tag" ) )
	{
		CDmElement *relativeTag = clip->GetValueElement< CDmElement >( "relative_tag" );
		if ( relativeTag )
		{
			event->SetUsingRelativeTag
			( 
				true,
				relativeTag->GetValueString( "tagname" ),
				relativeTag->GetValueString( "tagwav" ) 
			);
		}
	}
				
	switch ( event->GetType() )
	{
	default:
		break;
	case CChoreoEvent::SPEAK:
		{
			CDmElement *speakProperties = NULL;
			if ( clip->HasAttribute( "SPEAK" ) )
			{
				speakProperties = clip->GetValueElement< CDmElement >( "SPEAK" );
			}
			if ( speakProperties )
			{
				event->SetCloseCaptionType( CChoreoEvent::CCTypeForName( speakProperties->GetValueString( "closedcaption_type" ) ) );
				event->SetCloseCaptionToken( speakProperties->GetValueString( "closedcaption_token" ) );
				event->SetUsingCombinedFile( speakProperties->GetValue< bool >( "closedcaption_usingcombinedfile" ) );
				event->SetCombinedUsingGenderToken( speakProperties->GetValue< bool >( "closedcaption_combinedusesgender" ) );
				event->SetSuppressingCaptionAttenuation( speakProperties->GetValue< bool >( "closedcaption_noattenuate" ) );
			}
		}
		break;
	case CChoreoEvent::GESTURE:
		{
			CDmElement *gestureProperties = NULL;
			if ( clip->HasAttribute( "GESTURE" ) )
			{
				gestureProperties = clip->GetValueElement< CDmElement >( "GESTURE" );
			}
			if ( gestureProperties )
			{
				if ( Q_stricmp( clip->GetName(), "NULL" ) )
				{
					event->SetGestureSequenceDuration( gestureProperties->GetValue< float >( "sequenceduration" ) );
				}
			}
		}
		break;
	case CChoreoEvent::FLEXANIMATION:
		{
			// Save flex animation tracks as channels
			const CDmrElementArray<> array( clip, "channels" );
			if ( array.IsValid() )
			{
				// Now add a DmeChannel for each flex controller
				int numTracks = array.Count();
				for ( int i = 0 ; i < numTracks; ++i )
				{
					CDmeChannel *channel = CastElement< CDmeChannel >( array[ i ] );
					Assert( channel );
					if ( !channel )
					{
						bret = false;
						break;
					}

					if ( !channel->HasAttribute( "flexchannel" ) )
						continue;

					// Skip all helper channels, only care about flexchannels
					if ( !channel->GetValue< bool >( "flexchannel" ) )
						continue;

					// Skip the balance channels, we'll pull their data below
					if ( channel->GetValue< bool >( "isbalancechannel" ) )
						continue;

					Assert( !Q_stristr( channel->GetName(), "_balance" ) );

					CFlexAnimationTrack *track = event->AddTrack( channel->GetName() );
					Assert( track );

					if ( !track )
					{
						bret = false;
						break;
					}

					track->SetTrackActive( !channel->GetValue< bool >( "disabled" ) );
					track->SetComboType( channel->GetValue< bool >( "combo" ) );

					track->SetMin( channel->GetValue< float >( "rangemin" ) );
					track->SetMax( channel->GetValue< float >( "rangemax" ) );

					CDmeTypedLog<float> *log = static_cast< CDmeTypedLog<float> * >( channel->GetLog() );
					if ( log )
					{
						bool active[ 2 ];
						float value[ 2 ];
						int curveType[ 2 ];

						log->GetEdgeInfo( 0, active[ 0 ], value[ 0 ], curveType[ 0 ] );
						log->GetEdgeInfo( 1, active[ 1 ], value[ 1 ], curveType[ 1 ] );

						track->SetEdgeActive( true, active[ 0 ] ); 
						track->SetEdgeActive( false, active[ 1 ] ); 

						track->SetEdgeInfo( true, curveType[ 0 ], value[ 0 ] );
						track->SetEdgeInfo( false, curveType[ 1 ], value[ 1 ] );

                        int sampleCount = log->GetKeyCount();
						for ( int j = 0; j < sampleCount; ++j )
						{
							int curveType = log->GetKeyCurveType( j );
							float value = log->GetKeyValue( j );
							DmeTime_t time = log->GetKeyTime( j );
 
							CExpressionSample *sample = track->AddSample( time.GetSeconds(), value, 0 );
							sample->SetCurveType( curveType );
						}
					}

					// Right out the stereo "balance" curve
					if ( track->IsComboType() )
					{
						char balanceChannelName[ 512 ];
						Q_snprintf( balanceChannelName, sizeof( balanceChannelName ), "%s_balance", track->GetFlexControllerName() );

						// Find the balance data
						CDmeChannel *balanceChannel = FindAttributeInArray< CDmeChannel >( array, balanceChannelName );
						if ( balanceChannel )
						{
							CDmeTypedLog< float > *balance = static_cast< CDmeTypedLog< float > * >( balanceChannel->GetLog() );
							if ( balance )
							{
								// Now set up edge properties
								int sampleCount = balance->GetKeyCount();
								for ( int j = 0; j < sampleCount; ++j )
								{
									int curveType = balance->GetKeyCurveType( j );
									float value = balance->GetKeyValue( j );
									DmeTime_t time = balance->GetKeyTime( j );
		 
									CExpressionSample *sample = track->AddSample( time.GetSeconds(), value, 1 );
									sample->SetCurveType( curveType );
								}
							}
							else
							{
								char msg[ 512 ];
								Q_snprintf( msg, sizeof( msg ), "Error:  Missing balance channel for combo flex track (%s) in (%s)\n", track->GetFlexControllerName(), event->GetName() );
								Warning( msg );
							}
						}
					}
				}
			}
		}
		break;
	case CChoreoEvent::LOOP:
		{
			CDmElement *loopProperties = NULL;
			if ( clip->HasAttribute( "LOOP" ) )
			{
				loopProperties = clip->GetValueElement< CDmElement >( "LOOP" );
			}
			if ( loopProperties )
			{
				event->SetLoopCount( loopProperties->GetValue< int >( "loopcount" ) );
			}
		}
		break;
	}

	return true;
}

bool ConvertDmxToScene( CDmeFilmClip *dmx, CChoreoScene *scene )
{
	bool bret = true;

	// This should have been created correctly already
	// Assert( !Q_stricmp( scene->GetFilename(), dmx->GetName() ) );

	CDmElement *scaleSettings = dmx->GetValueElement< CDmElement >( "scalesettings" );
	Assert( scaleSettings );
	if ( scaleSettings )
	{
		CDmAttribute *setting = scaleSettings->FirstAttribute();
		for ( ; setting ; setting = setting->NextAttribute() )
		{
			if ( setting->GetType() != AT_INT )
				continue;

			scene->SetTimeZoom( setting->GetName(), setting->GetValue<int>() );
		}
	}

	// Deal with the scene ramp
	CDmeTrackGroup *pTrackGroup = dmx->FindTrackGroup( VCD_SCENE_RAMP_TRACK_GROUP_NAME );
	if ( pTrackGroup )
	{
		CDmeTrack *track = pTrackGroup->FindTrack( VCD_SCENE_RAMP_TRACK_GROUP_NAME );
		if ( track )
		{
			CDmeChannelsClip *pClip = CastElement< CDmeChannelsClip >( track->FindNamedClip( VCD_SCENE_RAMP_TRACK_GROUP_NAME ) );
			if ( pClip )
			{
				// scene_ramp should be the first subchannel
				const CDmrElementArray<> array( pClip, "channels" );
				Assert( array.IsValid() );
				if ( array.IsValid() && ( array.Count() > 0 ) )
				{
					CDmeChannel *channel = CastElement< CDmeChannel >( array[0] );
					if ( channel )
					{
						CDmeTypedLog< float > *ramp = static_cast< CDmeTypedLog< float > * >( channel->GetLog() );
						if ( ramp )
						{
							bool active[ 2 ];
							float value[ 2 ];
							int curveType[ 2 ];

							ramp->GetEdgeInfo( 0, active[ 0 ], value[ 0 ], curveType[ 0 ] );
							ramp->GetEdgeInfo( 1, active[ 1 ], value[ 1 ], curveType[ 1 ] );

							scene->GetSceneRamp()->SetEdgeActive( true, active[ 0 ] ); 
							scene->GetSceneRamp()->SetEdgeActive( false, active[ 1 ] ); 

							scene->GetSceneRamp()->SetEdgeInfo( true, curveType[ 0 ], value[ 0 ] );
							scene->GetSceneRamp()->SetEdgeInfo( false, curveType[ 1 ], value[ 1 ] );

							int rampCount = ramp->GetKeyCount();
							for ( int j = 0; j < rampCount; ++j )
							{
								CExpressionSample *sample = scene->AddSceneRamp( ramp->GetKeyTime( j ).GetSeconds(), ramp->GetKeyValue( j ), false );
								sample->SetCurveType( ramp->GetKeyCurveType( j ) );
							}
						}
					}
				}
			}
		}
	}

	// Deal with global events
	pTrackGroup = dmx->FindTrackGroup( VCD_GLOBAL_EVENTS_TRACK_GROUP_NAME );
	if ( pTrackGroup )
	{
		CDmeTrack *pTrack = pTrackGroup->FindTrack( VCD_GLOBAL_EVENTS_TRACK_GROUP_NAME );
		if ( pTrack )
		{
			// Now add global events
			int numEvents = pTrack->GetClipCount();
			for ( int event = 0; event < numEvents; ++event )
			{
				CDmeChannelsClip *pClip = CastElement< CDmeChannelsClip >( pTrack->GetClip( event ) );
				if ( !pClip )
					continue;

				bool success = ConvertDmxToEvent( scene, pTrackGroup, pTrack, pClip, true );
				if ( !success )
				{
					bret = false;
					Assert( 0 );
					break;
				}
			}
		}
	}

	int nNumTrackGroups = dmx->GetTrackGroupCount();
	for ( int i = 0; i < nNumTrackGroups; ++i )
	{
		CDmeTrackGroup *pTrackGroup = dmx->GetTrackGroup( i );
		if ( !pTrackGroup->GetValue< bool >( "isActor" ) )
			continue;

		// Walk the track groups
		int nNumTracks = pTrackGroup->GetTrackCount();
		for ( int j = 0 ; j < nNumTracks; ++j )
		{
			CDmeTrack *pTrack = pTrackGroup->GetTrack( j );
			
			EnsureActorAndChannelForTrack( scene, pTrackGroup, pTrack );

			int numEvents = pTrack->GetClipCount();
			for ( int clip = 0; clip < numEvents; ++clip )
			{
				CDmeChannelsClip *pClip = CastElement< CDmeChannelsClip >( pTrack->GetClip( clip ) );
				if ( !pClip )
					continue;
				
				bool success = ConvertDmxToEvent( scene, pTrackGroup, pTrack, pClip, false );
				if ( !success )
				{
					bret = false;
					Assert( 0 );
					break;
				}
			}
		}
	}

	scene->SetMapname( dmx->GetValueString( "associated_bsp" ) );
	scene->SetSceneFPS( dmx->GetValue< float >( "fps" ) );
	scene->SetUsingFrameSnap( dmx->GetValue< bool >( "snap" ) );

	return bret;
}