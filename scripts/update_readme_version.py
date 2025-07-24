Import("env")

import re
from pathlib import Path

# === Correct paths for this project structure ===
defines_path = Path("src/defines.h")
readme_path = Path("README.md")

# === Read version from defines.h ===
if not defines_path.exists():
    print(f"❌ Error: defines.h not found at {defines_path}")
else:
    defines_text = defines_path.read_text(encoding="utf-8")
    version_match = re.search(r'#define\s+DEVICE_VERSION\s+"([\d.]+)"', defines_text)
    if not version_match:
        print("❌ Error: DEVICE_VERSION not found in defines.h")
    else:
        version = version_match.group(1)
        print(f"📦 Found firmware version: {version}")

        if not readme_path.exists():
            print(f"❌ Error: README.md not found at {readme_path}")
        else:
            readme_text = readme_path.read_text(encoding="utf-8")
            badge_pattern = r'(https://img\.shields\.io/badge/Version-)([\d.]+)(-[a-zA-Z]+)'

            match = re.search(badge_pattern, readme_text)
            if match:
                current_version = match.group(2)
                if current_version == version:
                    print("⚠️ No changes made to README.md — version already up to date.")
                else:
                    new_readme_text, count = re.subn(
                        badge_pattern,
                        lambda m: f"{m.group(1)}{version}{m.group(3)}",
                        readme_text
                    )
                    readme_path.write_text(new_readme_text, encoding="utf-8")
                    print(f"✅ Updated firmware badge in README.md to version {version}")
            else:
                print("⚠️ Warning: No firmware version badge found in README.md.")
                print("ℹ️ Make sure it matches this pattern exactly:")
                print("   [![Version](https://img.shields.io/badge/Version-x.y.z-brightgreen)]()")