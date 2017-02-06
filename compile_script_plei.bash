#! /bin/bash
# Need to build SUNDIALS with -fPIC and set(CMAKE_MACOSX_RPATH 1)
export CANTERA_DIR=/nobackup/cma3/local/cantera/2.3.0
export SUN_INCLUDE=/nobackup/cma3/local/sundials/2.7.0/include
export SUN_LIB=/nobackup/cma3/local/sundials/2.7.0/lib
export BOOST_DIR=/nasa/boost/1.50.0//include
export EIGEN_INCLUDE=/nobackup/cma3/local/eigen/3.3.2/eigen/

scons -j20 build prefix=$CANTERA_DIR \
  CXX=icpc CC=icc FORTRAN=ifort python_package=full \
  optimize_flags='-O3 -ip -axCORE-AVX2 -xAVX' \
  env_vars='all' \
  sundials_include=$SUN_INCLUDE sundials_libdir=$SUN_LIB \
  boost_inc_dir=$BOOST_DIR f90_interface=y eigen_include=$EIGEN_INCLUDE
if [ -z "SCONS_TEST" ]; then
    scons -j20 test
fi  
scons -j20 install
