/*
    _____               __  ___          __            ____        _      __
   / ___/__ ___ _  ___ /  |/  /__  ___  / /_____ __ __/ __/_______(_)__  / /_
  / (_ / _ `/  ' \/ -_) /|_/ / _ \/ _ \/  '_/ -_) // /\ \/ __/ __/ / _ \/ __/
  \___/\_,_/_/_/_/\__/_/  /_/\___/_//_/_/\_\\__/\_, /___/\__/_/ /_/ .__/\__/
                                               /___/             /_/
                                             
  See Copyright Notice in gmMachine.h

*/

#include "gmConfig.h"
#include "gmCodeTree.h"

// Must be last header
#include "memdbgon.h"


gmCodeTreeNode * g_codeTree = NULL;



gmCodeTree::gmCodeTree() :
  m_mem(1, GMCODETREE_CHAINSIZE)
{
  g_codeTree = NULL;
  m_locked = false;
  m_errors = 0;
  m_log = 0;
}



gmCodeTree::~gmCodeTree()
{
  Unlock();
}



gmCodeTree &gmCodeTree::Get()
{
  static gmCodeTree codeTree;
  return codeTree;
}



void gmCodeTree::FreeMemory()
{
  if(m_locked == false)
  {
    m_mem.ResetAndFreeMemory();
  }
}



int gmCodeTree::Lock(const char * a_script, gmLog * a_log)
{
  if(m_locked == true) return 1;

  m_errors = 0;
  m_locked = true;
  m_log = a_log;
  g_codeTree = NULL;
  //gmdebug = 1;
  gmlineno = 1;

  // create a scan buffer
  YY_BUFFER_STATE buffer = gm_scan_string(a_script);
  if(buffer)
  {
    m_errors = gmparse();
    gm_delete_buffer(buffer);
  }
  return m_errors;
}



int gmCodeTree::Unlock()
{
  m_mem.Reset();
  g_codeTree = NULL;
  m_locked = false;
  m_errors = 0;
  m_log = NULL;
  return 0;
}



const gmCodeTreeNode * gmCodeTree::GetCodeTree() const
{
  return g_codeTree;
}



void * gmCodeTree::Alloc(int a_size, int a_align)
{
  return m_mem.AllocBytes(a_size, a_align);
}



#if GM_COMPILE_DEBUG

const char * gmGetOperatorTypeName(gmCodeTreeNodeOperationType a_type)
{
  switch(a_type)
  {
    case CTNOT_INVALID : return "CTNOT_INVALID";
    case CTNOT_DOT : return "CTNOT_DOT";
    case CTNOT_UNARY_PLUS : return "CTNOT_UNARY_PLUS";
    case CTNOT_UNARY_MINUS : return "CTNOT_UNARY_MINUS";
    case CTNOT_UNARY_NOT : return "CTNOT_UNARY_NOT";
    case CTNOT_UNARY_COMPLEMENT : return "CTNOT_UNARY_COMPLEMENT";
    case CTNOT_ARRAY_INDEX : return "CTNOT_ARRAY_INDEX";
    case CTNOT_TIMES : return "CTNOT_TIMES";
    case CTNOT_DIVIDE : return "CTNOT_DIVIDE";
    case CTNOT_REM : return "CTNOT_REM";
    case CTNOT_ADD : return "CTNOT_ADD";
    case CTNOT_MINUS : return "CTNOT_MINUS";
    case CTNOT_LT : return "CTNOT_LT";
    case CTNOT_GT : return "CTNOT_GT";
    case CTNOT_LTE : return "CTNOT_LTE";
    case CTNOT_GTE : return "CTNOT_GTE";
    case CTNOT_EQ : return "CTNOT_EQ";
    case CTNOT_NEQ : return "CTNOT_NEQ";
    case CTNOT_AND : return "CTNOT_AND";
    case CTNOT_OR : return "CTNOT_OR";
    case CTNOT_BIT_OR : return "CTNOT_BIT_OR";
    case CTNOT_BIT_XOR : return "CTNOT_BIT_XOR";
    case CTNOT_BIT_AND : return "CTNOT_BIT_AND";
    case CTNOT_SHIFT_LEFT : return "CTNOT_SHIFT_LEFT";
    case CTNOT_SHIFT_RIGHT : return "CTNOT_SHIFT_RIGHT";
    case CTNOT_ASSIGN : return "CTNOT_ASSIGN";
    case CTNOT_ASSIGN_FIELD : return "CTNOT_ASSIGN_FIELD";
    default : break;
  }
  return "UNKNOWN OPERATOR TYPE";
};



static void PrintRecursive(const gmCodeTreeNode * a_node, FILE * a_fp, bool a_firstCall)
{
  if(a_node)
  {
    static int indent;
    int i;

    if(a_firstCall)
    {
      indent = 0;
    }

    indent += 2;

    while(a_node != NULL)
    {
      for(i = 0; i < indent; ++i)
        fprintf(a_fp, " ");

      if(a_node->m_type == CTNT_DECLARATION)
      {
        //
        // DECLARATIONS
        //
        switch(a_node->m_subType)
        {
          case CTNDT_PARAMETER : fprintf(a_fp, "CTNDT_PARAMETER:%04d"GM_NL, a_node->m_lineNumber); break;
          case CTNDT_VARIABLE : fprintf(a_fp, "CTNDT_VARIABLE:%04d, type %d"GM_NL, a_node->m_lineNumber, a_node->m_subTypeType); break;
          default : fprintf(a_fp, "UNKNOWN DECLARATION:"GM_NL); break;
        }
      }
      else if(a_node->m_type == CTNT_STATEMENT)
      {
        //
        // STATEMENTS
        //
        switch(a_node->m_subType)
        {
          case CTNST_RETURN : fprintf(a_fp, "CTNST_RETURN:%04d"GM_NL, a_node->m_lineNumber); break;
          case CTNST_BREAK : fprintf(a_fp, "CTNST_BREAK:%04d"GM_NL, a_node->m_lineNumber); break;
          case CTNST_CONTINUE : fprintf(a_fp, "CTNST_CONTINUE:%04d"GM_NL, a_node->m_lineNumber); break;
          case CTNST_FOR : fprintf(a_fp, "CTNST_FOR:%04d"GM_NL, a_node->m_lineNumber); break;
          case CTNST_FOREACH : fprintf(a_fp, "CTNST_FOREACH:%04d"GM_NL, a_node->m_lineNumber); break;
          case CTNST_WHILE : fprintf(a_fp, "CTNST_WHILE:%04d"GM_NL, a_node->m_lineNumber); break;
          case CTNST_DOWHILE : fprintf(a_fp, "CTNST_DOWHILE:%04d"GM_NL, a_node->m_lineNumber); break;
          case CTNST_IF : fprintf(a_fp, "CTNST_IF:%04d"GM_NL, a_node->m_lineNumber); break;
          case CTNST_COMPOUND : fprintf(a_fp, "CTNST_COMPOUND:%04d"GM_NL, a_node->m_lineNumber); break;
          default : fprintf(a_fp, "UNKNOWN STATEMENT:"GM_NL); break;
        }
      }
      else if(a_node->m_type == CTNT_EXPRESSION)
      {
        //
        // EXPRESSIONS
        //
        switch(a_node->m_subType)
        {
          case CTNET_OPERATION :
          {
            if(a_node->m_subTypeType < CTNOT_MAX)
            {
              fprintf(a_fp, "CTNET_OPERATION:%04d : %s"GM_NL, a_node->m_lineNumber, gmGetOperatorTypeName((gmCodeTreeNodeOperationType) a_node->m_subTypeType));
            }
            else
            { 
              fprintf(a_fp, "UNKNOWN CTNET_OPERATION"GM_NL);
            }
            break;
          }

          case CTNET_CONSTANT :
          {
            switch(a_node->m_subTypeType)
            {
              case CTNCT_INT : fprintf(a_fp, "CTNCT_INT:%04d : %d"GM_NL, a_node->m_lineNumber, a_node->m_data.m_iValue); break;
              case CTNCT_FLOAT : fprintf(a_fp, "CTNCT_FLOAT:%04d : %f"GM_NL, a_node->m_lineNumber, a_node->m_data.m_fValue); break;
              case CTNCT_STRING : fprintf(a_fp, "CTNCT_STRING:%04d : %s"GM_NL, a_node->m_lineNumber, a_node->m_data.m_string); break;
              case CTNCT_NULL : fprintf(a_fp, "CTNCT_NULL:%04d"GM_NL, a_node->m_lineNumber); break;
              default: fprintf(a_fp, "UNKNOWN CTNET_CONSTANT"GM_NL);
            }
            break;
          }

          case CTNET_IDENTIFIER : fprintf(a_fp, "CTNET_IDENTIFIER:%04d : %s"GM_NL, a_node->m_lineNumber, a_node->m_data.m_string); break;
          case CTNET_THIS : fprintf(a_fp, "CTNET_THIS:%04d"GM_NL, a_node->m_lineNumber); break;
          case CTNET_CALL : fprintf(a_fp, "CTNET_CALL:%04d"GM_NL, a_node->m_lineNumber); break;
          case CTNET_FUNCTION : fprintf(a_fp, "CTNET_FUNCTION:%04d"GM_NL, a_node->m_lineNumber); break;
          case CTNET_TABLE : fprintf(a_fp, "CTNET_TABLE:%04d"GM_NL, a_node->m_lineNumber); break;
          default : fprintf(a_fp, "UNKNOWN EXPRESSION:"GM_NL); break;
        }
      }
      else
      {
        fprintf(a_fp, "UNKNOWN NODE TYPE"GM_NL);
      }

      // print the child nodes
      for(i = 0; i < GMCODETREE_NUMCHILDREN; ++i)
      {
        if(a_node->m_children[i])
        {
          PrintRecursive(a_node->m_children[i], a_fp, false);
        }
      }
      a_node = a_node->m_sibling;

    } // while(a_node != NULL)

    indent -= 2;
  }
}


void gmCodeTree::Print(FILE * a_fp)
{
  if(m_locked)
  {
    PrintRecursive(g_codeTree, a_fp, true);
  }
}


#endif // GM_COMPILE_DEBUG



gmCodeTreeNode * gmCodeTreeNode::Create(gmCodeTreeNodeType a_type, int a_subType, int a_lineNumber, int a_subTypeType)
{
  gmCodeTreeNode * node = (gmCodeTreeNode *) gmCodeTree::Get().Alloc(sizeof(gmCodeTreeNode), GM_DEFAULT_ALLOC_ALIGNMENT);
  GM_ASSERT(node != NULL);
  memset(node, 0, sizeof(gmCodeTreeNode));
  node->m_type = a_type;
  node->m_subType = a_subType;
  node->m_lineNumber = a_lineNumber;
  node->m_subTypeType = a_subTypeType;
  node->m_flags = 0;
  return node;
}



void gmCodeTreeNode::SetChild(int a_index, gmCodeTreeNode * a_node)
{
  GM_ASSERT(a_index >= 0 && a_index < GMCODETREE_NUMCHILDREN);

  m_children[a_index] = a_node;
  if(a_node != NULL)
  {
    a_node->m_parent = this;
  }
}


static bool gmFold(float &a_r, float a_a, int a_op)
{
  switch(a_op)
  {
    case CTNOT_UNARY_PLUS : a_r = a_a; break;
    case CTNOT_UNARY_MINUS : a_r = -a_a; break;
    default: return false;
  }
  return true;
}


static bool gmFold(int &a_r, int a_a, int a_op)
{
  switch(a_op)
  {
    case CTNOT_UNARY_PLUS : a_r = a_a; break;
    case CTNOT_UNARY_MINUS : a_r = -a_a; break;
    case CTNOT_UNARY_NOT : a_r = !a_a; break;
    case CTNOT_UNARY_COMPLEMENT : a_r = ~a_a; break;
    default: return false;
  }
  return true;
}


#include <math.h>
static bool gmFold(float &a_r, float a_a, float a_b, int a_op)
{
  switch(a_op)
  {
    case CTNOT_TIMES : a_r = a_a * a_b; break;
    case CTNOT_DIVIDE : if(a_b == 0) return false; a_r = a_a / a_b; break;
    case CTNOT_REM : a_r = fmodf(a_a, a_b); break;
    case CTNOT_ADD : a_r = a_a + a_b; break;
    case CTNOT_MINUS : a_r = a_a - a_b; break;
    default: return false;
  }
  return true;
}


static bool gmFold(int &a_r, int a_a, int a_b, int a_op)
{
  switch(a_op)
  {
    case CTNOT_TIMES : a_r = a_a * a_b; break;
    case CTNOT_DIVIDE : if(a_b == 0) return false; a_r = a_a / a_b; break;
    case CTNOT_REM : a_r = a_a % a_b; break;
    case CTNOT_ADD : a_r = a_a + a_b; break;
    case CTNOT_MINUS : a_r = a_a - a_b; break;
    case CTNOT_BIT_OR : a_r = a_a | a_b; break;
    case CTNOT_BIT_XOR : a_r = a_a ^ a_b; break;
    case CTNOT_BIT_AND : a_r = a_a & a_b; break;
    case CTNOT_SHIFT_LEFT : a_r = a_a << a_b; break;
    case CTNOT_SHIFT_RIGHT : a_r = a_a >> a_b; break;
    default: return false;
  }
  return true;
}


bool gmCodeTreeNode::ConstantFold()
{
  if(m_type == CTNT_EXPRESSION && m_subType == CTNET_OPERATION)
  {
    bool possibleUnaryFold = false;
    bool possibleFold = false;
    bool intOnly = false;

    switch(m_subTypeType)
    {
      case CTNOT_UNARY_PLUS :
      case CTNOT_UNARY_MINUS :
        possibleUnaryFold = true;
        break;
      case CTNOT_UNARY_NOT :
      case CTNOT_UNARY_COMPLEMENT :
        possibleUnaryFold = true;
        intOnly = true;
        break;
      case CTNOT_TIMES :
      case CTNOT_DIVIDE :
      case CTNOT_REM :
      case CTNOT_ADD :
      case CTNOT_MINUS :
        possibleFold = true;
        break;
      case CTNOT_BIT_OR :
      case CTNOT_BIT_XOR :
      case CTNOT_BIT_AND :
      case CTNOT_SHIFT_LEFT :
      case CTNOT_SHIFT_RIGHT :
        possibleFold = true;
        intOnly = true;
        break;
      default:
        break;
    }

    if(possibleUnaryFold)
    {
      gmCodeTreeNode * l = m_children[0];
      if(l && l->m_type == CTNT_EXPRESSION && l->m_subType == CTNET_CONSTANT)
      {
        if(l->m_subTypeType == CTNCT_INT || (l->m_subTypeType == CTNCT_FLOAT && !intOnly))
        {
          // we can fold....
          m_children[0] = NULL;
          m_subType = CTNET_CONSTANT;

          if(l->m_subTypeType == CTNCT_INT)
          {
            gmFold(m_data.m_iValue, l->m_data.m_iValue, m_subTypeType);
            m_subTypeType = CTNCT_INT;
          }
          else if(l->m_subTypeType == CTNCT_FLOAT)
          {
            gmFold(m_data.m_fValue, l->m_data.m_fValue, m_subTypeType);
            m_subTypeType = CTNCT_FLOAT;
          }
          return true;
        }
      }
    }
    else if(possibleFold)
    {
      gmCodeTreeNode * l = m_children[0], * r = m_children[1];
      if((l && l->m_type == CTNT_EXPRESSION && l->m_subType == CTNET_CONSTANT) && 
         (r && r->m_type == CTNT_EXPRESSION && r->m_subType == CTNET_CONSTANT))
      {
        if((l->m_subTypeType == CTNCT_INT || (l->m_subTypeType == CTNCT_FLOAT && !intOnly)) && 
           (r->m_subTypeType == CTNCT_INT || (r->m_subTypeType == CTNCT_FLOAT && !intOnly)))
        {
          // we can fold....
          m_children[0] = NULL; m_children[1] = NULL;
          m_subType = CTNET_CONSTANT;
          if(l->m_subTypeType == CTNCT_INT && r->m_subTypeType == CTNCT_INT)
          {
            gmFold(m_data.m_iValue, l->m_data.m_iValue, r->m_data.m_iValue, m_subTypeType);
            m_subTypeType = CTNCT_INT;
          }
          else if(l->m_subTypeType == CTNCT_FLOAT && r->m_subTypeType == CTNCT_FLOAT)
          {
            gmFold(m_data.m_fValue, l->m_data.m_fValue, r->m_data.m_fValue, m_subTypeType);
            m_subTypeType = CTNCT_FLOAT;
          }
          else if(l->m_subTypeType == CTNCT_INT && r->m_subTypeType == CTNCT_FLOAT)
          {
            gmFold(m_data.m_fValue, (float) l->m_data.m_iValue, r->m_data.m_fValue, m_subTypeType);
            m_subTypeType = CTNCT_FLOAT;
          }
          else if(l->m_subTypeType == CTNCT_FLOAT && r->m_subTypeType == CTNCT_INT)
          {
            gmFold(m_data.m_fValue, l->m_data.m_fValue, (float) r->m_data.m_iValue, m_subTypeType);
            m_subTypeType = CTNCT_FLOAT;
          }
          return true;
        }
      }
    }
  }
  return false;
}


void gmProcessSingleQuoteString(char * a_string)
{
  char * c = a_string;
  char * r = a_string;

  while(*c)
  {
    if(c[0] == '`' && c[1] == '`' && c[2])
    {
      *(r++) = *c;
      c += 2;
      continue;
    }
    else if(c[0] == '`')
    {
      ++c;
      continue;
    }

    *(r++) = *(c++);
  }
  *r = '\0';
}



void gmProcessDoubleQuoteString(char * a_string)
{
  char * c = a_string;
  char * r = a_string;

  while(*c)
  {
    if(c[0] == '\"')
    {
      ++c;
      continue;
    }
    else if(c[0] == '\\')
    {
      switch(c[1])
      {
        case '0' : case '1' : case '2' : case '3' : case '4' : case '5' :
        case '6' : case '7' : case '8' : case '9' :
        {
          char buffer[4]; int i = 0;
          while(i < 3 && isdigit(c[i+1]))
          {
            buffer[i] = c[i+1];
            ++i;
          }
          buffer[i] = '\0';
          *r = (char) (atoi(buffer) & 0xff);
          c += (i - 1);
          break;
        }
        case 'a' : *r = '\a'; break;
        case 'b' : *r = '\b'; break;
        case 'f' : *r = '\f'; break;
        case 'n' : *r = '\n'; break;
        case 'r' : *r = '\r'; break;
        case 't' : *r = '\t'; break;
        case 'v' : *r = '\v'; break;
        case '\'' : *r = '\''; break;
        case '\"' : *r = '\"'; break;
        case '\\' : *r = '\\'; break;
        default: *r = c[1];
      }
      ++r;
      c += 2;
      continue;
    }
    *(r++) = *(c++);
  }
  *r = '\0';
}



int gmerror(char * a_message)
{
  gmCodeTree & ct = gmCodeTree::Get();
  if(ct.GetLog())
  {
    ct.GetLog()->LogEntry("error (%d) %s", gmlineno, a_message);
  }
  return 0;
}

