autoreconf -siv
#aclocal
#autoconf
#automake -a --foreign
#./configure --prefix=$PWD
./configure QSIMINC=/home/hugh/Project/HARDWARE/ME/local_install/qsim-root/include
make
#make install
