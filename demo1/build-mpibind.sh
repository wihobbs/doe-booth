git clone https://github.com/LLNL/mpibind.git
mkdir build
cd mpibind
./bootstrap
./configure --prefix=$(pwd)/../build
make -j 8 install
export FLUX_SHELL_RC_PATH=/home/fluxuser/build/share/mpibind:$FLUX_SHELL_RC_PATH
