$projectDir = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path -Path $projectDir -ChildPath "build"

$outputFile = Join-Path -Path $projectDir -ChildPath "TaskbarManager-*.zip"
if (Test-Path -Path $outputFile) {
    Write-Host "删除之前的构建产物: $outputFile" -ForegroundColor Blue
    Remove-Item -Force $outputFile
}

if (Test-Path -Path $buildDir) {
    Write-Host "删除构建目录: $buildDir" -ForegroundColor Blue
    Remove-Item -Recurse -Force $buildDir
}

Write-Host "正在进行配置" -ForegroundColor Blue
cmake -S $projectDir -B $buildDir
Write-Host "正在编译项目" -ForegroundColor Blue
cmake --build $buildDir --config MinSizeRel
Write-Host "正在打包项目" -ForegroundColor Blue
cpack -C MinSizeRel --config (Join-Path -Path $buildDir -ChildPath "CPackConfig.cmake")
Write-Host "已完成项目构建[不包含WebView2 Runtime]" -ForegroundColor Green