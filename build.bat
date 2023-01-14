@echo off

if NOT "%Platform%" == "X64" IF NOT "%Platform%" == "x64" (call vcvarsall x64)

set exe_name=SpankTheFox
set compile_flags= -nologo /Zi /FC /I ../include/ /W3
set linker_flags= raylibdll.lib /INCREMENTAL:NO
set linker_path="../bin/"


if "%1" == "release" (
    echo "BUILDING RELEASE!"
    set compile_flags=%compile_flags% /O2
    set linker_flags=%linker_flags% /SUBSYSTEM:windows /ENTRY:mainCRTStartup
) else (
    set compile_flags=%compile_flags%  /Zi
)

if not exist build mkdir build
pushd build

del %exe_name%.exe

call cl %compile_flags% ../src/main.cpp /link %linker_flags% /libpath:%linker_path% /out:%exe_name%.exe

Xcopy ..\assets assets /E/I/q/y
copy ..\bin\raylib.dll . >NUL

%exe_name%.exe

popd
