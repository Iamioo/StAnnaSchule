"""Packs the finished game (binaries, sources, README, a few screenshots)."""
import os
import zipfile

root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
keep = ["README.md", "build_windows.bat", "build_linux.sh"]
dirs = ["bin", "src", "templeos", "tools", "android", "switch"]
shots = ["scene_0.png", "scene_1.png", "scene_2.png", "scene_5.png", "scene_13.png",
         "scene_19.png", "scene_20.png", "scene_21.png", "scene_22.png", "scene_25.png",
         "alle_etagen.png", "v3_uebersicht.png", "android_spiel_klein.png",
         "switch_ryujinx_v3_klein.png", "musik_vorschau.wav"]
skip = {".idsig"}

out = os.path.join(root, "StAnnaSchule.zip")
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as archive:
    for f in keep:
        archive.write(os.path.join(root, f), "StAnnaSchule/" + f)
    for d in dirs:
        for base, _, files in os.walk(os.path.join(root, d)):
            for f in files:
                if os.path.splitext(f)[1] in skip:
                    continue
                p = os.path.join(base, f)
                archive.write(p, "StAnnaSchule/" + os.path.relpath(p, root).replace(os.sep, "/"))
    for s in shots:
        archive.write(os.path.join(root, "shots", s), "StAnnaSchule/shots/" + s)
print("zip ok", os.path.getsize(out))
