mkdir -f build | out-null
cp raylib/raylib.dll .

if ($args[0] -eq "web") {
    em++ -o ./build/game.html main.cpp -Os -Wall ./raylib/libraylib.a -I./arena -I./raylib/include -L./raylib -s USE_GLFW=3 -DPLATFORM_WEB --shell-file ./raylib/minshell.html --preload-file=./res/
} else {
	clang -MJ compile_commands.json -I./arena -I./raylib -L./raylib -lraylib -o main.exe main.c
}
