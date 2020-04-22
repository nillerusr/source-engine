START_SCHEMA( GC, cbase.h )

//-----------------------------------------------------------------------------
// GameAccount
//
//-----------------------------------------------------------------------------
START_TABLE( k_ESchemaCatalogMain, GameAccount, TABLE_PROP_NORMAL )
MEM_FIELD_BIN( unAccountID, AccountID,	uint32 )		// Account ID of the user
MEM_FIELD_BIN( unRewardPoints, RewardPoints, uint32 )	// number of timed reward points (coplayed minutes) for this user
MEM_FIELD_BIN( unPointCap, PointCap, uint32 )			// Current maximum number of points
MEM_FIELD_BIN( unLastCapRollover, LastCapRollover, RTime32 ) // Last time the player's cap was adjusted
PRIMARY_KEY_CLUSTERED( 100, unAccountID )
WIPE_TABLE_BETWEEN_TESTS( k_EWipePolicyWipeForAllTests )
ALLOW_WIPE_TABLE_IN_PRODUCTION( false )
END_TABLE


//-----------------------------------------------------------------------------
// GameAccountClient
//
//-----------------------------------------------------------------------------
START_TABLE( k_ESchemaCatalogMain, GameAccountClient, TABLE_PROP_NORMAL )
MEM_FIELD_BIN( unAccountID, AccountID,	uint32 )			// Item Owner
PRIMARY_KEY_CLUSTERED( 80, unAccountID )
WIPE_TABLE_BETWEEN_TESTS( k_EWipePolicyWipeForAllTests )
ALLOW_WIPE_TABLE_IN_PRODUCTION( false )
END_TABLE




// --------------------------------------------------------
// WARNING!  All new tables need to be added to the end of the file
// if you expect to deploy the GC without deploying new clients.
// --------------------------------------------------------

// NEED A CARRIAGE RETURN HERE!
//-------------------------