copy /y .\game\bin\tier0.dll .\src\devtools\bin
cd .\src\devtools\bin\
vpc +tools
cd ..\..\..\
devenv .\src\utils\vpc\vpc.vcproj /rebuild "release|win32"
devenv .\src\utils\vpc\vpc.vcproj /build "release|win32"
copy /y .\game\bin\tier0.dll .\src\devtools\bin
cd .\src\devtools\bin\
vpc +everything /allgames
vpc +console /x360 /allgames
cd ..\..\..\

