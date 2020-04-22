A JSON parser based on http://www.json.org's JSON_checker code. 

Copyright (c) 2007-2010 Jean Gressmann (jean@0x42.de). 

For license information, see JSON_parser.c.

JSON parser features:
- arbitrary levels of JSON object/array nesting, 
- C-style comments, 
- UTF-16 & escape sequence (\uXXXX) decoding to UTF-8.
- Manual processing of floating point values.

The parser processes UTF-8 encoded JSON only. 
JSON object keys and JSON strings are returned as UTF-8 encoded C strings.


