set QTOUTNAME="qt55_static_x64_vs2013"

REM Set up the compiler environment.
call "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\bin\amd64\vcvars64.bat"

REM Set Qt build target (IMPORTANT).
set QMAKESPEC=win32-msvc2013

call bldscript.bat