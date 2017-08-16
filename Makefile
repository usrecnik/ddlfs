export LD_LIBRARY_PATH=../instantclient_12_2/

all:
	gcc main.c logging.c config.c fuse-impl.c oracle.c query.c vfs.c  \
		-o ddlfs \
		-I ${LD_LIBRARY_PATH}/sdk/include -L ${LD_LIBRARY_PATH} \
		-lclntsh -locci \
		-Wall -pedantic \
		`pkg-config fuse --cflags --libs`
clean:
	rm -vf *.o ddlfs

