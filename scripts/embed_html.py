Import("env")
import os
import re
import gzip
import hashlib
import json
from pathlib import Path

print("\n" + "="*60)
print("EMBED_HTML.PY SCRIPT STARTED!")
print("="*60 + "\n")

def strip_html_comments(content):
    """Remove HTML comments (<!-- ... -->) but preserve conditional comments"""
    # DOTALL flag makes . match newlines for multiline comments
    return re.sub(r'<!--(?!\[if)(?!<!).*?-->', '', content, flags=re.DOTALL)

def gzip_compress(content, content_type, filename):
    """Strip HTML comments and gzip compress content"""
    original_size = len(content.encode('utf-8'))

    # Strip HTML comments (safe operation that won't break code)
    if content_type == 'html':
        content = strip_html_comments(content)

    processed_size = len(content.encode('utf-8'))

    # Gzip compress
    compressed_data = gzip.compress(content.encode('utf-8'), compresslevel=9)
    compressed_size = len(compressed_data)

    # Calculate savings
    gzip_saving = ((processed_size - compressed_size) / processed_size * 100) if processed_size > 0 else 0
    total_saving = ((original_size - compressed_size) / original_size * 100) if original_size > 0 else 0

    print(f"    {filename}: {original_size}B → {compressed_size}B (gzip: -{gzip_saving:.1f}%, total: -{total_saving:.1f}%)")

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

def get_files_hash(web_src, js_files):
    """Calculate combined hash of all source files"""
    hasher = hashlib.sha256()

    # Hash HTML files
    for html_file in sorted(web_src.glob("*.html")):
        if html_file.exists():
            hasher.update(html_file.read_bytes())

    # Hash CSS
    css_file = web_src / "style.css"
    if css_file.exists():
        hasher.update(css_file.read_bytes())

    # Hash JS files
    for js_name in js_files:
        js_file = web_src / js_name
        if js_file.exists():
            hasher.update(js_file.read_bytes())

    # Note: Fragments no longer used - all HTML inlined in index.html

    return hasher.hexdigest()

def should_regenerate(web_src, generated, js_files):
    """Check if regeneration is needed"""
    cache_file = generated / ".embed_cache.json"
    target_file = generated / "web_content.h"
    
    # Always regenerate if target doesn't exist
    if not target_file.exists():
        print("  Target file missing, will generate")
        return True
    
    # Calculate current hash
    current_hash = get_files_hash(web_src, js_files)
    
    # Check cache
    if cache_file.exists():
        try:
            with open(cache_file, 'r') as f:
                cache = json.load(f)
                if cache.get('hash') == current_hash:
                    print(f"  No changes detected (hash: {current_hash[:8]}...)")
                    return False
        except:
            pass
    
    print(f"  Changes detected, regenerating (hash: {current_hash[:8]}...)")
    return True

def save_cache(generated, web_src, js_files):
    """Save current files hash to cache"""
    cache_file = generated / ".embed_cache.json"
    current_hash = get_files_hash(web_src, js_files)
    
    cache = {
        'hash': current_hash,
        'timestamp': os.path.getmtime(generated / "web_content.h"),
        'files': []
    }
    
    # Save file list for debugging
    for html_file in sorted(web_src.glob("*.html")):
        cache['files'].append(html_file.name)
    cache['files'].append("style.css")
    cache['files'].extend(js_files)
    
    with open(cache_file, 'w') as f:
        json.dump(cache, f, indent=2)

def process_html_files(source, target, env):
    """Embed HTML/CSS/JS files into C++ header"""
    print("Checking web files...")
    
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
    # Libraries (lib/) must be loaded first, in this order
    js_files = [
        # Alpine.js libraries (plugins before core)
        "lib/alpine-persist.min.js",
        "lib/alpine-collapse.min.js",
        "lib/alpine.min.js",
        # Alpine application (main entry point)
        "app.js",
        # Utility modules
        "utils.js",
        "form-utils.js",
        # TODO: Migrate crash-log.js to Alpine templates
        "crash-log.js"
    ]
    
    # Check if regeneration needed
    if not should_regenerate(web_src, generated, js_files):
        print("Web content is up-to-date, skipping generation")
        return
    
    print("Embedding web files...")
    
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

            # Compute version hash and append to asset URLs
            version_hash = get_files_hash(web_src, js_files)[:8]

            # Append ?v=<hash> to asset URLs in HTML
            content = content.replace('href="/style.css"', f'href="/style.css?v={version_hash}"')
            for js_name in js_files:
                content = content.replace(f'src="/{js_name}"', f'src="/{js_name}?v={version_hash}"')

            # Generate constant name
            const_name = f"HTML_{html_file.stem.upper()}"

            # Minify and compress
            compressed_data = gzip_compress(content, 'html', html_file.name)
            
            # Write compressed constant
            write_compressed_constant(f, const_name, compressed_data)
        
        # Write CSS constant
        if css_content:
            print(f"  Processing style.css")
            compressed_data = gzip_compress(css_content, 'css', 'style.css')
            write_compressed_constant(f, 'CSS_STYLE', compressed_data)
        
        # Write JS constants
        for js_name in js_files:
            js_file = web_src / js_name
            if js_file.exists():
                js_content = js_file.read_text(encoding='utf-8')
                print(f"  Processing {js_name}")

                # Generate constant name from full path (lib/alpine.min.js -> JS_LIB_ALPINE_MIN)
                name_for_const = js_name.replace('/', '_').replace('-', '_').replace('.min', '_MIN').replace('.js', '')
                const_name = f"JS_{name_for_const.upper()}"

                # Minify and compress
                compressed_data = gzip_compress(js_content, 'js', js_file.name)

                # Write compressed constant
                write_compressed_constant(f, const_name, compressed_data)
            else:
                print(f"  WARNING: {js_name} not found!")

        # Note: Fragments removed - all HTML now inlined in index.html
        # This reduces HTTP requests and improves gzip compression
        # (fragments_dir = web_src / "fragments" - no longer processed)

        f.write("#endif // WEB_CONTENT_H\n")
    
    # Save cache after successful generation
    save_cache(generated, web_src, js_files)
    
    print("Web files embedded successfully!")
    print("Generated: src/webui_gen/web_content.h")

# Execute immediately during script load
process_html_files(None, None, env)

# Also register for subsequent builds
env.AddPreAction("buildprog", process_html_files)