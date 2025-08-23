$projectDir = Split-Path -Parent $PSScriptRoot
$nsisDir = Join-Path $projectDir -ChildPath "nsis"

Write-Host "开始制作NSIS安装包" -ForegroundColor Blue

$sourceDir = Join-Path -Path $nsisDir -ChildPath "SourceDir"
if (Test-Path -Path $sourceDir) {
    Remove-Item -Recurse -Force $sourceDir
}

$installerExe = Join-Path -Path $nsisDir -ChildPath "Install TaskbarManager.exe"
if (Test-Path -Path $installerExe) {
    Remove-Item $installerExe
}

$zipName = "TaskbarManager-1.0.0-win64"
$zipFile = Join-Path -Path $projectDir -ChildPath "$zipName.zip"
Expand-Archive -LiteralPath $zipFile -DestinationPath $sourceDir
Move-Item "$sourceDir\$zipName\*" $sourceDir
Remove-Item -Recurse -Force "$sourceDir\$zipName"

.\nsis\build.ps1
Write-Host "已完成安装包制作[不包含WebView2 Runtime]" -ForegroundColor Green