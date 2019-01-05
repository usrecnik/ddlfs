LD_LIBRARY_PATH=..\..\instantclient_18_3

windows:
	
	"C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
	
	cl \
	/I $(LD_LIBRARY_PATH)\sdk\include \
	/I "C:\Program Files\Dokan\Dokan Library-1.2.1\include" \
	/W4 /RTCcsu /c /Gs /GS /ZI /Fd:ddlfs.pdb \
	main.c logging.c config.c fuse-impl.c query.c query_tables.c vfs.c oracle.c tempfs.c util.c dbro_refresh.c
	
	link /DEBUG:FULL /out:ddlfs.exe /LIBPATH:. /LIBPATH:$(LD_LIBRARY_PATH)\sdk\lib\msvc\ /LIBPATH:"C:\Program Files\Dokan\Dokan Library-1.2.1\lib" oci.lib dokanfuse1.lib *.obj
	del *.obj

linux:
	@mkdir -p ../target
	gcc main.c logging.c config.c fuse-impl.c query.c query_tables.c vfs.c \
		oracle.c tempfs.c util.c dbro_refresh.c \
		-I $(LD_LIBRARY_PATH)/sdk/include \
		-L $(LD_LIBRARY_PATH) -lclntsh \
		-g -o ../target/ddlfs \
		-Wall -pedantic \
		`pkg-config fuse --cflags --libs`

release: all
	./mkRelease.sh

clean:
	rm -rfv ../target