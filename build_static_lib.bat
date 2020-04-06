@echo off

if not exist "lib" mkdir lib
pushd lib

set INCLUDES=-I..\include\ -I..\include\vdb\ -I%SDL2_DIR%\include -I..\src\freetype\include
set FLAGS=-EHsc -MD -Oi -fp:fast -nologo -O2 -WX -W3 -wd4100 -wd4189 -wd4996 -wd4055

cl %INCLUDES% %FLAGS% -c ..\src\vdb.cpp
lib *.obj -nologo -out:vdb.lib

popd
