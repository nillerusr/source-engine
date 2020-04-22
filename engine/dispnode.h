//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DISPNODE_H
#define DISPNODE_H
#ifdef _WIN32
#pragma once
#endif

//=========== (C) Copyright 2000 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
// $Revision: $
// $NoKeywords: $
//=============================================================================


//=============================================================================
//
// Displacement Node Class
//
//  "Neighbor Nodes"     "Node Vert Indices"
//         1                 6----1----7    
//         |                 | \  |  / |
//     0 --+-- 2             |   \|/   |    "Corner {SW, SE, NW, NE} : {4, 5, 6, 7}"
//         |                 0 -Orig-- 2   
//         3                 |   /|\   |    "Edge {W, N, E, S} : {0, 1, 2, 3}"
//                           | /  |  \ |
//                           4----3----5
//

class CDispNode
{
public:

    enum{ W = 0, N, E, S, SW, SE, NW, NE };    

    //=========================================================================
    //
    // Construction/Decontruction
    //
    CDispNode() {};
    ~CDispNode() {};

    //=========================================================================
    //
    // 
    //
    inline void SetBoundingBox( const Vector& bbMin, const Vector& bbMax );
    inline void GetBoundingBox( Vector& bbMin, Vector& bbMax );
    inline void SetErrorTerm( float error );
    inline float GetErrorTerm( void );
    inline void SetNeighborNodeIndex( int direction, int nodeIndex );
    inline int GetNeighborNodeIndex( int direction );
    inline void SetOrigVertexIndex( int vertIndex );
    inline int GetOrigVertexIndex( void );
    inline void SetVertexIndex( int direction, int vertIndex );
    inline int GetVertexIndex( int direction );
    inline void SetTouched( bool bTouched );
    inline bool IsTouched( void );
    inline void SetActivity( bool bActive );
    inline bool IsActive( void );

private:

    Vector  m_BBox[2];                      // axial-aligned bounding box
    float   m_ErrorTerm;                    // LOD error term
    int     m_NeighborNodeIndices[4];       // neighbor node indices
    int     m_OrigVertIndex;                // origin vertex index
    int     m_VertIndices[8];               // neighboring vertex indices
    bool    m_bTouched;                     // "touched" flag
    bool    m_bActive;                      // "active" flag -- rendering
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CDispNode::SetBoundingBox( const Vector& bbMin, const Vector& bbMax )
{
	VectorCopy( bbMin, m_BBox[0] );
	VectorCopy( bbMax, m_BBox[1] );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CDispNode::GetBoundingBox( Vector& bbMin, Vector& bbMax )
{
	VectorCopy( m_BBox[0], bbMin );
	VectorCopy( m_BBox[1], bbMax );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CDispNode::SetErrorTerm( float error )
{
    m_ErrorTerm = error;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline float CDispNode::GetErrorTerm( void )
{
    return m_ErrorTerm;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CDispNode::SetNeighborNodeIndex( int direction, int nodeIndex )
{
    m_NeighborNodeIndices[direction] = nodeIndex;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline int CDispNode::GetNeighborNodeIndex( int direction )
{
    return m_NeighborNodeIndices[direction];
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CDispNode::SetOrigVertexIndex( int vertIndex )
{
    m_OrigVertIndex = vertIndex;
}

    
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline int CDispNode::GetOrigVertexIndex( void )
{
    return m_OrigVertIndex;
}

    
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CDispNode::SetVertexIndex( int direction, int vertIndex )
{
    m_VertIndices[direction] = vertIndex;
}

    
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline int CDispNode::GetVertexIndex( int direction )
{
    return m_VertIndices[direction];
}

    
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CDispNode::SetTouched( bool bTouched )
{
    m_bTouched = bTouched;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline bool CDispNode::IsTouched( void )
{
    return m_bTouched;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CDispNode::SetActivity( bool bActive )
{
    m_bActive = bActive;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline bool CDispNode::IsActive( void )
{
    return m_bActive;
}
#endif // DISPNODE_H
