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
    
    # Define all JavaScript files to process
    js_files = [
        "main.js",
        "crash-log.js",
        "utils.js",
        "device-config.js",
        "form-utils.js",
        "status-updates.js"
    ]
    
    # Read CSS file
    css_content = ""
    css_file = web_src / "style.css"
    if css_file.exists():
        css_content = css_file.read_text(encoding='utf-8')
        print(f"  Found {css_file.name}")
    
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
        for js_name in js_files:
            js_file = web_src / js_name
            if js_file.exists():
                js_content = js_file.read_text(encoding='utf-8')
                print(f"  Found {js_file.name}")
                
                # Generate constant name (replace - with _)
                const_name = f"JS_{js_file.stem.replace('-', '_').upper()}"
                
                f.write(f'const char {const_name}[] PROGMEM = R"rawliteral(\n')
                f.write(js_content)
                f.write('\n)rawliteral";\n\n')
            else:
                print(f"  WARNING: {js_name} not found!")
        
        f.write("#endif // WEB_CONTENT_H\n")
    
    print("Web files embedded successfully!")
    print("Generated: src/generated/web_content.h")

# Execute immediately during script load
process_html_files(None, None, env)

# Also register for subsequent builds
env.AddPreAction("buildprog", process_html_files)