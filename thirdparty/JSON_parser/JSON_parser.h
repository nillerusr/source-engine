/* See JSON_parser.c for copyright information and licensing. */

#ifndef JSON_PARSER_H
#define JSON_PARSER_H

/* JSON_parser.h */


#include <stddef.h>

/* Windows DLL stuff */
#ifdef JSON_PARSER_DLL
#   ifdef _MSC_VER
#	    ifdef JSON_PARSER_DLL_EXPORTS
#		    define JSON_PARSER_DLL_API __declspec(dllexport)
#	    else
#		    define JSON_PARSER_DLL_API __declspec(dllimport)
#       endif
#   else
#	    define JSON_PARSER_DLL_API 
#   endif
#else
#	define JSON_PARSER_DLL_API 
#endif

/* Determine the integer type use to parse non-floating point numbers */
#if __STDC_VERSION__ >= 199901L || HAVE_LONG_LONG == 1
typedef long long JSON_int_t;
#define JSON_PARSER_INTEGER_SSCANF_TOKEN "%lld"
#define JSON_PARSER_INTEGER_SPRINTF_TOKEN "%lld"
#else 
typedef long JSON_int_t;
#define JSON_PARSER_INTEGER_SSCANF_TOKEN "%ld"
#define JSON_PARSER_INTEGER_SPRINTF_TOKEN "%ld"
#endif


#ifdef __cplusplus
extern "C" {
#endif 

typedef enum 
{
    JSON_E_NONE = 0,
    JSON_E_INVALID_CHAR,
    JSON_E_INVALID_KEYWORD,
    JSON_E_INVALID_ESCAPE_SEQUENCE,
    JSON_E_INVALID_UNICODE_SEQUENCE,
    JSON_E_INVALID_NUMBER,
    JSON_E_NESTING_DEPTH_REACHED,
    JSON_E_UNBALANCED_COLLECTION,
    JSON_E_EXPECTED_KEY,
    JSON_E_EXPECTED_COLON,
    JSON_E_OUT_OF_MEMORY
} JSON_error;

typedef enum 
{
    JSON_T_NONE = 0,
    JSON_T_ARRAY_BEGIN,
    JSON_T_ARRAY_END,
    JSON_T_OBJECT_BEGIN,
    JSON_T_OBJECT_END,
    JSON_T_INTEGER,
    JSON_T_FLOAT,
    JSON_T_NULL,
    JSON_T_TRUE,
    JSON_T_FALSE,
    JSON_T_STRING,
    JSON_T_KEY,
    JSON_T_MAX
} JSON_type;

typedef struct JSON_value_struct {
    union {
        JSON_int_t integer_value;
        
        double float_value;
        
        struct {
            const char* value;
            size_t length;
        } str;
    } vu;
} JSON_value;

typedef struct JSON_parser_struct* JSON_parser;

/*! \brief JSON parser callback 

    \param ctx The pointer passed to new_JSON_parser.
    \param type An element of JSON_type but not JSON_T_NONE.    
    \param value A representation of the parsed value. This parameter is NULL for
        JSON_T_ARRAY_BEGIN, JSON_T_ARRAY_END, JSON_T_OBJECT_BEGIN, JSON_T_OBJECT_END,
        JSON_T_NULL, JSON_T_TRUE, and JSON_T_FALSE. String values are always returned
        as zero-terminated C strings.

    \return Non-zero if parsing should continue, else zero.
*/    
typedef int (*JSON_parser_callback)(void* ctx, int type, const JSON_value* value);


/**
   A typedef for allocator functions semantically compatible with malloc().
*/
typedef void* (*JSON_malloc_t)(size_t n);
/**
   A typedef for deallocator functions semantically compatible with free().
*/
typedef void (*JSON_free_t)(void* mem);

/*! \brief The structure used to configure a JSON parser object 
*/
typedef struct {
    /** Pointer to a callback, called when the parser has something to tell
        the user. This parameter may be NULL. In this case the input is
        merely checked for validity.
    */
    JSON_parser_callback    callback;
    /**
       Callback context - client-specified data to pass to the
       callback function. This parameter may be NULL.
    */
    void*                   callback_ctx;
    /** Specifies the levels of nested JSON to allow. Negative numbers yield unlimited nesting.
        If negative, the parser can parse arbitrary levels of JSON, otherwise
        the depth is the limit.
    */
    int                     depth;
    /**
       To allow C style comments in JSON, set to non-zero.
    */
    int                     allow_comments;
    /**
       To decode floating point numbers manually set this parameter to
       non-zero.
    */
    int                     handle_floats_manually;
    /**
       The memory allocation routine, which must be semantically
       compatible with malloc(3). If set to NULL, malloc(3) is used.

       If this is set to a non-NULL value then the 'free' member MUST be
       set to the proper deallocation counterpart for this function.
       Failure to do so results in undefined behaviour at deallocation
       time.
    */
    JSON_malloc_t       malloc;
    /**
       The memory deallocation routine, which must be semantically
       compatible with free(3). If set to NULL, free(3) is used.

       If this is set to a non-NULL value then the 'alloc' member MUST be
       set to the proper allocation counterpart for this function.
       Failure to do so results in undefined behaviour at deallocation
       time.
    */
    JSON_free_t         free;
} JSON_config;

/*! \brief Initializes the JSON parser configuration structure to default values.

    The default configuration is
    - 127 levels of nested JSON (depends on JSON_PARSER_STACK_SIZE, see json_parser.c)
    - no parsing, just checking for JSON syntax
    - no comments
    - Uses realloc() for memory de/allocation.

    \param config. Used to configure the parser.
*/
JSON_PARSER_DLL_API void init_JSON_config(JSON_config * config);

/*! \brief Create a JSON parser object 

    \param config. Used to configure the parser. Set to NULL to use
        the default configuration. See init_JSON_config.  Its contents are
        copied by this function, so it need not outlive the returned
        object.
    
    \return The parser object, which is owned by the caller and must eventually
    be freed by calling delete_JSON_parser().
*/
JSON_PARSER_DLL_API JSON_parser new_JSON_parser(JSON_config const* config);

/*! \brief Destroy a previously created JSON parser object. */
JSON_PARSER_DLL_API void delete_JSON_parser(JSON_parser jc);

/*! \brief Parse a character.

    \return Non-zero, if all characters passed to this function are part of are valid JSON.
*/
JSON_PARSER_DLL_API int JSON_parser_char(JSON_parser jc, int next_char);

/*! \brief Finalize parsing.

    Call this method once after all input characters have been consumed.
    
    \return Non-zero, if all parsed characters are valid JSON, zero otherwise.
*/
JSON_PARSER_DLL_API int JSON_parser_done(JSON_parser jc);

/*! \brief Determine if a given string is valid JSON white space 

    \return Non-zero if the string is valid, zero otherwise.
*/
JSON_PARSER_DLL_API int JSON_parser_is_legal_white_space_string(const char* s);

/*! \brief Gets the last error that occurred during the use of JSON_parser.

    \return A value from the JSON_error enum.
*/
JSON_PARSER_DLL_API int JSON_parser_get_last_error(JSON_parser jc);

/*! \brief Re-sets the parser to prepare it for another parse run.

    \return True (non-zero) on success, 0 on error (e.g. !jc).
*/
JSON_PARSER_DLL_API int JSON_parser_reset(JSON_parser jc);


#ifdef __cplusplus
}
#endif 
    

#endif /* JSON_PARSER_H */
