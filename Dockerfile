FROM registry.fedoraproject.org/fedora-minimal AS build

#########
### Install build deps (large)
#########
RUN microdnf install gcc cmake boost-devel gcc-c++ openssl-devel \
  cpprest-devel mariadb-devel mariadb-connector-c-devel make && \
  microdnf clean all

#########
### Clone repo
#########
WORKDIR /build
COPY . /build/gofer3
WORKDIR /build/gofer3/build

#########
### Build Gopher3 source
#########
RUN rm -rf * && \
  cmake -DCMAKE_BUILD_TYPE=Debug .. && \
  make

#########
### Only install run deps
#########
FROM registry.fedoraproject.org/fedora-minimal
RUN microdnf install gdb openssl cpprest mariadb-connector-c boost-program-options \
  boost-system boost-filesystem boost-chrono boost-thread boost-date-time \
  boost-atomic && microdnf clean all
WORKDIR / 
COPY --from=build /build/gofer3/build/gofer3 /usr/local/bin/gofer3
COPY --from=build /build/gofer3/build/updateDB /usr/local/bin/updateDB
COPY script/entrypoint.sh /bin/entrypoint.sh

WORKDIR /run
VOLUME /run

#########
### PORT and CMD
#########
EXPOSE 5432
ENTRYPOINT ["/bin/entrypoint.sh"]
