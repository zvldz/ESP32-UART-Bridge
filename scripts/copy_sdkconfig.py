import shutil
import os

Import("env")

build_env = env["PIOENV"]

print("[INFO] custom sdkconfig copy script STARTED")
print("[INFO] Current environment:", env["PIOENV"])

sdkconfig_src = f"sdkconfig.{build_env}.defaults"
sdkconfig_dst = "sdkconfig.defaults"

if os.path.isfile(sdkconfig_src):
    print(f"[INFO] Using {sdkconfig_src} → {sdkconfig_dst}")
    shutil.copyfile(sdkconfig_src, sdkconfig_dst)
else:
    print(f"[WARNING] {sdkconfig_src} not found. Using existing/default sdkconfig.defaults.")
