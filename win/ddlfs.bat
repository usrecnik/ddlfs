@echo off

rem check if "C:\Program Files\ddlfs\instant_client\" is already in PATH
echo %PATH% | find /I "C:\Program Files\ddlfs\instant_client\" > nul
if %errorlevel% neq 0 (
    set PATH=C:\Program Files\ddlfs\instant_client\;%PATH%
)

"C:\Program Files\ddlfs\ddlfs_.exe" %*
