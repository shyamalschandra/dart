before_install() {
  cd /tmp

  # Install nlopt from source since Ubuntu 12.04 does not provide debian package for nlopt
  curl -o nlopt-2.4.1.tar.gz http://ab-initio.mit.edu/nlopt/nlopt-2.4.1.tar.gz
  tar -xf nlopt-2.4.1.tar.gz
  (cd nlopt-2.4.1/; sh autogen.sh; make CPPFLAGS='-fPIC' && sudo make install)
}

wget -O boost_1_55_0.tar.gz http://sourceforge.net/projects/boost/files/boost/1.55.0/boost_1_55_0.tar.gz/download
tar xzvf boost_1_55_0.tar.gz
cd boost_1_55_0/

sudo apt-get update
sudo apt-get install build-essential g++ python-dev autotools-dev libicu-dev build-essential libbz2-dev 

./bootstrap.sh --prefix=/usr/local

n=`cat /proc/cpuinfo | grep "cpu cores" | uniq | awk '{print $NF}'`

sudo ./b2 --with=all -j $n install 

sudo sh -c 'echo "/usr/local/lib" >> /etc/ld.so.conf.d/local.conf'

sudo ldconfig

# sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y
# sudo apt-get update -qq
# if [ "$CXX" = "g++" ]; then sudo apt-get install -qq g++-4.8; fi
# if [ "$CXX" = "g++" ]; then export CXX="g++-4.8" CC="gcc-4.8"; fi

sudo add-apt-repository --yes ppa:libccd-debs/ppa
sudo add-apt-repository --yes ppa:fcl-debs/ppa
sudo add-apt-repository --yes ppa:dartsim/ppa
sudo apt-get update

APT_CORE='
cmake
freeglut3-dev
libassimp-dev
libboost-all-dev
libccd-dev
libeigen3-dev
libfcl-dev
libxi-dev
libxmu-dev
'

APT=$APT_CORE' 
libflann-dev
libgtest-dev
libtinyxml-dev
libtinyxml2-dev
liburdfdom-dev
liburdfdom-headers-dev
'

if [ $BUILD_CORE_ONLY = OFF ]; then
  sudo apt-get --yes --force-yes install $APT
else
  sudo apt-get --yes --force-yes install $APT_CORE
fi

# (before_install)

