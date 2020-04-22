//========= Copyright Valve Corporation, All rights reserved. ============//
//
// A class used to build flex animation controls for an animation set
//
//=============================================================================

#ifndef FLEXCONTROLBUILDER_H
#define FLEXCONTROLBUILDER_H
#ifdef _WIN32
#pragma once
#endif


#include "movieobjects/timeutils.h"
#include "tier1/utlvector.h"
#include "movieobjects/dmelog.h"


//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class CDmeAnimationSet;
class CDmeGameModel;
class CDmeFilmClip;
class CDmeChannelsClip;
class CDmElement;
class CDmeChannel;
class CDmeBalanceToStereoCalculatorOperator;
class CDmeGlobalFlexControllerOperator;


//-----------------------------------------------------------------------------
//
// Utility class for dealing with the complex task of building flex controls
//
//-----------------------------------------------------------------------------
class CFlexControlBuilder
{
public:
	// Main entry point for creating flex animation set controls
	void CreateAnimationSetControls( CDmeFilmClip *pMovie, CDmeAnimationSet *pAnimationSet, 
		CDmeGameModel *pGameModel, CDmeFilmClip *pSourceClip, CDmeChannelsClip *pDestClip, bool bUseExistingLogs );

private:
	enum ControlField_t
	{
		CONTROL_VALUE = 0,
		CONTROL_BALANCE,
		CONTROL_MULTILEVEL,

		CONTROL_FIELD_COUNT,
	};

	enum OutputField_t
	{
		OUTPUT_MONO = 0,
		OUTPUT_RIGHT = 0,
		OUTPUT_LEFT,
		OUTPUT_MULTILEVEL,

		OUTPUT_FIELD_COUNT,
	};

	struct FlexControllerInfo_t
	{
		char m_pFlexControlName[256];
		float m_flDefaultValue;
		int m_nGlobalIndex;
	};

	struct ExistingLogInfo_t
	{
		CDmeFloatLog *m_pLog;
		DmeTime_t m_GlobalOffset;
		double m_flGlobalScale;
	};

	struct ControlInfo_t
	{
		char m_pControlName[256];
		bool m_bIsStereo : 1;
		bool m_bIsMulti : 1;
		CDmElement *m_pControl;

		int m_pControllerIndex[OUTPUT_FIELD_COUNT];

		CDmeChannel *m_ppControlChannel[CONTROL_FIELD_COUNT];
		float m_pDefaultValue[CONTROL_FIELD_COUNT];
		ExistingLogInfo_t m_pExistingLog[CONTROL_FIELD_COUNT];
	};

	// Removes a channel from the channels clip referring to it.
	void RemoveChannelFromClips( CDmeChannel *pChannel );

	// Removes a stereo operator from the animation set referring to it
	void RemoveStereoOpFromSet( CDmeBalanceToStereoCalculatorOperator *pChannel );

	// Builds the list of flex controls (outputs) in the current game model
	void BuildDesiredFlexControlList( CDmeGameModel *pGameModel );

	// This builds a list of the desired input controls we need to have controls for
	// by the time we're all done with this enormous process.
	void BuildDesiredControlList( CDmeGameModel *pGameModel );

	// finds controls whose channels don't point to anything anymore, and deletes both the channels and the control
	void RemoveUnusedControlsAndChannels( CDmeAnimationSet *pAnimationSet, CDmeChannelsClip *pChannelsClip );

	// I'll bet you can guess what this does
	void RemoveUnusedExistingFlexControllers( CDmeGameModel *pGameModel );

	// Fixup list of existing flex controller logs
	// - reattach flex controls that were removed from the gamemodel's list
	void FixupExistingFlexControlLogList( CDmeFilmClip *pCurrentClip, CDmeGameModel *pGameModel );

	// Converts existing logs to balance/value if necessary while building the list
	// Also deletes existing infrastructure (balance ops, channels, flex controller ops).
	void BuildExistingFlexControlLogList( CDmeFilmClip *pCurrentClip, CDmeGameModel *pGameModel );

	// Finds a desired flex controller index in the m_FlexControllerInfo array
	int FindDesiredFlexController( const char *pFlexControllerName ) const;

	// Blows away the various elements trying to control a flex controller op
	void CleanupExistingFlexController( CDmeGameModel *pGameModel, CDmeGlobalFlexControllerOperator *pOp );

	// Finds a channels clip containing a particular channel
	CDmeChannelsClip* FindChannelsClipContainingChannel( CDmeFilmClip *pClip, CDmeChannel *pSearch );

	// Returns an existing mono log
	void GetExistingMonoLog( ExistingLogInfo_t *pLog, CDmeFilmClip *pClip, CDmeGlobalFlexControllerOperator *pMonoOp );

	// Returns an existing stereo log, performing conversion if necessary
	void GetExistingStereoLog( ExistingLogInfo_t *pLogs, CDmeFilmClip *pClip,
		CDmeGlobalFlexControllerOperator *pRightOp, CDmeGlobalFlexControllerOperator *pLeftOp );

	// Returns an existing value/balance log
	void GetExistingVBLog( ExistingLogInfo_t *pLogs, CDmeFilmClip *pClip, CDmeBalanceToStereoCalculatorOperator *pStereoOp );

	// Converts an existing left/right log into a value/balance log
	void ConvertExistingLRLogs( ExistingLogInfo_t *pLogs, CDmeFilmClip *pClip, CDmeChannel *pLeftChannel, CDmeChannel *pRightChannel );

	// Computes a global offset and scale to convert from log time to global time
	void ComputeChannelTimeTransform( DmeTime_t *pOffset, double *pScale, CDmeChannelsClip *pChannelsClip );
	bool ComputeChannelTimeTransform( DmeTime_t *pOffset, double *pScale, CDmeFilmClip* pClip, CDmeChannel* pChannel );

	// Initializes the fields of a flex control
	void InitializeFlexControl( ControlInfo_t &info );

	// Creates all controls for flexes
	void CreateFlexControls( CDmeAnimationSet *pAnimationSet );

	// Build the infrastructure of the ops that connect that control to the dmegamemodel
	void AttachControlsToGameModel( CDmeAnimationSet *pAnimationSet, CDmeGameModel *pGameModel, CDmeChannelsClip *pChannelsClip );

	// Connects a mono control to a single flex controller op
	void BuildFlexControllerOps( CDmeGameModel *pGameModel, CDmeChannelsClip *pChannelsClip, ControlInfo_t &info, ControlField_t field );

	// Connects a stereo control to a two flex controller ops
	void BuildStereoFlexControllerOps( CDmeAnimationSet *pAnimationSet,	CDmeGameModel *pGameModel, CDmeChannelsClip *pChannelsClip, ControlInfo_t &info );

	// Attaches existing logs and sets default values for logs
	void SetupLogs( CDmeChannelsClip *pChannelsClip, bool bUseExistingLogs );

	// Destination flex controllers
	CUtlVector< FlexControllerInfo_t > m_FlexControllerInfo;

	// Destination controls
	CUtlVector< ControlInfo_t > m_ControlInfo;

	CDmeFilmClip *m_pMovie;
};


//-----------------------------------------------------------------------------
// Initialize default global flex controller
//-----------------------------------------------------------------------------
void SetupDefaultFlexController();


#endif // FLEXCONTROLBUILDER_H
