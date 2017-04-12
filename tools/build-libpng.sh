set -ex

mkdir ~/build-libpng
cd ~/build-libpng
wget "http://prdownloads.sourceforge.net/libpng/libpng-1.6.28.tar.xz"
tar xf libpng-1.6.28.tar.xz
cd libpng-1.6.28/
./configure || cat config.log
make install
