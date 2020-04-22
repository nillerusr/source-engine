This directory contains tools that will be run when doing VS 11 /analyze builds. These stubs exist to prevent the normal link/lib/mt tools from running so that VS 11 /analyze builds can skip the link stage. That is currently necessary because VS 11 builds do not link correctly, but also gives a modest speedup.

The different tools are actually identical, but they print different messages because they print argv[0].
