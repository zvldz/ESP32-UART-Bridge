Import("env")
import os
import re
import gzip
import subprocess
import sys
from pathlib import Path

# Try to import minification libraries
try:
    from minify_html import minify as html_minify
    from jsmin import jsmin
    from cssmin import cssmin
    MINIFY_AVAILABLE = True
    print("Minification libraries available")
except ImportError:
    print("Minification libraries not found. Installing...")
    try:
        subprocess.check_call([sys.executable, "-m", "pip", "install", "minify-html", "jsmin", "cssmin"])
        from minify_html import minify as html_minify
        from jsmin import jsmin
        from cssmin import cssmin
        MINIFY_AVAILABLE = True
        print("Minification libraries installed successfully")
    except Exception as e:
        print(f"Failed to install minification libraries: {e}")
        print("Proceeding without minification...")
        MINIFY_AVAILABLE = False

print("\n" + "="*60)
print("EMBED_HTML.PY SCRIPT STARTED!")
print("="*60 + "\n")

def minify_and_compress(content, content_type, filename):
    """Minify and gzip compress content"""
    original_size = len(content.encode('utf-8'))
    
    # Minify content if libraries are available
    if MINIFY_AVAILABLE:
        try:
            if content_type == 'html':
                # Skip HTML minification to preserve text readability
                # gzip compression will handle size reduction
                pass
            elif content_type == 'js':
                content = jsmin(content)
            elif content_type == 'css':
                content = cssmin(content)
        except Exception as e:
            print(f"  Warning: Failed to minify {filename}: {e}")
    
    minified_size = len(content.encode('utf-8'))
    
    # Gzip compress
    compressed_data = gzip.compress(content.encode('utf-8'), compresslevel=9)
    compressed_size = len(compressed_data)
    
    # Calculate savings
    minify_saving = ((original_size - minified_size) / original_size * 100) if original_size > 0 else 0
    gzip_saving = ((minified_size - compressed_size) / minified_size * 100) if minified_size > 0 else 0
    total_saving = ((original_size - compressed_size) / original_size * 100) if original_size > 0 else 0
    
    print(f"    {filename}: {original_size}B → {minified_size}B → {compressed_size}B "
          f"(minify: -{minify_saving:.1f}%, gzip: -{gzip_saving:.1f}%, total: -{total_saving:.1f}%)")
    
    return compressed_data

def write_compressed_constant(f, const_name, compressed_data):
    """Write compressed data as C++ constant with length"""
    f.write(f'const uint8_t {const_name}_GZ[] PROGMEM = {{\n')
    
    # Write bytes in rows of 16
    for i in range(0, len(compressed_data), 16):
        row = compressed_data[i:i+16]
        hex_values = ', '.join(f'0x{b:02X}' for b in row)
        f.write(f'  {hex_values}')
        if i + 16 < len(compressed_data):
            f.write(',')
        f.write('\n')
    
    f.write('};\n')
    f.write(f'const size_t {const_name}_GZ_LEN = {len(compressed_data)};\n\n')

def process_html_files(source, target, env):
    """Embed HTML/CSS/JS files into C++ header"""
    print("Embedding web files...")
    
    web_src = Path("src/webui_src")
    generated = Path("src/webui_gen")
    
    print(f"  Looking for webui_src at: {web_src.absolute()}")
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
        f.write("// Generated from files in src/webui_src/\n\n")
        
        # Process HTML files
        for html_file in sorted(web_src.glob("*.html")):
            print(f"  Processing {html_file.name}")
            
            content = html_file.read_text(encoding='utf-8')
            
            # Generate constant name
            const_name = f"HTML_{html_file.stem.upper()}"
            
            # Minify and compress
            compressed_data = minify_and_compress(content, 'html', html_file.name)
            
            # Write compressed constant
            write_compressed_constant(f, const_name, compressed_data)
        
        # Write CSS constant
        if css_content:
            print(f"  Processing style.css")
            compressed_data = minify_and_compress(css_content, 'css', 'style.css')
            write_compressed_constant(f, 'CSS_STYLE', compressed_data)
        
        # Write JS constants
        for js_name in js_files:
            js_file = web_src / js_name
            if js_file.exists():
                js_content = js_file.read_text(encoding='utf-8')
                print(f"  Processing {js_file.name}")
                
                # Generate constant name (replace - with _)
                const_name = f"JS_{js_file.stem.replace('-', '_').upper()}"
                
                # Minify and compress
                compressed_data = minify_and_compress(js_content, 'js', js_file.name)
                
                # Write compressed constant
                write_compressed_constant(f, const_name, compressed_data)
            else:
                print(f"  WARNING: {js_name} not found!")
        
        f.write("#endif // WEB_CONTENT_H\n")
    
    print("Web files embedded successfully!")
    print("Generated: src/webui_gen/web_content.h")

# Execute immediately during script load
process_html_files(None, None, env)

# Also register for subsequent builds
env.AddPreAction("buildprog", process_html_files)