# Builds bin\StAnna.apk without the Android NDK:
#  - C compiled with devkitPro's freestanding GCCs (devkitA64 / devkitARM)
#  - linked by the ld.lld that ships in the SDK build-tools, against tiny
#    stub libraries that only carry the right SONAMEs and symbol names
#  - packaged with aapt2 / zipalign / apksigner from the SDK
# (tools print warnings on stderr; failures are caught by exit code in Run)
$ErrorActionPreference = "Continue"
$root = Split-Path -Parent $PSScriptRoot
$sdk = if ($env:ANDROID_HOME) { $env:ANDROID_HOME } else { "$env:LOCALAPPDATA\Android\Sdk" }
$bt = Get-ChildItem "$sdk\build-tools" | Sort-Object Name | Select-Object -Last 1 -ExpandProperty FullName
$platform = Get-ChildItem "$sdk\platforms" | Sort-Object Name | Select-Object -Last 1 -ExpandProperty FullName
$lld = "$bt\lld.exe"
$out = "$root\build\android"
New-Item -ItemType Directory -Force $out, "$root\bin" | Out-Null

function Run($exe) {
    & $exe @args
    if ($LASTEXITCODE -ne 0) { throw "$exe failed ($LASTEXITCODE)" }
}

# symbols the stub libraries must provide
$stubs = @{
    "libandroid.so" = "ANativeWindow_setBuffersGeometry ANativeWindow_lock ANativeWindow_unlockAndPost ANativeWindow_getWidth ANativeWindow_getHeight AInputQueue_getEvent AInputQueue_preDispatchEvent AInputQueue_finishEvent AInputEvent_getType AKeyEvent_getAction AKeyEvent_getKeyCode AMotionEvent_getAction AMotionEvent_getPointerCount AMotionEvent_getX AMotionEvent_getY ANativeActivity_finish ANativeActivity_setWindowFlags"
    "liblog.so"     = "__android_log_print"
    "libdl.so"      = "dlopen dlsym"
    "libc.so"       = "clock_gettime pthread_create pthread_join usleep memset memcpy memmove __aeabi_memcpy __aeabi_memset __aeabi_memclr"
}

$abis = @(
    @{ abi = "arm64-v8a"; cc = "C:\devkitPro\devkitA64\bin\aarch64-none-elf-gcc.exe"; emu = "aarch64linux";
       flags = @("-mno-outline-atomics"); libgcc = $null },
    @{ abi = "armeabi-v7a"; cc = "C:\devkitPro\devkitARM\bin\arm-none-eabi-gcc.exe"; emu = "armelf_linux_eabi";
       flags = @("-march=armv7-a", "-mthumb", "-mfloat-abi=softfp", "-mfpu=vfpv3-d16");
       libgcc = "thumb" }
)

foreach ($a in $abis) {
    $d = "$out\$($a.abi)"
    New-Item -ItemType Directory -Force "$d\stubs" | Out-Null
    $common = @("-O2", "-fPIC", "-ffreestanding", "-fno-stack-protector", "-fvisibility=hidden",
                "-fno-asynchronous-unwind-tables", "-fno-unwind-tables", "-std=gnu99", "-Wall", "-Wno-pointer-sign",
                "-Wno-unused-but-set-variable") + $a.flags

    foreach ($lib in $stubs.Keys) {
        $c = ($stubs[$lib] -split " " | ForEach-Object { "__attribute__((visibility(`"default`"))) void $_(void) {}" }) -join "`n"
        Set-Content -Encoding ascii "$d\stubs\$lib.c" $c
        Run $a.cc @common -c "$d\stubs\$lib.c" -o "$d\stubs\$lib.o"
        Run $lld -flavor gnu -m $a.emu -shared -soname $lib -o "$d\stubs\$lib" "$d\stubs\$lib.o"
    }

    Run $a.cc @common -c "$root\src\game.c" -o "$d\game.o"
    Run $a.cc @common -c "$root\src\plat_android.c" -o "$d\plat_android.o"
    $extra = @()
    if ($a.libgcc) {
        $extra += (& $a.cc @($a.flags) -print-libgcc-file-name).Trim()
    }
    Run $lld -flavor gnu -m $a.emu -shared -soname libstanna.so --hash-style=both `
        -z noexecstack -z relro -z now -z max-page-size=16384 --no-undefined --gc-sections `
        -o "$d\libstanna.so" "$d\game.o" "$d\plat_android.o" @extra `
        "$d\stubs\libandroid.so" "$d\stubs\liblog.so" "$d\stubs\libdl.so" "$d\stubs\libc.so"
    Write-Output "built $($a.abi)/libstanna.so ($((Get-Item "$d\libstanna.so").Length) bytes)"
}

# resources + manifest -> base apk
Run "$bt\aapt2.exe" compile --dir "$root\android\res" -o "$out\res.zip"
Run "$bt\aapt2.exe" link -o "$out\base.apk" -I "$platform\android.jar" `
    --manifest "$root\android\AndroidManifest.xml" --min-sdk-version 16 --target-sdk-version 34 `
    "$out\res.zip"

# add the native libraries (stored, so they could also be mapped directly)
python -I -c @"
import sys, zipfile, shutil
src, dst, base = sys.argv[1], sys.argv[2], sys.argv[3]
shutil.copy(src, dst)
with zipfile.ZipFile(dst, 'a') as z:
    for abi in ('arm64-v8a', 'armeabi-v7a'):
        z.write(f'{base}/{abi}/libstanna.so', f'lib/{abi}/libstanna.so', compress_type=zipfile.ZIP_STORED)
"@ "$out\base.apk" "$out\unaligned.apk" "$out"
if ($LASTEXITCODE -ne 0) { throw "zip failed" }

Run "$bt\zipalign.exe" -f -P 16 4 "$out\unaligned.apk" "$out\aligned.apk"

$ks = "$out\debug.keystore"
if (-not (Test-Path $ks)) {
    Run keytool -genkeypair -keystore $ks -storepass android -keypass android -alias stanna `
        -keyalg RSA -keysize 2048 -validity 10000 -dname "CN=St. Anna Schule"
}
Run "$bt\apksigner.bat" sign --ks $ks --ks-pass pass:android --key-pass pass:android `
    --min-sdk-version 16 --out "$root\bin\StAnna.apk" "$out\aligned.apk"
Write-Output "Built bin\StAnna.apk"
