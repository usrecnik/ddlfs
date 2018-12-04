LD_LIBRARY_PATH=..\..\instantclient_18_3\

all:
	"C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
	
	cl \
	/I "..\..\instant_client\instantclient_18_3\sdk\include" \
	/I "C:\Program Files\Dokan\DokanLibrary-1.2.0\include" \
	/W4 /RTCcsu /c /Gs /GS \
	main.c logging.c config.c fuse-impl.c query.c query_tables.c vfs.c oracle.c tempfs.c util.c dbro_refresh.c
	
	link /out:ddlfs.exe /LIBPATH:. /LIBPATH:E:\urh\instant_client\instantclient_18_3\sdk\lib\msvc\ /LIBPATH:"C:\Program Files\Dokan\DokanLibrary-1.2.0\lib" oci.lib dokanfuse1.lib *.obj