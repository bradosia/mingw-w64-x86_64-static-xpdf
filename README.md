# xpdf static libraries for MinGW64
Includes a static library linked test program in windows with MinGW64 compiler. All static libraries are prebuilt and included with the executable. 

Features demonstrated:
* Open PDF
* print full text
* PDF to PNG

Compiler: 
- gcc (x86_64-posix-seh-rev0, Built by MinGW-W64 project) 8.1.0

IDE: 
* Eclipse IDE for C/C++ Developers
	* Version: 2018-09 (4.9.0)
	* Build id: 20180917-1800

Library: 
* xpdf 4.02

## How static library was made
1. Download xpdf source from http://www.xpdfreader.com/download.html
2. place into directory /c/mingw/xpdf-4.02
3. copy and overwrite with the patch provided
4. Using MSYS2 shell, attempt to build xpdf with 

```shell
cd /c/mingw/xpdf-4.02
cmake -DBUILD_SHARED_LIBS=OFF -DBUILD_STATIC_LIBS=ON -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles" -S. -Bbuild
cd build
make
```
5. Tackle each error that comes up by installing required libraries
6. static libraries located at /c/mingw/xpdf-4.02/build/xpdf/libxpdf.a


