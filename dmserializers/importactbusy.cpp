//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "importkeyvaluebase.h"
#include "dmserializers.h"
#include "datamodel/idatamodel.h"
#include "datamodel/dmelement.h"
#include "tier1/KeyValues.h"
#include "tier1/utlbuffer.h"
#include "datamodel/dmattribute.h"


//-----------------------------------------------------------------------------
// Serialization class for Key Values
//-----------------------------------------------------------------------------
class CImportActBusy : public CImportKeyValueBase
{
public:
	virtual const char *GetName() const { return "actbusy"; }
	virtual const char *GetDescription() const { return "ActBusy Script File"; }
	virtual int GetCurrentVersion() const { return 0; } // doesn't store a version

	bool Serialize( CUtlBuffer &outBuf, CDmElement *pRoot );
	CDmElement* UnserializeFromKeyValues( KeyValues *pKeyValues );

private:
	// Reads a single element
	bool UnserializeActBusyKey( CDmAttribute *pChildren, KeyValues *pKeyValues );

	// Writes out the actbusy header
	void SerializeHeader( CUtlBuffer &buf );
};


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CImportActBusy s_ImportActBusy;

void InstallActBusyImporter( IDataModel *pFactory )
{
	pFactory->AddSerializer( &s_ImportActBusy );
}


//-----------------------------------------------------------------------------
// Writes out the actbusy header
//-----------------------------------------------------------------------------
void CImportActBusy::SerializeHeader( CUtlBuffer &buf )
{
	buf.Printf( "// \"act busy name\"\t\tThis is the name that a mapmaker must specify in the hint node.\n" );
	buf.Printf( "// {\n" ); 
	buf.Printf( "// \t\"busy_anim\"\t\t\t\"Activity Name\".\n" );
	buf.Printf( "// \t\"entry_anim\"\t\t\"Activity Name\"\n" );
	buf.Printf( "// \t\"exit_anim\"\t\t\t\"Activity Name\"\n" );
	buf.Printf( "// \t\"busy_sequence\"\t\t\"Sequence Name\". If specified, this is used over the activity name. Specify it in the hint node.\n" );
	buf.Printf( "// \t\"entry_sequence\"\t\"Sequence Name\". If specified, this is used over the entry anim.\n" );
	buf.Printf( "// \t\"exit_sequence\"\t\t\"Sequence Name\". If specified, this is used over the exit anim.\n" );
	buf.Printf( "// \t\"min_time\"\t\t\t\"Minimum time to spend in this busy anim\"\n" );
	buf.Printf( "// \t\"max_time\"\t\t\t\"Maximum time to spend in this busy anim\"	0 = only stop when interrupted by external event\n" );
	buf.Printf( "// \t\"interrupts\"\t\tOne of:\n" );
	buf.Printf( "// \t\t\t\t\t\t\"BA_INT_NONE\"\t\tbreak out only when time runs out. No external influence will break me out.\n" );
	buf.Printf( "// \t\t\t\t\t\t\"BA_INT_DANGER\"\t\tbreak out of this anim only if threatened\n" );
	buf.Printf( "// \t\t\t\t\t\t\"BA_INT_PLAYER\"\t\tbreak out of this anim if I can see the player, or I'm threatened\n" );
	buf.Printf( "// \t\t\t\t\t\t\"BA_INT_AMBUSH\"\t\tsomeone please define this - I have no idea what it does\n" );
	buf.Printf( "// \t\t\t\t\t\t\"BA_INT_COMBAT\"\t\tbreak out of this anim if combat occurs in my line of sight (bullet hits, grenades, etc), -OR- the max time is reached\n" );
	buf.Printf( "// }\n" );
	buf.Printf( "//\n" );
}


//-----------------------------------------------------------------------------
// Writes out a new actbusy file
//-----------------------------------------------------------------------------
bool CImportActBusy::Serialize( CUtlBuffer &buf, CDmElement *pRoot )
{
	SerializeHeader( buf );
 	buf.Printf( "\"ActBusy.txt\"\n" );
	buf.Printf( "{\n" );

	CDmAttribute *pChildren = pRoot->GetAttribute( "children" );
	if ( !pChildren || pChildren->GetType() != AT_ELEMENT_ARRAY )
		return NULL;

	CDmrElementArray<> children( pChildren );
	int nCount = children.Count();

	buf.PushTab();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmElement *pChild = children[i];
 		buf.Printf( "\"%s\"\n", pChild->GetName() );
		buf.Printf( "{\n" );

		buf.PushTab();
		PrintStringAttribute( pChild, buf, "busy_anim", true );
		PrintStringAttribute( pChild, buf, "entry_anim", true );
		PrintStringAttribute( pChild, buf, "exit_anim", true );
		PrintStringAttribute( pChild, buf, "busy_sequence", true );
		PrintStringAttribute( pChild, buf, "entry_sequence", true );
		PrintStringAttribute( pChild, buf, "exit_sequence", true );
		PrintFloatAttribute( pChild, buf, "min_time" );
		PrintFloatAttribute( pChild, buf, "max_time" );
		PrintStringAttribute( pChild, buf, "interrupts" );
		buf.PopTab();

		buf.Printf( "}\n" );
	}
	buf.PopTab();

	buf.Printf( "}\n" );

	return true;
}


//-----------------------------------------------------------------------------
// Reads a single element
//-----------------------------------------------------------------------------
bool CImportActBusy::UnserializeActBusyKey( CDmAttribute *pChildren, KeyValues *pKeyValues )
{
	CDmElement *pActBusy = CreateDmElement( "DmElement", pKeyValues->GetName(), NULL );
	if ( !pActBusy )
		return false;

	// Each act busy needs to have an editortype associated with it so it displays nicely in editors
	pActBusy->SetValue( "editorType", "actBusy" );

	float flZero = 0.0f;
	AddStringAttribute( pActBusy, pKeyValues, "busy_anim", "" );
	AddStringAttribute( pActBusy, pKeyValues, "entry_anim", "" );
	AddStringAttribute( pActBusy, pKeyValues, "exit_anim", "" );
	AddStringAttribute( pActBusy, pKeyValues, "busy_sequence", "" );
	AddStringAttribute( pActBusy, pKeyValues, "entry_sequence", "" );
	AddStringAttribute( pActBusy, pKeyValues, "exit_sequence", "" );
	AddFloatAttribute( pActBusy, pKeyValues, "min_time", &flZero );
	AddFloatAttribute( pActBusy, pKeyValues, "max_time", &flZero );
	AddStringAttribute( pActBusy, pKeyValues, "interrupts", "BA_INT_NONE" );

	CDmrElementArray<> children( pChildren );
	children.AddToTail( pActBusy );

	return true;
}


//-----------------------------------------------------------------------------
// Main entry point for the unserialization
//-----------------------------------------------------------------------------
CDmElement* CImportActBusy::UnserializeFromKeyValues( KeyValues *pKeyValues )
{
	// Create the main element
	CDmElement *pElement = CreateDmElement( "DmElement", "ActBusyList", NULL );
	if ( !pElement )
		return NULL;

	// Each act busy list needs to have an editortype associated with it so it displays nicely in editors
	pElement->SetValue( "editorType", "actBusyList" );

	// All actbusy keys are elements of a single element array attribute 'children'
	CDmAttribute *pChildren = pElement->AddAttribute( "children", AT_ELEMENT_ARRAY );
	if ( !pChildren )
		return NULL;

	// Under the root are all the actbusy keys
	for ( KeyValues *pActBusyKey = pKeyValues->GetFirstTrueSubKey(); pActBusyKey != NULL; pActBusyKey = pActBusyKey->GetNextTrueSubKey() )
	{
		if ( !UnserializeActBusyKey( pChildren, pActBusyKey ) )
		{
			Warning( "Error importing actbusy element %s\n", pActBusyKey->GetName() );
			return NULL;
		}
	}

	// Resolve all element references recursively
	RecursivelyResolveElement( pElement );

	return pElement;
}
