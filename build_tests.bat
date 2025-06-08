@echo off
cl tests/hello.c -nologo -Fe:hello.exe -link -incremental:no -opt:ref
cl tests/greet.c -nologo -Fe:greet.exe -link -incremental:no -opt:ref
del *.obj > NUL 2> NUL
del *.ilk > NUL 2> NUL
