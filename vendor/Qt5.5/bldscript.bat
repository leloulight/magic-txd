REM rd /S /Q qt5
git clone git://code.qt.io/qt/qt5.git
cd qt5

set _ROOT=%CD%
SET PATH=%_ROOT%\qtbase\bin;%_ROOT%\gnuwin32\bin;%PATH%
SET _ROOT=

git checkout 5.5
perl init-repository ^
    --module-subset=qtbase,qtimageformats
call "../configure_repo.bat"
nmake
nmake install
cd ..
move "C:/QtCustom" %QTOUTNAME%