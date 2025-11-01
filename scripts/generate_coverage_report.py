#!/usr/bin/env python3
"""
Generate a code coverage summary report from gcov data
"""
import subprocess
import os
import sys
import re
from pathlib import Path

def get_coverage(gcda_path):
    """Run gcov and extract coverage info"""
    try:
        result = subprocess.run(
            ['gcov', '-b', '-m', str(gcda_path)],
            capture_output=True,
            text=True,
            cwd=gcda_path.parent
        )
        
        # Look for our source files in the output
        for line in result.stdout.split('\n'):
            if 'File' in line and 'src/' in line and '.cpp' in line and 'Lines executed' in line:
                # Extract file name and coverage
                match = re.search(r"File '([^']+src/[^']+)'", line)
                if match:
                    filepath = match.group(1)
                    filename = os.path.basename(filepath)
                    
                    # Extract coverage percentages
                    line_match = re.search(r'Lines executed:([0-9.]+)%', line)
                    branch_match = re.search(r'Branches executed:([0-9.]+)%', line)
                    
                    lines_pct = line_match.group(1) if line_match else None
                    branch_pct = branch_match.group(1) if branch_match else None
                    
                    return {
                        'file': filename,
                        'lines': lines_pct,
                        'branches': branch_pct
                    }
        return None
    except Exception as e:
        return None

def main():
    # Default to build directory
    build_dir = sys.argv[1] if len(sys.argv) > 1 else 'build/linux-coverage'
    
    if not os.path.exists(build_dir):
        print(f"Error: Build directory '{build_dir}' not found")
        print("Run: cmake --preset linux-coverage && cmake --build build/linux-coverage --target gitter_tests")
        sys.exit(1)
    
    build_path = Path(build_dir).resolve()
    
    # Find all .gcda files
    gcda_files = []
    for pattern in [
        'CMakeFiles/gitter_lib.dir/src/cli/commands/*.gcda',
        'CMakeFiles/gitter_lib.dir/src/core/*.gcda',
        'CMakeFiles/gitter_lib.dir/src/util/*.gcda',
        'CMakeFiles/gitter_lib.dir/src/cli/*.gcda'
    ]:
        found = list(build_path.glob(pattern))
        if not found:
            # Try absolute path
            abs_pattern = str(build_path / pattern)
            import glob
            found = [Path(f) for f in glob.glob(abs_pattern)]
        gcda_files.extend(found)
    
    gcda_files = [f for f in gcda_files if 'googletest' not in str(f)]
    
    if not gcda_files:
        print("No coverage data found. Make sure tests have been run.")
        sys.exit(1)
    
    print("=== Gitter Code Coverage Summary ===\n")
    
    # Collect coverage data
    coverage_data = []
    for gcda in sorted(gcda_files):
        data = get_coverage(gcda)
        if data:
            coverage_data.append(data)
    
    # Print results
    for data in sorted(coverage_data, key=lambda x: x['file']):
        print(f"{data['file']}")
        if data['lines']:
            print(f"  Lines: {data['lines']}%")
        if data['branches']:
            print(f"  Branches: {data['branches']}%")
        print()
    
    # Calculate overall statistics
    if coverage_data:
        total_lines = sum(float(d['lines']) for d in coverage_data if d['lines'])
        total_branches = sum(float(d['branches']) for d in coverage_data if d['branches'])
        count = len(coverage_data)
        
        print(f"=== Overall Statistics ===")
        print(f"Files tested: {count}")
        if count > 0:
            avg_lines = total_lines / count
            avg_branches = total_branches / count
            print(f"Average line coverage: {avg_lines:.2f}%")
            print(f"Average branch coverage: {avg_branches:.2f}%")

if __name__ == '__main__':
    main()

