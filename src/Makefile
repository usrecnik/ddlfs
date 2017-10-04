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
	./mkRelease.sh

clean:
	rm -rfv ../target

