@echo off
del *.pdb > NUL 2> NUL
del *.rdi > NUL 2> NUL
cl src/dush.c -nologo -Fe:dush.exe -Z7 -W4 -external:anglebrackets -external:W0 -DAGGRESSIVE_ASSERTS=0 -DAGGRESSIVE_MEM_ZERO=0 -O2 -link -incremental:no -opt:ref
del *.obj > NUL 2> NUL
del *.ilk > NUL 2> NUL
