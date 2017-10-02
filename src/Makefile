export LD_LIBRARY_PATH=../../instantclient_12_2/
PKG_NAME=ddlfs
PKG_VERS=1.0
PKG_ARCH=amd64
PKG_MAIN="Urh Srecnik" <urh.srecnik@abakus.si>
PKG_DESC=Filesystem which represents Oracle Database objects as their DDL stored in .sql files.
PKG_FULL_NAME=${PKG_NAME}-${PKG_VERS}
TMP_BUILD=../target/${PKG_FULL_NAME}/
DEB_CF=../target/${PKG_FULL_NAME}/DEBIAN/control

all:
	@mkdir -p ../target
	gcc main.c logging.c config.c fuse-impl.c query.c vfs.c oracle.c \
		-I ${LD_LIBRARY_PATH}/sdk/include \
		-L ${LD_LIBRARY_PATH} -lclntsh  \
		-o ../target/ddlfs \
		-Wall -pedantic \
		`pkg-config fuse --cflags --libs`

release: all
	@mkdir -p ../target/${PKG_FULL_NAME}/DEBIAN
	@mkdir -p ../target/${PKG_FULL_NAME}/usr/lib/ddlfs/
	@mkdir -p ../target/${PKG_FULL_NAME}/usr/bin
	@mkdir -p ../target/${PKG_FULL_NAME}/usr/share/man/man1
	@cp wrapper.sh ../target/${PKG_FULL_NAME}/usr/bin/ddlfs
	@cp ../target/ddlfs ../target/${PKG_FULL_NAME}/usr/lib/ddlfs/ddlfs
	@chmod a+x ../target/${PKG_FULL_NAME}/usr/bin/ddlfs
	@chmod a+x ../target/${PKG_FULL_NAME}/usr/lib/ddlfs/ddlfs
	@cp ../docs/ddlfs.man ../target/${PKG_FULL_NAME}/usr/share/man/man1/ddlfs.1
	@unzip ../../instantclient_12_2/instantclient-basic-linux.x64-12.2.0.1.0.zip -d ../target/${PKG_FULL_NAME}/usr/lib/ddlfs
	@mv -v ../target/${PKG_FULL_NAME}/usr/lib/ddlfs/instantclient_12_2 ../target/${PKG_FULL_NAME}/usr/lib/ddlfs/ic
	@echo "Package: ${PKG_NAME}"       > ${DEB_CF}
	@echo "Version: ${PKG_VERS}"      >> ${DEB_CF}
	@echo "Architecture: ${PKG_ARCH}" >> ${DEB_CF}
	@echo "Maintainer: ${PKG_MAIN}"   >> ${DEB_CF}
	@echo "Depends: fuse, libaio1"    >> ${DEB_CF}
	@echo "Description: ${PKG_DESC}"  >> ${DEB_CF}
	@echo ' n/a'                      >> ${DEB_CF} # extended description
	@chown -R root:root ../target/*
	@dpkg-deb -b ../target/${PKG_FULL_NAME}
	@alien --to-rpm ../target/${PKG_FULL_NAME}.deb
	@mv *.rpm ../target/${PKG_FULL_NAME}.rpm

clean:
	rm -rfv ../target

