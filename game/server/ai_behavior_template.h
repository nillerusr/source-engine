//========== Copyright © 2007, Valve Corporation, All rights reserved. ========
//
// Purpose:
//
//=============================================================================


#include "ai_behavior_template_generate.h"

AI_BEHAVIOR_METHOD_0V( PrescheduleThink );
AI_BEHAVIOR_METHOD_0V( OnScheduleChange );
AI_BEHAVIOR_METHOD_0( bool, IsCrouching );
AI_BEHAVIOR_METHOD_1( bool, IsValidEnemy, CBaseEntity * );
AI_BEHAVIOR_METHOD_0( CBaseEntity *, BestEnemy );
AI_BEHAVIOR_METHOD_2( bool,	IsCoverPosition, const Vector &, const Vector & );
AI_BEHAVIOR_METHOD_2( bool, IsValidCover, const Vector &, CAI_Hint const * );
AI_BEHAVIOR_METHOD_3( bool, IsValidShootPosition, const Vector &, CAI_Node *, CAI_Hint const * );
AI_BEHAVIOR_METHOD_3( bool, WeaponLOSCondition, const Vector &, const Vector &, bool );
AI_BEHAVIOR_METHOD_0( float, GetMaxTacticalLateralMovement );
AI_BEHAVIOR_METHOD_1( bool, ShouldIgnoreSound, CSound * );
AI_BEHAVIOR_METHOD_1V( OnSeeEntity, CBaseEntity * );
AI_BEHAVIOR_METHOD_2V( OnFriendDamaged, CBaseCombatCharacter *, CBaseEntity * );
AI_BEHAVIOR_METHOD_0( bool, IsInterruptable );
AI_BEHAVIOR_METHOD_0( bool, IsNavigationUrgent );
AI_BEHAVIOR_METHOD_0( bool, ShouldPlayerAvoid );
AI_BEHAVIOR_METHOD_1( int, OnTakeDamage_Alive, const CTakeDamageInfo & );
AI_BEHAVIOR_METHOD_0( float, GetDefaultNavGoalTolerance);
AI_BEHAVIOR_METHOD_0( float, GetReasonableFacingDist );
AI_BEHAVIOR_METHOD_0( bool, CanFlinch );
AI_BEHAVIOR_METHOD_1( bool, IsCrouchedActivity, Activity );
AI_BEHAVIOR_METHOD_1( bool, QueryHearSound, CSound * );
AI_BEHAVIOR_METHOD_1( bool, CanRunAScriptedNPCInteraction, bool );
AI_BEHAVIOR_METHOD_2( Activity, GetFlinchActivity, bool, bool );
AI_BEHAVIOR_METHOD_3( bool, OnCalcBaseMove, AILocalMoveGoal_t *, float, AIMoveResult_t * );
AI_BEHAVIOR_METHOD_1V( ModifyOrAppendCriteria, AI_CriteriaSet& );
AI_BEHAVIOR_METHOD_3V( Teleport, const Vector *, const QAngle *, const Vector * );
AI_BEHAVIOR_METHOD_1V( HandleAnimEvent, animevent_t * );
AI_BEHAVIOR_METHOD_1( bool, FValidateHintType, CAI_Hint * );
AI_BEHAVIOR_METHOD_0( bool, ShouldAlwaysThink );
AI_BEHAVIOR_METHOD_0( bool, IsCurTaskContinuousMove );
AI_BEHAVIOR_METHOD_0V( AimGun );
AI_BEHAVIOR_METHOD_1( Activity, NPC_TranslateActivity, Activity );
AI_BEHAVIOR_METHOD_0V( OnMovementFailed );
AI_BEHAVIOR_METHOD_0V( OnMovementComplete );
// AI_BEHAVIOR_METHOD_0( float, GetJumpGravity );
// AI_BEHAVIOR_METHOD_6C( bool, IsJumpLegal, const Vector &, const Vector &, const Vector &, float, float, float );
// AI_BEHAVIOR_METHOD_4( bool, MovementCost, int, const Vector &, const Vector &, float * );
