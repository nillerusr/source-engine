/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#ifndef _GMCODETREE_H_
#define _GMCODETREE_H_

#include "gmConfig.h"
#include "gmMem.h"
#include "gmMemChain.h"
#include "gmLog.h"
#include "gmScanner.h"

#define GMCODETREE_NUMCHILDREN  4

// fwd decl
struct gmCodeTreeNode;

/// \class gmCodeTree
/// \brief gmCodeTree is a singleton class for creating code trees.
class gmCodeTree
{
protected:
  gmCodeTree();

public:
  ~gmCodeTree();

  /// \brief Get() will return the singlton parser.
  static gmCodeTree &Get();

  /// \brief FreeMemory() will free all memory allocated by the code tree.  must be unlocked
  void FreeMemory();

  /// \brief Lock() will create a code tree for the passed script.  Note that the code tree is valid until
  ///        Unlock() is called.
  /// \param a_script is a null terminated script string.
  /// \return the number of errors encounted when parsing.
  /// \sa Unlock()
  int Lock(const char * a_script, gmLog * a_log = NULL);

  /// \brief Unlock() will unlock the singleton code tree such that it may be used again.
  /// \return 0 on success
  /// \sa Lock()
  int Unlock();

  /// \brief GetCodeTree() will return the code tree resulting from a Lock() operation.
  /// \return NULL on failure
  /// \sa Lock()
  const gmCodeTreeNode * GetCodeTree() const;

  inline gmLog * GetLog() const { return m_log; }

  /// \brief Alloc() will return memory from the code tree memory pool.  This method is used by the
  ///        parser when building the code tree.
  /// \return NULL on failure
  void * Alloc(int a_size, int a_align = GM_DEFAULT_ALLOC_ALIGNMENT);

#if GM_COMPILE_DEBUG
  
  /// \brief Print() will write the tree to the given file.  this is purely for debugging.
  /// \param a_fp is an open file for writing.
  void Print(FILE * a_fp);

#endif // GM_COMPILE_DEBUG

private:

  bool m_locked;
  int m_errors;
  gmLog * m_log;
  gmMemChain m_mem;
};



/// \enum gmCodeTreeNodeType
/// \brief gmCodeTreeNodeType indicates the type of a gmCodeTreeNode.
enum gmCodeTreeNodeType
{
  CTNT_INVALID = 0,
  CTNT_DECLARATION,
  CTNT_STATEMENT,
  CTNT_EXPRESSION,
};



/// \enum gmCodeTreeNodeDeclarationType
/// \brief if a tree node is of type CTNT_DECLARATION, gmCodeTreeNodeDeclarationType are the sub types
enum gmCodeTreeNodeDeclarationType
{
  CTNDT_PARAMETER = 0,
  CTNDT_VARIABLE,
};



/// \enum gmCodeTreeVariableType
/// \brief if a treenode is CTNT_DECLARATION, CTNDT_VARIABLE, this is the type of variable decl.
enum gmCodeTreeVariableType
{
  CTVT_LOCAL = 0,
  CTVT_GLOBAL,
  CTVT_MEMBER,
};



/// \enum gmCodeTreeNodeStatementType
/// \brief if a tree node is of type CTNT_STATEMENT, gmCodeTreeNodeStatementType are the sub types
enum gmCodeTreeNodeStatementType
{
  CTNST_INVALID = 0,
  CTNST_RETURN,
  CTNST_BREAK,
  CTNST_CONTINUE,
  CTNST_FOR,
  CTNST_FOREACH,
  CTNST_WHILE,
  CTNST_DOWHILE,
  CTNST_IF,
  CTNST_COMPOUND,
};



/// \enum gmCodeTreeNodeExpressionType
/// \brief
enum gmCodeTreeNodeExpressionType
{
  CTNET_INVALID = 0,
  CTNET_OPERATION,
  CTNET_CONSTANT,
  CTNET_IDENTIFIER,
  CTNET_THIS,
  CTNET_CALL,
  CTNET_FUNCTION,
  CTNET_TABLE,
};



/// \enum gmCodeTreeNodeOperationType
/// \brief
enum gmCodeTreeNodeOperationType
{
  CTNOT_INVALID = 0,
  CTNOT_DOT,
  CTNOT_UNARY_PLUS,
  CTNOT_UNARY_MINUS,
  CTNOT_UNARY_COMPLEMENT,
  CTNOT_UNARY_NOT, 
  CTNOT_ARRAY_INDEX,
  CTNOT_TIMES,
  CTNOT_DIVIDE,
  CTNOT_REM,
  CTNOT_ADD,
  CTNOT_MINUS,
  CTNOT_LT,
  CTNOT_GT,
  CTNOT_LTE,
  CTNOT_GTE,
  CTNOT_EQ,
  CTNOT_NEQ,
  CTNOT_AND,
  CTNOT_OR,
  CTNOT_BIT_OR,
  CTNOT_BIT_XOR,
  CTNOT_BIT_AND,
  CTNOT_SHIFT_LEFT,
  CTNOT_SHIFT_RIGHT,
  CTNOT_ASSIGN,
  CTNOT_ASSIGN_FIELD,
  CTNOT_MAX,
};



/// \enum gmCodeTreeNodeConstantType
enum gmCodeTreeNodeConstantType
{
  CTNCT_INVALID = 0,
  CTNCT_INT,
  CTNCT_FLOAT,
  CTNCT_STRING,
  CTNCT_NULL,
};



/// \union gmCodeTreeNodeUnion
/// \brief
union gmCodeTreeNodeData
{
  char * m_string;
  int m_iValue;
  float m_fValue;
};



/// \struct gmCodeTreeNode
/// \brief gmCodeTreeNode is the tree node structure used to represent the game monkey script syntax tree.
struct gmCodeTreeNode
{
  // flags
  enum
  {
    CTN_POP     = (1 << 0),
    CTN_MEMBER  = (1 << 1),
  };

  /// \brief Create() will create a tree node.  the singleton gmCodeTree must be locked.
  /// \return a tree node
  static gmCodeTreeNode * Create(gmCodeTreeNodeType a_type, int a_subType, int a_lineNumber, int a_subTypeType = 0);

  /// \brief SetChild() will set the child at the given index.
  /// \param a_node is the child node, whose parent pointer will be assigned to this.
  void SetChild(int a_index, gmCodeTreeNode * a_node);

  /// \brief ConstantFold() will pull child nodes into this node, and make this node a constant if possible
  bool ConstantFold();

  gmCodeTreeNodeType m_type;
  int m_subType;
  int m_subTypeType;
  int m_flags;

  gmCodeTreeNode * m_children[GMCODETREE_NUMCHILDREN];
  gmCodeTreeNode * m_sibling;
  gmCodeTreeNode * m_parent;

  int m_lineNumber;
  gmCodeTreeNodeData m_data;
};

//
// misc lexing and parsing functions.
//

void gmProcessSingleQuoteString(char * a_string);
void gmProcessDoubleQuoteString(char * a_string);
int gmerror(char * a_message);
int gmparse(void);

#endif // _GMCODETREE_H_
