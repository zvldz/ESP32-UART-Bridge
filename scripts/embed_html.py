Import("env")
import os
import re
from pathlib import Path

print("\n" + "="*60)
print("EMBED_HTML.PY SCRIPT STARTED!")
print("="*60 + "\n")

def process_html_files(source, target, env):
    """Embed HTML/CSS/JS files into C++ header"""
    print("Embedding web files...")
    
    web_src = Path("src/web_src")
    generated = Path("src/generated")
    
    print(f"  Looking for web_src at: {web_src.absolute()}")
    print(f"  Current directory: {os.getcwd()}")
    
    # Create generated directory
    generated.mkdir(exist_ok=True)
    print(f"  Created/verified directory: {generated.absolute()}")
    
    # Check if source directory exists
    if not web_src.exists():
        print(f"ERROR: {web_src} directory not found!")
        env.Exit(1)
    
    # Read CSS file
    css_content = ""
    css_file = web_src / "style.css"
    if css_file.exists():
        css_content = css_file.read_text(encoding='utf-8')
        print(f"  Found {css_file.name}")
    
    # Read main.js file
    js_main_content = ""
    js_main_file = web_src / "main.js"
    if js_main_file.exists():
        js_main_content = js_main_file.read_text(encoding='utf-8')
        print(f"  Found {js_main_file.name}")
    
    # Read crash-log.js file
    js_crash_content = ""
    js_crash_file = web_src / "crash-log.js"
    if js_crash_file.exists():
        js_crash_content = js_crash_file.read_text(encoding='utf-8')
        print(f"  Found {js_crash_file.name}")
    
    # Generate web_content.h
    with open(generated / "web_content.h", "w", encoding='utf-8') as f:
        f.write("#ifndef WEB_CONTENT_H\n")
        f.write("#define WEB_CONTENT_H\n\n")
        f.write("#include <Arduino.h>\n\n")
        f.write("// Auto-generated file, do not edit manually!\n")
        f.write("// Generated from files in src/web_src/\n\n")
        
        # Process HTML files
        for html_file in sorted(web_src.glob("*.html")):
            print(f"  Processing {html_file.name}")
            
            content = html_file.read_text(encoding='utf-8')
            
            # Generate constant name
            const_name = f"HTML_{html_file.stem.upper()}"
            
            # Write constant
            f.write(f'const char {const_name}[] PROGMEM = R"rawliteral(\n')
            f.write(content)
            f.write('\n)rawliteral";\n\n')
        
        # Write CSS constant
        if css_content:
            f.write('const char CSS_STYLE[] PROGMEM = R"rawliteral(\n')
            f.write(css_content)
            f.write('\n)rawliteral";\n\n')
        
        # Write JS constants
        if js_main_content:
            f.write('const char JS_MAIN[] PROGMEM = R"rawliteral(\n')
            f.write(js_main_content)
            f.write('\n)rawliteral";\n\n')
        
        if js_crash_content:
            f.write('const char JS_CRASH_LOG[] PROGMEM = R"rawliteral(\n')
            f.write(js_crash_content)
            f.write('\n)rawliteral";\n\n')
        
        f.write("#endif // WEB_CONTENT_H\n")
    
    print("Web files embedded successfully!")
    print("Generated: src/generated/web_content.h")

# Execute immediately during script load
process_html_files(None, None, env)

# Also register for subsequent builds
env.AddPreAction("buildprog", process_html_files)