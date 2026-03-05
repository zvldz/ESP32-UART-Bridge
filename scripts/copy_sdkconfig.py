import shutil
import os

Import("env")

# Verify minimum Arduino framework version (BLE memory management fix)
# Disabled while pinned to 3.3.6 (3.3.7 has BTDM regression on MiniKit BLE)
# MIN_ARDUINO_VERSION = (3, 3, 7)
#
# try:
#     arduino_ver = env.GetProjectOption("platform_packages", [])
#     # Get Arduino version from platform package metadata
#     platform = env.PioPlatform()
#     pkg = platform.get_package("framework-arduinoespressif32")
#     ver = pkg.metadata.version
#     current = (ver.major, ver.minor, ver.patch)
#     if current < MIN_ARDUINO_VERSION:
#         min_str = ".".join(str(x) for x in MIN_ARDUINO_VERSION)
#         cur_str = ".".join(str(x) for x in current)
#         env.Exit(f"[ERROR] Arduino framework {cur_str} is too old. Minimum required: {min_str} (BLE fix: esp32-hal-bt-mem.h)")
#     else:
#         print(f"[INFO] Arduino framework version: {ver} (>= {'.'.join(str(x) for x in MIN_ARDUINO_VERSION)} OK)")
# except Exception as e:
#     print(f"[WARNING] Could not verify Arduino framework version: {e}")

build_env = env["PIOENV"]

print("[INFO] custom sdkconfig copy script STARTED")
print("[INFO] Current environment:", env["PIOENV"])

sdkconfig_src = os.path.join("sdkconfig", f"sdkconfig.{build_env}.defaults")
sdkconfig_dst = "sdkconfig.defaults"

if os.path.isfile(sdkconfig_src):
    print(f"[INFO] Using {sdkconfig_src} → {sdkconfig_dst}")
    shutil.copyfile(sdkconfig_src, sdkconfig_dst)
else:
    print(f"[WARNING] {sdkconfig_src} not found. Using existing/default sdkconfig.defaults.")
