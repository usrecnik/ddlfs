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
	@mkdir -p ../target/${PKG_FULL_NAME}/usr/lib/ddlfs/ic
	@mkdir -p ../target/${PKG_FULL_NAME}/usr/bin
	@cp ../target/ddlfs ../target/${PKG_FULL_NAME}/usr/bin/
	@cp ${LD_LIBRARY_PATH}/libclntsh.so.12.1     ../target/${PKG_FULL_NAME}/usr/lib/ddlfs/ic
	@cp ${LD_LIBRARY_PATH}/libclntshcore.so.12.1 ../target/${PKG_FULL_NAME}/usr/lib/ddlfs/ic
	@cp ${LD_LIBRARY_PATH}/libmql1.so            ../target/${PKG_FULL_NAME}/usr/lib/ddlfs/ic
	@cp ${LD_LIBRARY_PATH}/libipc1.so            ../target/${PKG_FULL_NAME}/usr/lib/ddlfs/ic
	@cp ${LD_LIBRARY_PATH}/libnnz12.so           ../target/${PKG_FULL_NAME}/usr/lib/ddlfs/ic
	@cp ${LD_LIBRARY_PATH}/libons.so             ../target/${PKG_FULL_NAME}/usr/lib/ddlfs/ic
	@echo "Package: ${PKG_NAME}"       > ${DEB_CF}
	@echo "Version: ${PKG_VERS}"      >> ${DEB_CF}
	@echo "Architecture: ${PKG_ARCH}" >> ${DEB_CF}
	@echo "Maintainer: ${PKG_MAIN}"   >> ${DEB_CF}
	@echo "Depends: fuse"             >> ${DEB_CF}
	@echo "Description: ${PKG_DESC}"  >> ${DEB_CF}
	@echo ' n/a'                      >> ${DEB_CF} # extended description
	@dpkg-deb -b ../target/${PKG_FULL_NAME}
	@alien --to-rpm ../target/${PKG_FULL_NAME}.deb
	@mv *.rpm ../target/${PKG_FULL_NAME}.rpm

clean:
	rm -rfv ../target

