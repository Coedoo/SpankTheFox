@echo off

if not exist web_build mkdir web_build

pushd web_build

call emsdk activate latest
if "%1" == "release" (
    echo "BUILDING RELEASE!"
    call emcc -o index.html ../src/main.cpp -Os -Wall ../bin/libraylib.a -s USE_GLFW=3 --shell-file ../src/minshell.html --preload-file ../assets -DWEB_BUILD -sSTACK_SIZE=1048576 -s TOTAL_MEMORY=67108864

) else (
    call emcc -o index.html ../src/main.cpp -Wall ../bin/libraylib.a -s USE_GLFW=3 --shell-file ../src/shell.html --preload-file ../assets -DWEB_BUILD -sSTACK_SIZE=1048576 -s TOTAL_MEMORY=67108864
)
popd