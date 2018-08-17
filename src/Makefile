export LD_LIBRARY_PATH=../../instantclient_18_3/

all:
	@mkdir -p ../target
	gcc main.c logging.c config.c fuse-impl.c query.c query_tables.c vfs.c oracle.c tempfs.c util.c \
		-I ${LD_LIBRARY_PATH}/sdk/include \
		-L ${LD_LIBRARY_PATH} -lclntsh \
		-g -o ../target/ddlfs \
		-Wall -pedantic \
		`pkg-config fuse --cflags --libs`

release: all
	./mkRelease.sh

clean:
	rm -rfv ../target

