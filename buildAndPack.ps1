if (Test-Path -Path .\TaskbarManager-*.zip) {
    Remove-Item -Force .\TaskbarManager-*.zip
}

if (Test-Path -Path .\build\) {
    Remove-Item -Recurse -Force .\build\
}
cmake -S . -B build
cmake --build .\build\ --config MinSizeRel
cpack -C MinSizeRel --config .\build\CPackConfig.cmake