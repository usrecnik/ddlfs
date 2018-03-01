export LD_LIBRARY_PATH=../../instantclient_12_2/

all:
	@mkdir -p ../target
	gcc main.c logging.c config.c fuse-impl.c query.c vfs.c oracle.c tempfs.c \
		-I ${LD_LIBRARY_PATH}/sdk/include \
		-L ${LD_LIBRARY_PATH} -lclntsh \
		-o ../target/ddlfs \
		-Wall -pedantic \
		`pkg-config fuse --cflags --libs`

release: all
	./mkRelease.sh

clean:
	rm -rfv ../target

