param (
    [Parameter(Mandatory = $true)][ValidateSet("ZIP", "NSIS")][string] $Type,
    [switch] $IncludeVCRedist,
    [switch] $IncludeWebView2Runtime
)

Write-Host "====================构建准备====================" -ForegroundColor Red
Write-Host "构建类型: $Type" -ForegroundColor Blue
Write-Host "是否包含 Microsoft Visual C++ 可再发行程序包: $IncludeVCRedist" -ForegroundColor Blue
Write-Host "是否包含 WebView2 Runtime: $IncludeWebView2Runtime" -ForegroundColor Blue

$projectDir = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path -Path $projectDir -ChildPath "build"
Write-Host "项目目录: $projectDir" -ForegroundColor Blue
Write-Host "项目构建目录: $buildDir" -ForegroundColor Blue
Write-Host "构建脚本所在目录: $PSScriptRoot" -ForegroundColor Blue

$vcRedistUri = "https://aka.ms/vs/17/release/vc_redist.x64.exe"
$vcRedistExe = Join-Path -Path $PSScriptRoot -ChildPath "VC_redist.x64.exe"
if ($IncludeVCRedist) {
    Write-Host "检查本地是否存在 VC_redist.x64.exe 缓存" -ForegroundColor Yellow
    if (!(Test-Path -Path $vcRedistExe)) {
        Write-Host "本地没有 VC_redist.x64.exe 缓存，开始下载" -ForegroundColor Red
        Invoke-WebRequest -Uri $vcRedistUri -OutFile $vcRedistExe
    }
    else {
        Write-Host "本地存在 VC_redist.x64.exe 缓存" -ForegroundColor Green
    }
}

$runtimeUri = "https://msedge.sf.dl.delivery.mp.microsoft.com/filestreamingservice/files/a44ec87b-94e4-4ebd-84ed-65102358926f/Microsoft.WebView2.FixedVersionRuntime.139.0.3405.111.x64.cab"
$runtimeCab = Join-Path -Path $PSScriptRoot -ChildPath "Microsoft.WebView2.FixedVersionRuntime.cab"
$runtimeCabRootDir = "Microsoft.WebView2.FixedVersionRuntime.139.0.3405.111.x64"
if ($IncludeWebView2Runtime) {
    Write-Host "检查本地是否存在 WebView2 Runtime 缓存" -ForegroundColor Yellow
    if (!(Test-Path -Path $runtimeCab)) {
        Write-Host "本地没有 WebView2 Runtime 缓存，开始下载" -ForegroundColor Red
        Invoke-WebRequest -Uri $runtimeUri -OutFile $runtimeCab
    }
    else {
        Write-Host "本地存在 WebView2 Runtime 缓存" -ForegroundColor Green
    }
}

Write-Host "====================构建说明====================" -ForegroundColor Red
if ($Type.ToUpper().Equals("ZIP")) {
    Write-Host "[1]构建类型: 应用程序映像，对ZIP文件解压缩来获得程序。" -ForegroundColor Green
}
elseif ($Type.ToUpper().Equals("NSIS")) {
    Write-Host "[1]构建类型: 应用程序包，跟随安装程序引导操作来获得程序。" -ForegroundColor Green
}
Write-Host "构建是否包含 Microsoft Visual C++ 可再发行程序包: $($IncludeVCRedist ? '包含': '不包含')" -ForegroundColor Green
Write-Host "构建是否包含 WebView2 Runtime: $($IncludeWebView2Runtime ? '包含': '不包含')" -ForegroundColor Green


Write-Host "====================构建开始====================" -ForegroundColor Red
$version = "1.0.0"
$basePackageName = "TaskbarManager-$version-win64"
$baseOutputFile = Join-Path -Path $projectDir -ChildPath "$basePackageName.zip"
Write-Host "基础应用程序映像: $baseOutputFile" -ForegroundColor Blue
if (Test-Path -Path $baseOutputFile) {
    Write-Host "删除之前的构建产物: $baseOutputFile" -ForegroundColor Gray
    Remove-Item -Force $baseOutputFile
}
if (Test-Path -Path $buildDir) {
    Write-Host "删除之前的构建目录: $buildDir" -ForegroundColor Gray
    Remove-Item -Recurse -Force $buildDir
}
Write-Host "[生成基础应用程序映像]开始配置、编译、打包" -ForegroundColor Green
cmake -S $projectDir -B $buildDir
cmake --build $buildDir --config MinSizeRel
cpack -C MinSizeRel --config (Join-Path -Path $buildDir -ChildPath "CPackConfig.cmake")
Write-Host "[生成基础应用程序映像]已完成" -ForegroundColor Green

if ($Type.ToUpper().Equals("ZIP")) {
    $workDir = Join-Path -Path $PSScriptRoot -ChildPath $basePackageName
    $runtimeDir = Join-Path -Path $workDir -ChildPath "webview2_runtime"
    Expand-Archive -LiteralPath $baseOutputFile -DestinationPath $PSScriptRoot
    Write-Host "[生成应用程序映像]开始" -ForegroundColor Green
    if (-not $IncludeVCRedist -and -not $IncludeWebView2Runtime) {
        Write-Host "[生成应用程序映像]完成: $baseOutputFile" -ForegroundColor Green
    }
    elseif ($IncludeVCRedist -and -not $IncludeWebView2Runtime) {
        $outputFileName = "$basePackageName-VC_redist.zip"
        $outputFile = Join-Path -Path $projectDir -ChildPath $outputFileName
        if (Test-Path -Path $outputFile) {
            Remove-Item -Force $outputFile
        }
        Copy-Item $vcRedistExe $workDir
        Compress-Archive -Path $workDir -DestinationPath $outputFile
        Write-Host "[生成应用程序映像]完成: $outputFileName" -ForegroundColor Green
    }
    elseif (-not $IncludeVCRedist -and $IncludeWebView2Runtime) {
        $outputFileName = "$basePackageName-WebView2_Runtime.zip"
        $outputFile = Join-Path -Path $projectDir -ChildPath $outputFileName
        if (Test-Path -Path $outputFile) {
            Remove-Item -Force $outputFile
        }
        expand $runtimeCab -F:* $workDir
        Move-Item "$workDir\$runtimeCabRootDir\*" $runtimeDir
        Remove-Item -Recurse -Force "$workDir\$runtimeCabRootDir"
        Compress-Archive -Path $workDir -DestinationPath $outputFile
        Write-Host "[生成应用程序映像]完成: $outputFileName" -ForegroundColor Green
    }
    elseif ($IncludeVCRedist -and $IncludeWebView2Runtime) {
        $outputFileName = "$basePackageName-VC_redist-WebView2_Runtime.zip"
        $outputFile = Join-Path -Path $projectDir -ChildPath $outputFileName
        if (Test-Path -Path $outputFile) {
            Remove-Item -Force $outputFile
        }
        Copy-Item $vcRedistExe $workDir
        expand $runtimeCab -F:* $workDir
        Move-Item "$workDir\$runtimeCabRootDir\*" $runtimeDir
        Remove-Item -Recurse -Force "$workDir\$runtimeCabRootDir"
        Compress-Archive -Path $workDir -DestinationPath $outputFile
        Write-Host "[生成应用程序映像]完成: $outputFileName" -ForegroundColor Green
    }
    Remove-Item -Recurse -Force $workDir
}
elseif ($Type.ToUpper().Equals("NSIS")) {
    $nsisDir = Join-Path $projectDir -ChildPath "nsis"
    $sourceDir = Join-Path -Path $nsisDir -ChildPath "SourceDir"
    $runtimeDir = Join-Path -Path $sourceDir -ChildPath "webview2_runtime"
    $installerExe = Join-Path -Path $nsisDir -ChildPath "Install TaskbarManager.exe"
    if (Test-Path -Path $sourceDir) {
        Remove-Item -Recurse -Force $sourceDir
    }
    if (Test-Path -Path $installerExe) {
        Remove-Item $installerExe
    }
    Expand-Archive -LiteralPath $baseOutputFile -DestinationPath $sourceDir
    Move-Item "$sourceDir\$basePackageName\*" $sourceDir
    Remove-Item -Recurse -Force "$sourceDir\$basePackageName"
    Write-Host "[生成应用程序包]开始" -ForegroundColor Green
    if (-not $IncludeVCRedist -and -not $IncludeWebView2Runtime) {
    }
    elseif ($IncludeVCRedist -and -not $IncludeWebView2Runtime) {
        Copy-Item $vcRedistExe $sourceDir
    }
    elseif (-not $IncludeVCRedist -and $IncludeWebView2Runtime) {
        expand $runtimeCab -F:* $sourceDir
        Move-Item "$sourceDir\$runtimeCabRootDir\*" $runtimeDir
        Remove-Item -Recurse -Force "$sourceDir\$runtimeCabRootDir"
    }
    elseif ($IncludeVCRedist -and $IncludeWebView2Runtime) {
        Copy-Item $vcRedistExe $sourceDir
        expand $runtimeCab -F:* $sourceDir
        Move-Item "$sourceDir\$runtimeCabRootDir\*" $runtimeDir
        Remove-Item -Recurse -Force "$sourceDir\$runtimeCabRootDir"
    }
    .\nsis\build.ps1
    Write-Host "[生成应用程序包]完成: $installerExe" -ForegroundColor Green
}