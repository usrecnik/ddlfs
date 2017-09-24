export LD_LIBRARY_PATH=../instantclient_12_2/
PKG_NAME=ddlfs
PKG_VERS=1.0
PKG_ARCH=amd64
PKG_MAIN="Urh Srecnik" <urh.srecnik@abakus.si>
PKG_DESC=Filesystem which represents Oracle Database objects as their DDL stored in .sql files.
PKG_FULL_NAME=${PKG_NAME}-${PKG_VERS}
TMP_BUILD=target/${PKG_FULL_NAME}/
DEB_CF=target/${PKG_FULL_NAME}/DEBIAN/control

all:
	gcc main.c logging.c config.c fuse-impl.c query.c vfs.c oracle.c \
		-I ${LD_LIBRARY_PATH}/sdk/include \
		-L ${LD_LIBRARY_PATH} -lclntsh  \
		-o ddlfs \
		-Wall -pedantic \
		`pkg-config fuse --cflags --libs`

release: all
	# .deb package
	@mkdir -p target/${PKG_FULL_NAME}/DEBIAN
	@mkdir -p target/${PKG_FULL_NAME}/usr/bin
	cp ddlfs target/${PKG_FULL_NAME}/usr/bin/
	@echo "Package: ${PKG_NAME}"       > ${DEB_CF}
	@echo "Version: ${PKG_VERS}"      >> ${DEB_CF}
	@echo "Architecture: ${PKG_ARCH}" >> ${DEB_CF}
	@echo "Maintainer: ${PKG_MAIN}"   >> ${DEB_CF}
	@echo "Description: ${PKG_DESC}"  >> ${DEB_CF}
	@echo ' n/a'                      >> ${DEB_CF} # extended description
	dpkg-deb -b target/${PKG_FULL_NAME}
	#.rpm package (converted from .deb)
	alien --to-rpm target/${PKG_FULL_NAME}.deb
	mv *.rpm target/${PKG_FULL_NAME}.rpm

clean:
	rm -vf *.o ddlfs
	rm -rf target/

