LD_LIBRARY_PATH=../../instantclient_23_6

all:
	@echo LD_LIBRARY_PATH=${LD_LIBRARY_PATH}	
	@mkdir -p ../target
	gcc main.c logging.c config.c fuse-impl.c query.c query_tables.c vfs.c \
		oracle.c tempfs.c util.c dbro_refresh.c \
		-I $(LD_LIBRARY_PATH)/sdk/include \
		-L $(LD_LIBRARY_PATH) -lclntsh \
		-g -o ../target/ddlfs \
		-Wall -pedantic \
		`pkg-config fuse3 --cflags --libs`

release: all
	./mkRelease.sh

clean:
	rm -rfv ../target
