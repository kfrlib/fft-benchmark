rmdir /s /q build
mkdir build
cd build

set PATH=%PATH%;C:\msys64\mingw64\bin
cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -DENABLE_KISSFFT=OFF ..
ninja
IF %ERRORLEVEL% NEQ 0 pause
