if ((Test-Path -Path .\build\)) {
    rm -Recurse -Force .\build\
}
cmake -S . -B build
cmake --build .\build\ --config MinSizeRel
cpack -C MinSizeRel --config .\build\CPackConfig.cmake