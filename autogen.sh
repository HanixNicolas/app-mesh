#!/bin/bash
################################################################################
## This script is used to install all 3rd-party dependency libraries
################################################################################
set -x
set -e
MACHINE_TYPE="$(uname -m)"
ARM="arm"
AARC="aarc"
WGWT_A="wget --continue --backups=1 --tries=30 --no-check-certificate"

# prepare dep dir
mkdir -p dep
cd dep
export ROOTDIR=$(pwd)

# check root permission
if [ "$(id -u)" != "0" ]; then
	echo "This script must be run as root"
	exit 1
fi

# install compiler and tools
if [ -f "/usr/bin/yum" ]; then
	#RHEL
	RHEL_VER=$(cat /etc/redhat-release | sed -r 's/.* ([0-9]+)\..*/\1/')
	yum install -y epel-release
	if [[ $systemver = "7" ]]; then
		yum install -y https://repo.ius.io/ius-release-el7.rpm
		yum remove git -y
		yum install git222 -y
	else
		yum install git -y
	fi
	yum install -y make gcc-c++ libtool openldap-devel
	yum install -y dos2unix wget which
	yum install -y golang

	#yum install -y boost169-devel boost169-static
	#export BOOST_LIBRARYDIR=/usr/lib64/boost169
	#export BOOST_INCLUDEDIR=/usr/include/boost169
	#ln -s /usr/include/boost169/boost /usr/local/include/boost
	#ln -s /usr/lib64/boost169/ /usr/local/lib64/boost

	# https://www.cnblogs.com/fujinzhou/p/5735578.html
	yum install -y ruby rubygems ruby-devel
	yum install -y rpm-build
	# reduce binary size
	# https://stackoverflow.com/questions/15996699/what-modifications-will-lead-to-size-reduction-of-binary-size-in-c-code
	yum install -y http://ftp.tu-chemnitz.de/pub/linux/dag/redhat/el7/en/x86_64/rpmforge/RPMS/ucl-1.03-2.el7.rf.x86_64.rpm
	yum install -y http://ftp.tu-chemnitz.de/pub/linux/dag/redhat/el7/en/x86_64/rpmforge/RPMS/upx-3.91-1.el7.rf.x86_64.rpm
	# other platform package download
	# https://centos.pkgs.org/7/repoforge-x86_64/upx-3.91-1.el7.rf.x86_64.rpm.html
elif [ -f "/usr/bin/apt" ]; then
	#Ubuntu
	# for old archived ubuntu version, the apt update may fail, run below command before update
	# sed -i s/archive.ubuntu/old-releases.ubuntu/g /etc/apt/sources.list
	# sed -i s/security.ubuntu/old-releases.ubuntu/g /etc/apt/sources.list
	export DEBIAN_FRONTEND=noninteractive
	apt update
	apt install -y dos2unix g++ git wget make zlib1g-dev alien libldap2-dev
	#apt install -y libboost-all-dev libace-dev
	#apt install -y libcpprest-dev liblog4cpp5-dev
	apt install -y ruby ruby-dev rubygems
	# reduce binary size
	apt install -y upx-ucl
	apt install -y golang

	# https://gemfury.com/help/could-not-verify-ssl-certificate/
	apt install -y ca-certificates
	export SSL_CERT_FILE=/etc/ssl/certs/ca-certificates.crt
	ruby -rnet/http -e "Net::HTTP.get URI('https://gem.fury.io')"
fi

go get github.com/valyala/fasthttp

# check libssl in case of openssl_update.sh not executed
if [ -f "/usr/include/openssl/ssl.h" ] || [ -f "/usr/local/include/openssl/ssl.h" ]; then
	echo 'ssl installed'
else
	if [ -f "/usr/bin/yum" ]; then
		yum install -y openssl-devel
	else
		apt install -y libssl-dev
	fi
fi

# install cmake (depend on g++, make, openssl-devel)
# https://askubuntu.com/questions/355565/how-do-i-install-the-latest-version-of-cmake-from-the-command-line
if [ true ]; then
	version=3.20
	build=5
	$WGWT_A https://cmake.org/files/v$version/cmake-$version.$build.tar.gz
	tar -xzvf cmake-$version.$build.tar.gz
	cd cmake-$version.$build/
	./bootstrap
	make -j6
	make install
fi

#install fpm
gem install fpm

# build boost_1_74_0
if [ true ]; then
	# https://www.cnblogs.com/eagle6688/p/5840773.html
	if [ -f "/usr/bin/yum" ]; then
		yum install -y python2-devel
	elif [ -f "/usr/bin/apt" ]; then
		apt install -y python-dev
	fi
	# https://www.boost.org/users/download/
	$WGWT_A https://boostorg.jfrog.io/artifactory/main/release/1.74.0/source/boost_1_74_0.tar.gz
	tar zxvf boost_1_74_0.tar.gz
	cd ./boost_1_74_0
	./bootstrap.sh
	./b2
	./b2 install
	ls -al /usr/local/lib/libboost_system.so.1.74.0 /usr/local/include/boost/thread.hpp
fi
cd $ROOTDIR

# cpprestsdk (use -DBUILD_SHARED_LIBS=0 for static link):
# https://stackoverflow.com/questions/49877907/cpp-rest-sdk-in-centos-7
git clone --depth=1 -b 2.10.18 https://github.com/microsoft/cpprestsdk.git cpprestsdk
cd cpprestsdk
git submodule update --init
cd Release
cmake .. -DCMAKE_BUILD_TYPE=Release -DBOOST_ROOT=/usr/local -DBUILD_SHARED_LIBS=1 -DCMAKE_CXX_FLAGS="-Wno-error=cast-align -Wno-error=conversion -Wno-error=missing-field-initializers"
make -j6
make install
ls -al /usr/local/lib*/libcpprest.so
cd $ROOTDIR

# build log4cpp:
# https://my.oschina.net/u/1983790/blog/1587568
if [ -z "${MACHINE_TYPE##*$ARM*}" -o -z "${MACHINE_TYPE##*$AARC*}" ]; then
	# arm64 will failed with log4cpp build, use package directly
	apt install -y liblog4cpp5-dev
else
	# yum install log4cpp -y
	$WGWT_A https://jaist.dl.sourceforge.net/project/log4cpp/log4cpp-1.1.x%20%28new%29/log4cpp-1.1/log4cpp-1.1.3.tar.gz
	tar zxvf log4cpp-1.1.3.tar.gz
	cd log4cpp/
	./autogen.sh
	./configure
	make
	make install
	ls -al /usr/local/lib*/liblog4cpp.a
fi
cd $ROOTDIR

# build ACE
if [ true ]; then
	# ubuntu does not need build ACE
	# ACE:
	# https://www.cnblogs.com/tanzi-888/p/5342431.html
	# http://download.dre.vanderbilt.edu/
	$WGWT_A https://github.com/DOCGroup/ACE_TAO/releases/download/ACE%2BTAO-6_5_9/ACE-6.5.9.tar.gz
	tar zxvf ACE-6.5.9.tar.gz
	cd ACE_wrappers
	export ACE_ROOT=$(pwd)
	cp ace/config-linux.h ace/config.h
	cp include/makeinclude/platform_linux.GNU include/makeinclude/platform_macros.GNU
	make
	make install INSTALL_PREFIX=/usr/local
	ls -al /usr/local/lib*/libACE.so
fi
cd $ROOTDIR

# cryptopp: AES encrypt
$WGWT_A https://github.com/weidai11/cryptopp/archive/CRYPTOPP_8_3_0.zip
unzip -o CRYPTOPP_8_3_0.zip
export CXXFLAGS="-DNDEBUG -Os -std=c++11"
cd cryptopp-CRYPTOPP_8_3_0/
make -j6
make install
cd $ROOTDIR

# cfssl: generate SSL certification file
if [ -z "${MACHINE_TYPE##*$ARM*}" -o -z "${MACHINE_TYPE##*$AARC*}" ]; then
	# cfssl have no arm64 binary, just use package instead
	apt install -y golang-cfssl
else
	# SSL
	# https://www.cnblogs.com/fanqisoft/p/10765038.html
	# https://www.bookstack.cn/read/tidb-v2.1/how-to-secure-generate-self-signed-certificates.md
	cd $ROOTDIR
	$WGWT_A https://pkg.cfssl.org/R1.2/cfssl_linux-amd64
	chmod +x cfssl_linux-amd64
	upx cfssl_linux-amd64
	$WGWT_A https://pkg.cfssl.org/R1.2/cfssljson_linux-amd64
	chmod +x cfssljson_linux-amd64
	upx cfssljson_linux-amd64
	$WGWT_A https://pkg.cfssl.org/R1.2/cfssl-certinfo_linux-amd64
	chmod +x cfssl-certinfo_linux-amd64
	upx cfssl-certinfo_linux-amd64
	mv cfssl_linux-amd64 /usr/bin/cfssl
	mv cfssljson_linux-amd64 /usr/bin/cfssljson
fi
