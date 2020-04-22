//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"

#include "vcollide_parse_private.h"

#include "tier1/strtools.h" 
#include "vphysics/constraints.h"
#include "vphysics/vehicles.h"
#include "filesystem_helpers.h"
#include "bspfile.h"
#include "utlbuffer.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static void ReadVector( const char *pString, Vector& out )
{
	float x, y, z;
	sscanf( pString, "%f %f %f", &x, &y, &z );
	out[0] = x;
	out[1] = y;
	out[2] = z;
}

static void ReadAngles( const char *pString, QAngle& out )
{
	float x, y, z;
	sscanf( pString, "%f %f %f", &x, &y, &z );
	out[0] = x;
	out[1] = y;
	out[2] = z;
}

static void ReadVector4D( const char *pString, Vector4D& out )
{
	float x, y, z, w;
	sscanf( pString, "%f %f %f %f", &x, &y, &z, &w );
	out[0] = x;
	out[1] = y;
	out[2] = z;
	out[3] = w;
}

class CVPhysicsParse : public IVPhysicsKeyParser
{
public:
				~CVPhysicsParse() {}

				CVPhysicsParse( const char *pKeyData );
	void		NextBlock( void );

	const char *GetCurrentBlockName( void );
	bool		Finished( void );
	void		ParseSolid( solid_t *pSolid, IVPhysicsKeyHandler *unknownKeyHandler );
	void		ParseFluid( fluid_t *pFluid, IVPhysicsKeyHandler *unknownKeyHandler );
	void		ParseRagdollConstraint( constraint_ragdollparams_t *pConstraint, IVPhysicsKeyHandler *unknownKeyHandler );
	void		ParseSurfaceTable( int *table, IVPhysicsKeyHandler *unknownKeyHandler );
	void		ParseSurfaceTablePacked( CUtlVector<char> &out );
	void		ParseVehicle( vehicleparams_t *pVehicle, IVPhysicsKeyHandler *unknownKeyHandler );
	void		ParseCustom( void *pCustom, IVPhysicsKeyHandler *unknownKeyHandler );
	void		SkipBlock( void ) { ParseCustom(NULL, NULL); }

private:
	void		ParseVehicleAxle( vehicle_axleparams_t &axle );
	void		ParseVehicleWheel( vehicle_wheelparams_t &wheel );
	void		ParseVehicleSuspension( vehicle_suspensionparams_t &suspension );
	void		ParseVehicleBody( vehicle_bodyparams_t &body );
	void		ParseVehicleEngine( vehicle_engineparams_t &engine );
	void		ParseVehicleEngineBoost( vehicle_engineparams_t &engine );
	void		ParseVehicleSteering( vehicle_steeringparams_t &steering );

	const char *m_pText;
	char m_blockName[MAX_KEYVALUE];
};


CVPhysicsParse::CVPhysicsParse( const char *pKeyData )
{
	m_pText = pKeyData;
	NextBlock();
}

void CVPhysicsParse::NextBlock( void )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];
	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( !Q_strcmp(value, "{") )
		{
			V_strcpy_safe( m_blockName, key );
			return;
		}
	}

	// Make sure m_blockName is initialized -- should this be done?
	m_blockName[ 0 ] = 0;
}


const char *CVPhysicsParse::GetCurrentBlockName( void )
{
	if ( m_pText )
		return m_blockName;

	return NULL;
}


bool CVPhysicsParse::Finished( void )
{
	if ( m_pText )
		return false;

	return true;
}


void CVPhysicsParse::ParseSolid( solid_t *pSolid, IVPhysicsKeyHandler *unknownKeyHandler )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];
	key[0] = 0;

	if ( unknownKeyHandler )
	{
		unknownKeyHandler->SetDefaults( pSolid );
	}
	else
	{
		memset( pSolid, 0, sizeof(*pSolid) );
	}
	
	// disable these until the ragdoll is created
	pSolid->params.enableCollisions = false;

	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
		{
			NextBlock();
			return;
		}

		if ( !Q_stricmp( key, "index" ) )
		{
			pSolid->index = atoi(value);
		}
		else if ( !Q_stricmp( key, "name" ) )
		{
			Q_strncpy( pSolid->name, value, sizeof(pSolid->name) );
		}
		else if ( !Q_stricmp( key, "parent" ) )
		{
			Q_strncpy( pSolid->parent, value, sizeof(pSolid->parent) );
		}
		else if ( !Q_stricmp( key, "surfaceprop" ) )
		{
			Q_strncpy( pSolid->surfaceprop, value, sizeof(pSolid->surfaceprop) );
		}
		else if ( !Q_stricmp( key, "mass" ) )
		{
			pSolid->params.mass = atof(value);
		}
		else if ( !Q_stricmp( key, "massCenterOverride" ) )
		{
			ReadVector( value, pSolid->massCenterOverride );
			pSolid->params.massCenterOverride = &pSolid->massCenterOverride;
		}
		else if ( !Q_stricmp( key, "inertia" ) )
		{
			pSolid->params.inertia = atof(value);
		}
		else if ( !Q_stricmp( key, "damping" ) )
		{
			pSolid->params.damping = atof(value);
		}
		else if ( !Q_stricmp( key, "rotdamping" ) )
		{
			pSolid->params.rotdamping = atof(value);
		}
		else if ( !Q_stricmp( key, "volume" ) )
		{
			pSolid->params.volume = atof(value);
		}
		else if ( !Q_stricmp( key, "drag" ) )
		{
			pSolid->params.dragCoefficient = atof(value);
		}
		else if ( !Q_stricmp( key, "rollingdrag" ) )
		{
			//pSolid->params.rollingDrag = atof(value);
		}
		else
		{
			if ( unknownKeyHandler )
			{
				unknownKeyHandler->ParseKeyValue( pSolid, key, value );
			}
		}
	}
}

void CVPhysicsParse::ParseRagdollConstraint( constraint_ragdollparams_t *pConstraint, IVPhysicsKeyHandler *unknownKeyHandler )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	if ( unknownKeyHandler )
	{
		unknownKeyHandler->SetDefaults( pConstraint );
	}
	else
	{
		memset( pConstraint, 0, sizeof(*pConstraint) );
		pConstraint->childIndex = -1;
		pConstraint->parentIndex = -1;
	}

	// BUGBUG: xmin/xmax, ymin/ymax, zmin/zmax specify clockwise rotations.  
	// BUGBUG: HL rotations are counter-clockwise, so reverse these limits at some point!!!
	pConstraint->useClockwiseRotations = true;

	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
		{
			NextBlock();
			return;
		}

		if ( !Q_stricmp( key, "parent" ) )
		{
			pConstraint->parentIndex = atoi( value );
		}
		else if ( !Q_stricmp( key, "child" ) )
		{
			pConstraint->childIndex = atoi( value );
		}
		else if ( !Q_stricmp( key, "xmin" ) )
		{
			pConstraint->axes[0].minRotation = atof( value );
		}
		else if ( !Q_stricmp( key, "xmax" ) )
		{
			pConstraint->axes[0].maxRotation = atof( value );
		}
		else if ( !Q_stricmp( key, "xfriction" ) )
		{
			pConstraint->axes[0].angularVelocity = 0;
			pConstraint->axes[0].torque = atof( value );
		}
		else if ( !Q_stricmp( key, "ymin" ) )
		{
			pConstraint->axes[1].minRotation = atof( value );
		}
		else if ( !Q_stricmp( key, "ymax" ) )
		{
			pConstraint->axes[1].maxRotation = atof( value );
		}
		else if ( !Q_stricmp( key, "yfriction" ) )
		{
			pConstraint->axes[1].angularVelocity = 0;
			pConstraint->axes[1].torque = atof( value );
		}
		else if ( !Q_stricmp( key, "zmin" ) )
		{
			pConstraint->axes[2].minRotation = atof( value );
		}
		else if ( !Q_stricmp( key, "zmax" ) )
		{
			pConstraint->axes[2].maxRotation = atof( value );
		}
		else if ( !Q_stricmp( key, "zfriction" ) )
		{
			pConstraint->axes[2].angularVelocity = 0;
			pConstraint->axes[2].torque = atof( value );
		}
		else
		{
			if ( unknownKeyHandler )
			{
				unknownKeyHandler->ParseKeyValue( pConstraint, key, value );
			}
		}
	}
}


void CVPhysicsParse::ParseFluid( fluid_t *pFluid, IVPhysicsKeyHandler *unknownKeyHandler )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	pFluid->index = -1;
	if ( unknownKeyHandler )
	{
		unknownKeyHandler->SetDefaults( pFluid );
	}
	else
	{
		memset( pFluid, 0, sizeof(*pFluid) );
		// HACKHACK: This is a reasonable default even though it is hardcoded
		V_strcpy_safe( pFluid->surfaceprop, "water" );
	}

	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
		{
			NextBlock();
			return;
		}

		if ( !Q_stricmp( key, "index" ) )
		{
			pFluid->index = atoi( value );
		}
		else if ( !Q_stricmp( key, "damping" ) )
		{
			pFluid->params.damping = atof( value );
		}
		else if ( !Q_stricmp( key, "surfaceplane" ) )
		{
			ReadVector4D( value, pFluid->params.surfacePlane );
		}
		else if ( !Q_stricmp( key, "currentvelocity" ) )
		{
			ReadVector( value, pFluid->params.currentVelocity );
		}
		else if ( !Q_stricmp( key, "contents" ) )
		{
			pFluid->params.contents = atoi( value );
		}
		else if ( !Q_stricmp( key, "surfaceprop" ) )
		{
			Q_strncpy( pFluid->surfaceprop, value, sizeof(pFluid->surfaceprop) );
		}
		else
		{
			if ( unknownKeyHandler )
			{
				unknownKeyHandler->ParseKeyValue( pFluid, key, value );
			}
		}
	}
}

void CVPhysicsParse::ParseSurfaceTable( int *table, IVPhysicsKeyHandler *unknownKeyHandler )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
		{
			NextBlock();
			return;
		}

		int propIndex = physprops->GetSurfaceIndex( key );
		int tableIndex = atoi(value);
		if ( tableIndex >= 0 && tableIndex < 128 )
		{
			table[tableIndex] = propIndex;
		}
	}
}

void CVPhysicsParse::ParseSurfaceTablePacked( CUtlVector<char> &out )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	int lastIndex = 0;
	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
		{
			NextBlock();
			return;
		}

		int len = Q_strlen( key );
		int outIndex = out.AddMultipleToTail( len + 1 );
		memcpy( &out[outIndex], key, len+1 );
		int tableIndex = atoi(value);
		Assert( tableIndex == lastIndex + 1);
		lastIndex = tableIndex;
	}
}

void CVPhysicsParse::ParseVehicleAxle( vehicle_axleparams_t &axle )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	memset( &axle, 0, sizeof(axle) );
	key[0] = 0;
	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
			return;

		// parse subchunks
		if ( value[0] == '{' )
		{
			if ( !Q_stricmp( key, "wheel" ) )
			{
				ParseVehicleWheel( axle.wheels );
			}
			else if ( !Q_stricmp( key, "suspension" ) )
			{
				ParseVehicleSuspension( axle.suspension );
			}
			else
			{
				SkipBlock();
			}
		}
		else if ( !Q_stricmp( key, "offset" ) )
		{
			ReadVector( value, axle.offset );
		}
		else if ( !Q_stricmp( key, "wheeloffset" ) )
		{
			ReadVector( value, axle.wheelOffset );
		}
		else if ( !Q_stricmp( key, "torquefactor" ) )
		{
			axle.torqueFactor = atof( value );
		}
		else if ( !Q_stricmp( key, "brakefactor" ) )
		{
			axle.brakeFactor = atof( value );
		}
	}
}

void CVPhysicsParse::ParseVehicleWheel( vehicle_wheelparams_t &wheel )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
			return;
		
		if ( !Q_stricmp( key, "radius" ) )
		{
			wheel.radius = atof( value );
		}
		else if ( !Q_stricmp( key, "mass" ) )
		{
			wheel.mass = atof( value );
		}
		else if ( !Q_stricmp( key, "inertia" ) )
		{
			wheel.inertia = atof( value );
		}
		else if ( !Q_stricmp( key, "damping" ) )
		{
			wheel.damping = atof( value );
		}
		else if ( !Q_stricmp( key, "rotdamping" ) )
		{
			wheel.rotdamping = atof( value );
		}
		else if ( !Q_stricmp( key, "frictionscale" ) )
		{
			wheel.frictionScale = atof( value );
		}
		else if ( !Q_stricmp( key, "material" ) )
		{
			wheel.materialIndex = physprops->GetSurfaceIndex( value );
		}
		else if ( !Q_stricmp( key, "skidmaterial" ) )
		{
			wheel.skidMaterialIndex = physprops->GetSurfaceIndex( value );
		}
		else if ( !Q_stricmp( key, "brakematerial" ) )
		{
			wheel.brakeMaterialIndex = physprops->GetSurfaceIndex( value );
		}
	}
}


void CVPhysicsParse::ParseVehicleSuspension( vehicle_suspensionparams_t &suspension )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
			return;
		
		if ( !Q_stricmp( key, "springconstant" ) )
		{
			suspension.springConstant = atof( value );
		}
		else if ( !Q_stricmp( key, "springdamping" ) )
		{
			suspension.springDamping = atof( value );
		}
		else if ( !Q_stricmp( key, "stabilizerconstant" ) )
		{
			suspension.stabilizerConstant = atof( value );
		}
		else if ( !Q_stricmp( key, "springdampingcompression" ) )
		{
			suspension.springDampingCompression = atof( value );
		}
		else if ( !Q_stricmp( key, "maxbodyforce" ) )
		{
			suspension.maxBodyForce = atof( value );
		}
	}
}


void CVPhysicsParse::ParseVehicleBody( vehicle_bodyparams_t &body )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
			return;
		
		if ( !Q_stricmp( key, "massCenterOverride" ) )
		{
			ReadVector( value, body.massCenterOverride );
		}
		else if ( !Q_stricmp( key, "addgravity" ) )
		{
			body.addGravity = atof( value );
		}
		else if ( !Q_stricmp( key, "maxAngularVelocity" ) )
		{
			body.maxAngularVelocity = atof( value );
		}
		else if ( !Q_stricmp( key, "massOverride" ) )
		{
			body.massOverride = atof( value );
		}
		else if ( !Q_stricmp( key, "tiltforce" ) )
		{
			body.tiltForce = atof( value );
		}
		else if ( !Q_stricmp( key, "tiltforceheight" ) )
		{
			body.tiltForceHeight = atof( value );
		}
		else if ( !Q_stricmp( key, "countertorquefactor" ) )
		{
			body.counterTorqueFactor = atof( value );
		}
		else if ( !Q_stricmp( key, "keepuprighttorque" ) )
		{
			body.keepUprightTorque = atof( value );
		}
	}
}


void CVPhysicsParse::ParseVehicleEngineBoost( vehicle_engineparams_t &engine )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
			return;
		// parse subchunks
		if ( !Q_stricmp( key, "force" ) )
		{
			engine.boostForce = atof( value );
		}
		else if ( !Q_stricmp( key, "duration" ) )
		{
			engine.boostDuration = atof( value );
		}
		else if ( !Q_stricmp( key, "delay" ) )
		{
			engine.boostDelay = atof( value );
		}
		else if ( !Q_stricmp( key, "maxspeed" ) )
		{
			engine.boostMaxSpeed = atof( value );
		}
		else if ( !Q_stricmp( key, "torqueboost" ) )
		{
			engine.torqueBoost = atoi( value ) != 0 ? true : false;
		}

	}
}

void CVPhysicsParse::ParseVehicleEngine( vehicle_engineparams_t &engine )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
			return;
		// parse subchunks
		if ( value[0] == '{' )
		{
			if ( !Q_stricmp( key, "boost" ) )
			{
				ParseVehicleEngineBoost( engine );
			}
			else
			{
				SkipBlock();
			}
		}
		else if ( !Q_stricmp( key, "gear" ) )
		{
			// Protect against exploits/overruns
			if ( engine.gearCount < ARRAYSIZE(engine.gearRatio) )
			{
				engine.gearRatio[engine.gearCount] = atof( value );
				engine.gearCount++;
			}
			else
			{
				Assert( 0 );
			}
		}
		else if ( !Q_stricmp( key, "horsepower" ) )
		{
			engine.horsepower = atof( value );
		}
		else if ( !Q_stricmp( key, "maxSpeed" ) )
		{
			engine.maxSpeed = atof( value );
		}
		else if ( !Q_stricmp( key, "maxReverseSpeed" ) )
		{
			engine.maxRevSpeed = atof( value );
		}
		else if ( !Q_stricmp( key, "axleratio" ) )
		{
			engine.axleRatio = atof( value );
		}
		else if ( !Q_stricmp( key, "maxRPM" ) )
		{
			engine.maxRPM = atof( value );
		}
		else if ( !Q_stricmp( key, "throttleTime" ) )
		{
			engine.throttleTime = atof( value );
		}
		else if ( !Q_stricmp( key, "AutoTransmission" ) )
		{
			engine.isAutoTransmission = atoi( value ) != 0 ? true : false;
		}
		else if ( !Q_stricmp( key, "shiftUpRPM" ) )
		{
			engine.shiftUpRPM = atof( value );
		}
		else if ( !Q_stricmp( key, "shiftDownRPM" ) )
		{
			engine.shiftDownRPM = atof( value );
		}
		else if ( !Q_stricmp( key, "autobrakeSpeedGain" ) )
		{
			engine.autobrakeSpeedGain = atof( value );
		}
		else if ( !Q_stricmp( key, "autobrakeSpeedFactor" ) )
		{
			engine.autobrakeSpeedFactor = atof( value );
		}
	}
}


void CVPhysicsParse::ParseVehicleSteering( vehicle_steeringparams_t &steering )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
			return;
		// parse subchunks
		if ( !Q_stricmp( key, "degreesSlow" ) )
		{
			steering.degreesSlow = atof( value );
		}
		else if ( !Q_stricmp( key, "degreesFast" ) )
		{
			steering.degreesFast = atof( value );
		}
		else if ( !Q_stricmp( key, "degreesBoost" ) )
		{
			steering.degreesBoost = atof( value );
		}
		else if ( !Q_stricmp( key, "fastcarspeed" ) )
		{
			steering.speedFast = atof( value );
		}
		else if ( !Q_stricmp( key, "slowcarspeed" ) )
		{
			steering.speedSlow = atof( value );
		}
		else if ( !Q_stricmp( key, "slowsteeringrate" ) )
		{
			steering.steeringRateSlow = atof( value );
		}
		else if ( !Q_stricmp( key, "faststeeringrate" ) )
		{
			steering.steeringRateFast = atof( value );
		}
		else if ( !Q_stricmp( key, "steeringRestRateSlow" ) )
		{
			steering.steeringRestRateSlow = atof( value );
		}
		else if ( !Q_stricmp( key, "steeringRestRateFast" ) )
		{
			steering.steeringRestRateFast = atof( value );
		}
		else if ( !Q_stricmp( key, "throttleSteeringRestRateFactor" ) )
		{
			steering.throttleSteeringRestRateFactor = atof( value );
		}
		else if ( !Q_stricmp( key, "boostSteeringRestRateFactor" ) )
		{
			steering.boostSteeringRestRateFactor = atof( value );
		}
		else if ( !Q_stricmp( key, "boostSteeringRateFactor" ) )
		{
			steering.boostSteeringRateFactor = atof( value );
		}
		else if ( !Q_stricmp( key, "steeringExponent" ) )
		{
			steering.steeringExponent = atof( value );
		}
		else if ( !Q_stricmp( key, "turnThrottleReduceSlow" ) )
		{
			steering.turnThrottleReduceSlow = atof( value );
		}
		else if ( !Q_stricmp( key, "turnThrottleReduceFast" ) )
		{
			steering.turnThrottleReduceFast = atof( value );
		}
		else if ( !Q_stricmp( key, "brakeSteeringRateFactor" ) )
		{
			steering.brakeSteeringRateFactor = atof( value );
		}
		else if ( !Q_stricmp( key, "powerSlideAccel" ) )
		{
			steering.powerSlideAccel = atof( value );
		}
		else if ( !Q_stricmp( key, "skidallowed" ) )
		{
			steering.isSkidAllowed = atoi( value ) != 0 ? true : false;
		}
		else if ( !Q_stricmp( key, "dustcloud" ) )
		{
			steering.dustCloud = atoi( value ) != 0 ? true : false;
		}
	}
}

void CVPhysicsParse::ParseVehicle( vehicleparams_t *pVehicle, IVPhysicsKeyHandler *unknownKeyHandler )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	if ( unknownKeyHandler )
	{
		unknownKeyHandler->SetDefaults( pVehicle );
	}
	else
	{
		memset( pVehicle, 0, sizeof(*pVehicle) );
	}

	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );
		if ( key[0] == '}' )
		{
			NextBlock();
			return;
		}

		// parse subchunks
		if ( value[0] == '{' )
		{
			if ( !Q_stricmp( key, "axle" ) )
			{
				// Protect against exploits/overruns
				if ( pVehicle->axleCount < ARRAYSIZE(pVehicle->axles) )
				{
					ParseVehicleAxle( pVehicle->axles[pVehicle->axleCount] );
					pVehicle->axleCount++;
				}
				else
				{
					Assert( 0 );
				}
			}
			else if ( !Q_stricmp( key, "body" ) )
			{
				ParseVehicleBody( pVehicle->body );
			}
			else if ( !Q_stricmp( key, "engine" ) )
			{
				ParseVehicleEngine( pVehicle->engine );
			}
			else if ( !Q_stricmp( key, "steering" ) )
			{
				ParseVehicleSteering( pVehicle->steering );
			}
			else
			{
				SkipBlock();
			}
		}
		else if ( !Q_stricmp( key, "wheelsperaxle" ) )
		{
			pVehicle->wheelsPerAxle = atoi( value );
		}
	}
}

void CVPhysicsParse::ParseCustom( void *pCustom, IVPhysicsKeyHandler *unknownKeyHandler )
{
	char key[MAX_KEYVALUE], value[MAX_KEYVALUE];

	key[0] = 0;
	int indent = 0;
	if ( unknownKeyHandler )
	{
		unknownKeyHandler->SetDefaults( pCustom );
	}

	while ( m_pText )
	{
		m_pText = ParseKeyvalue( m_pText, key, value );

		if ( m_pText )
		{
			if ( key[0] == '{' )
			{
				indent++;
			}
			else if ( value[0] == '{' )
			{
				// They've got a named block here
				// Increase our indent, and let them parse the key
				indent++;
				if ( unknownKeyHandler )
				{
					unknownKeyHandler->ParseKeyValue( pCustom, key, value );
				}
			}
			else if ( key[0] == '}' )
			{
				indent--;
				if ( indent < 0 )
				{
					NextBlock();
					return;
				}
			}
			else
			{
				if ( unknownKeyHandler )
				{
					unknownKeyHandler->ParseKeyValue( pCustom, key, value );
				}
			}
		}
	}
}

IVPhysicsKeyParser *CreateVPhysicsKeyParser( const char *pKeyData )
{
	return new CVPhysicsParse( pKeyData );
}

void DestroyVPhysicsKeyParser( IVPhysicsKeyParser *pParser )
{
	delete pParser;
}

//-----------------------------------------------------------------------------
// Helper functions for parsing script file
//-----------------------------------------------------------------------------

const char *ParseKeyvalue( const char *pBuffer, char (&key)[MAX_KEYVALUE], char (&value)[MAX_KEYVALUE] )
{
	// Make sure value is always null-terminated.
	value[0] = 0;

	pBuffer = ParseFile( pBuffer, key, NULL );

	// no value on a close brace
	if ( key[0] == '}' && key[1] == 0 )
	{
		value[0] = 0;
		return pBuffer;
	}

	Q_strlower( key );
	
	pBuffer = ParseFile( pBuffer, value, NULL );

	Q_strlower( value );

	return pBuffer;
}
