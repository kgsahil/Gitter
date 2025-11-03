#!/usr/bin/env python3
"""
Generate a comprehensive combined markdown from all Gitter documentation.
Preserves all formatting, images, and beautifies the output.
"""

import os
import sys
import re
from pathlib import Path


def create_toc_entry(title, level):
    """Create a table of contents entry with proper indentation."""
    indent = "  " * (level - 1)
    anchor = create_anchor(title)
    return f"{indent}- [{title}](#{anchor})"


def create_anchor(text):
    """Create an anchor from text for table of contents."""
    # Convert to lowercase, replace spaces with hyphens
    anchor = text.lower()
    anchor = re.sub(r'[^\w\s-]', '', anchor)
    anchor = re.sub(r'[-\s]+', '-', anchor)
    return anchor.strip('-')


def process_markdown_content(content, filename, for_pdf=False):
    """Process markdown content to fix internal links and enhance formatting."""
    # Add nice dividers between major sections
    content = re.sub(r'\n(## )', r'\n\n---\n\n\1', content)
    
    # Remove any duplicate horizontal rules
    content = re.sub(r'---+(\n---+)+', '---', content)
    
    # Remove emoji and special unicode for PDF compatibility
    if for_pdf:
        # Remove HTML tags and badges for PDF compatibility (before emoji removal to clean up properly)
        content = re.sub(r'<div[^>]*>\n*', '', content)  # Remove opening div tags with optional newline
        content = re.sub(r'</div>\n*', '', content)  # Remove closing div tags with optional newline
        content = re.sub(r'\[!\[.*?\]\(.*?\)\]\(.*?\)', '', content)  # Remove badge links
        content = re.sub(r'<img[^>]*>', '', content)  # Remove img tags
        
        # Remove common emoji and special unicode
        content = ''.join(char if ord(char) < 127 else ' ' for char in content)
        # Clean up extra spaces
        content = re.sub(r'  +', ' ', content)
        
        # Clean up excessive newlines (more aggressive for PDF)
        content = re.sub(r'\n{4,}', '\n\n', content)  # Max 2 newlines
        content = re.sub(r'\n{3,}', '\n\n', content)  # Max 2 newlines anywhere
    else:
        # Clean up excessive newlines
        content = re.sub(r'\n{4,}', '\n\n\n', content)
    
    return content


def read_markdown_file(filepath):
    """Read a markdown file and return its contents."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            return f.read()
    except Exception as e:
        print(f"Error reading {filepath}: {e}", file=sys.stderr)
        return None


def main():
    # Get the project root directory
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    
    # Check if PDF-safe version requested
    for_pdf = '--pdf-safe' in sys.argv
    
    if for_pdf:
        print("Generating PDF-compatible version (removing emoji and special Unicode)...")
    
    # Define the order of documentation files
    doc_files = [
        ("README.md", "Main Documentation"),
        ("docs/ARCHITECTURE.md", "Architecture"),
        ("docs/HASHER_DESIGN.md", "Hasher Design"),
        ("docs/TREE_STORAGE.md", "Tree Storage"),
        ("docs/COMMIT_SUMMARY.md", "Commit Implementation"),
        ("docs/LOG_SUMMARY.md", "Log Implementation"),
        ("docs/RESET_SUMMARY.md", "Reset Implementation"),
        ("docs/CHECKOUT_SUMMARY.md", "Checkout Implementation"),
        ("docs/DOCKER_USAGE.md", "Docker Usage"),
        ("docs/COVERAGE.md", "Code Coverage"),
        ("docs/CRITICAL_GIT_BEHAVIOR_GAPS.md", "Critical Behavior Gaps"),
    ]
    
    # Build combined content
    combined_content = []
    
    # Beautiful header
    combined_content.append("# Gitter - Complete Documentation\n")
    combined_content.append("> A comprehensive guide to the Gitter Git-like Version Control System\n")
    combined_content.append(">\n")
    combined_content.append("> Built from scratch to understand Git internals\n")
    combined_content.append(">\n")
    combined_content.append("> Generated from repository documentation\n\n")
    
    combined_content.append("---\n\n")
    
    # Build table of contents - just the main document sections
    combined_content.append("## Table of Contents\n\n")
    
    for doc_file, doc_title in doc_files:
        filepath = project_root / doc_file
        if not filepath.exists():
            continue
        combined_content.append(f"- [{doc_title}](#{create_anchor(doc_title)})\n")
    
    combined_content.append("\n---\n\n")
    
    # Now add the actual content
    for doc_file, doc_title in doc_files:
        filepath = project_root / doc_file
        if not filepath.exists():
            continue
        
        print(f"Processing {doc_file}...")
        content = read_markdown_file(filepath)
        if content is None:
            continue
        
        # Process content
        content = process_markdown_content(content, doc_file, for_pdf)
        
        # Add page break before major sections
        combined_content.append(f"\n\n<!-- Page Break -->\n\n")
        
        # Add section header with document title
        if doc_file == "README.md":
            # For README, we keep its title
            combined_content.append(content)
        else:
            # For other docs, add the document title as a section
            combined_content.append(f"# {doc_title}\n\n")
            # Remove the first # heading if it exists (already added above)
            content = re.sub(r'^# .+\n\n?', '', content, count=1)
            combined_content.append(content)
        
        combined_content.append("\n\n---\n\n")
    
    # Final cleanup for PDF (one more pass on the full content)
    if for_pdf:
        full_content = ''.join(combined_content)
        full_content = re.sub(r'\n{3,}', '\n\n', full_content)  # Max 2 newlines
        combined_content = full_content.split('\n')
        combined_content = [line + '\n' for line in combined_content]
    
    # Write combined markdown
    output_name = "GITTER_COMPLETE_DOCS_PDF.md" if for_pdf else "GITTER_COMPLETE_DOCS.md"
    combined_md = project_root / output_name
    print(f"\nWriting combined markdown to {combined_md}...")
    with open(combined_md, 'w', encoding='utf-8') as f:
        f.write(''.join(combined_content))
    
    print(f"\n✓ Combined markdown written to: {combined_md}")
    print(f"Total size: {combined_md.stat().st_size:,} bytes")
    print(f"Total lines: {len(combined_content)}")
    
    # Create a second version without TOC for cleaner structure
    no_toc_content = combined_content.copy()
    # Remove TOC section
    toc_end_idx = None
    for i, line in enumerate(no_toc_content):
        if line.startswith('## Table of Contents'):
            toc_start_idx = i
            # Find the next --- separator
            for j in range(i + 1, len(no_toc_content)):
                if no_toc_content[j].strip() == '---':
                    toc_end_idx = j
                    break
            break
    
    if toc_end_idx:
        del no_toc_content[toc_start_idx:toc_end_idx + 1]
    
    output_name_no_toc = "GITTER_COMPLETE_DOCS_NO_TOC.md" if not for_pdf else "GITTER_COMPLETE_DOCS_PDF_NO_TOC.md"
    combined_md_no_toc = project_root / output_name_no_toc
    print(f"\nWriting version without TOC to {combined_md_no_toc}...")
    with open(combined_md_no_toc, 'w', encoding='utf-8') as f:
        f.write(''.join(no_toc_content))
    
    print(f"✓ No-TOC version written to: {combined_md_no_toc}")
    print(f"Total size: {combined_md_no_toc.stat().st_size:,} bytes")
    
    if for_pdf:
        print("\n✓ PDF-compatible markdown generated!")
        print("You can now convert to PDF using: pandoc GITTER_COMPLETE_DOCS_PDF_NO_TOC.md -o output.pdf")
    
    print("\nDone!")


if __name__ == "__main__":
    main()

