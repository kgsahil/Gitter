# SHA-1 Strategy Pattern & Git-Compliant Object Storage

## Overview

This document describes the implementation of the Strategy Pattern for hash algorithms and Git-compliant object storage with zlib compression.

## Changes Made

### 1. Hash Algorithm Strategy Pattern

#### New Files Created

**`src/util/IHasher.hpp`** - Strategy interface for hash algorithms
- `IHasher` abstract base class with virtual methods:
  - `reset()` - Reset hasher state
  - `update(data, len)` - Update hash with bytes
  - `digest()` - Finalize and return hash
  - `name()` - Get algorithm name ("sha1", "sha256")
  - `digestSize()` - Get digest size in bytes (20 for SHA-1, 32 for SHA-256)
  - `toHex(bytes)` - Static method to convert binary hash to hex string

- `HasherFactory` for creating hasher instances:
  - `createDefault()` - Returns SHA-1 hasher (Git default)
  - `create(algorithm)` - Creates specific hasher by name

**`src/util/Sha1Hasher.hpp/.cpp`** - SHA-1 implementation
- Implements `IHasher` interface
- Produces 160-bit (20-byte) digests
- Git's default hash algorithm
- Note: SHA-1 is cryptographically broken but still used by Git for backward compatibility

**Updated `src/util/Hasher.hpp/.cpp`** - SHA-256 implementation
- Renamed `Sha256` class to `Sha256Hasher`
- Now implements `IHasher` interface
- Produces 256-bit (32-byte) digests
- Git is transitioning to SHA-256 for improved security

### 2. Git-Compliant Object Storage

#### Updated `src/core/ObjectStore.hpp/.cpp`

**Key Changes:**

1. **2-Character Directory Structure**
   ```
   Old: .gitter/objects/<full-hash>
   New: .gitter/objects/<first-2-chars>/<remaining-chars>
   
   Example: hash "abc123..." → .gitter/objects/ab/c123...
   ```
   - Matches Git's standard object storage layout
   - Prevents too many files in single directory
   - Improves filesystem performance

2. **Zlib Compression**
   - All objects are now compressed with zlib before writing
   - Uses `deflate()` for compression, `inflate()` for decompression
   - Matches Git's loose object storage format
   - Significantly reduces disk space usage

3. **Strategy Pattern Integration**
   - Constructor now accepts `std::unique_ptr<IHasher>`
   - Defaults to SHA-1 if no hasher provided
   - Allows runtime selection of hash algorithm
   - Example:
     ```cpp
     // Use default SHA-1
     ObjectStore store1(repoRoot);
     
     // Use SHA-256
     ObjectStore store2(repoRoot, std::make_unique<Sha256Hasher>());
     ```

4. **New Methods**
   - `readObject(hash)` - Read and decompress object from storage
   - `getObjectPath(hash)` - Get path for object with 2-char directory structure

### 3. Enhanced Index (Staging Area)

#### Updated `src/core/Index.hpp/.cpp`

**New Fields in `IndexEntry`:**

1. **`mode` (uint32_t)** - File permissions
   - Git uses octal mode: `0100644` (regular file) or `0100755` (executable)
   - Tracks whether file is executable
   - Essential for preserving permissions across platforms

2. **`ctimeNs` (uint64_t)** - Creation/status change time
   - Git tracks both mtime and ctime
   - Used for detecting file metadata changes
   - Approximated with mtime on systems without true ctime

**Updated On-Disk Format:**
```
Old: path<TAB>hash<TAB>size<TAB>mtime
New: path<TAB>hash<TAB>size<TAB>mtime<TAB>mode<TAB>ctime
```

Example:
```
src/main.cpp    a3b2c1...    1024    1234567890000000000    33188    1234567890000000000
```

### 4. Updated AddCommand

#### `src/cli/commands/AddCommand.cpp`

**New Functionality:**

1. **File Permission Detection**
   - Reads file permissions using `std::filesystem::status()`
   - Normalizes to Git mode:
     - `0100644` - Regular file (non-executable)
     - `0100755` - Executable file
   - Checks owner, group, and others execute bits

2. **Ctime Tracking**
   - Captures creation/status change time
   - Currently approximated with mtime (platform-dependent)

## Git Object Format

### Blob Object Structure

```
"blob <size>\0<content>"
```

Example for file containing "hello world":
```
"blob 11\0hello world"
```

### Storage Process

1. **Create Git Object**
   ```
   header = "blob " + filesize + "\0"
   object = header + file_content
   ```

2. **Compute Hash**
   ```
   hash = SHA1(object)  // or SHA256
   ```

3. **Compress**
   ```
   compressed = zlib_deflate(object)
   ```

4. **Write to Disk**
   ```
   path = .gitter/objects/<hash[0:2]>/<hash[2:]>
   write(path, compressed)
   ```

### Index to Blob Linking

1. **File Added to Staging**
   ```bash
   gitter add file.txt
   ```

2. **Blob Created**
   - File content read
   - Git blob object created: `"blob <size>\0<content>"`
   - Hash computed (SHA-1 by default)
   - Compressed and stored in `.gitter/objects/<aa>/<bbbb...>`

3. **Index Entry Created**
   - Path: `file.txt`
   - Hash: `<blob-hash>`
   - Size: `<file-size>`
   - Mtime: `<modification-time>`
   - Mode: `0100644` or `0100755`
   - Ctime: `<creation-time>`

4. **Index Saved**
   - TSV format written to `.gitter/index`

5. **Commit Process** (future)
   - Read index entries
   - Build tree objects from index (directory structure)
   - Tree entries point to blob hashes
   - Commit object points to root tree

## Benefits of These Changes

### 1. Git Compatibility
- ✅ Matches Git's object storage layout exactly
- ✅ Uses same blob format: `"blob <size>\0<content>"`
- ✅ Implements 2-char directory structure
- ✅ Uses zlib compression
- ✅ Defaults to SHA-1 for compatibility

### 2. Extensibility
- ✅ Strategy Pattern allows swapping hash algorithms
- ✅ Easy to add SHA-256 support (already implemented)
- ✅ Can support future hash algorithms (SHA3, BLAKE2, etc.)

### 3. Performance
- ✅ Zlib compression reduces disk usage
- ✅ 2-char directory structure improves filesystem performance
- ✅ Index tracks file metadata for fast dirty detection

### 4. Correctness
- ✅ Preserves file permissions (executable bit)
- ✅ Tracks both mtime and ctime
- ✅ Proper Git blob format ensures interoperability

## Migration Notes

### Existing Repositories

If you have an existing `.gitter` repository created before these changes:

1. **Object Storage**
   - Old objects are stored flat: `.gitter/objects/<hash>`
   - New objects use 2-char dirs: `.gitter/objects/<aa>/<bb...>`
   - Old objects will NOT be automatically migrated
   - Recommendation: Re-initialize repository or manually migrate

2. **Index Format**
   - Old index: `path<TAB>hash<TAB>size<TAB>mtime`
   - New index: `path<TAB>hash<TAB>size<TAB>mtime<TAB>mode<TAB>ctime`
   - Old index will be read with mode=0, ctime=0
   - Next `gitter add` will update to new format

3. **Hash Algorithm**
   - Old: SHA-256 (32-byte hashes)
   - New: SHA-1 by default (20-byte hashes)
   - Hashes are NOT compatible
   - Recommendation: Re-add files to regenerate with SHA-1

### Clean Start

For new repositories:
```bash
rm -rf .gitter
gitter init
gitter add .
gitter status
```

## Testing

### Verify Object Storage

```bash
# Add a file
echo "hello world" > test.txt
gitter add test.txt

# Check object storage structure
ls -la .gitter/objects/
# Should see 2-char directories like: 95/  d5/  etc.

# Find the blob
find .gitter/objects -type f
# Example output: .gitter/objects/95/d09f2b10159347eece71399a7e2e907ea3df4f
```

### Verify Compression

```bash
# Check if object is compressed (should be binary, not readable)
cat .gitter/objects/95/d09f2b10159347eece71399a7e2e907ea3df4f
# Should show binary data (zlib compressed)
```

### Verify Index Format

```bash
# Check index format
cat .gitter/index
# Should show: path<TAB>hash<TAB>size<TAB>mtime<TAB>mode<TAB>ctime
# Example: test.txt    95d09f2b...    12    1234567890000000000    33188    1234567890000000000
```

### Verify Hash Algorithm

```bash
# SHA-1 produces 40-character hex hashes
cat .gitter/index | cut -f2
# Should show 40-character hashes (SHA-1)
```

## Future Enhancements

1. **Pack Files**
   - Implement Git's pack file format for efficient storage
   - Delta compression for similar objects

2. **SHA-256 Transition**
   - Add config option to choose hash algorithm
   - Support mixed SHA-1/SHA-256 repositories

3. **Tree Objects**
   - Implement tree object creation from index
   - Build directory hierarchy

4. **Commit Objects**
   - Link commits to tree objects
   - Store author, committer, timestamp, message

5. **Object Reading**
   - Implement `gitter cat-file` to inspect objects
   - Parse blob, tree, commit objects

## References

- [Git Internals - Git Objects](https://git-scm.com/book/en/v2/Git-Internals-Git-Objects)
- [Git Index Format](https://git-scm.com/docs/index-format)
- [SHA-1 vs SHA-256 in Git](https://git-scm.com/docs/hash-function-transition)
- [zlib Documentation](https://zlib.net/manual.html)

