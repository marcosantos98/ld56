clang++ -o main.exe main.cpp -I./arena -I./raylib/include -isystem./cute_headers -L./raylib/lib -lraylib -luser32 -lshell32 -lgdi32 -lwinmm -fms-runtime-lib=libcmt -Xlinker /NODEFAULTLIB -lmsvcrt -lucrt -lvcruntime -lkernel32 -ggdb

if ($LastExitCode -eq 0) {
	./main.exe
} else {
	Write-Host "Failed compilation!" -ForegroundColor Red
}
