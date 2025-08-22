.\buildAndPack.ps1
$workDirName = "TaskbarManager-1.0.0-win64"
$zip = ".\$workDirName.zip"
$zipWithWebView2FixedVersionRuntime = ".\$workDirName-WebView2FixedVersionRuntime.zip"

Expand-Archive -LiteralPath $zip -DestinationPath .

$uri = "https://msedge.sf.dl.delivery.mp.microsoft.com/filestreamingservice/files/a44ec87b-94e4-4ebd-84ed-65102358926f/Microsoft.WebView2.FixedVersionRuntime.139.0.3405.111.x64.cab"
$file = ".\Microsoft.WebView2.FixedVersionRuntime.cab"
$rootDir = "Microsoft.WebView2.FixedVersionRuntime.139.0.3405.111.x64"

if (!(Test-Path -Path $file)) {
    Invoke-WebRequest -Uri $uri -OutFile $file
}

mkdir ".\$workDirName\webview2_runtime"
expand $file -F:* ".\$workDirName"
Move-Item ".\$workDirName\$rootDir\*" ".\$workDirName\webview2_runtime"
Remove-Item -Recurse -Force ".\$workDirName\$rootDir"

Compress-Archive -Path ".\$workDirName" -DestinationPath $zipWithWebView2FixedVersionRuntime
Remove-Item -Recurse -Force ".\$workDirName"