# run as 'powershell -ExecutionPolicy Bypass -File .\instant-client-files.ps1'
Get-ChildItem -Path "..\..\instantclient_23_5" -File | ForEach-Object {
    "<File Source=`"..\..\instantclient_23_5\$($_.Name)`" />"
}