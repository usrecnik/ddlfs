UNAME := $(shell uname)
ifeq ($(UNAME), Windows)
    LD_LIBRARY_PATH=..\..\instantclient_23_5
else
    LD_LIBRARY_PATH=../../instantclient_23_5
endif

windows:
	"C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
	
	cl \
	/I $(LD_LIBRARY_PATH)\sdk\include \
	/I "C:\Program Files\Dokan\Dokan Library-1.2.1\include" \
	/W4 /RTCcsu /c /Gs /GS /ZI /Fd:ddlfs.pdb \
	main.c logging.c config.c fuse-impl.c query.c query_tables.c vfs.c oracle.c tempfs.c util.c dbro_refresh.c
	
	link /DEBUG:FULL /out:ddlfs.exe /LIBPATH:. /LIBPATH:$(LD_LIBRARY_PATH)\sdk\lib\msvc\ /LIBPATH:"C:\Program Files\Dokan\Dokan Library-1.2.1\lib" oci.lib dokanfuse1.lib *.obj
	del *.obj

	if not exist "..\target" mkdir ..\target
	"C:\Program Files (x86)\WiX Toolset v3.11\bin\heat.exe" dir E:\urh\instantclient_18_3\ -gg -cg clientCG -ke -srd -dr InstantClient -platform x64 -sfrag -sreg -out ic.wxs
	"C:\Program Files (x86)\WiX Toolset v3.11\bin\candle.exe" -arch x64 ic.wxs WindowsInstaller.wxs
	"C:\Program Files (x86)\WiX Toolset v3.11\bin\light.exe"  WindowsInstaller.wixobj ic.wixobj -b E:\urh\instantclient_18_3\ -out ..\target\ddlfs.msi
	del *.wixobj
	del ic.wxs
	del ..\target\ddlfs.wixpdb
	
linux:
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
