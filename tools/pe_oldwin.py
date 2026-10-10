"""Marks a 32-bit exe as runnable on Windows 4.0 (95/98/NT4).

Modern linkers refuse subsystem versions below 5.01, but the exe built by
build_windows.bat only imports functions that exist since Windows 95.
"""
import struct
import sys

path = sys.argv[1]
data = bytearray(open(path, "rb").read())
pe = struct.unpack_from("<I", data, 0x3C)[0]
if data[pe:pe + 4] != b"PE\0\0":
    sys.exit("not a PE file")
opt = pe + 24
if struct.unpack_from("<H", data, opt)[0] != 0x10B:
    sys.exit("not a 32-bit exe")
struct.pack_into("<HH", data, opt + 40, 4, 0)  # operating system version
struct.pack_into("<HH", data, opt + 48, 4, 0)  # subsystem version
open(path, "wb").write(data)
print(f"{path}: marked for Windows 4.0+")
