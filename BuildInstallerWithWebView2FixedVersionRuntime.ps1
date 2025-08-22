if (Test-Path -Path .\nsis\SourceDir\) {
    Remove-Item -Recurse -Force .\nsis\SourceDir\
}
if (Test-Path -Path '.\nsis\Install TaskbarManager.exe') {
    Remove-Item '.\nsis\Install TaskbarManager.exe'
}

Expand-Archive -LiteralPath .\TaskbarManager-1.0.0-win64-WebView2FixedVersionRuntime.zip -DestinationPath .\nsis\SourceDir\
Move-Item .\nsis\SourceDir\TaskbarManager-1.0.0-win64\* .\nsis\SourceDir\
Remove-Item -Recurse -Force .\nsis\SourceDir\TaskbarManager-1.0.0-win64\

.\nsis\build.ps1