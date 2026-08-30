cmake -DOUTDIR=. -P CommitInfo.cmake
cmake -P BuildDate.cmake

xml2info.exe  -clobber SDF2.xml
xml2cmake.exe -clobber SDF2.xml

cmake -B builddir
cmake --build builddir --config Release --target ESDFDatabase_ser --target ISDFDatabase --target MSDFDatabase
