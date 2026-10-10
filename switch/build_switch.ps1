# Builds bin\StAnna.nro for the Nintendo Switch Homebrew Menu with devkitPro
# (devkitA64 + libnx). Needs DEVKITPRO, default C:\devkitPro.
$ErrorActionPreference = "Continue"
$root = Split-Path -Parent $PSScriptRoot
$dkp = if ($env:DEVKITPRO) { $env:DEVKITPRO } else { "C:\devkitPro" }
$env:DEVKITPRO = $dkp
$cc = "$dkp\devkitA64\bin\aarch64-none-elf-gcc.exe"
$tools = "$dkp\tools\bin"
$out = "$root\build\switch"
New-Item -ItemType Directory -Force $out, "$root\bin" | Out-Null

function Run($exe) {
    & $exe @args
    if ($LASTEXITCODE -ne 0) { throw "$exe failed ($LASTEXITCODE)" }
}

$arch = @("-march=armv8-a+crc+crypto", "-mtune=cortex-a57", "-mtp=soft", "-fPIE")
$cflags = @("-O2", "-ffunction-sections", "-Wall", "-Wno-pointer-sign", "-D__SWITCH__",
            "-I$dkp\libnx\include") + $arch

Run $cc @cflags -c "$root\src\game.c" -o "$out\game.o"
Run $cc @cflags -c "$root\src\plat_switch.c" -o "$out\plat_switch.o"
Run $cc "-specs=$dkp\libnx\switch.specs" -g @arch "-Wl,-Map,$out\stanna.map" `
    "$out\game.o" "$out\plat_switch.o" "-L$dkp\libnx\lib" -lnx -o "$out\stanna.elf"

# 256x256 JPEG icon from the jumpscare face
if (-not (Test-Path "$out\icon.jpg")) {
    Start-Process "$root\bin\StAnna.exe" -ArgumentList "-icon", "8", "`"$out\face.bmp`"" -Wait -NoNewWindow
    python -I -c @"
import sys
from PIL import Image
face = Image.open(sys.argv[1]).convert('RGB')
icon = Image.new('RGB', (256, 256), (0x14, 0x10, 0x1c))
icon.paste(face, (0, 0))
icon.save(sys.argv[2], quality=95)
"@ "$out\face.bmp" "$out\icon.jpg"
}

Run "$tools\nacptool.exe" --create "St. Anna Schule" "Herr Thuener kommt" "1.0.0" "$out\stanna.nacp"
Run "$tools\elf2nro.exe" "$out\stanna.elf" "$root\bin\StAnna.nro" "--icon=$out\icon.jpg" "--nacp=$out\stanna.nacp"
Write-Output "Built bin\StAnna.nro ($((Get-Item "$root\bin\StAnna.nro").Length) bytes)"
