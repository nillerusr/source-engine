//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Shared object based on a CBaseRecord subclass
//
//=============================================================================
#include "stdafx.h"

#include "gcsdk_gcmessages.pb.h"

#include "google/protobuf/descriptor.h"
#include "google/protobuf/reflection_ops.h"
#include "google/protobuf/descriptor.pb.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::Descriptor;
using ::google::protobuf::Message;
using ::google::protobuf::Reflection;


namespace GCSDK
{

//----------------------------------------------------------------------------
// Purpose: returns true if the field has the key_field option
//----------------------------------------------------------------------------
inline bool IsKeyField( const FieldDescriptor *pFieldDescriptor )
{
	return pFieldDescriptor->options().GetExtension( key_field );
}

//----------------------------------------------------------------------------
// Purpose: Copies a field from one protobuf message to another
//----------------------------------------------------------------------------
void CopyProtoBufField( ::google::protobuf::Message & msgDest, const ::google::protobuf::Message & msgSource, const ::google::protobuf::FieldDescriptor *pFieldDest, const ::google::protobuf::FieldDescriptor *pFieldSource )
{
	Assert( pFieldDest->cpp_type() == pFieldSource->cpp_type() );
	Assert( pFieldDest->is_repeated() == pFieldSource->is_repeated() );
	if( pFieldSource->cpp_type() != pFieldDest->cpp_type() || pFieldSource->is_repeated() != pFieldDest->is_repeated() )
		return;

	if ( pFieldDest->is_repeated() )
	{
		int nFieldSize = msgSource.GetReflection()->FieldSize( msgSource, pFieldSource );
		switch( pFieldDest->cpp_type() )
		{
		case ::google::protobuf::FieldDescriptor::CPPTYPE_INT32:
			{
				for ( int i = 0; i < nFieldSize; i++ )
				{
					if ( msgDest.GetReflection()->FieldSize( msgDest, pFieldDest ) <= i )
					{
						msgDest.GetReflection()->AddInt32( &msgDest, pFieldDest, msgSource.GetReflection()->GetRepeatedInt32( msgSource, pFieldSource, i ) );
					}
					else
					{
						msgDest.GetReflection()->SetRepeatedInt32( &msgDest, pFieldDest, i, msgSource.GetReflection()->GetRepeatedInt32( msgSource, pFieldSource, i ) );
					}
				}
			}
			break;

		case ::google::protobuf::FieldDescriptor::CPPTYPE_INT64:
			{
				for ( int i = 0; i < nFieldSize; i++ )
				{
					if ( msgDest.GetReflection()->FieldSize( msgDest, pFieldDest ) <= i )
					{
						msgDest.GetReflection()->AddInt64( &msgDest, pFieldDest, msgSource.GetReflection()->GetRepeatedInt64( msgSource, pFieldSource, i ) );
					}
					else
					{
						msgDest.GetReflection()->SetRepeatedInt64( &msgDest, pFieldDest, i, msgSource.GetReflection()->GetRepeatedInt64( msgSource, pFieldSource, i ) );
					}
				}
			}
			break;

		case ::google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
			{
				for ( int i = 0; i < nFieldSize; i++ )
				{
					if ( msgDest.GetReflection()->FieldSize( msgDest, pFieldDest ) <= i )
					{
						msgDest.GetReflection()->AddUInt32( &msgDest, pFieldDest, msgSource.GetReflection()->GetRepeatedUInt32( msgSource, pFieldSource, i ) );
					}
					else
					{
						msgDest.GetReflection()->SetRepeatedUInt32( &msgDest, pFieldDest, i, msgSource.GetReflection()->GetRepeatedUInt32( msgSource, pFieldSource, i ) );
					}
				}
			}
			break;

		case ::google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
			{
				for ( int i = 0; i < nFieldSize; i++ )
				{
					if ( msgDest.GetReflection()->FieldSize( msgDest, pFieldDest ) <= i )
					{
						msgDest.GetReflection()->AddUInt64( &msgDest, pFieldDest, msgSource.GetReflection()->GetRepeatedUInt64( msgSource, pFieldSource, i ) );
					}
					else
					{
						msgDest.GetReflection()->SetRepeatedUInt64( &msgDest, pFieldDest, i, msgSource.GetReflection()->GetRepeatedUInt64( msgSource, pFieldSource, i ) );
					}
				}
			}
			break;

		case ::google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
			{
				for ( int i = 0; i < nFieldSize; i++ )
				{
					if ( msgDest.GetReflection()->FieldSize( msgDest, pFieldDest ) <= i )
					{
						msgDest.GetReflection()->AddDouble( &msgDest, pFieldDest, msgSource.GetReflection()->GetRepeatedDouble( msgSource, pFieldSource, i ) );
					}
					else
					{
						msgDest.GetReflection()->SetRepeatedDouble( &msgDest, pFieldDest, i, msgSource.GetReflection()->GetRepeatedDouble( msgSource, pFieldSource, i ) );
					}
				}
			}
			break;

		case ::google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
			{
				for ( int i = 0; i < nFieldSize; i++ )
				{
					if ( msgDest.GetReflection()->FieldSize( msgDest, pFieldDest ) <= i )
					{
						msgDest.GetReflection()->AddFloat( &msgDest, pFieldDest, msgSource.GetReflection()->GetRepeatedFloat( msgSource, pFieldSource, i ) );
					}
					else
					{
						msgDest.GetReflection()->SetRepeatedFloat( &msgDest, pFieldDest, i, msgSource.GetReflection()->GetRepeatedFloat( msgSource, pFieldSource, i ) );
					}
				}
			}
			break;

		case ::google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
			{
				for ( int i = 0; i < nFieldSize; i++ )
				{
					if ( msgDest.GetReflection()->FieldSize( msgDest, pFieldDest ) <= i )
					{
						msgDest.GetReflection()->AddBool( &msgDest, pFieldDest, msgSource.GetReflection()->GetRepeatedBool( msgSource, pFieldSource, i ) );
					}
					else
					{
						msgDest.GetReflection()->SetRepeatedBool( &msgDest, pFieldDest, i, msgSource.GetReflection()->GetRepeatedBool( msgSource, pFieldSource, i ) );
					}
				}
			}
			break;

		case ::google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
			{
				for ( int i = 0; i < nFieldSize; i++ )
				{
					if ( msgDest.GetReflection()->FieldSize( msgDest, pFieldDest ) <= i )
					{
						msgDest.GetReflection()->AddEnum( &msgDest, pFieldDest, msgSource.GetReflection()->GetRepeatedEnum( msgSource, pFieldSource, i ) );
					}
					else
					{
						msgDest.GetReflection()->SetRepeatedEnum( &msgDest, pFieldDest, i, msgSource.GetReflection()->GetRepeatedEnum( msgSource, pFieldSource, i ) );
					}
				}
			}
			break;

		case ::google::protobuf::FieldDescriptor::CPPTYPE_STRING:
			{
				for ( int i = 0; i < nFieldSize; i++ )
				{
					if ( msgDest.GetReflection()->FieldSize( msgDest, pFieldDest ) <= i )
					{
						msgDest.GetReflection()->AddString( &msgDest, pFieldDest, msgSource.GetReflection()->GetRepeatedString( msgSource, pFieldSource, i ) );
					}
					else
					{
						msgDest.GetReflection()->SetRepeatedString( &msgDest, pFieldDest, i, msgSource.GetReflection()->GetRepeatedString( msgSource, pFieldSource, i ) );
					}
				}
			}
			break;

		case ::google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
			{
				for ( int i = 0; i < nFieldSize; i++ )
				{
					if ( msgDest.GetReflection()->FieldSize( msgDest, pFieldDest ) <= i )
					{
						msgDest.GetReflection()->AddMessage( &msgDest, pFieldDest )->CopyFrom( msgSource.GetReflection()->GetRepeatedMessage( msgSource, pFieldSource, i ) );
					}
					else
					{
						msgDest.GetReflection()->MutableRepeatedMessage( &msgDest, pFieldDest, i )->CopyFrom( msgSource.GetReflection()->GetRepeatedMessage( msgSource, pFieldSource, i ) );
					}
				}
			}
			break;

		}
	}
	else
	{
		switch( pFieldDest->cpp_type() )
		{
			case ::google::protobuf::FieldDescriptor::CPPTYPE_INT32:
			{
				msgDest.GetReflection()->SetInt32( &msgDest, pFieldDest, msgSource.GetReflection()->GetInt32( msgSource, pFieldSource ) );
			}
			break;

			case ::google::protobuf::FieldDescriptor::CPPTYPE_INT64:
			{
				msgDest.GetReflection()->SetInt64( &msgDest, pFieldDest, msgSource.GetReflection()->GetInt64( msgSource, pFieldSource ) );
			}
			break;

			case ::google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
			{
				msgDest.GetReflection()->SetUInt32( &msgDest, pFieldDest, msgSource.GetReflection()->GetUInt32( msgSource, pFieldSource ) );
			}
			break;

			case ::google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
			{
				msgDest.GetReflection()->SetUInt64( &msgDest, pFieldDest, msgSource.GetReflection()->GetUInt64( msgSource, pFieldSource ) );
			}
			break;

			case ::google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
			{
				msgDest.GetReflection()->SetDouble( &msgDest, pFieldDest, msgSource.GetReflection()->GetDouble( msgSource, pFieldSource ) );
			}
			break;

			case ::google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
			{
				msgDest.GetReflection()->SetFloat( &msgDest, pFieldDest, msgSource.GetReflection()->GetFloat( msgSource, pFieldSource ) );
			}
			break;

			case ::google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
			{
				msgDest.GetReflection()->SetBool( &msgDest, pFieldDest, msgSource.GetReflection()->GetBool( msgSource, pFieldSource ) );
			}
			break;

			case ::google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
			{
				msgDest.GetReflection()->SetEnum( &msgDest, pFieldDest, msgSource.GetReflection()->GetEnum( msgSource, pFieldSource ) );
			}
			break;

			case ::google::protobuf::FieldDescriptor::CPPTYPE_STRING:
			{
				msgDest.GetReflection()->SetString( &msgDest, pFieldDest, msgSource.GetReflection()->GetString( msgSource, pFieldSource ) );
			}
			break;

			case ::google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
			{
				msgDest.GetReflection()->MutableMessage( &msgDest, pFieldDest )->CopyFrom( msgSource.GetReflection()->GetMessage( msgSource, pFieldSource ) );
			}
			break;

		}
	}
}

//----------------------------------------------------------------------------
// Purpose: compares fields in two protobuf objects
//----------------------------------------------------------------------------
bool IsProtoBufFieldLess( const ::google::protobuf::Message & msgLHS, const ::google::protobuf::Message & msgRHS, const ::google::protobuf::FieldDescriptor *pFieldLHS, const ::google::protobuf::FieldDescriptor *pFieldRHS )
{
	Assert( pFieldLHS->cpp_type() == pFieldRHS->cpp_type() );
	if( pFieldLHS->cpp_type() != pFieldRHS->cpp_type() )
		return false;

	switch( pFieldLHS->cpp_type() )
	{
		case ::google::protobuf::FieldDescriptor::CPPTYPE_INT32:
		{
			return msgLHS.GetReflection()->GetInt32( msgLHS, pFieldLHS ) < msgRHS.GetReflection()->GetInt32( msgRHS, pFieldRHS );
		}
		break;

		case ::google::protobuf::FieldDescriptor::CPPTYPE_INT64:
		{
			return msgLHS.GetReflection()->GetInt64( msgLHS, pFieldLHS ) < msgRHS.GetReflection()->GetInt64( msgRHS, pFieldRHS );
		}
		break;

		case ::google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
		{
			return msgLHS.GetReflection()->GetUInt32( msgLHS, pFieldLHS ) < msgRHS.GetReflection()->GetUInt32( msgRHS, pFieldRHS );
		}
		break;

		case ::google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
		{
			return msgLHS.GetReflection()->GetUInt64( msgLHS, pFieldLHS ) < msgRHS.GetReflection()->GetUInt64( msgRHS, pFieldRHS );
		}
		break;

		case ::google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
		{
			return msgLHS.GetReflection()->GetDouble( msgLHS, pFieldLHS ) < msgRHS.GetReflection()->GetDouble( msgRHS, pFieldRHS );
		}
		break;

		case ::google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
		{
			return msgLHS.GetReflection()->GetFloat( msgLHS, pFieldLHS ) < msgRHS.GetReflection()->GetFloat( msgRHS, pFieldRHS );
		}
		break;

		case ::google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
		{
			return msgLHS.GetReflection()->GetBool( msgLHS, pFieldLHS ) < msgRHS.GetReflection()->GetBool( msgRHS, pFieldRHS );
		}
		break;

		case ::google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
		{
			return msgLHS.GetReflection()->GetEnum( msgLHS, pFieldLHS )->number() < msgRHS.GetReflection()->GetEnum( msgRHS, pFieldRHS )->number();
		}
		break;

		case ::google::protobuf::FieldDescriptor::CPPTYPE_STRING:
		{
			return Q_stricmp( msgLHS.GetReflection()->GetString( msgLHS, pFieldLHS ).c_str(), msgRHS.GetReflection()->GetString( msgRHS, pFieldRHS ).c_str() ) > 0;
		}
		break;

		case ::google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
		{
			return false;
		}
		break;

	}

	// unknown field types return false
	return false;
}

//----------------------------------------------------------------------------
// Purpose: Returns true if the key fields in LHS are less than the key fields
//			in RHS
//----------------------------------------------------------------------------
bool CProtoBufSharedObjectBase::BIsKeyLess( const CSharedObject & soRHS  ) const
{
	const ::google::protobuf::Message & msgLHS = *GetPObject();
	const ::google::protobuf::Message & msgRHS = *((const CProtoBufSharedObjectBase &)soRHS).GetPObject();

	const ::google::protobuf::Descriptor *pDescriptor = msgLHS.GetDescriptor();

	for( int nField = 0; nField < pDescriptor->field_count(); nField++ )
	{
		const ::google::protobuf::FieldDescriptor *pFieldDescriptor = pDescriptor->field( nField );
		if( !IsKeyField( pFieldDescriptor ) )
			continue;

		if( IsProtoBufFieldLess( msgLHS, msgRHS, pFieldDescriptor, pFieldDescriptor ) )
			return true;

		// if the fields are less when reversed it means we know the keys aren't equal and don't
		// need to keep looping
		if( IsProtoBufFieldLess( msgRHS, msgLHS, pFieldDescriptor, pFieldDescriptor ) )
			return false;
	}

	return false;
}


void CProtoBufSharedObjectBase::RecursiveAddProtoBufToKV( KeyValues *pKVDest, const ::google::protobuf::Message & msg )
{
	using ::google::protobuf::FieldDescriptor;
	const ::google::protobuf::Descriptor *pDescriptor = msg.GetDescriptor();
	const ::google::protobuf::Reflection *pReflection = msg.GetReflection();
	
	for ( int iField = 0; iField < pDescriptor->field_count(); iField++ )
	{
		const ::google::protobuf::FieldDescriptor *pField = pDescriptor->field( iField );
		const char *pFieldName = pField->name().c_str();

		if ( pField->is_repeated() )
		{
			KeyValues *pKVContainer = pKVDest->FindKey( pFieldName, true );
			for ( int iRepeated = 0; iRepeated < pReflection->FieldSize( msg, pField ); iRepeated++ )
			{
				KeyValues *pKVNode = pKVContainer->CreateNewKey();
				switch ( pField->cpp_type() )
				{
				case FieldDescriptor::CPPTYPE_INT32:	pKVNode->SetInt(	NULL, pReflection->GetRepeatedInt32( msg, pField, iRepeated ) );			break;
				case FieldDescriptor::CPPTYPE_INT64:	pKVNode->SetUint64( NULL, (uint64)pReflection->GetRepeatedInt64( msg, pField, iRepeated ) );	break;
				case FieldDescriptor::CPPTYPE_UINT32:	pKVNode->SetInt(	NULL, (int32)pReflection->GetRepeatedUInt32( msg, pField, iRepeated ) );	break;
				case FieldDescriptor::CPPTYPE_UINT64:	pKVNode->SetUint64( NULL, pReflection->GetRepeatedUInt64( msg, pField, iRepeated ) );			break;
				case FieldDescriptor::CPPTYPE_DOUBLE:	pKVNode->SetFloat(	NULL, (float)pReflection->GetRepeatedDouble( msg, pField, iRepeated ) );	break;
				case FieldDescriptor::CPPTYPE_FLOAT:	pKVNode->SetFloat(	NULL, pReflection->GetRepeatedFloat( msg, pField, iRepeated ) );			break;
				case FieldDescriptor::CPPTYPE_BOOL:		pKVNode->SetBool(	NULL, pReflection->GetRepeatedBool( msg, pField, iRepeated ) );				break;
				case FieldDescriptor::CPPTYPE_ENUM:		pKVNode->SetInt(	NULL, pReflection->GetRepeatedEnum( msg, pField, iRepeated )->number() );	break;
				case FieldDescriptor::CPPTYPE_STRING:	pKVNode->SetString(	NULL, pReflection->GetRepeatedString( msg, pField, iRepeated ).c_str() );	break;
				case FieldDescriptor::CPPTYPE_MESSAGE:
					{
						const ::google::protobuf::Message &subMsg = pReflection->GetRepeatedMessage( msg, pField, iRepeated );
						RecursiveAddProtoBufToKV( pKVNode, subMsg );
						break;
					}
				default:
					AssertMsg1( false, "Unknown cpp_type %d", pField->cpp_type() );
					break;
				}
			}
		}
		else
		{
			switch ( pField->cpp_type() )
			{
			case FieldDescriptor::CPPTYPE_INT32:	pKVDest->SetInt(	pFieldName, pReflection->GetInt32( msg, pField ) );			break;
			case FieldDescriptor::CPPTYPE_INT64:	pKVDest->SetUint64( pFieldName, (uint64)pReflection->GetInt64( msg, pField ) );	break;
			case FieldDescriptor::CPPTYPE_UINT32:	pKVDest->SetInt(	pFieldName, (int32)pReflection->GetUInt32( msg, pField ) );	break;
			case FieldDescriptor::CPPTYPE_UINT64:	pKVDest->SetUint64( pFieldName, pReflection->GetUInt64( msg, pField ) );		break;
			case FieldDescriptor::CPPTYPE_DOUBLE:	pKVDest->SetFloat(	pFieldName, (float)pReflection->GetDouble( msg, pField ) );	break;
			case FieldDescriptor::CPPTYPE_FLOAT:	pKVDest->SetFloat(	pFieldName, pReflection->GetFloat( msg, pField ) );			break;
			case FieldDescriptor::CPPTYPE_BOOL:		pKVDest->SetBool(	pFieldName, pReflection->GetBool( msg, pField ) );			break;
			case FieldDescriptor::CPPTYPE_ENUM:		pKVDest->SetInt(	pFieldName, pReflection->GetEnum( msg, pField )->number() );break;
			case FieldDescriptor::CPPTYPE_STRING:	pKVDest->SetString(	pFieldName, pReflection->GetString( msg, pField ).c_str() );break;
			case FieldDescriptor::CPPTYPE_MESSAGE:
				{
					KeyValues *pKVSub = pKVDest->FindKey( pFieldName, true );
					const ::google::protobuf::Message &subMsg = pReflection->GetMessage( msg, pField );
					RecursiveAddProtoBufToKV( pKVSub, subMsg );
					break;
				}
			default:
				AssertMsg1( false, "Unknown cpp_type %d", pField->cpp_type() );
				break;
			}
		}
	}
}


KeyValues *CProtoBufSharedObjectBase::CreateKVFromProtoBuf( const ::google::protobuf::Message & msg )
{
	KeyValues *pKVDest = new KeyValues( msg.GetDescriptor()->name().c_str() );
	RecursiveAddProtoBufToKV( pKVDest, msg );
	return pKVDest;
}


//----------------------------------------------------------------------------
// Purpose: Dumps a debug string for the object
//----------------------------------------------------------------------------
void CProtoBufSharedObjectBase::Dump( const ::google::protobuf::Message & msg )
{
	// print line by line to get round our limited emitinfo buffer
	CUtlStringList lines;
	V_SplitString( msg.DebugString().c_str(), "\n", lines );
	for ( int i = 0; i < lines.Count(); i++ )
	{
		EmitInfo( SPEW_SHAREDOBJ, SPEW_ALWAYS, LOG_ALWAYS, "%s\n", lines[i] );
	}
}

//----------------------------------------------------------------------------
// Purpose: Dumps a debug string for the object
//----------------------------------------------------------------------------
void CProtoBufSharedObjectBase::Dump() const
{
	Dump( *GetPObject() );
}

//=============================================================================

//----------------------------------------------------------------------------
// Purpose: Parses the message bits for creating this object from the message.
//			This will be called on the client/gameserver when it first learns
//			about the item.
//----------------------------------------------------------------------------
bool CProtoBufSharedObjectBase::BParseFromMessage( const CUtlBuffer & buffer ) 
{
	::google::protobuf::Message & msg = *GetPObject();

	return msg.ParseFromArray( buffer.Base(), buffer.TellMaxPut() );
}


//----------------------------------------------------------------------------
// Purpose: Parses the message bits for creating this object from the message.
//			This will be called on the client/gameserver when it first learns
//			about the item.
//----------------------------------------------------------------------------
bool CProtoBufSharedObjectBase::BParseFromMessage( const std::string &buffer ) 
{
	::google::protobuf::Message & msg = *GetPObject();

	return msg.ParseFromString( buffer );
}

//----------------------------------------------------------------------------
// Purpose: Overrides all the fields in msgLocal that are present in the 
//			network message
//----------------------------------------------------------------------------
bool CProtoBufSharedObjectBase::BUpdateFromNetwork( const CSharedObject & objUpdate )
{
	::google::protobuf::Message & msg = *GetPObject();
	const CProtoBufSharedObjectBase & pbobjUpdate = (const CProtoBufSharedObjectBase &)objUpdate;
	
	// merge the update onto the local message
	msg.CopyFrom( *pbobjUpdate.GetPObject() );
	return true;
}


#ifdef GC

//----------------------------------------------------------------------------
// Purpose: Static help class that seralizes to a buffer
//----------------------------------------------------------------------------
bool CProtoBufSharedObjectBase::SerializeToBuffer( const ::google::protobuf::Message & msg, CUtlBuffer & bufOutput )
{
	uint32 unSize = msg.ByteSize();
	bufOutput.Clear();
	bufOutput.EnsureCapacity( unSize );
	msg.SerializeWithCachedSizesToArray( (uint8*)bufOutput.Base() );
	bufOutput.SeekPut( CUtlBuffer::SEEK_HEAD, unSize );
	return true;
}

//----------------------------------------------------------------------------
// Purpose: Adds the relevant message bits to create this object to the
//			message. This will be called whenever a subscriber is added.
//----------------------------------------------------------------------------
bool CProtoBufSharedObjectBase::BAddToMessage( CUtlBuffer & bufOutput )  const
{
	const ::google::protobuf::Message & msg = *GetPObject();
	SerializeToBuffer( msg, bufOutput );
	return true;
}


//----------------------------------------------------------------------------
// Purpose: Adds the relevant message bits to create this object to the
//			message. This will be called whenever a subscriber is added.
//----------------------------------------------------------------------------
bool CProtoBufSharedObjectBase::BAddToMessage( std::string *pBuffer )  const
{
	const ::google::protobuf::Message & msg = *GetPObject();

	return msg.SerializeToString( pBuffer );
}

//----------------------------------------------------------------------------
// Purpose: Parses the message bits for creating this object from the message.
//			This will be called on the client/gameserver when it first learns
//			about the item.
//----------------------------------------------------------------------------
bool CProtoBufSharedObjectBase::BParseFromMemcached( CUtlBuffer & buffer ) 
{
	::google::protobuf::Message & msg = *GetPObject();
	return msg.ParseFromArray( buffer.Base(), buffer.TellMaxPut() );
}

//----------------------------------------------------------------------------
// Purpose: Adds the relevant message bits to create this object to the
//			message. This will be called whenever a subscriber is added.
//----------------------------------------------------------------------------
bool CProtoBufSharedObjectBase::BAddToMemcached( CUtlBuffer & bufOutput )  const
{
	const ::google::protobuf::Message & msg = *GetPObject();
	SerializeToBuffer( msg, bufOutput );
	return true;
}

/*

//----------------------------------------------------------------------------
// Purpose: Adds all the information required for this object on the client
//----------------------------------------------------------------------------
bool CProtoBufSharedObjectBase::BAppendToMessage( CUtlBuffer & bufOutput ) const
{
	const ::google::protobuf::Message & msg = *GetPObject();

	uint32 unSize = msg.ByteSize();
	bufOutput.EnsureCapacity( bufOutput.Size() + unSize );
	msg.SerializeWithCachedSizesToArray( (uint8*)bufOutput.Base() + bufOutput.TellPut() );
	bufOutput.SeekPut( CUtlBuffer::SEEK_HEAD, bufOutput.TellPut() + unSize );
	return true;
}

//----------------------------------------------------------------------------
// Purpose: Adds all the information required for this object on the client
//----------------------------------------------------------------------------
bool CProtoBufSharedObjectBase::BAppendToMessage( std::string *pBuffer ) const
{
	const ::google::protobuf::Message & msg = *GetPObject();

	msg.AppendToString( pBuffer );
	return true;
}
*/


//----------------------------------------------------------------------------
::google::protobuf::Message *CProtoBufSharedObjectBase::BuildDestroyToMessage( const ::google::protobuf::Message & msg )
{
	const ::google::protobuf::Descriptor *pDescriptor = msg.GetDescriptor();
	::google::protobuf::Message *pMessageToSend = msg.New();

	for( int nField = 0; nField < pDescriptor->field_count(); nField++ )
	{
		const ::google::protobuf::FieldDescriptor *pFieldDescriptor = pDescriptor->field( nField );
		if( !IsKeyField( pFieldDescriptor ) )
			continue;

		CopyProtoBufField( *pMessageToSend, msg, pFieldDescriptor, pFieldDescriptor );
	}

	return pMessageToSend;
}

//----------------------------------------------------------------------------
// Purpose: Adds just the key fields to the message
//----------------------------------------------------------------------------
bool CProtoBufSharedObjectBase::BAddDestroyToMessage( CUtlBuffer & bufDestroy ) const
{
	const ::google::protobuf::Message & msg = *GetPObject();

	::google::protobuf::Message *pMessageToSend = BuildDestroyToMessage( msg );

	SerializeToBuffer( *pMessageToSend, bufDestroy );
	delete pMessageToSend;
	return true;
}

//----------------------------------------------------------------------------
// Purpose: Adds just the key fields to the message
//----------------------------------------------------------------------------
bool CProtoBufSharedObjectBase::BAddDestroyToMessage( std::string *pBuffer ) const
{
	const ::google::protobuf::Message & msg = *GetPObject();

	::google::protobuf::Message *pMessageToSend = BuildDestroyToMessage( msg );
	pMessageToSend->SerializeToString( pBuffer );
	delete pMessageToSend;
	return true;
}

#endif //GC

//----------------------------------------------------------------------------
// Purpose: Copy the data from the specified schema shared object into this. 
//			Both objects must be of the same type.
//----------------------------------------------------------------------------
void CProtoBufSharedObjectBase::Copy( const CSharedObject & soRHS )
{
	Assert( GetTypeID() == soRHS.GetTypeID() );
	const CProtoBufSharedObjectBase & soRHSBase = (CProtoBufSharedObjectBase &)soRHS;
	GetPObject()->CopyFrom( *soRHSBase.GetPObject() );
}


//----------------------------------------------------------------------------
// Purpose: Claims all memory for the object.
//----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CProtoBufSharedObjectBase::Validate( CValidator &validator, const char *pchName )
{
	CSharedObject::Validate( validator, pchName );

	// these are INSIDE the function instead of outside so the interface 
	// doesn't change
	VALIDATE_SCOPE();
}
#endif


} // namespace GCSDK


