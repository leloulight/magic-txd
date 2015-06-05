rd qt5
call "C:\Program Files (x86)\Microsoft Visual Studio 12.0\Common7\Tools\VsDevCmd.bat"
git clone git://code.qt.io/qt/qt5.git
cd qt5
git checkout 5.4
perl init-repository ^
    --module-subset=qtbase,qtdoc,qtdocgallery,qtenginio,qtfeedback,qtimageformats,qtmultimedia,qttools,qttranslations,qtxmlpatterns
call configure -static -debug-and-release -mp ^
    -opensource -nomake examples -nomake tests ^
    -prefix C:\QtCustom\
nmake
nmake install
cd ..
move "C:/QtCustom" "qt51_static"