.\script\BuildAndPack.ps1

Write-Host "开始" -ForegroundColor Blue
$projectDir = Split-Path -Parent $PSScriptRoot

$workDirName = "TaskbarManager-1.0.0-win64"
$workDir = Join-Path -Path $PSScriptRoot -ChildPath $workDirName

$zip = Join-Path $projectDir -ChildPath "$workDirName.zip"
$zipWithWebView2Runtime = Join-Path $projectDir -ChildPath "$workDirName-WebView2Runtime.zip"

Write-Host "解压缩 $zip 到 $PSScriptRoot" -ForegroundColor Blue
Expand-Archive -LiteralPath $zip -DestinationPath $PSScriptRoot

$runtimeUri = "https://msedge.sf.dl.delivery.mp.microsoft.com/filestreamingservice/files/a44ec87b-94e4-4ebd-84ed-65102358926f/Microsoft.WebView2.FixedVersionRuntime.139.0.3405.111.x64.cab"
$runtimeCab = Join-Path -Path $PSScriptRoot -ChildPath "Microsoft.WebView2.FixedVersionRuntime.cab"
$runtimeCabRootDir = "Microsoft.WebView2.FixedVersionRuntime.139.0.3405.111.x64"

if (!(Test-Path -Path $runtimeCab)) {
    Write-Host "本地没有 WebView2 Runtime 缓存，开始下载" -ForegroundColor Blue
    Invoke-WebRequest -Uri $runtimeUri -OutFile $runtimeCab
}

$runtimeDir = Join-Path -Path $workDir -ChildPath "webview2_runtime"
expand $runtimeCab -F:* $workDir
Move-Item "$workDir\$runtimeCabRootDir\*" $runtimeDir
Remove-Item -Recurse -Force "$workDir\$runtimeCabRootDir"

$vcRedistUri = "https://aka.ms/vs/17/release/vc_redist.x64.exe"
$vcRedistExe = Join-Path -Path $PSScriptRoot -ChildPath "VC_redist.x64.exe"
if (!(Test-Path -Path $vcRedistExe)) {
    Write-Host "本地没有 VC_redist.x64.exe 缓存，开始下载" -ForegroundColor Blue
    Invoke-WebRequest -Uri $vcRedistUri -OutFile $vcRedistExe
}
Copy-Item $vcRedistExe $workDir

Compress-Archive -Path $workDir -DestinationPath $zipWithWebView2Runtime
Remove-Item -Recurse -Force $workDir
Write-Host "已完成项目构建[包含WebView2 Runtime]" -ForegroundColor Green