@echo off

if NOT "%Platform%" == "X64" IF NOT "%Platform%" == "x64" (call vcvarsall x64)

set exe_name=SpankTheFox
set compile_flags= -nologo /Zi /FC /I ../include/ /W3
set linker_flags= raylibdll.lib gdi32.lib user32.lib kernel32.lib opengl32.lib /INCREMENTAL:NO
set linker_path="../bin/"

if not exist build mkdir build
pushd build

del %exe_name%.exe

call cl %compile_flags% ../src/main.cpp /link %linker_flags% /libpath:%linker_path% /out:%exe_name%.exe

Xcopy ..\assets assets /E/I/q/y
copy ..\bin\raylib.dll . >NUL

%exe_name%.exe

popd
