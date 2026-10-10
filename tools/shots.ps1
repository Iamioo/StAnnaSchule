# Renders the debug scenes of bin\StAnna.exe to shots\scene_N.png and,
# with -Worlds, every floor to shots\floor_N.png
param([int[]]$Scenes = (0..18), [switch]$Worlds)
$root = Split-Path -Parent $PSScriptRoot
New-Item -ItemType Directory -Force "$root\shots" | Out-Null
foreach ($n in $Scenes) {
    $bmp = "$root\shots\scene_$n.bmp"
    $p = Start-Process -FilePath "$root\bin\StAnna.exe" -ArgumentList "-shot", $n, "`"$bmp`"" -Wait -PassThru -NoNewWindow
    if ($p.ExitCode -ne 0) { Write-Output "scene $n failed ($($p.ExitCode))" }
}
if ($Worlds) {
    foreach ($f in 1..6) {
        $bmp = "$root\shots\floor_$f.bmp"
        $p = Start-Process -FilePath "$root\bin\StAnna.exe" -ArgumentList "-world", $f, "`"$bmp`"" -Wait -PassThru -NoNewWindow
        if ($p.ExitCode -ne 0) { Write-Output "floor $f failed ($($p.ExitCode))" }
    }
}
python -I -c @"
import sys, glob, os
from PIL import Image
for b in glob.glob(os.path.join(sys.argv[1], '*.bmp')):
    Image.open(b).save(b[:-4] + '.png', optimize=True)
    os.remove(b)
print('ok')
"@ "$root\shots"
