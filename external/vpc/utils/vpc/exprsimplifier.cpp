//===== Copyright (c) 1996-2006, Valve Corporation, All rights reserved. ======//
//
// Purpose: 	ExprSimplifier builds a binary tree from an infix expression (in the
//				form of a character array).
//
//===========================================================================//
#include "vpc.h"

static ExprTree				mExprTree;							// Tree representation of the expression
static char					mCurToken;							// Current token read from the input expression
static const char			*mExpression;						// Array of the expression characters
static int					mCurPosition;						// Current position in the input expression
static char					mIdentifier[MAX_IDENTIFIER_LEN];	// Stores the identifier string
static GetSymbolProc_t		g_pGetSymbolProc;

//-----------------------------------------------------------------------------
//	Sets mCurToken to the next token in the input string. Skips all whitespace.
//-----------------------------------------------------------------------------
static char GetNextToken( void )
{
	// while whitespace, Increment CurrentPosition
	while( mExpression[mCurPosition] == ' ' )
		++mCurPosition;
    
	// CurrentToken = Expression[CurrentPosition]
	mCurToken = mExpression[mCurPosition++];
  
	return mCurToken;
}


//-----------------------------------------------------------------------------
//	Utility funcs
//-----------------------------------------------------------------------------
static void FreeNode( ExprNode *node )
{
	delete node;
}

static ExprNode *AllocateNode( void )
{
	return new ExprNode;
}

static void FreeTree( ExprTree& node )
{
	if(!node)
		return;

	FreeTree(node->left);
	FreeTree(node->right);
	FreeNode(node);
	node = 0;
}

static bool IsConditional( const char token )
{
	char nextchar = ' ';
	if ( token == OR_OP || token == AND_OP )
	{
		nextchar = mExpression[mCurPosition++];
		if ( (token & nextchar) == token )
		{
			return true;
		}
		else
			g_pVPC->VPCSyntaxError( "Bad expression token: %c %c", token, nextchar );
	}

	return false;
}

static bool IsNotOp( const char token )
{
	if ( token == NOT_OP )
		return true;
	else
		return false;
}

static bool IsIdentifierOrConstant( const char token )
{
	bool success = false;
	if ( token == '$' )
	{
		// store the entire identifier
		int i = 0;
		mIdentifier[i++] = token;
		while( (isalnum( mExpression[mCurPosition] ) || mExpression[mCurPosition] == '_') && i < MAX_IDENTIFIER_LEN )
		{
			mIdentifier[i] = mExpression[mCurPosition];
			++mCurPosition;
			++i;
		}

		if ( i < MAX_IDENTIFIER_LEN - 1 )
		{
			mIdentifier[i] = '\0';
			success = true;
		}
	}
	else
	{
		if ( isdigit( token ) )
		{
			int i = 0;
			mIdentifier[i++] = token;
			while( isdigit( mExpression[mCurPosition] ) && ( i < MAX_IDENTIFIER_LEN ) )
			{
				mIdentifier[i] = mExpression[mCurPosition];
				++mCurPosition;
				++i;
			}
			if ( i < MAX_IDENTIFIER_LEN - 1 )
			{
				mIdentifier[i] = '\0';
				success = true;
			}
		}
	}

	return success;
}

static void MakeExprNode( ExprTree &tree, char token, Kind kind, ExprTree left, ExprTree right )
{
	tree = AllocateNode();
	tree->left = left;
	tree->right = right;
	tree->kind = kind;

	switch ( kind )
	{
	case CONDITIONAL:
		tree->data.cond = token;
		break;
	case LITERAL:
		if ( isdigit( mIdentifier[0] ) )
		{
			tree->data.value = atoi( mIdentifier ) != 0;
		}
		else
		{
			tree->data.value = g_pGetSymbolProc( mIdentifier );
		}
		break;
	case NOT:
		break;
	default:
		g_pVPC->VPCError( "Error in ExpTree" );
	}
}

static void MakeExpression( ExprTree& tree );
//-----------------------------------------------------------------------------
//	Makes a factor :: { <expression> } | <identifier>.
//-----------------------------------------------------------------------------
static void MakeFactor( ExprTree& tree )
{
	if ( mCurToken == '(' )
	{
		// Get the next token
		GetNextToken();

		// Make an expression, setting Tree to point to it
		MakeExpression( tree );
	}
	else if ( IsIdentifierOrConstant( mCurToken ) )
	{
		// Make a literal node, set Tree to point to it, set left/right children to NULL. 
		MakeExprNode( tree, mCurToken, LITERAL, NULL, NULL );
	}
	else if ( IsNotOp( mCurToken ) )
	{
		// do nothing
		return;
	}
	else
	{
		// This must be a bad token
		g_pVPC->VPCSyntaxError( "Bad expression token: %c", mCurToken );
	}

	// Get the next token
	GetNextToken();
}


//-----------------------------------------------------------------------------
//	Makes a term :: <factor> { <not> }.
//-----------------------------------------------------------------------------
static void MakeTerm( ExprTree& tree )
{
	// Make a factor, setting Tree to point to it
	MakeFactor( tree );

	// while the next token is !
	while( IsNotOp( mCurToken ) )
	{
		// Make an operator node, setting left child to Tree and right to NULL. (Tree points to new node)
		MakeExprNode( tree, mCurToken, NOT, tree, NULL );

		// Get the next token.
		GetNextToken();

		// Make a factor, setting the right child of Tree to point to it.
		MakeFactor(tree->right);
	}
}


//-----------------------------------------------------------------------------
//	Makes a complete expression :: <term> { <cond> <term> }.
//-----------------------------------------------------------------------------
static void MakeExpression( ExprTree& tree )
{
	// Make a term, setting Tree to point to it
	MakeTerm( tree );

	// while the next token is a conditional
	while ( IsConditional( mCurToken ) )
	{
		// Make a conditional node, setting left child to Tree and right to NULL. (Tree points to new node)
		MakeExprNode( tree, mCurToken, CONDITIONAL, tree, NULL );

		// Get the next token.
		GetNextToken();

		// Make a term, setting the right child of Tree to point to it.
		MakeTerm( tree->right );
	}
}


//-----------------------------------------------------------------------------
//	returns true for success, false for failure
//-----------------------------------------------------------------------------
static bool BuildExpression( void )
{
	// Get the first token, and build the tree.
	GetNextToken();

	MakeExpression( mExprTree );
	return true;
}

//-----------------------------------------------------------------------------
//	returns the value of the node after resolving all children
//-----------------------------------------------------------------------------
static bool SimplifyNode( ExprTree& node )
{
	if( !node )
		return false;

	// Simplify the left and right children of this node
	bool leftVal	= SimplifyNode(node->left);
	bool rightVal	= SimplifyNode(node->right);

	// Simplify this node
	switch( node->kind )
	{
	case NOT:
		// the child of '!' is always to the right
		node->data.value = !rightVal;
		break;
	
	case CONDITIONAL:
		if ( node->data.cond == AND_OP )
		{
			node->data.value = leftVal && rightVal;
		}
		else // OR_OP
		{	
			node->data.value = leftVal || rightVal;
		}
		break;

	default: // LITERAL
		break;
	}

	// This node has beed resolved
	node->kind = LITERAL;
	return node->data.value;
}


//-----------------------------------------------------------------------------
//	Interface to solve a conditional expression. Returns false on failure.
//-----------------------------------------------------------------------------
bool EvaluateExpression( bool &result, const char *InfixExpression, GetSymbolProc_t pGetSymbolProc )
{
	if ( !InfixExpression )
		return false;

	g_pGetSymbolProc = pGetSymbolProc;

	bool success	= false;
	mExpression		= InfixExpression;
	mExprTree		= 0;
	mCurPosition	= 0;

	// Building the expression tree will fail on bad syntax
	if ( BuildExpression() )
	{
		success = true;
		result = SimplifyNode( mExprTree );
	}

	FreeTree( mExprTree );
	return success;
}












