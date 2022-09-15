@echo off
del tz.exe
del tz.pdb
cl /I.\ /nologo tz.cxx /O2it /EHac /Zi /Gy /D_AMD64_ /link ntdll.lib ole32.lib /OPT:REF


