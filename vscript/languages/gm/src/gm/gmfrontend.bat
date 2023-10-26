rem Strip CR from compiler script files
..\..\bin\StripCR gmParser.y /nobak
..\..\bin\StripCR gmScanner.l /nobak
..\..\bin\StripCR flex.skl /nobak
..\..\bin\StripCR bison.hairy /nobak
..\..\bin\StripCR bison.simple /nobak
rem generate compiler
..\..\bin\bison -o gmParser.cpp -d -l -p gm gmParser.y  
..\..\bin\flex -ogmScanner.cpp -Pgm -Sflex.skl gmScanner.l

rem Strip CR generated files
..\..\bin\StripCR gmParser.cpp /nobak
..\..\bin\StripCR gmScanner.cpp /nobak

rem use following for verbose bison
rem ..\..\bin\bison -o gmParser.cpp -d -l -v -p gm gmParser.y  
