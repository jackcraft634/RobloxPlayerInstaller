$ErrorActionPreference = "Stop"
$project = $PSScriptRoot
$dist = Join-Path $project "dist"
$src = Join-Path $project "src"
New-Item -ItemType Directory -Force $dist | Out-Null

$compiler = "C:\Tools\llvm-mingw-20261020-ucrt-x86_64\bin\x86_64-w64-mingw32-g++.exe"
$windres = "C:\Tools\llvm-mingw-20261020-ucrt-x86_64\bin\llvm-windres.exe"
if (-not (Test-Path $compiler)) { throw "Compiler not found: $compiler" }
if (-not (Test-Path $windres)) { throw "Resource compiler not found: $windres" }

Push-Location $src
try {
  & $windres app.rc -O coff -o app.res
  if ($LASTEXITCODE -ne 0) { throw "Resource compilation failed" }
  $out = Join-Path $dist "RobloxRenewal.exe"
  & $compiler -std=c++17 -O2 -s -DNDEBUG -DUNICODE -D_UNICODE -municode -m64 `
    -static -static-libgcc -static-libstdc++ -mwindows main.cpp app.res `
    -lwininet -lole32 -lshell32 -luser32 -lkernel32 `
    -o $out
  if ($LASTEXITCODE -ne 0) { throw "Build failed" }
  Copy-Item $out ([Environment]::GetFolderPath("Desktop")) -Force
} finally {
  Pop-Location
}
Write-Host "Built $out"
