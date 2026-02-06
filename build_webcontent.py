#!/usr/bin/env python3
"""Generate WebContent.h from data folder files"""

import os
import re

def escape_for_c(content):
    """Escape string for C/C++ string literal"""
    result = []
    for char in content:
        if char == '\\':
            result.append('\\\\')
        elif char == '"':
            result.append('\\"')
        elif char == '\n':
            result.append('\\n')
        elif char == '\r':
            pass  # Skip carriage returns
        elif char == '\t':
            result.append('\\t')
        else:
            result.append(char)
    return ''.join(result)

def minify_js(content):
    """Safe JS minification - only remove empty lines and leading/trailing whitespace"""
    lines = content.split('\n')
    result = []
    for line in lines:
        # Keep the line content but strip leading/trailing whitespace
        stripped = line.strip()
        if stripped:  # Skip completely empty lines
            result.append(stripped)
    # Join with semicolon+space to be safe (avoids ASI issues)
    # Actually, just use newlines to be completely safe
    return '\n'.join(result)

def minify_css(content):
    """Simple CSS minification"""
    # Remove comments
    content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
    # Collapse whitespace
    content = re.sub(r'\s+', ' ', content)
    # Remove spaces around special chars
    content = re.sub(r'\s*([{}:;,])\s*', r'\1', content)
    return content.strip()

def main():
    # Read files
    with open('data/index.html', 'r', encoding='utf-8') as f:
        html = f.read()
    with open('data/style.css', 'r', encoding='utf-8') as f:
        css = minify_css(f.read())
    with open('data/app.js', 'r', encoding='utf-8') as f:
        js = minify_js(f.read())

    # Inject CSS and JS into HTML
    html = html.replace('<link rel="stylesheet" href="style.css">', f'<style>{css}</style>')
    html = html.replace('<script src="app.js"></script>', f'<script>{js}</script>')

    # Escape for C string
    escaped = escape_for_c(html)

    # Write header file
    with open('WebContent.h', 'w', encoding='utf-8') as f:
        f.write('#ifndef WEB_CONTENT_H\n')
        f.write('#define WEB_CONTENT_H\n\n')
        f.write('#include <pgmspace.h>\n\n')
        f.write('const char INDEX_HTML[] PROGMEM = "')
        f.write(escaped)
        f.write('";\n\n')
        f.write('#endif\n')

    print('WebContent.h regenerated successfully!')
    print(f'Total size: {len(escaped)} chars')

if __name__ == '__main__':
    main()
