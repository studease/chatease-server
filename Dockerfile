FROM daocloud.io/library/centos:7
MAINTAINER liushaoping <spieled916@gmail.com>
ADD . /var/tmp/studease
# install dependency packages and make source code
RUN yum update -y && yum install -y wget gcc gcc-c++ openssl openssl-devel make gmake && cd /tmp && wget https://cmake.org/files/v3.5/cmake-3.5.2.tar.gz && tar -xzf cmake-3.5.2.tar.gz && cd cmake-3.5.2 && ./bootstrap && gmake && make install && cd /var/tmp/studease &&  mkdir build && cd build && cmake .. && make && cp chatease-server /chatease-server && rm -rf /tmp/* && yum clean all && yum erase -y wget make gmake openssl gcc gcc-c++ openssl-devel && rm -rf /var/tmp
# add source code into docker
# ADD CMakeLists.txt /var/tmp/studease/CMakeLists.txt
# ADD conf /var/tmp/studease/conf
# ADD data /var/tmp/studease/data
# ADD Debug /var/tmp/studease/Debug
# ADD src /var/tmp/studease/src

EXPOSE 80
CMD ["/chatease-server"]
