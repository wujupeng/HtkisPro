$Source = "C:\Users\DELL\IDEProjects\HtkisPro\build\Release\proxy_server.exe"
$Dest = "\\192.168.2.3\share\app\proxy_server.exe"

if (Test-Path $Source) {
    $Size = [math]::Round((Get-Item $Source).Length / 1MB, 2)
    Write-Host "Copying proxy_server.exe ($Size MB) to $Dest ..."
    Copy-Item -Path $Source -Destination $Dest -Force
    if (Test-Path $Dest) {
        $DestSize = [math]::Round((Get-Item $Dest).Length / 1MB, 2)
        Write-Host "Success! Deployed: $Dest ($DestSize MB)" -ForegroundColor Green
    } else {
        Write-Host "Failed: destination not accessible" -ForegroundColor Red
    }
} else {
    Write-Host "Source not found: $Source" -ForegroundColor Red
}