//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//


#include <KeyValues.h>
#include "filesystem.h"
#include "utldict.h"
#include "interval.h"
#include "engine/IEngineSound.h"
#include "soundemittersystembase.h"
#include "utlbuffer.h"
#include "soundchars.h"
#include "vstdlib/random.h"
#include "checksum_crc.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "ifilelist.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define MANIFEST_FILE				"scripts/game_sounds_manifest.txt"
#define GAME_SOUNDS_HEADER_BLOCK	"scripts/game_sounds_header.txt"

static IFileSystem* filesystem = 0;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSoundEmitterSystemBase::CSoundEmitterSystemBase() : 
	m_nInitCount( 0 ),
	m_uManifestPlusScriptChecksum( 0 )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int	
//-----------------------------------------------------------------------------
int	 CSoundEmitterSystemBase::First() const
{
	return m_Sounds.FirstHandle();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : i - 
// Output : int
//-----------------------------------------------------------------------------
int CSoundEmitterSystemBase::Next( int i ) const
{
	return m_Sounds.NextHandle(i);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int	CSoundEmitterSystemBase::InvalidIndex() const
{
	return m_Sounds.InvalidHandle();
}

//-----------------------------------------------------------------------------
//
// implementation of IUniformRandomStream
//
//-----------------------------------------------------------------------------
class CSoundEmitterUniformRandomStream : public IUniformRandomStream
{
public:
	// Sets the seed of the random number generator
	void	SetSeed( int iSeed )
	{
		// Never call this from the client or game!
		Assert(0);
	}

	// Generates random numbers
	float	RandomFloat( float flMinVal = 0.0f, float flMaxVal = 1.0f )
	{
		return ::RandomFloat( flMinVal, flMaxVal );
	}

	int		RandomInt( int iMinVal, int iMaxVal )
	{
		return ::RandomInt( iMinVal, iMaxVal );
	}

	float	RandomFloatExp( float flMinVal = 0.0f, float flMaxVal = 1.0f, float flExponent = 1.0f )
	{
		return ::RandomFloatExp( flMinVal, flMaxVal, flExponent );
	}

};

static CSoundEmitterUniformRandomStream g_RandomStream;
IUniformRandomStream *randomStream = &g_RandomStream;


//-----------------------------------------------------------------------------
// Connect, disconnect
//-----------------------------------------------------------------------------
bool CSoundEmitterSystemBase::Connect( CreateInterfaceFn factory )
{
	// If someone already connected us up, don't redo the connection
	if ( NULL != filesystem )
	{
		return true;
	}

	filesystem = (IFileSystem *)factory( FILESYSTEM_INTERFACE_VERSION, NULL );
	if( !filesystem )
	{
		Error( "The soundemittersystem system requires the filesystem to run!\n" );
		return false;
	}

	return true;
}


void CSoundEmitterSystemBase::Disconnect()
{
	filesystem = NULL;
}


//-----------------------------------------------------------------------------
// Query interface
//-----------------------------------------------------------------------------
void *CSoundEmitterSystemBase::QueryInterface( const char *pInterfaceName )
{
	// Loading the engine DLL mounts *all* soundemitter interfaces
	CreateInterfaceFn factory = Sys_GetFactoryThis();	// This silly construction is necessary
	return factory( pInterfaceName, NULL );				// to prevent the LTCG compiler from crashing.
}


//-----------------------------------------------------------------------------
// Init, shutdown
//-----------------------------------------------------------------------------
InitReturnVal_t CSoundEmitterSystemBase::Init()
{
	return INIT_OK;
}

void CSoundEmitterSystemBase::Shutdown()
{
}


//-----------------------------------------------------------------------------
// Purpose: Helper for checksuming script files and manifest to determine if soundname caches
//  need to be blown away.
// Input  : *crc - 
//			*filename - 
// Output : static void
//-----------------------------------------------------------------------------
static void AccumulateFileNameAndTimestampIntoChecksum( CRC32_t *crc, char const *filename )
{
	if ( IsX360() )
	{
		// this is an expensive i/o operation due to search path fall through
		// 360 doesn't need or use the checksums
		return;
	}

	long ft = filesystem->GetFileTime( filename, "GAME" );
	CRC32_ProcessBuffer( crc, &ft, sizeof( ft ) );
	CRC32_ProcessBuffer( crc, filename, Q_strlen( filename ) );
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSoundEmitterSystemBase::InternalModInit()
{
	/*
	if ( m_SoundKeyValues.Count() > 0 )
	{
		Shutdown();
	}
	*/

	LoadGlobalActors();

	m_uManifestPlusScriptChecksum = 0u;

	CRC32_t crc;
	CRC32_Init( &crc );

	KeyValues *manifest = new KeyValues( MANIFEST_FILE );
	if ( filesystem->LoadKeyValues( *manifest, IFileSystem::TYPE_SOUNDEMITTER, MANIFEST_FILE, "GAME" ) )
	{
		AccumulateFileNameAndTimestampIntoChecksum( &crc, MANIFEST_FILE );

		for ( KeyValues *sub = manifest->GetFirstSubKey(); sub != NULL; sub = sub->GetNextKey() )
		{
			if ( !Q_stricmp( sub->GetName(), "precache_file" ) )
			{
				AccumulateFileNameAndTimestampIntoChecksum( &crc, sub->GetString() );

				// Add and always precache
				AddSoundsFromFile( sub->GetString(), false );
				continue;
			}
			else if ( !Q_stricmp( sub->GetName(), "preload_file" ) )
			{
				AccumulateFileNameAndTimestampIntoChecksum( &crc, sub->GetString() );

				// Add and always precache
				AddSoundsFromFile( sub->GetString(), true );
				continue;
			}
			else if ( !Q_stricmp( sub->GetName(), "faceposer_file" ) )
			{
				// do nothing for these files; they're only used for faceposer
				continue;
			}

			Warning( "CSoundEmitterSystemBase::BaseInit:  Manifest '%s' with bogus file type '%s', expecting 'declare_file' or 'precache_file'\n", 
				MANIFEST_FILE, sub->GetName() );
		}
	}
	else
	{
		Error( "Unable to load manifest file '%s'\n", MANIFEST_FILE );
	}
	manifest->deleteThis();

	CRC32_Final( &crc );

	m_uManifestPlusScriptChecksum =( unsigned int )crc;

// Only print total once, on server
#if !defined( CLIENT_DLL ) && !defined( FACEPOSER )
	DevMsg( 1, "CSoundEmitterSystem:  Registered %i sounds\n", m_Sounds.Count() );
#endif

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSoundEmitterSystemBase::ModInit()
{
	++m_nInitCount;

	if ( m_nInitCount > 1 )
	{
		return true;
	}

	return InternalModInit();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSoundEmitterSystemBase::InternalModShutdown()
{
	int i;
	m_SoundKeyValues.RemoveAll();

	for ( UtlHashHandle_t nIndex = m_Sounds.FirstHandle(); nIndex != m_Sounds.InvalidHandle(); nIndex = m_Sounds.NextHandle( nIndex ) )
	{
		delete m_Sounds[ nIndex ];
	}

	m_Sounds.Purge();

	for ( i = 0; i < m_SavedOverrides.Count() ; ++i )
	{
		delete m_SavedOverrides[ i ];
	}
	m_SavedOverrides.Purge();
	m_Waves.RemoveAll();
	m_ActorGenders.Purge();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSoundEmitterSystemBase::ModShutdown()
{
	if ( --m_nInitCount > 0 )
		return;

	InternalModShutdown();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pName - 
//-----------------------------------------------------------------------------
int	CSoundEmitterSystemBase::GetSoundIndex( const char *pName ) const
{
	if ( !pName )
		return -1;

	CSoundEntry search;
	search.m_Name = pName;
	UtlHashHandle_t idx = m_Sounds.Find( pName );
	if ( idx == m_Sounds.InvalidHandle() )
		return -1;

	return idx;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSoundEmitterSystemBase::IsValidIndex( int index )
{
	return m_Sounds.IsValidHandle( index );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : index - 
// Output : char const
//-----------------------------------------------------------------------------
const char *CSoundEmitterSystemBase::GetSoundName( int index )
{
	if ( !IsValidIndex( index ) )
		return "";

	return m_Sounds[ index ]->m_Name.Get();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : int
//-----------------------------------------------------------------------------
int CSoundEmitterSystemBase::GetSoundCount( void )
{
	return m_Sounds.Count();
}

void CSoundEmitterSystemBase::EnsureAvailableSlotsForGender( SoundFile *pSoundnames, int c, gender_t gender )
{
	int i;
	if ( c <= 0 )
	{
		return;
	}

	CUtlVector< int > slots;

	bool needsreset = false;
	for ( i = 0; i < c; i++ )
	{
		if ( pSoundnames[ i ].gender != gender )
			continue;

		// There was at least one match for the gender
		needsreset = true;

		// This sound is unavailable
		if ( !pSoundnames[ i ].available )
			continue;

		slots.AddToTail( i );
	}

	if ( slots.Count() == 0 && needsreset )
	{
		// Reset all slots for the specified gender!!!
		for ( i = 0; i < c; i++ )
		{
			if ( pSoundnames[ i ].gender != gender )
				continue;

			pSoundnames[ i ].available = true;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : gender - 
//			soundnames - 
//-----------------------------------------------------------------------------
int	CSoundEmitterSystemBase::FindBestSoundForGender( SoundFile *pSoundnames, int c, gender_t gender )
{
	// Check for recycling of random sounds...
	EnsureAvailableSlotsForGender( pSoundnames, c, gender );

	if ( c <= 0 )
	{
		return -1;
	}

	CUtlVector< int > slots;

	for ( int i = 0; i < c; i++ )
	{
		if ( pSoundnames[ i ].gender == gender &&
			 pSoundnames[ i ].available )
		{
			slots.AddToTail( i );
		}
	}

	if ( slots.Count() >= 1 )
	{
		int idx = slots[ randomStream->RandomInt( 0, slots.Count() - 1 ) ];
		return idx;
	}

	int idx = randomStream->RandomInt( 0, c - 1 );
	return idx;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *soundname - 
//			params - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSoundEmitterSystemBase::GetParametersForSound( const char *soundname, CSoundParameters& params, gender_t gender, bool isbeingemitted /*= false*/ )
{
	HSOUNDSCRIPTHANDLE index = (HSOUNDSCRIPTHANDLE)GetSoundIndex( soundname );
	if ( index == SOUNDEMITTER_INVALID_HANDLE )
	{
		static CUtlSymbolTable soundWarnings;
		char key[ 256 ];
		Q_snprintf( key, sizeof( key ), "%s:%s", soundname, params.soundname );
		if ( UTL_INVAL_SYMBOL == soundWarnings.Find( key ) )
		{
			soundWarnings.AddString( key );

			DevMsg( "CSoundEmitterSystemBase::GetParametersForSound:  No such sound %s\n", soundname );
		}
		return false;
	}

	return GetParametersForSoundEx( soundname, index, params, gender, isbeingemitted );
}

CSoundParametersInternal *CSoundEmitterSystemBase::InternalGetParametersForSound( int index )
{
	if ( !m_Sounds.IsValidHandle( index ) )
	{
		Assert( !"CSoundEmitterSystemBase::InternalGetParametersForSound:  Bogus index" );
		return NULL;
	}

	return &m_Sounds[ index ]->m_SoundParams;
}

static void SplitName( char const *input, int splitchar, int splitlen, char *before, int beforelen, char *after, int afterlen )
{
	char const *in = input;
	char *out = before;

	int c = 0;
	int l = 0;
	int maxl = beforelen;
	while ( *in )
	{
		if ( c == splitchar )
		{
			while ( --splitlen >= 0 )
			{
				in++;
			}

			*out = 0;
			out = after;
			maxl = afterlen;
			c++;
			continue;
		}

		if ( l >= maxl )
		{
			in++;
			c++;
			continue;
		}

		*out++ = *in++;
		l++;
		c++;
	}

	*out = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : params - 
//			*wavename - 
//			gender - 
//-----------------------------------------------------------------------------
void CSoundEmitterSystemBase::AddSoundName( CSoundParametersInternal& params, char const *wavename, gender_t gender )
{
	CUtlSymbol sym = m_Waves.AddString( wavename );
	SoundFile e;
	e.symbol = sym;
	e.gender = gender;
	if ( gender != GENDER_NONE )
	{
		params.SetUsesGenderToken( true );
	}
	params.AddSoundName( e );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : params - 
//			*wavename - 
//-----------------------------------------------------------------------------
void CSoundEmitterSystemBase::ExpandSoundNameMacros( CSoundParametersInternal& params, char const *wavename )
{
	char const *p = Q_stristr( wavename, SOUNDGENDER_MACRO );

	if ( !p )
	{
		AddSoundName( params, wavename, GENDER_NONE );
		return;
	}

	int offset = p - wavename;
	Assert( offset >= 0 );
	int duration = SOUNDGENDER_MACRO_LENGTH;

	// Create a "male" and "female" version of the sound
	char before[ 256 ], after[ 256 ];
	Q_memset( before, 0, sizeof( before ) );
	Q_memset( after, 0, sizeof( after ) );

	SplitName( wavename, offset, duration, before, sizeof( before ), after, sizeof( after ) );

	char temp[ 256 ];
	Q_snprintf( temp, sizeof( temp ), "%s%s%s", before, "male", after );
	AddSoundName( params, temp, GENDER_MALE );
	Q_snprintf( temp, sizeof( temp ), "%s%s%s", before, "female", after );
	AddSoundName( params, temp, GENDER_FEMALE );

	// Add the conversion entry with the gender tags still in it
	CUtlSymbol sym = m_Waves.AddString( wavename );
	SoundFile e;
	e.symbol = sym;
	e.gender = GENDER_NONE;
	params.AddConvertedName( e );
}

void CSoundEmitterSystemBase::GenderExpandString( gender_t gender, char const *in, char *out, int maxlen )
{
	// Assume the worst
	Q_strncpy( out, in, maxlen );

	char const *p = Q_stristr( in, SOUNDGENDER_MACRO );
	if ( !p )
	{
		return;
	}

	// Look up actor gender
	if ( gender == GENDER_NONE )
	{
		return;
	}

	int offset = p - in;
	Assert( offset >= 0 );
	int duration = SOUNDGENDER_MACRO_LENGTH;

	// Create a "male" and "female" version of the sound
	char before[ 256 ], after[ 256 ];
	Q_memset( before, 0, sizeof( before ) );
	Q_memset( after, 0, sizeof( after ) );

	SplitName( in, offset, duration, before, sizeof( before ), after, sizeof( after ) );

	switch ( gender )
	{
	default:
	case GENDER_NONE:
		{
			Assert( !"CSoundEmitterSystemBase::GenderExpandString:  expecting MALE or FEMALE!" );
		}
		break;
	case GENDER_MALE:
		{
			Q_snprintf( out, maxlen, "%s%s%s", before, "male", after );
		}
		break;
	case GENDER_FEMALE:
		{
			Q_snprintf( out, maxlen, "%s%s%s", before, "female", after );
		}
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *actorname - 
//			*in - 
//			*out - 
//			maxlen - 
//-----------------------------------------------------------------------------
void CSoundEmitterSystemBase::GenderExpandString( char const *actormodel, char const *in, char *out, int maxlen )
{
	gender_t gender = GetActorGender( actormodel );
	GenderExpandString( gender, in, out, maxlen );
}

void CSoundEmitterSystemBase::LoadGlobalActors()
{
	// Now load the global actor list from the scripts/globalactors.txt file
	KeyValues *allActors = NULL;
	
	allActors = new KeyValues( "allactors" );
	if ( allActors->LoadFromFile( filesystem, "scripts/global_actors.txt", NULL ) )
	{
		KeyValues *pvkActor;
		for ( pvkActor = allActors->GetFirstSubKey(); pvkActor != NULL; pvkActor = pvkActor->GetNextKey() )
		{
			UtlHashHandle_t idx = m_ActorGenders.Find( pvkActor->GetName() );
			if ( idx == m_ActorGenders.InvalidHandle() )
			{
				if ( m_ActorGenders.Count() > 254 )
				{
					Warning( "Exceeded max number of actors in scripts/global_actors.txt\n" );
					break;
				}

				gender_t gender = GENDER_NONE;
				if ( !Q_stricmp( pvkActor->GetString(), "male" ) )
				{
					gender = GENDER_MALE;
				}
				else if (!Q_stricmp( pvkActor->GetString(), "female" ) )
				{
					gender = GENDER_FEMALE;
				}
				m_ActorGenders.Insert( pvkActor->GetName(), gender );
			}
		}
	}
	allActors->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *actorname - 
// Output : gender_t
//-----------------------------------------------------------------------------
gender_t CSoundEmitterSystemBase::GetActorGender( char const *actormodel )
{
	char actor[ 256 ];
	actor[0] = 0;
	if ( actormodel )
	{
		Q_FileBase( actormodel, actor, sizeof( actor ) );
	}

	UtlHashHandle_t idx = m_ActorGenders.Find( actor );
	if ( idx == m_ActorGenders.InvalidHandle() )
		return GENDER_NONE;

	return m_ActorGenders[ idx ];
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *soundname - 
//			params - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CSoundEmitterSystemBase::InitSoundInternalParameters( const char *soundname, KeyValues *kv, CSoundParametersInternal& params )
{
	KeyValues *pKey = kv->GetFirstSubKey();
	while ( pKey )
	{
		if ( !Q_strcasecmp( pKey->GetName(), "channel" ) )
		{
			params.ChannelFromString( pKey->GetString() );
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "volume" ) )
		{
			params.VolumeFromString( pKey->GetString() );
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "pitch" ) )
		{
			params.PitchFromString( pKey->GetString() );
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "wave" ) )
		{
			ExpandSoundNameMacros( params, pKey->GetString() );
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "rndwave" ) )
		{
			KeyValues *pWaves = pKey->GetFirstSubKey();
			while ( pWaves )
			{
				ExpandSoundNameMacros( params, pWaves->GetString() );

				pWaves = pWaves->GetNextKey();
			}
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "attenuation" ) || !Q_strcasecmp( pKey->GetName(), "CompatibilityAttenuation" ) )
		{
			if ( !Q_strncasecmp( pKey->GetString(), "SNDLVL_", strlen( "SNDLVL_" ) ) )
			{
				DevMsg( "CSoundEmitterSystemBase::GetParametersForSound:  sound %s has \"attenuation\" with %s value!\n",
					soundname, pKey->GetString() );
			}

			if ( !Q_strncasecmp( pKey->GetString(), "ATTN_", strlen( "ATTN_" ) ) )
			{
				params.SetSoundLevel( ATTN_TO_SNDLVL( TranslateAttenuation( pKey->GetString() ) ) );
			}
			else
			{
				interval_t interval;
				interval = ReadInterval( pKey->GetString() );

				// Translate from attenuation to soundlevel
				float start = interval.start;
				float end	= interval.start + interval.range;

				params.SetSoundLevel( ATTN_TO_SNDLVL( start ), ATTN_TO_SNDLVL( end ) - ATTN_TO_SNDLVL( start ) );
			}

			// Goldsrc compatibility mode.. feed the sndlevel value through the sound engine interface in such a way
			// that it can reconstruct the original sndlevel value and flag the sound as using Goldsrc attenuation.
			bool bCompatibilityAttenuation = !Q_strcasecmp( pKey->GetName(), "CompatibilityAttenuation" );
			if ( bCompatibilityAttenuation )
			{
				if ( params.GetSoundLevel().range != 0 )
				{
					Warning( "CompatibilityAttenuation for sound %s must have same start and end values.\n", soundname );
				}

				params.SetSoundLevel( SNDLEVEL_TO_COMPATIBILITY_MODE( params.GetSoundLevel().start ) );
			}
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "soundlevel" ) )
		{
			if ( !Q_strncasecmp( pKey->GetString(), "ATTN_", strlen( "ATTN_" ) ) )
			{
				DevMsg( "CSoundEmitterSystemBase::GetParametersForSound:  sound %s has \"soundlevel\" with %s value!\n",
					soundname, pKey->GetString() );
			}

			params.SoundLevelFromString( pKey->GetString() );
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "play_to_owner_only" ) )
		{
			params.SetOnlyPlayToOwner( pKey->GetInt() ? true : false );
		}
		else if ( !Q_strcasecmp( pKey->GetName(), "delay_msec" ) )
		{
			// Don't allow negative delay
			params.SetDelayMsec( max( 0, pKey->GetInt() ) );

		}

		pKey = pKey->GetNextKey();
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *soundname - 
// Output : char const
//-----------------------------------------------------------------------------
const char *CSoundEmitterSystemBase::GetWavFileForSound( const char *soundname, char const *actormodel )
{
	gender_t gender = GetActorGender( actormodel );
	return GetWavFileForSound( soundname, gender );
}

const char *CSoundEmitterSystemBase::GetWavFileForSound( const char *soundname, gender_t gender )
{
	CSoundParameters params;
	if ( !GetParametersForSound( soundname, params, gender ) )
	{
		return soundname;
	}

	if ( !params.soundname[ 0 ] )
	{
		return soundname;
	}

	static char outsound[ 512 ];
	Q_strncpy( outsound, params.soundname, sizeof( outsound ) );
	return outsound;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *soundname - 
// Output : soundlevel_t
//-----------------------------------------------------------------------------
soundlevel_t CSoundEmitterSystemBase::LookupSoundLevel( const char *soundname )
{
	CSoundParameters params;
	if ( !GetParametersForSound( soundname, params, GENDER_NONE ) )
	{
		return SNDLVL_NORM;
	}

	return params.soundlevel;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *filename - 
//-----------------------------------------------------------------------------
void CSoundEmitterSystemBase::AddSoundsFromFile( const char *filename, bool bPreload, bool bIsOverride /*=false*/, bool bRefresh /*=false*/ )
{
	CSoundScriptFile sf;
	sf.hFilename = filesystem->FindOrAddFileName( filename );
	sf.dirty = false;

	int scriptindex = m_SoundKeyValues.AddToTail( sf );

	int replaceCount = 0;
	int newOverrideCount = 0;
	int duplicatedReplacements = 0;

	// Open the soundscape data file, and abort if we can't
	KeyValues *kv = new KeyValues( "" );
	if ( filesystem->LoadKeyValues( *kv, IFileSystem::TYPE_SOUNDEMITTER, filename, "GAME" ) )
	{
		// parse out all of the top level sections and save their names
		KeyValues *pKeys = kv;
		while ( pKeys )
		{
			if ( pKeys->GetFirstSubKey() )
			{
				if ( m_Sounds.Count() >= 65534 )
				{
					Warning( "Exceeded maximum number of sound emitter entries\n" );
					break;
				}

				CSoundEntry *pEntry;

				{
					MEM_ALLOC_CREDIT();
					pEntry = new CSoundEntry;
				}

				pEntry->m_Name = pKeys->GetName();
				pEntry->m_bRemoved			= false;
				pEntry->m_nScriptFileIndex	= scriptindex;
				pEntry->m_bIsOverride		= bIsOverride;

				if ( bIsOverride )
				{
					++newOverrideCount;
				}

				UtlHashHandle_t lookup = m_Sounds.Insert( pEntry ); // insert returns existing item if found
				if ( m_Sounds[ lookup ] != pEntry )
				{
					if ( bIsOverride )
					{
						MEM_ALLOC_CREDIT();

						// Store off the old sound if it's not already an "override" from another file!!!
						// Otherwise, just whack it again!!!
						if ( !m_Sounds[ lookup ]->IsOverride() )
						{
							m_SavedOverrides.AddToTail( m_Sounds[ lookup ] );
						}
						else
						{
							++duplicatedReplacements;
						}

						InitSoundInternalParameters( pKeys->GetName(), pKeys, pEntry->m_SoundParams );
						pEntry->m_SoundParams.SetShouldPreload( bPreload ); // this gets handled by game code after initting.

						m_Sounds.ReplaceKey( lookup, pEntry );

						++replaceCount;
					}
					else if ( bRefresh )
					{
						InitSoundInternalParameters( pKeys->GetName(), pKeys, m_Sounds[ lookup ]->m_SoundParams );
					}
#if 0
					else
					{
					 	DevMsg( "CSoundEmitterSystem::AddSoundsFromFile(%s):  Entry %s duplicated, skipping\n", filename, pKeys->GetName() );
					}
#endif
				}
				else
				{
					MEM_ALLOC_CREDIT();

					InitSoundInternalParameters( pKeys->GetName(), pKeys, pEntry->m_SoundParams );
					pEntry->m_SoundParams.SetShouldPreload( bPreload ); // this gets handled by game code after initting.
				}
			}
			pKeys = pKeys->GetNextKey();
		}

		kv->deleteThis();
	}
	else
	{
		if ( !bIsOverride )
		{
			Warning( "CSoundEmitterSystem::AddSoundsFromFile:  No such file %s\n", filename );
		}

		// Discard
		m_SoundKeyValues.Remove( scriptindex );

		kv->deleteThis();

		return;
	}

	
	if ( bIsOverride )
	{
		DevMsg( "SoundEmitter:  adding map sound overrides from %s [%i total, %i replacements, %i duplicated replacements]\n", 
			filename,
			newOverrideCount,
			replaceCount,
			duplicatedReplacements );
	}

	Assert( scriptindex >= 0 );
}

//-----------------------------------------------------------------------------
// Purpose: Reload a sound emitter file (used to refresh files after sv_pure is turned on)
//-----------------------------------------------------------------------------
void CSoundEmitterSystemBase::ReloadSoundEntriesInList( IFileList *pFilesToReload )
{
	int i, c;
	c = m_SoundKeyValues.Count();
	CUtlVector< const char * > processed;
	for ( i = 0; i < c ; i++ )
	{
		const char *pszFileName = GetSoundScriptName( i );
		if ( pszFileName && pszFileName[0] )
		{
			if ( processed.Find( pszFileName) == processed.InvalidIndex() && pFilesToReload->IsFileInList( pszFileName ) )
			{
				Msg( "Reloading sound file '%s' due to pure settings.\n", pszFileName );

				AddSoundsFromFile( pszFileName, false, false, true );
				
				// Now mark this file name as being reloaded
				processed.AddToTail( pszFileName );
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: Force ModShutdown and ModInit, skips checks for how many systems have 
// requested inits (for con commands).
//-----------------------------------------------------------------------------
void CSoundEmitterSystemBase::Flush()
{
	InternalModShutdown();
	InternalModInit();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CSoundEmitterSystemBase::CheckForMissingWavFiles( bool verbose )
{
	int missing = 0;

	int c = GetSoundCount();
	int i;
	char testfile[ 512 ];

	for ( i = 0; i < c; i++ )
	{
		CSoundParametersInternal *internal = InternalGetParametersForSound( i );
		if ( !internal )
		{
			Assert( 0 );
			continue;
		}

		int waveCount = internal->NumSoundNames();
		for ( int wave = 0; wave < waveCount; wave++ )
		{
			CUtlSymbol sym = internal->GetSoundNames()[ wave ].symbol;
			const char *name = m_Waves.String( sym );
			if ( !name || !name[ 0 ] )
			{
				Assert( 0 );
				continue;
			}

			// Skip ! sentence stuff
			if ( name[0] == CHAR_SENTENCE )
				continue;
			Q_snprintf( testfile, sizeof( testfile ), "sound/%s", PSkipSoundChars( name ) );
			if ( filesystem->FileExists( testfile ) )
				continue;

			internal->SetHadMissingWaveFiles( true );

			++missing;

			if ( verbose )
			{
				DevMsg( "Sound %s references missing file %s\n", GetSoundName( i ), name );
			}
		}
	}

	return missing;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *key - 
// Output : float
//-----------------------------------------------------------------------------
float CSoundEmitterSystemBase::TranslateAttenuation( const char *key )
{
	if ( !key )
	{
		Assert( 0 );
		return ATTN_NORM;
	}

	if ( !Q_strcasecmp( key, "ATTN_NONE" ) )
		return ATTN_NONE;

	if ( !Q_strcasecmp( key, "ATTN_NORM" ) )
		return ATTN_NORM;

	if ( !Q_strcasecmp( key, "ATTN_IDLE" ) )
		return ATTN_IDLE;

	if ( !Q_strcasecmp( key, "ATTN_STATIC" ) )
		return ATTN_STATIC;

	if ( !Q_strcasecmp( key, "ATTN_RICOCHET" ) )
		return ATTN_RICOCHET;

	if ( !Q_strcasecmp( key, "ATTN_GUNFIRE" ) )
		return ATTN_GUNFIRE;

	DevMsg( "CSoundEmitterSystem:  Unknown attenuation key %s\n", key );

	return ATTN_NORM;
}



//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *key - 
// Output : soundlevel_t
//-----------------------------------------------------------------------------
soundlevel_t CSoundEmitterSystemBase::TranslateSoundLevel( const char *key )
{
	return TextToSoundLevel( key );
}

//-----------------------------------------------------------------------------
// Purpose: Convert "chan_xxx" into integer value for channel
// Input  : *name - 
// Output : static int
//-----------------------------------------------------------------------------
int CSoundEmitterSystemBase::TranslateChannel( const char *name )
{
	return TextToChannel( name );
}

const char *CSoundEmitterSystemBase::GetSourceFileForSound( int index ) const
{
	if ( index < 0 || index >= (int)m_Sounds.Count() )
	{
		Assert( 0 );
		return "";
	}

	CSoundEntry const *entry = m_Sounds[ index ];
	int scriptindex = entry->m_nScriptFileIndex;
	if ( scriptindex < 0 || scriptindex >= m_SoundKeyValues.Count() )
	{
		Assert( 0 );
		return "";
	}
	static char fn[ 512 ];
	if ( filesystem->String( m_SoundKeyValues[ scriptindex ].hFilename, fn, sizeof( fn ) ))
	{
		return fn;
	}
	Assert( 0 );
	return "";
}

const char *CSoundEmitterSystemBase::GetWaveName( CUtlSymbol& sym )
{
	return m_Waves.String( sym );
}

int	CSoundEmitterSystemBase::FindSoundScript( const char *name ) const
{
	int i, c;

	FileNameHandle_t hFilename = filesystem->FindFileName( name );
	if ( hFilename )
	{
		// First, make sure it's known
		c = m_SoundKeyValues.Count();
		for ( i = 0; i < c ; i++ )
		{
			if ( m_SoundKeyValues[ i ].hFilename == hFilename )
			{
				return i;
			}
		}
	}

	return m_SoundKeyValues.InvalidIndex();
}

bool CSoundEmitterSystemBase::AddSound( const char *soundname, const char *scriptfile, const CSoundParametersInternal& params )
{
	int idx = GetSoundIndex( soundname );


	int i = FindSoundScript( scriptfile );
	if ( i == m_SoundKeyValues.InvalidIndex() )
	{
		Warning( "CSoundEmitterSystemBase::AddSound( '%s', '%s', ... ), script file not list in manifest '%s'\n",
			soundname, scriptfile, MANIFEST_FILE );
		return false;
	}

	MEM_ALLOC_CREDIT();

	// More like an update...
	if ( IsValidIndex( idx ) )
	{
		CSoundEntry *entry = m_Sounds[ idx ];

		entry->m_bRemoved				= false;
		entry->m_nScriptFileIndex		= i;
		entry->m_SoundParams.CopyFrom( params );

		m_SoundKeyValues[ i ].dirty = true;

		return true;
	}

	CSoundEntry *pEntry = new CSoundEntry;
	pEntry->m_Name = soundname;
	pEntry->m_bRemoved			= false;
	pEntry->m_nScriptFileIndex	= i;
	pEntry->m_SoundParams.CopyFrom( params );

	m_Sounds.Insert( pEntry );

	m_SoundKeyValues[ i ].dirty = true;

	return true;
}

void CSoundEmitterSystemBase::RemoveSound( const char *soundname )
{
	int idx = GetSoundIndex( soundname );
	if ( !IsValidIndex( idx ) )
	{
		Warning( "Can't remove %s, no such sound!\n", soundname );
		return;
	}

	m_Sounds[ idx ]->m_bRemoved = true;

	// Mark script as dirty
	int scriptindex = m_Sounds[ idx ]->m_nScriptFileIndex;
	if ( scriptindex < 0 || scriptindex >= m_SoundKeyValues.Count() )
	{
		Assert( 0 );
		return;
	}

	m_SoundKeyValues[ scriptindex ].dirty = true;
}

void CSoundEmitterSystemBase::MoveSound( const char *soundname, const char *newscript )
{
	int idx = GetSoundIndex( soundname );
	if ( !IsValidIndex( idx ) )
	{
		Warning( "Can't move '%s', no such sound!\n", soundname );
		return;
	}

	int oldscriptindex = m_Sounds[ idx ]->m_nScriptFileIndex;
	if ( oldscriptindex < 0 || oldscriptindex >= m_SoundKeyValues.Count() )
	{
		Assert( 0 );
		return;
	}

	int newscriptindex = FindSoundScript( newscript );
	if ( newscriptindex == m_SoundKeyValues.InvalidIndex() )
	{
		Warning( "CSoundEmitterSystemBase::MoveSound( '%s', '%s' ), script file not list in manifest '%s'\n",
			soundname, newscript, MANIFEST_FILE );
		return;
	}

	// No actual change
	if ( oldscriptindex == newscriptindex )
	{
		return;
	}

	// Move it
	m_Sounds[ idx ]->m_nScriptFileIndex = newscriptindex;

	// Mark both scripts as dirty
	m_SoundKeyValues[ oldscriptindex ].dirty = true;
	m_SoundKeyValues[ newscriptindex ].dirty = true;
}

int CSoundEmitterSystemBase::GetNumSoundScripts() const
{
	return m_SoundKeyValues.Count();
}

const char *CSoundEmitterSystemBase::GetSoundScriptName( int index ) const
{
	if ( index < 0 || index >= m_SoundKeyValues.Count() )
		return NULL;

	static char fn[ 512 ];
	if ( filesystem->String( m_SoundKeyValues[ index ].hFilename, fn, sizeof( fn ) ) )
	{
		return fn;
	}
	return "";
}

bool CSoundEmitterSystemBase::IsSoundScriptDirty( int index ) const
{
	if ( index < 0 || index >= m_SoundKeyValues.Count() )
		return false;

	return m_SoundKeyValues[ index ].dirty;
}

void CSoundEmitterSystemBase::SaveChangesToSoundScript( int scriptindex )
{
	const char *outfile = GetSoundScriptName( scriptindex );
	if ( !outfile )
	{
		Msg( "CSoundEmitterSystemBase::SaveChangesToSoundScript:  No script file for index %i\n", scriptindex );
		return;
	}

	if ( filesystem->FileExists( outfile ) &&
		 !filesystem->IsFileWritable( outfile ) )
	{
		Warning( "%s is not writable, can't save data to file\n", outfile );
		return;
	}

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	// FIXME:  Write sound script header
	if ( filesystem->FileExists( GAME_SOUNDS_HEADER_BLOCK ) )
	{
		FileHandle_t header = filesystem->Open( GAME_SOUNDS_HEADER_BLOCK, "rb", NULL );
		if ( header != FILESYSTEM_INVALID_HANDLE )
		{
			int len = filesystem->Size( header );
			
			unsigned char *data = new unsigned char[ len + 1 ];
			Q_memset( data, 0, len + 1 );
			
			filesystem->Read( data, len, header );
			filesystem->Close( header );

			data[ len ] = 0;

			char *p = (char *)data;
			while ( *p )
			{
				if ( *p != '\r' )
				{
					buf.PutChar( *p );
				}
				++p;
			}

			delete[] data;
		}

		buf.Printf( "\n" );
	}


	int c = GetSoundCount();
	for ( int i = 0; i < c; i++ )
	{
		if ( Q_stricmp( outfile, GetSourceFileForSound( i ) ) )
			continue;

		// It's marked for deletion, just skip it
		if ( m_Sounds[ i ]->m_bRemoved )
			continue;

		CSoundParametersInternal *p = InternalGetParametersForSound( i );
		if ( !p )
			continue;
		
		buf.Printf( "\"%s\"\n{\n", GetSoundName( i ) );

		buf.Printf( "\t\"channel\"\t\t\"%s\"\n", p->ChannelToString() );
		buf.Printf( "\t\"volume\"\t\t\"%s\"\n", p->VolumeToString() );
		buf.Printf( "\t\"pitch\"\t\t\t\"%s\"\n", p->PitchToString() );
		buf.Printf( "\n" );
		buf.Printf( "\t\"soundlevel\"\t\"%s\"\n", p->SoundLevelToString() );

		if ( p->OnlyPlayToOwner() )
		{
			buf.Printf( "\t\"play_to_owner_only\"\t\"1\"\n" );
		}

		if ( p->GetDelayMsec() != 0 )
		{
			buf.Printf( "\t\"delay_msec\"\t\"%i\"\n", p->GetDelayMsec() );
		}

		int totalCount = 0;

		int waveCount = p->NumSoundNames();
		int convertedCount = p->NumConvertedNames();

		totalCount = ( waveCount - 2 * convertedCount ) + convertedCount;

		if  ( totalCount > 0 )
		{
			buf.Printf( "\n" );

			if ( waveCount == 1 )
			{
				Assert( p->GetSoundNames()[ 0 ].gender == GENDER_NONE );
				buf.Printf( "\t\"wave\"\t\t\t\"%s\"\n", GetWaveName( p->GetSoundNames()[ 0 ].symbol ) );
			}
			else if ( convertedCount == 1 )
			{
				Assert( p->GetConvertedNames()[ 0 ].gender == GENDER_NONE );
				buf.Printf( "\t\"wave\"\t\t\t\"%s\"\n", GetWaveName( p->GetConvertedNames()[ 0 ].symbol ) );
			}
			else
			{
				buf.Printf( "\t\"rndwave\"\n" );
				buf.Printf( "\t{\n" );

				int wave;
				for ( wave = 0; wave < waveCount; wave++ )
				{
					// Skip macro-expanded names
					if ( p->GetSoundNames()[ wave ].gender != GENDER_NONE )
						continue;

					buf.Printf( "\t\t\"wave\"\t\"%s\"\n", GetWaveName( p->GetSoundNames()[ wave ].symbol ) );
				}
				for ( wave = 0; wave < convertedCount; wave++ )
				{
					buf.Printf( "\t\t\"wave\"\t\"%s\"\n", GetWaveName( p->GetConvertedNames()[ wave ].symbol ) );
				}

				buf.Printf( "\t}\n" );
			}

		}

		buf.Printf( "}\n" );

		if ( i != c - 1 )
		{
			buf.Printf( "\n" );
		}
	}

	// Write it out baby
	FileHandle_t fh = filesystem->Open( outfile, "wt" );
	if (fh)
	{
		filesystem->Write( buf.Base(), buf.TellPut(), fh );
		filesystem->Close(fh);

		// Changed saved successfully
		m_SoundKeyValues[ scriptindex ].dirty = false;
	}
	else
	{
		Warning( "SceneManager_SaveSoundsToScriptFile:  Unable to write file %s!!!\n", outfile );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : CUtlSymbol
//-----------------------------------------------------------------------------
CUtlSymbol CSoundEmitterSystemBase::AddWaveName( const char *name )
{
	return m_Waves.AddString( name );
}

void CSoundEmitterSystemBase::RenameSound( const char *soundname, const char *newname )
{
	// Same name?
	if ( !Q_stricmp( soundname, newname ) )
	{
		return;
	}

	int oldindex = GetSoundIndex( soundname );
	if ( !IsValidIndex( oldindex ) )
	{
		Msg( "Can't rename %s, no such sound\n", soundname );
		return;
	}

	int check = GetSoundIndex( newname );
	if ( IsValidIndex( check ) )
	{
		Msg( "Can't rename %s to %s, new name already in list\n", soundname, newname );
		return;
	}

	MEM_ALLOC_CREDIT();

	// Copy out old entry
	CSoundEntry *pEntry = m_Sounds[ oldindex ];
	// Remove it
	m_Sounds.Remove( pEntry );
	pEntry->m_Name = newname;
	// Re-insert in new spot
	m_Sounds.Insert( pEntry );

	// Mark associated script as dirty
	m_SoundKeyValues[ pEntry->m_nScriptFileIndex ].dirty = true;
}

void CSoundEmitterSystemBase::UpdateSoundParameters( const char *soundname, const CSoundParametersInternal& params )
{
	int idx = GetSoundIndex( soundname );
	if ( !IsValidIndex( idx ) )
	{
		Msg( "Can't UpdateSoundParameters %s, no such sound\n", soundname );
		return;
	}

	CSoundEntry *entry = m_Sounds[ idx ];

	if ( entry->m_SoundParams == params )
	{
		// No changes
		return;
	}

	// Update parameters
	entry->m_SoundParams.CopyFrom( params );
	// Set dirty flag
	m_SoundKeyValues[ entry->m_nScriptFileIndex ].dirty = true;
}

bool CSoundEmitterSystemBase::IsUsingGenderToken( char const *soundname )
{
	int soundindex = GetSoundIndex( soundname );
	if ( soundindex < 0 )
		return false;

	// Look up the sound level from the soundemitter system
	CSoundParametersInternal *params = InternalGetParametersForSound( soundindex );
	if ( !params )
		return false;

	return params->UsesGenderToken();
}
unsigned int CSoundEmitterSystemBase::GetManifestFileTimeChecksum()
{
	return m_uManifestPlusScriptChecksum;
}

bool CSoundEmitterSystemBase::GetParametersForSoundEx( const char *soundname, HSOUNDSCRIPTHANDLE& handle, CSoundParameters& params, gender_t gender, bool isbeingemitted /*= false*/ )
{
	if ( handle == SOUNDEMITTER_INVALID_HANDLE )
	{
		handle = GetSoundIndex( soundname );
		if ( handle == SOUNDEMITTER_INVALID_HANDLE )
			return false;
	}

	CSoundParametersInternal *internal = InternalGetParametersForSound( (int)handle );
	if ( !internal )
	{
		Assert( 0 );
		DevMsg( "CSoundEmitterSystemBase::GetParametersForSound:  No such sound %s\n", soundname );
		return false;
	}

	params.channel = internal->GetChannel();
	params.volume = internal->GetVolume().Random();
	params.pitch = internal->GetPitch().Random();
	params.pitchlow = internal->GetPitch().start;
	params.pitchhigh = params.pitchlow + internal->GetPitch().range;
	params.delay_msec = internal->GetDelayMsec();
	params.count = internal->NumSoundNames();
	params.soundname[ 0 ] = 0;

	int bestIndex = FindBestSoundForGender( internal->GetSoundNames(), internal->NumSoundNames(), gender );

	if ( bestIndex >= 0 )
	{
		Q_strncpy( params.soundname, GetWaveName( internal->GetSoundNames()[ bestIndex ].symbol), sizeof( params.soundname ) );

		// If we are actually emitting the sound, mark it as not available...
		if ( isbeingemitted )
		{
			internal->GetSoundNames()[ bestIndex ].available = 0;
		}
	}
	params.soundlevel = (soundlevel_t)(int)internal->GetSoundLevel().Random();
	params.play_to_owner_only = internal->OnlyPlayToOwner();

	if ( !params.soundname[ 0 ] )
	{
		DevMsg( "CSoundEmitterSystemBase::GetParametersForSound:  sound %s has no wave or rndwave key!\n", soundname );
		return false;
	}

	if ( internal->HadMissingWaveFiles() &&
		params.soundname[ 0 ] != CHAR_SENTENCE )
	{
		char testfile[ 256 ];
		Q_snprintf( testfile, sizeof( testfile ), "sound/%s", PSkipSoundChars( params.soundname ) );
		if ( !filesystem->FileExists( testfile ) )
		{
			// Prevent repetitive spew...
			static CUtlSymbolTable soundWarnings;
			char key[ 256 ];
			Q_snprintf( key, sizeof( key ), "%s:%s", soundname, params.soundname );
			if ( UTL_INVAL_SYMBOL == soundWarnings.Find( key ) )
			{
				soundWarnings.AddString( key );
			
				DevMsg( "CSoundEmitterSystemBase::GetParametersForSound:  sound '%s' references wave '%s' which doesn't exist on disk!\n", 
					soundname,
					params.soundname );
			}
			return false;
		}
	}

	return true;
}

soundlevel_t CSoundEmitterSystemBase::LookupSoundLevelByHandle( char const *soundname, HSOUNDSCRIPTHANDLE& handle )
{
	if ( handle == SOUNDEMITTER_INVALID_HANDLE )
	{
		handle = (HSOUNDSCRIPTHANDLE)GetSoundIndex( soundname );
		if ( handle == SOUNDEMITTER_INVALID_HANDLE )
			return SNDLVL_NORM;
	}

	CSoundParametersInternal *internal = InternalGetParametersForSound( (int)handle );
	if ( !internal )
	{
		return SNDLVL_NORM;
	}

	return (soundlevel_t)(int)internal->GetSoundLevel().Random();
}


// Called from both client and server (single player) or just one (server only in dedicated server and client only if connected to a remote server)
// Called by LevelInitPreEntity to override sound scripts for the mod with level specific overrides based on custom mapnames, etc.
void CSoundEmitterSystemBase::AddSoundOverrides( char const *scriptfile, bool bPreload /*= false*/ )
{
	FileNameHandle_t handle = filesystem->FindOrAddFileName( scriptfile );
	if ( m_OverrideFiles.Find( handle ) != m_OverrideFiles.InvalidIndex() )
		return;

	m_OverrideFiles.AddToTail( handle );
	// These are overrides
	AddSoundsFromFile( scriptfile, bPreload, true );
}

// Called by either client or server in LevelShutdown to clear out custom overrides
void CSoundEmitterSystemBase::ClearSoundOverrides()
{
	int i;
	int removed = 0;

	for ( UtlHashHandle_t i = m_Sounds.FirstHandle(); i != m_Sounds.InvalidHandle(); )
	{
		CSoundEntry *entry = m_Sounds[ i ];
		if ( entry->IsOverride() )
		{
			i = m_Sounds.RemoveAndAdvance( i );
			++removed;
		}
		else
		{
			i = m_Sounds.NextHandle( i );
		}
	}

	if (removed > 0 || m_SavedOverrides.Count() > 0 )
	{
		Warning( "SoundEmitter:  removing map sound overrides [%i to remove, %i to restore]\n", 
			removed,
			m_SavedOverrides.Count() );
	}

	// Now restore the original entries into the main dictionary.
	for ( i = 0; i < m_SavedOverrides.Count(); ++i )
	{
		CSoundEntry *entry = m_SavedOverrides[ i ];
		m_Sounds.Insert( entry );
	}

	m_SavedOverrides.Purge();
	m_OverrideFiles.Purge();
}

CSoundEmitterSystemBase g_SoundEmitterSystemBase;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CSoundEmitterSystemBase, ISoundEmitterSystemBase, 
						SOUNDEMITTERSYSTEM_INTERFACE_VERSION, g_SoundEmitterSystemBase );