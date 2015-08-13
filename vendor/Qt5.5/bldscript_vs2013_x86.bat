set QTOUTNAME="qt55_static_x86_vs2013"

REM Set up the compiler environment.
call "C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\bin\vcvars32.bat"

REM Set Qt build target (IMPORTANT).
set QMAKESPEC=win32-msvc2013

call bldscript.bat