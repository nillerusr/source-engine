@echo Fixing cl.read.1.tlog errors in %cd%

@rem This used to use forfiles for the directory scanning, but that
@rem is about ten times slower than doing it all in Python.
@rem forfiles /m cl.read.1.tlog /s /c "cmd /c %~dp0fix_tlog.py @path"

@rem Run the Python script and let it do the iteration:
%~dp0fix_tlog.py
