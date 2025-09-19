## 第三方库说明
使用了CGAL6.0库(修改版)及其附加依赖库。 Link : [CGAL Github](https://github.com/CGAL/cgal)

### CGAL
The Computational Geometry Algorithms Library (CGAL) is a C++ library that aims to provide easy access to efficient and reliable algorithms in computational geometry.

### 依赖说明
CGAL是header-only的，他依赖于boost库，但是所依赖的部分也是header-only的，此外，CGAL还依赖GNU Multiple Precision Arithmetic (GMP) and GNU Multiple Precision Floating-Point Reliably (MPFR) 两个库，他们不是header-only的，故使用其预编译库。ref : [CGAL关于依赖的说明](https://doc.cgal.org/latest/Manual/thirdparty.html)  

本项目依赖的第三方库及其附加依赖库详细说明如下：
+ `CGAL/` 基于CGAL6.0修改的库(header部分)
+ `boost/` boost-1.82.0版本的boost目录(header部分)
+ `gmp-win/` GMP 和 MPFR 在 Windows 64bit 系统的预编译库，由CGAL官方提供 [下载地址](https://github.com/CGAL/cgal/releases)
+ `gmp-linux/` GMP 和 MPFR 在 Ubuntu 22.04.5 LTS 系统下构建的预编译库，由本人从源码编译而来，其源码版本分别为 gmp-6.3.0 和 mpfr-4.2.2，源码下载地址[GMP](https://gmplib.org/)、[MPFR](https://www.mpfr.org/)