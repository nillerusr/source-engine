FILE(REMOVE ${OUTPUT})

string(REPLACE " " ";" COMMAND "${COMMAND}")
execute_process(COMMAND ${COMMAND}
  OUTPUT_VARIABLE _stdout ERROR_VARIABLE _stderr RESULT_VARIABLE _res)
if ( NOT(_res EQUAL 0) )
  message(FATAL_ERROR "${COMMAND}:${_res}:${_stdout}:${_stderr}")
endif ()

LIST(GET COMMAND -1 _infile)

FILE(READ ${OUTPUT} _outputstring)
FILE(READ ${INPUT} _inputstring)
if (NOT(_outputstring STREQUAL _inputstring))
  message(FATAL_ERROR "files do not match: ${OUTPUT} ${INPUT}")
endif ()
