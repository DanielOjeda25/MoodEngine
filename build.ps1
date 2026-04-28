# Wrapper que invoca cmake --build con el PATH del CMake de Visual
# Studio 2022 en frente. Permite que las reglas de permisos del agente
# en .claude/settings.local.json sean cortas y precisas:
#   "PowerShell(./build.ps1 *)"   o   "PowerShell(.\build.ps1 *)"
# en lugar de un comando inline largo con prefijo PATH + pipes.
#
# Uso: ./build.ps1 [args para cmake --build]
# Ejemplo: ./build.ps1 build/debug --config Debug --target MoodEditor

$cmakeBin = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
if (-not ($env:PATH -split ';' -contains $cmakeBin)) {
    $env:PATH = $cmakeBin + ';' + $env:PATH
}

cmake --build @args
