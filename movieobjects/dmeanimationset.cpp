//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmeanimationset.h"
#include "movieobjects/dmebookmark.h"
#include "movieobjects/dmegamemodel.h"
#include "movieobjects/dmecombinationoperator.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "datamodel/dmehandle.h"
#include "phonemeconverter.h"
#include "tier1/utlstringmap.h"
#include "tier2/tier2.h"
#include "filesystem.h"
#include "studio.h"
#include "tier3/tier3.h"
#include "tier1/utlbuffer.h"

//-----------------------------------------------------------------------------
// CDmePresetGroup - container for animation set info
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmePreset, CDmePreset );

void CDmePreset::OnConstruction()
{
	m_ControlValues.Init( this, "controlValues" );
	m_nProceduralType.InitAndSet( this, "procedural", PROCEDURAL_PRESET_NOT );
}

void CDmePreset::OnDestruction()
{
}

CDmaElementArray< CDmElement > &CDmePreset::GetControlValues()
{
	return m_ControlValues;
}

const CDmaElementArray< CDmElement > &CDmePreset::GetControlValues() const
{
	return m_ControlValues;
}

int CDmePreset::FindControlValueIndex( const char *pControlName )
{
	int c = m_ControlValues.Count();
	for ( int i = 0; i < c; ++i )
	{
		CDmElement *e = m_ControlValues.Get( i );
		if ( !Q_stricmp( e->GetName(), pControlName ) )
			return i;
	}
	return -1;
}

CDmElement *CDmePreset::FindControlValue( const char *pControlName )
{
	int i = FindControlValueIndex( pControlName );
	if ( i >= 0 )
		return m_ControlValues.Get(i);
	return NULL;
}

CDmElement *CDmePreset::FindOrAddControlValue( const char *pControlName )
{
	CDmElement *pControlValues = FindControlValue( pControlName );
	if ( !pControlValues )
	{
		// Create the default groups in order
		pControlValues = CreateElement< CDmElement >( pControlName, GetFileId() );
		m_ControlValues.AddToTail( pControlValues );
	}
	return pControlValues;
}

void CDmePreset::RemoveControlValue( const char *pControlName )
{
	int i = FindControlValueIndex( pControlName );
	if ( i >= 0 )
	{
		m_ControlValues.Remove( i );
	}
}


//-----------------------------------------------------------------------------
// Is the preset read-only?
//-----------------------------------------------------------------------------
bool CDmePreset::IsReadOnly()
{
	DmAttributeReferenceIterator_t h = g_pDataModel->FirstAttributeReferencingElement( GetHandle() );
	while ( h != DMATTRIBUTE_REFERENCE_ITERATOR_INVALID )
	{
		CDmAttribute *pAttribute = g_pDataModel->GetAttribute( h );
		CDmePresetGroup *pOwner = CastElement<CDmePresetGroup>( pAttribute->GetOwner() );
		if ( pOwner && pOwner->m_bIsReadOnly )
			return true;
		h = g_pDataModel->NextAttributeReferencingElement( h );
	}
	return false;
}


//-----------------------------------------------------------------------------
// Copies control values
//-----------------------------------------------------------------------------
void CDmePreset::CopyControlValuesFrom( CDmePreset *pSource )
{
	m_ControlValues.RemoveAll();

	const CDmaElementArray< CDmElement > &sourceValues = pSource->GetControlValues();
	int nCount = sourceValues.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmElement *pCopy = sourceValues[i]->Copy( );
		m_ControlValues.AddToTail( pCopy );
	}
}

void CDmePreset::SetProceduralPresetType( int nType )
{
	Assert( nType >= 0 && nType < NUM_PROCEDURAL_PRESET_TYPES );
	m_nProceduralType = nType;
}

bool CDmePreset::IsProcedural() const
{
	return m_nProceduralType != PROCEDURAL_PRESET_NOT;
}

int CDmePreset::GetProceduralPresetType() const
{
	return m_nProceduralType;
}

IMPLEMENT_ELEMENT_FACTORY( DmeProceduralPresetSettings, CDmeProceduralPresetSettings );

void CDmeProceduralPresetSettings::OnConstruction()
{
	m_flJitterScale.InitAndSet( this, "jitterscale", 1.0f );
	m_flSmoothScale.InitAndSet( this, "smoothscale", 1.0f );
	m_flSharpenScale.InitAndSet( this, "sharpenscale", 1.0f );
	m_flSoftenScale.InitAndSet( this, "softenscale", 1.0f );

	m_nJitterIterations.InitAndSet( this, "jitteriterations", 5 );
	m_nSmoothIterations.InitAndSet( this, "smoothiterations", 5 );
	m_nSharpenIterations.InitAndSet( this, "sharpeniterations", 1 );
	m_nSoftenIterations.InitAndSet( this, "softeniterations", 1 );

	// 1/12 second now ( 833 ten thousandths )
	m_nStaggerInterval.InitAndSet( this, "staggerinterval", 10000 / 12 );
}

void CDmeProceduralPresetSettings::OnDestruction()
{
}

//-----------------------------------------------------------------------------
// CDmePresetRemap - copies presets from one group to another
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmePresetRemap, CDmePresetRemap );

void CDmePresetRemap::OnConstruction()
{
	m_SourcePresetGroup.Init( this, "sourcePresetGroup" );
	m_SrcPresets.Init( this, "srcPresets" );
	m_DestPresets.Init( this, "destPresets" );
}

void CDmePresetRemap::OnDestruction()
{
}


const char *CDmePresetRemap::FindSourcePreset( const char *pDestPresetName )
{
	int nCount = m_DestPresets.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( pDestPresetName, m_DestPresets[i] ) )
			return m_SrcPresets[i];
	}
	return NULL;
}

void CDmePresetRemap::AddRemap( const char *pSourcePresetName, const char *pDestPresetName )
{
	m_SrcPresets.AddToTail( pSourcePresetName );
	m_DestPresets.AddToTail( pDestPresetName );
}

void CDmePresetRemap::RemoveAll()
{
	m_SrcPresets.RemoveAll();
	m_DestPresets.RemoveAll();
}


//-----------------------------------------------------------------------------
// Iteration
//-----------------------------------------------------------------------------
int CDmePresetRemap::GetRemapCount()
{
	return m_SrcPresets.Count();
}

const char *CDmePresetRemap::GetRemapSource( int i )
{
	return m_SrcPresets[i];
}

const char *CDmePresetRemap::GetRemapDest( int i )
{
	return m_DestPresets[i];
}


//-----------------------------------------------------------------------------
// CDmePresetGroup - container for animation set info
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmePresetGroup, CDmePresetGroup );

void CDmePresetGroup::OnConstruction()
{
	m_Presets.Init( this, "presets" );
	m_bIsVisible.InitAndSet( this, "visible", true );
	m_bIsReadOnly.Init( this, "readonly" );
}

void CDmePresetGroup::OnDestruction()
{
}

CDmaElementArray< CDmePreset > &CDmePresetGroup::GetPresets()
{
	return m_Presets;
}

const CDmaElementArray< CDmePreset > &CDmePresetGroup::GetPresets() const
{
	return m_Presets;
}

//-----------------------------------------------------------------------------
// Finds the index of a particular preset group
//-----------------------------------------------------------------------------
int CDmePresetGroup::FindPresetIndex( CDmePreset *pPreset )
{
	int c = m_Presets.Count();
	for ( int i = 0; i < c; ++i )
	{
		CDmePreset *e = m_Presets.Get( i );
		if ( pPreset == e )
			return i;
	}
	return -1; 
}


CDmePreset *CDmePresetGroup::FindPreset( const char *pPresetName )
{
	int i;
	int c = m_Presets.Count();
	for ( i = 0; i < c; ++i )
	{
		CDmePreset *e = m_Presets.Get( i );
		if ( !Q_stricmp( e->GetName(), pPresetName ) )
			return e;
	}
	return NULL;
}

CDmePreset *CDmePresetGroup::FindOrAddPreset( const char *pPresetName, int nType /*=PROCEDURAL_PRESET_NOT*/ )
{
	CDmePreset *pPreset = FindPreset( pPresetName );
	if ( !pPreset )
	{
		// Create the default groups in order
		pPreset = CreateElement< CDmePreset >( pPresetName, GetFileId() );
		pPreset->SetProceduralPresetType( nType );
		m_Presets.AddToTail( pPreset );
	}
	return pPreset;
}

bool CDmePresetGroup::RemovePreset( CDmePreset *pPreset )
{
	int i = FindPresetIndex( pPreset );
	if ( i >= 0 )
	{
		m_Presets.Remove( i );
		return true;
	}
	return false;
}

void CDmePresetGroup::MovePresetUp( CDmePreset *pPreset )
{
	int i = FindPresetIndex( pPreset );
	if ( i >= 1 )
	{
		m_Presets.Swap( i, i-1 );
	}
}

void CDmePresetGroup::MovePresetDown( CDmePreset *pPreset )
{
	int i = FindPresetIndex( pPreset );
	if ( i >= 0 && i < m_Presets.Count() - 1 )
	{
		m_Presets.Swap( i, i+1 );
	}
}


//-----------------------------------------------------------------------------
// Reorder presets
//-----------------------------------------------------------------------------
void CDmePresetGroup::MovePresetInFrontOf( CDmePreset *pPreset, CDmePreset *pInFrontOf )
{
	if ( pPreset == pInFrontOf )
		return;

	int nEnd = pInFrontOf ? FindPresetIndex( pInFrontOf ) : m_Presets.Count();
	Assert( nEnd >= 0 );
	 
	RemovePreset( pPreset );
	if ( nEnd > m_Presets.Count() )
	{
		nEnd = m_Presets.Count();
	}
	m_Presets.InsertBefore( nEnd, pPreset );
}


//-----------------------------------------------------------------------------
// The preset remap
//-----------------------------------------------------------------------------
CDmePresetRemap *CDmePresetGroup::GetPresetRemap()
{
	return GetValueElement< CDmePresetRemap >( "presetRemap" );
}

CDmePresetRemap *CDmePresetGroup::GetOrAddPresetRemap()
{
	CDmePresetRemap *pPresetRemap = GetPresetRemap();
	if ( !pPresetRemap )
	{
		pPresetRemap = CreateElement< CDmePresetRemap >( "PresetRemap", GetFileId() );
		SetValue( "presetRemap", pPresetRemap );
	}
	return pPresetRemap;
}



//-----------------------------------------------------------------------------
// Finds a control index
//-----------------------------------------------------------------------------
struct ExportedControl_t
{
	CUtlString m_Name;
	bool m_bIsStereo;
	bool m_bIsMulti;
	int m_nFirstIndex;
};


//-----------------------------------------------------------------------------
// Builds a unique list of controls found in the presets
//-----------------------------------------------------------------------------
static int FindExportedControlIndex( const char *pControlName, CUtlVector< ExportedControl_t > &uniqueControls )
{
	int nCount = uniqueControls.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( pControlName, uniqueControls[i].m_Name ) )
			return i;
	}
	return -1;
}


//-----------------------------------------------------------------------------
// Builds a unique list of controls found in the presets
//-----------------------------------------------------------------------------
static int BuildExportedControlList( CDmeAnimationSet *pAnimationSet, const CDmePresetGroup *pPresetGroup, CUtlVector< ExportedControl_t > &uniqueControls )
{
	int nGlobalIndex = 0;
	const CDmrElementArrayConst< CDmePreset > &presets = pPresetGroup->GetPresets();
	int nPresetCount = presets.Count();
	for ( int iPreset = 0; iPreset < nPresetCount; ++iPreset )
	{
		CDmePreset *pPreset = presets[iPreset];
		const CDmrElementArray< CDmElement > &controls = pPreset->GetControlValues();

		int nControlCount = controls.Count();
		for ( int i = 0; i < nControlCount; ++i )
		{
			const char *pControlName = controls[i]->GetName();
			int nIndex = FindExportedControlIndex( pControlName, uniqueControls );
			if ( nIndex >= 0 )
				continue;
			CDmAttribute *pValueAttribute = controls[i]->GetAttribute( "value" );
			if ( !pValueAttribute || pValueAttribute->GetType() != AT_FLOAT )
				continue;

			if ( pAnimationSet )
			{

				CDmElement *pControl = pAnimationSet->FindControl( pControlName );
				if ( !pControl )
					continue;

				int j = uniqueControls.AddToTail();
				ExportedControl_t &control = uniqueControls[j];
				control.m_Name = pControlName;
				control.m_bIsStereo = pControl->GetValue<bool>( "combo" );
				control.m_bIsMulti = pControl->GetValue<bool>( "multi" );
				control.m_nFirstIndex = nGlobalIndex;
				nGlobalIndex += 1 + control.m_bIsStereo + control.m_bIsMulti;
			}
			else
			{
				int j = uniqueControls.AddToTail();
				ExportedControl_t &control = uniqueControls[j];
				control.m_Name = pControlName;
				// this isn't quite as reliable as querying the animation set but if we don't have one...
				control.m_bIsStereo = controls[ i ]->GetAttribute( "balance" ) ? true : false;
				control.m_bIsMulti = controls[ i ]->GetAttribute( "multilevel" ) ? true : false;
				control.m_nFirstIndex = nGlobalIndex;
				nGlobalIndex += 1 + control.m_bIsStereo + control.m_bIsMulti;
			}
		}
	}
	return nGlobalIndex;
}


//-----------------------------------------------------------------------------
// Exports this preset group to a faceposer .txt expression file
// Either an animation set or a combination operator are required so that
// the default value for unspecified 
//-----------------------------------------------------------------------------
bool CDmePresetGroup::ExportToTXT( const char *pFileName, CDmeAnimationSet *pAnimationSet /* = NULL */, CDmeCombinationOperator *pComboOp /* = NULL */ ) const
{
	const CDmePresetGroup *pPresetGroup = this;

	// find all used controls
	CUtlVector< ExportedControl_t > exportedControls;
	BuildExportedControlList( pAnimationSet, pPresetGroup, exportedControls );

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );

	// Output the unique keys
	buf.Printf( "$keys " );
	int nExportedControlCount = exportedControls.Count();
	for ( int i = 0; i < nExportedControlCount; ++i )
	{
		char pTempBuf[MAX_PATH];

		ExportedControl_t &control = exportedControls[i];
		if ( !control.m_bIsStereo )
		{
			buf.Printf("%s ", control.m_Name.Get() );
		}
		else
		{
			Q_snprintf( pTempBuf, sizeof(pTempBuf), "right_%s", control.m_Name.Get() );
			buf.Printf("%s ", pTempBuf );
			Q_snprintf( pTempBuf, sizeof(pTempBuf), "left_%s", control.m_Name.Get() );
			buf.Printf("%s ", pTempBuf );
		}

		if ( control.m_bIsMulti )
		{
			Q_snprintf( pTempBuf, sizeof(pTempBuf), "multi_%s", control.m_Name.Get() );
			buf.Printf("%s ", pTempBuf );
		}
	}
	buf.Printf( "\n" );
	buf.Printf( "$hasweighting\n" );
	buf.Printf( "$normalized\n" );

	// Output all presets
	const CDmrElementArrayConst< CDmePreset > &presets = pPresetGroup->GetPresets();
	int nPresetCount = presets.Count();
	for ( int iPreset = 0; iPreset < nPresetCount; ++iPreset )
	{
		CDmePreset *pPreset = presets[iPreset];
		const char *pPresetName = pPreset->GetName();

		// Hack for 'silence' and for p_ naming scheme
		if ( !Q_stricmp( pPresetName, "p_silence" ) )
		{
			pPresetName = "<sil>";
		}
		if ( pPresetName[0] == 'p' && pPresetName[1] == '_' )
		{
			pPresetName = &pPresetName[2];
		}

		buf.Printf( "\"%s\" \t", pPresetName );

		int nPhonemeIndex = TextToPhonemeIndex( pPresetName );
		int nCode = CodeForPhonemeByIndex( nPhonemeIndex );
		if ( nCode < 128 )
		{
			buf.Printf( "\"%c\" \t", nCode );
		}
		else
		{
			buf.Printf( "\"0x%x\"\t", nCode );
		}

		for ( int i = 0; i < nExportedControlCount; ++i )
		{
			ExportedControl_t &control = exportedControls[i];
			CDmElement *pControlValue = pPreset->FindControlValue( control.m_Name );
			if ( !pControlValue )
			{
				CDmElement *pControl = pAnimationSet ? pAnimationSet->FindControl( control.m_Name ) : NULL;
				if ( !pControl )
				{
					const ControlIndex_t nIndex = pComboOp ? pComboOp->FindControlIndex( control.m_Name ) : -1;
					if ( nIndex >= 0 )
					{
						buf.Printf( "%.5f\t0.000\t", pComboOp->GetControlDefaultValue( nIndex ) );
						if ( control.m_bIsStereo )
						{
							buf.Printf( "%.5f\t0.000\t", pComboOp->GetControlDefaultValue( nIndex ) );
						}
						if ( control.m_bIsMulti )
						{
							buf.Printf( "%.5f\t0.000\t", pComboOp->GetControlDefaultValue( nIndex ) );
						}
					}
					else
					{
						buf.Printf( "0.000\t0.000\t" );
						if ( control.m_bIsStereo )
						{
							buf.Printf( "0.000\t0.000\t" );
						}
						if ( control.m_bIsMulti )
						{
							buf.Printf( "0.000\t0.000\t" );
						}
					}

					continue;
				}

				if ( !control.m_bIsStereo )
				{
					buf.Printf( "%.5f\t1.000\t", pControl->GetValue<float>( "defaultValue" ) );
				}
				else
				{
					float flValue, flBalance, flLeft, flRight;
					flValue = pControl->GetValue<float>( "defaultValue", 0.0f );
					flBalance = pControl->GetValue<float>( "defaultBalance", 0.5f );
					ValueBalanceToLeftRight( &flLeft, &flRight, flValue, flBalance );
					buf.Printf( "%.5f\t1.000\t", flRight );
					buf.Printf( "%.5f\t1.000\t", flLeft );
				}

				if ( control.m_bIsMulti )
				{
					buf.Printf( "%.5f\t1.000\t", pControl->GetValue<float>( "defaultMultilevel" ) );
				}
				continue;
			}

			if ( !control.m_bIsStereo )
			{
				buf.Printf( "%.5f\t1.000\t", pControlValue->GetValue<float>( "value" ) );
			}
			else
			{
				float flValue, flBalance, flLeft, flRight;
				flValue = pControlValue->GetValue<float>( "value" );
				flBalance = pControlValue->GetValue<float>( "balance" );
				ValueBalanceToLeftRight( &flLeft, &flRight, flValue, flBalance );
				buf.Printf( "%.5f\t1.000\t", flRight );
				buf.Printf( "%.5f\t1.000\t", flLeft );
			}

			if ( control.m_bIsMulti )
			{
				buf.Printf( "%.5f\t1.000\t", pControlValue->GetValue<float>( "multilevel" ) );
			}
		}
		const char *pDesc = DescForPhonemeByIndex( nPhonemeIndex );
		buf.Printf( "\"%s\"\n", pDesc ? pDesc : pPresetName );
	}

	return g_pFullFileSystem->WriteFile( pFileName, NULL, buf );
}

#ifdef ALIGN4
#undef ALIGN4
#endif // #ifdef ALIGN4
#define ALIGN4( a ) a = (byte *)((int)((byte *)a + 3) & ~ 3)


//-----------------------------------------------------------------------------
// Exports this preset group to a faceposer .vfe expression file
//-----------------------------------------------------------------------------
bool CDmePresetGroup::ExportToVFE( const char *pFileName, CDmeAnimationSet *pAnimationSet /* = NULL */, CDmeCombinationOperator *pComboOp /* = NULL */ ) const
{	  
	const CDmePresetGroup *pPresetGroup = this;

	int i;
	const CDmrElementArrayConst< CDmePreset > &presets = pPresetGroup->GetPresets();

	// find all used controls
	CUtlVector< ExportedControl_t > exportedControls;
	int nTotalControlCount = BuildExportedControlList( pAnimationSet, pPresetGroup, exportedControls );
	const int nExportedControlCount = exportedControls.Count();

	byte *pData = (byte *)calloc( 1024 * 1024, 1 );
	byte *pDataStart = pData;

	flexsettinghdr_t *fhdr = (flexsettinghdr_t *)pData;

	fhdr->id = ('V' << 16) + ('F' << 8) + ('E');
	fhdr->version = 0;
	if ( !g_pFullFileSystem->FullPathToRelativePathEx( pFileName, "GAME", fhdr->name, sizeof(fhdr->name) ) )
	{
		Q_strncpy( fhdr->name, pFileName, sizeof(fhdr->name) );
	}

	// allocate room for header
	pData += sizeof( flexsettinghdr_t );
	ALIGN4( pData );

	// store flex settings
	flexsetting_t *pSetting = (flexsetting_t *)pData;
	fhdr->numflexsettings = presets.Count();
	fhdr->flexsettingindex = pData - pDataStart;
	pData += sizeof( flexsetting_t ) * fhdr->numflexsettings;
	ALIGN4( pData );
	for ( i = 0; i < fhdr->numflexsettings; i++ )
	{
		CDmePreset *pPreset = presets[i];
		Assert( pPreset );

		pSetting[i].index = i;
		pSetting[i].settingindex = pData - (byte *)(&pSetting[i]);

		flexweight_t *pFlexWeights = (flexweight_t *)pData;

		for ( int j = 0; j < nExportedControlCount; j++ )
		{
			ExportedControl_t &control = exportedControls[ j ];
			CDmElement *pControlValue = pPreset->FindControlValue( control.m_Name );
			if ( !pControlValue )
			{
				const ControlIndex_t nIndex = pComboOp ? pComboOp->FindControlIndex( control.m_Name ) : -1;
				if ( nIndex >= 0 )
				{
					if ( !control.m_bIsStereo )
					{
						pSetting[i].numsettings++;
						pFlexWeights->key = control.m_nFirstIndex;
						pFlexWeights->weight = pComboOp->GetControlDefaultValue( nIndex );
						pFlexWeights->influence = 1.0f;
						pFlexWeights++;
					}
					else
					{
						float flValue, flBalance, flLeft, flRight;
						flValue = pComboOp->GetControlDefaultValue( nIndex );
						flBalance = 0.5;
						ValueBalanceToLeftRight( &flLeft, &flRight, flValue, flBalance );

						pSetting[i].numsettings += 2;
						pFlexWeights->key = control.m_nFirstIndex;
						pFlexWeights->weight = flRight;
						pFlexWeights->influence = 1.0f;
						pFlexWeights++;
						pFlexWeights->key = control.m_nFirstIndex + 1;
						pFlexWeights->weight = flLeft;
						pFlexWeights->influence = 1.0f;
						pFlexWeights++;
					}

					if ( control.m_bIsMulti )
					{
						pSetting[i].numsettings++;
						pFlexWeights->key = control.m_nFirstIndex + 1 + control.m_bIsStereo;
						pFlexWeights->weight = 0.5f;
						pFlexWeights->influence = 1.0f;
						pFlexWeights++;
					}
				}
				else
				{
					pSetting[i].numsettings++;
					pFlexWeights->key = control.m_nFirstIndex;
					pFlexWeights->weight = 0.0f;
					pFlexWeights->influence = 0.0f;
					pFlexWeights++;

					if ( control.m_bIsStereo )
					{
						pSetting[i].numsettings++;
						pFlexWeights->key = control.m_nFirstIndex + 1;
						pFlexWeights->weight = 0.0f;
						pFlexWeights->influence = 0.0f;
						pFlexWeights++;
					}

					if ( control.m_bIsMulti )
					{
						pSetting[i].numsettings++;
						pFlexWeights->key = control.m_nFirstIndex + 1 + control.m_bIsStereo;
						pFlexWeights->weight = 0.5f;
						pFlexWeights->influence = 0.0f;
						pFlexWeights++;
					}
				}

				continue;
			}

			if ( !control.m_bIsStereo )
			{
				pSetting[i].numsettings++;
				pFlexWeights->key = control.m_nFirstIndex;
				pFlexWeights->weight = pControlValue->GetValue<float>( "value" );
				pFlexWeights->influence = 1.0f;
				pFlexWeights++;
			}
			else
			{
				float flValue, flBalance, flLeft, flRight;
				flValue = pControlValue->GetValue<float>( "value" );
				flBalance = pControlValue->GetValue<float>( "balance" );
				ValueBalanceToLeftRight( &flLeft, &flRight, flValue, flBalance );

				pSetting[i].numsettings += 2;
				pFlexWeights->key = control.m_nFirstIndex;
				pFlexWeights->weight = flRight;
				pFlexWeights->influence = 1.0f;
				pFlexWeights++;
				pFlexWeights->key = control.m_nFirstIndex + 1;
				pFlexWeights->weight = flLeft;
				pFlexWeights->influence = 1.0f;
				pFlexWeights++;
			}

			if ( control.m_bIsMulti )
			{
				pSetting[i].numsettings++;
				pFlexWeights->key = control.m_nFirstIndex + 1 + control.m_bIsStereo;
				pFlexWeights->weight = pControlValue->GetValue<float>( "multilevel" );
				pFlexWeights->influence = 1.0f;
				pFlexWeights++;
			}
		}

		pData = (byte *)pFlexWeights;
		ALIGN4( pData );
	}

	int numindexes = 1;
	for (i = 0; i < fhdr->numflexsettings; i++)
	{
		if ( pSetting[i].index >= numindexes )
		{
			numindexes = pSetting[i].index + 1;
		}
	}

	// store indexed table
	int *pIndex = (int *)pData;
	fhdr->numindexes = numindexes;
	fhdr->indexindex = pData - pDataStart;
	pData += sizeof( int ) * numindexes;
	ALIGN4( pData );
	for (i = 0; i < numindexes; i++)
	{
		pIndex[i] = -1;
	}
	for (i = 0; i < fhdr->numflexsettings; i++)
	{
		pIndex[pSetting[i].index] = i;
	}

	// store flex setting names
	for (i = 0; i < fhdr->numflexsettings; i++)
	{
		CDmePreset *pPreset = presets[i];
		const char *pPresetName = pPreset->GetName();

		// Hack for 'silence' and for p_ naming scheme
		if ( pPresetName[0] == 'p' && pPresetName[1] == '_' )
		{
			pPresetName = &pPresetName[2];
		}
		if ( !Q_stricmp( pPresetName, "silence" ) )
		{
			pPresetName = "<sil>";
		}

		pSetting[i].nameindex = pData - (byte *)(&pSetting[i]);
		strcpy( (char *)pData, pPresetName );
		pData += Q_strlen( pPresetName ) + 1;
	}
	ALIGN4( pData );

	// store key names
	char **pKeynames = (char **)pData;
	fhdr->numkeys = nTotalControlCount;
	fhdr->keynameindex = pData - pDataStart;
	pData += sizeof(char *) * nTotalControlCount;
	int j = 0;
	for ( i = 0; i < nExportedControlCount; ++i )
	{
		char pTempBuf[MAX_PATH];

		ExportedControl_t &control = exportedControls[i];
		if ( !control.m_bIsStereo )
		{
			pKeynames[j++] = (char *)(pData - pDataStart);
			strcpy( (char *)pData, control.m_Name );
			pData += Q_strlen( control.m_Name ) + 1;
		}
		else
		{
			pKeynames[j++] = (char *)(pData - pDataStart);
			Q_snprintf( pTempBuf, sizeof(pTempBuf), "right_%s", control.m_Name.Get() );
			strcpy( (char *)pData, pTempBuf );
			pData += Q_strlen( pTempBuf ) + 1;

			pKeynames[j++] = (char *)(pData - pDataStart);
			Q_snprintf( pTempBuf, sizeof(pTempBuf), "left_%s", control.m_Name.Get() );
			strcpy( (char *)pData, pTempBuf );
			pData += Q_strlen( pTempBuf ) + 1;
		}

		if ( control.m_bIsMulti )
		{
			pKeynames[j++] = (char *)(pData - pDataStart);
			Q_snprintf( pTempBuf, sizeof(pTempBuf), "multi_%s", control.m_Name.Get() );
			strcpy( (char *)pData, pTempBuf );
			pData += Q_strlen( pTempBuf ) + 1;
		}
	}
	Assert( j == nTotalControlCount );
	ALIGN4( pData );

	// allocate room for remapping
	int *keymapping = (int *)pData;
	fhdr->keymappingindex = pData - pDataStart;
	pData += sizeof( int ) * nTotalControlCount;
	for (i = 0; i < nTotalControlCount; i++)
	{
		keymapping[i] = -1;
	}
	ALIGN4( pData );

	fhdr->length = pData - pDataStart;

	FileHandle_t fh = g_pFullFileSystem->Open( pFileName, "wb" );
	if ( !fh )
	{
		ConWarning( "Unable to write to %s (read-only?)\n", pFileName );
		free( pDataStart );
		return false;
	}

	g_pFullFileSystem->Write( pDataStart, fhdr->length, fh );
	g_pFullFileSystem->Close( fh );
	free( pDataStart );
	return true;
}


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// CDmeAnimationSet - container for animation set info
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeAnimationSet, CDmeAnimationSet );

void CDmeAnimationSet::OnConstruction()
{
	m_Controls.Init( this, "controls" );
	m_PresetGroups.Init( this, "presetGroups" );
	m_SelectionGroups.Init( this, "selectionGroups" );
	m_PhonemeMap.Init( this, "phonememap" );
	m_Operators.Init( this, "operators" );
	m_Bookmarks.Init( this, "bookmarks" );
}

void CDmeAnimationSet::OnDestruction()
{
}

CDmaElementArray< CDmElement > &CDmeAnimationSet::GetControls()
{
	return m_Controls;
}

CDmaElementArray< CDmePresetGroup > &CDmeAnimationSet::GetPresetGroups()
{
	return m_PresetGroups;
}

CDmaElementArray< CDmeOperator > &CDmeAnimationSet::GetOperators()
{
	return m_Operators;
}

void CDmeAnimationSet::AddOperator( CDmeOperator *pOperator )
{
	m_Operators.AddToTail( pOperator );
}

void CDmeAnimationSet::RemoveOperator( CDmeOperator *pOperator )
{
	int nCount = m_Operators.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( m_Operators[i] == pOperator )
		{
			m_Operators.Remove(i);
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Finds the index of a particular preset group
//-----------------------------------------------------------------------------
void CDmeAnimationSet::OnElementUnserialized()
{
	BaseClass::OnElementUnserialized();

	CDmeGameModel *pGameModel = GetValueElement< CDmeGameModel >( "gameModel" );
	if ( pGameModel )
	{
		// NOTE: The model preset manager can't possibly have the right
		// file id at this point; it's up to the preset group manager to queue
		// application requests until it gets one
		g_pModelPresetGroupMgr->ApplyModelPresets( pGameModel->GetModelName(), this );
	}
}


//-----------------------------------------------------------------------------
// Finds the index of a particular preset group
//-----------------------------------------------------------------------------
int CDmeAnimationSet::FindPresetGroupIndex( CDmePresetGroup *pPresetGroup )
{
	int c = m_PresetGroups.Count();
	for ( int i = 0; i < c; ++i )
	{
		CDmePresetGroup *e = m_PresetGroups.Get( i );
		if ( pPresetGroup == e )
			return i;
	}
	return -1; 
}

int CDmeAnimationSet::FindPresetGroupIndex( const char *pGroupName )
{
	int c = m_PresetGroups.Count();
	for ( int i = 0; i < c; ++i )
	{
		CDmePresetGroup *e = m_PresetGroups.Get( i );
		if ( e && !Q_stricmp( e->GetName(), pGroupName ) )
			return i;
	}
	return -1; 
}


//-----------------------------------------------------------------------------
// Find by name
//-----------------------------------------------------------------------------
CDmePresetGroup *CDmeAnimationSet::FindPresetGroup( const char *pGroupName )
{
	int nIndex = FindPresetGroupIndex( pGroupName );
	if ( nIndex >= 0 )
		return m_PresetGroups[nIndex];
	return NULL;
}


//-----------------------------------------------------------------------------
// Find or add by name
//-----------------------------------------------------------------------------
CDmePresetGroup *CDmeAnimationSet::FindOrAddPresetGroup( const char *pGroupName )
{
	CDmePresetGroup *pPresetGroup = FindPresetGroup( pGroupName );
	if ( !pPresetGroup )
	{
		// Create the default groups in order
		pPresetGroup = CreateElement< CDmePresetGroup >( pGroupName, GetFileId() );
		m_PresetGroups.AddToTail( pPresetGroup );
	}
	return pPresetGroup;
}


//-----------------------------------------------------------------------------
// Remove preset group
//-----------------------------------------------------------------------------
bool CDmeAnimationSet::RemovePresetGroup( CDmePresetGroup *pPresetGroup )
{
	int i = FindPresetGroupIndex( pPresetGroup );
	if ( i >= 0 )
	{
		m_PresetGroups.Remove( i );
		return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Move preset group up/down in the list
//-----------------------------------------------------------------------------
void CDmeAnimationSet::MovePresetGroupUp( CDmePresetGroup *pPresetGroup )
{
	int i = FindPresetGroupIndex( pPresetGroup );
	if ( i >= 1 )
	{
		m_PresetGroups.Swap( i, i-1 );
	}
}

void CDmeAnimationSet::MovePresetGroupDown( CDmePresetGroup *pPresetGroup )
{
	int i = FindPresetGroupIndex( pPresetGroup );
	if ( i >= 0 && i < m_PresetGroups.Count() - 1 )
	{
		m_PresetGroups.Swap( i, i+1 );
	}
}


//-----------------------------------------------------------------------------
// Reorder preset groups
//-----------------------------------------------------------------------------
void CDmeAnimationSet::MovePresetGroupInFrontOf( CDmePresetGroup *pPresetGroup, CDmePresetGroup *pInFrontOf )
{
	if ( pPresetGroup == pInFrontOf )
		return;

#ifdef DBGFLAG_ASSERT
	int nStart = FindPresetGroupIndex( pPresetGroup );
#endif

	int nEnd = pInFrontOf ? FindPresetGroupIndex( pInFrontOf ) : m_PresetGroups.Count();
	Assert( nStart >= 0 && nEnd >= 0 );

	RemovePresetGroup( pPresetGroup );
	if ( nEnd > m_PresetGroups.Count() )
	{
		nEnd = m_PresetGroups.Count();
	}
	m_PresetGroups.InsertBefore( nEnd, pPresetGroup );
}


CDmePreset *CDmeAnimationSet::FindOrAddPreset( const char *pGroupName, const char *pPresetName, int nType /*=PROCEDURAL_PRESET_NOT*/ )
{
	CDmePresetGroup *pPresetGroup = FindOrAddPresetGroup( pGroupName );
	return pPresetGroup->FindOrAddPreset( pPresetName, nType );
}

bool CDmeAnimationSet::RemovePreset( CDmePreset *pPreset )
{
	int c = m_PresetGroups.Count();
	for ( int i = 0; i < c; ++i )
	{
		if ( m_PresetGroups[i]->RemovePreset( pPreset ) )
			return true;
	}
	return false;
}


const CDmaElementArray< CDmeBookmark > &CDmeAnimationSet::GetBookmarks() const
{
	return m_Bookmarks;
}

CDmaElementArray< CDmeBookmark > &CDmeAnimationSet::GetBookmarks()
{
	return m_Bookmarks;
}


CDmaElementArray< CDmElement > &CDmeAnimationSet::GetSelectionGroups()
{
	return m_SelectionGroups;
}

CDmaElementArray< CDmePhonemeMapping > &CDmeAnimationSet::GetPhonemeMap()
{
	return m_PhonemeMap;
}

void CDmeAnimationSet::RestoreDefaultPhonemeMap()
{
	CUndoScopeGuard guard( "RestoreDefaultPhonemeMap" );

	int i;
	int c = m_PhonemeMap.Count();
	for ( i = 0; i < c; ++i )
	{
		g_pDataModel->DestroyElement( m_PhonemeMap[ i ]->GetHandle() );
	}
	m_PhonemeMap.Purge();

	int phonemeCount = NumPhonemes();
	for ( i = 0; i < phonemeCount; ++i )
	{
		const char *pName = NameForPhonemeByIndex( i );
		CDmePhonemeMapping *mapping = CreateElement< CDmePhonemeMapping >( pName, GetFileId() );
		char presetName[ 256 ];
		Q_snprintf( presetName, sizeof( presetName ), "p_%s", pName );
		mapping->m_Preset = presetName;
		mapping->m_Weight = 1.0f;

		m_PhonemeMap.AddToTail( mapping );
	}
}

CDmePhonemeMapping *CDmeAnimationSet::FindMapping( const char *pRawPhoneme )
{
	int c = m_PhonemeMap.Count();
	for ( int i = 0; i < c; ++i )
	{
		CDmePhonemeMapping *e = m_PhonemeMap.Get( i );
		Assert( e );
		if ( !e )
			continue;

		if ( !Q_stricmp( e->GetName(), pRawPhoneme ) )
			return e;
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Finds a control 
//-----------------------------------------------------------------------------
CDmElement *CDmeAnimationSet::FindControl( const char *pControlName )
{
	int c = m_Controls.Count();
	for ( int i = 0; i < c; ++i )
	{
		CDmElement *e = m_Controls.Get( i );
		if ( !Q_stricmp( e->GetName(), pControlName ) )
			return e;
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Finds or adds a control 
//-----------------------------------------------------------------------------
CDmElement *CDmeAnimationSet::FindOrAddControl( const char *pControlName )
{
	CDmElement *pControl = FindControl( pControlName );
	if ( !pControl )
	{
		// If not, then create one
		pControl = CreateElement< CDmElement >( pControlName, GetFileId() );
		m_Controls.AddToTail( pControl );
	}
	return pControl;
}


CDmElement *CDmeAnimationSet::FindSelectionGroup( const char *pSelectionGroupName )
{
	int c = m_SelectionGroups.Count();
	for ( int i = 0; i < c; ++i )
	{
		CDmElement *e = m_SelectionGroups.Get( i );
		if ( !Q_stricmp( e->GetName(), pSelectionGroupName ) )
			return e;
	}
	return NULL;
}

CDmElement *CDmeAnimationSet::FindOrAddSelectionGroup( const char *pSelectionGroupName )
{
	CDmElement *pSelectionGroup = FindSelectionGroup( pSelectionGroupName );
	if ( !pSelectionGroup )
	{
		// Create the default groups in order
		pSelectionGroup = CreateElement< CDmElement >( pSelectionGroupName, GetFileId() );
		pSelectionGroup->AddAttribute( "selectedControls", AT_STRING_ARRAY );
		m_SelectionGroups.AddToTail( pSelectionGroup );
	}
	return pSelectionGroup;
}

void CDmeAnimationSet::CollectOperators( CUtlVector< DmElementHandle_t > &operators )
{
	int numOperators = m_Operators.Count();
	for ( int i = 0; i < numOperators; ++i )
	{
		DmElementHandle_t h = m_Operators.GetHandle( i );
		if ( h != DMELEMENT_HANDLE_INVALID )
		{
			operators.AddToTail( h );
		}
	}
}

struct PPType_t
{
	int type;
	char const *name;
};

static PPType_t g_PresetNames[ NUM_PROCEDURAL_PRESET_TYPES ] =
{
	{ PROCEDURAL_PRESET_NOT, "NotProcedural!!!" },
	{ PROCEDURAL_PRESET_IN_CROSSFADE, "In" },
	{ PROCEDURAL_PRESET_OUT_CROSSFADE, "Out" },
	{ PROCEDURAL_PRESET_REVEAL, "Reveal" },
	{ PROCEDURAL_PRESET_PASTE, "Paste" },
	{ PROCEDURAL_PRESET_JITTER, "Jitter" },
	{ PROCEDURAL_PRESET_SMOOTH, "Smooth" },
	{ PROCEDURAL_PRESET_SHARPEN, "Sharpen" },
	{ PROCEDURAL_PRESET_SOFTEN, "Soften" },
	{ PROCEDURAL_PRESET_STAGGER, "Stagger" },
};

void CDmeAnimationSet::EnsureProceduralPresets()
{
	// Note:  Starts at index 1 to skip the PROCEDURAL_PRESET_NOT case
	for ( int i = 1; i < NUM_PROCEDURAL_PRESET_TYPES; ++i )
	{
		FindOrAddPreset( "Procedural", g_PresetNames[ i ].name, g_PresetNames[ i ].type );
	}
}

//-----------------------------------------------------------------------------
// A cache of preset groups to be associated with specific models
//-----------------------------------------------------------------------------
class CModelPresetGroupManager : public IModelPresetGroupManager
{
public:
	CModelPresetGroupManager();
	virtual void AssociatePresetsWithFile( DmFileId_t fileId );
	virtual void ApplyModelPresets( const char *pModelName, CDmeAnimationSet *pAnimationSet );

private:
	struct QueuedPresetRequest_t
	{
		CUtlString m_ModelName;
		CDmeHandle< CDmeAnimationSet > m_hAnimationSet;
	};

	typedef CUtlVector< CDmeHandle< CDmePresetGroup, true > > PresetGroupList_t;
	
	// Loads model presets from .pre files matching the model name
	void LoadModelPresets( const char *pModelName, PresetGroupList_t &list );

	CUtlStringMap< PresetGroupList_t > m_Lookup;
	DmFileId_t m_FileId;
	CUtlVector< QueuedPresetRequest_t > m_QueuedPresetRequest;
};


//-----------------------------------------------------------------------------
// Singleton
//-----------------------------------------------------------------------------
static CModelPresetGroupManager s_ModelPresetGroupManager;
IModelPresetGroupManager *g_pModelPresetGroupMgr = &s_ModelPresetGroupManager;


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CModelPresetGroupManager::CModelPresetGroupManager()
{
	m_FileId = DMFILEID_INVALID;
}


//-----------------------------------------------------------------------------
// Associates presets in the cache with a particular file
//-----------------------------------------------------------------------------
void CModelPresetGroupManager::AssociatePresetsWithFile( DmFileId_t fileId )
{
	m_FileId = fileId;
	m_Lookup.Clear();
	if ( m_FileId != DMFILEID_INVALID )
	{
		int nCount = m_QueuedPresetRequest.Count();
		for ( int i = 0; i < nCount; ++i )
		{
			QueuedPresetRequest_t &request = m_QueuedPresetRequest[i];
			if ( request.m_hAnimationSet.Get() )
			{
				ApplyModelPresets( request.m_ModelName, request.m_hAnimationSet.Get() );
			}
		}
		m_QueuedPresetRequest.Purge();
	}
}


//-----------------------------------------------------------------------------
// Loads model presets from .pre files matching the model name
//-----------------------------------------------------------------------------
void CModelPresetGroupManager::LoadModelPresets( const char *pModelName, PresetGroupList_t &list )
{
	list.RemoveAll();

	char pPresetPath[MAX_PATH];
	Q_ExtractFilePath( pModelName, pPresetPath, sizeof(pPresetPath) );

	char pPresetNameBuf[MAX_PATH];
	Q_StripExtension( pModelName, pPresetNameBuf, sizeof(pPresetNameBuf) );
	int nLen = Q_strlen( pPresetNameBuf );
	Q_snprintf( &pPresetNameBuf[nLen], MAX_PATH - nLen, "*.pre" );

	CDisableUndoScopeGuard sg;

	FileFindHandle_t fh;
	const char *pFileName = g_pFullFileSystem->FindFirstEx( pPresetNameBuf, "GAME", &fh );
	for ( ; pFileName; pFileName = g_pFullFileSystem->FindNext( fh ) )
	{
		char pRelativePresetPath[MAX_PATH];
		Q_ComposeFileName(pPresetPath,  pFileName, pRelativePresetPath, sizeof(pRelativePresetPath) );

		CDmElement* pRoot = NULL;
		DmFileId_t fileid = g_pDataModel->RestoreFromFile( pRelativePresetPath, "GAME", NULL, &pRoot, CR_FORCE_COPY );
		if ( fileid == DMFILEID_INVALID || !pRoot )
			continue;

		CDmePresetGroup *pPresetGroup = CastElement<CDmePresetGroup>( pRoot );
		if ( !pPresetGroup )
		{
			if ( pRoot )
			{
				g_pDataModel->RemoveFileId( pRoot->GetFileId() );
			}
			continue;
		}

		pPresetGroup->SetFileId( m_FileId, TD_DEEP );

		// Presets used through the model preset manager must be read only + shared
		pPresetGroup->m_bIsReadOnly = true;
		pPresetGroup->SetShared( true );

		int i = list.AddToTail();
		list[i] = pPresetGroup;
	}
	g_pFullFileSystem->FindClose( fh );
}


//-----------------------------------------------------------------------------
// Applies model presets associated with a particular model to an animation set
//-----------------------------------------------------------------------------
void CModelPresetGroupManager::ApplyModelPresets( const char *pModelName, CDmeAnimationSet *pAnimationSet )
{
	if ( m_FileId == DMFILEID_INVALID )
	{
		int i = m_QueuedPresetRequest.AddToTail();
		m_QueuedPresetRequest[i].m_ModelName = pModelName;
		m_QueuedPresetRequest[i].m_hAnimationSet = pAnimationSet;
		return;
	}

	if ( !m_Lookup.Defined( pModelName ) )
	{
		LoadModelPresets( pModelName, m_Lookup[pModelName] );
	}

	PresetGroupList_t &list = m_Lookup[pModelName];
	int nCount = list.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmePresetGroup *pPresetGroup = list[i];
		int nIndex = pAnimationSet->FindPresetGroupIndex( pPresetGroup->GetName() );
		if ( nIndex >= 0 )
		{
			pAnimationSet->GetPresetGroups().Set( nIndex, pPresetGroup );
		}
		else
		{
			pAnimationSet->GetPresetGroups().AddToTail( pPresetGroup );
		}
	}
}
