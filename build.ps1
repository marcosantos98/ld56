clang++ -o main.exe main.cpp -I./arena -I./raylib/include -L./raylib/lib -lraylib -luser32 -lshell32 -lgdi32 -lwinmm -fms-runtime-lib=libcmt -Xlinker /NODEFAULTLIB -lmsvcrt -lucrt -lvcruntime -lmsvcprt -lkernel32 -ggdb

if ($LastExitCode -eq 0) {
	./main.exe
} else {
	Write-Host "Failed compilation!" -ForegroundColor Red
}
