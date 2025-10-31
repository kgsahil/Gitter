# Log Implementation Guide

## Overview

The `gitter log` command displays commit history in reverse chronological order (newest to oldest), showing up to 10 commits by default.

## Architecture

### Components

```
gitter log
    ↓
LogCommand
    ↓
ObjectStore::readCommit()
    ↓
CommitObject (parsed)
    ↓
Display formatted output
```

### Key Classes

1. **CommitObject** - Struct holding parsed commit data
2. **ObjectStore::readCommit()** - Reads and parses commit objects
3. **LogCommand** - Traverses commit chain and displays history

## Commit Object Structure

### Git Format

```
commit <size>\0tree <tree-hash>
parent <parent-hash>
author Name <email> <timestamp> <timezone>
committer Name <email> <timestamp> <timezone>

<commit message>
```

### Example

```
commit 180\0tree abc123...
parent def456...
author John Doe <john@example.com> 1698765432 +0000
committer John Doe <john@example.com> 1698765432 +0000

Fix critical bug in authentication

This commit addresses the security vulnerability
discovered in the login flow.
```

## CommitObject Structure

```cpp
struct CommitObject {
    std::string hash;                       // Full SHA-1 hash
    std::string treeHash;                   // Tree object hash
    std::vector<std::string> parentHashes;  // Parent commits (0, 1, or more)
    std::string authorName;                 // "John Doe"
    std::string authorEmail;                // "john@example.com"
    int64_t authorTimestamp;                // Unix timestamp
    std::string authorTimezone;             // "+0000", "-0800", etc.
    std::string committerName;              // Usually same as author
    std::string committerEmail;             // Usually same as author
    int64_t committerTimestamp;             // Unix timestamp
    std::string committerTimezone;          // Timezone
    std::string message;                    // Full commit message
    
    std::string shortMessage() const;       // First line only
    std::string shortHash() const;          // First 7 chars
};
```

## Parsing Algorithm

### Step 1: Read Object

```cpp
std::string fullObject = readObject(hash);
// Returns: "commit 180\0tree abc...\nparent def...\n..."
```

### Step 2: Split Header and Content

```cpp
size_t headerEnd = fullObject.find('\0');
std::string header = fullObject.substr(0, headerEnd);   // "commit 180"
std::string content = fullObject.substr(headerEnd + 1); // "tree abc...\n..."
```

### Step 3: Verify Object Type

```cpp
if (header.rfind("commit ", 0) != 0) {
    throw std::runtime_error("Not a commit object");
}
```

### Step 4: Parse Content Line by Line

```cpp
std::istringstream iss(content);
std::string line;

while (std::getline(iss, line)) {
    if (line.empty()) {
        // Blank line = start of commit message
        inMessage = true;
    } else if (line.rfind("tree ", 0) == 0) {
        commit.treeHash = line.substr(5);
    } else if (line.rfind("parent ", 0) == 0) {
        commit.parentHashes.push_back(line.substr(7));
    } else if (line.rfind("author ", 0) == 0) {
        // Parse: "author Name <email> timestamp timezone"
        parseAuthorLine(line);
    } else if (line.rfind("committer ", 0) == 0) {
        // Parse: "committer Name <email> timestamp timezone"
        parseCommitterLine(line);
    } else if (inMessage) {
        // Accumulate message lines
        message += line + "\n";
    }
}
```

### Step 5: Parse Author/Committer Lines

```cpp
// Input: "author John Doe <john@example.com> 1698765432 +0000"
std::string authorLine = line.substr(7);  // Remove "author "

size_t emailStart = authorLine.find('<');
size_t emailEnd = authorLine.find('>');

commit.authorName = authorLine.substr(0, emailStart - 1);  // "John Doe"
commit.authorEmail = authorLine.substr(emailStart + 1, 
                                       emailEnd - emailStart - 1);  // "john@example.com"

std::string rest = authorLine.substr(emailEnd + 2);  // "1698765432 +0000"
std::istringstream restStream(rest);
restStream >> commit.authorTimestamp >> commit.authorTimezone;
```

## Log Display Algorithm

### Step 1: Resolve HEAD to Commit Hash

```cpp
// Read .gitter/HEAD
std::string headContent = readFile(".gitter/HEAD");

if (headContent.rfind("ref: ", 0) == 0) {
    // HEAD points to branch: "ref: refs/heads/main"
    std::string refPath = headContent.substr(5);
    std::string commitHash = readFile(".gitter/" + refPath);
} else {
    // Detached HEAD: direct commit hash
    std::string commitHash = headContent;
}
```

### Step 2: Traverse Commit Chain

```cpp
std::string currentHash = resolveHEAD();
int maxCommits = 10;
int count = 0;

while (!currentHash.empty() && count < maxCommits) {
    CommitObject commit = store.readCommit(currentHash);
    
    // Display commit
    displayCommit(commit);
    
    // Move to parent
    if (!commit.parentHashes.empty()) {
        currentHash = commit.parentHashes[0];  // Follow first parent
    } else {
        break;  // Root commit
    }
    
    ++count;
}
```

### Step 3: Format and Display Each Commit

```cpp
// Yellow commit hash
std::cout << "\033[33mcommit " << commit.hash << "\033[0m\n";

// Author line
std::cout << "Author: " << commit.authorName 
          << " <" << commit.authorEmail << ">\n";

// Date line (formatted timestamp)
std::time_t timestamp = commit.authorTimestamp;
std::tm* timeinfo = std::localtime(&timestamp);
std::strftime(buffer, sizeof(buffer), "%a %b %d %H:%M:%S %Y", timeinfo);
std::cout << "Date:   " << buffer << " " << commit.authorTimezone << "\n";

// Blank line
std::cout << "\n";

// Message (indented with 4 spaces)
for (const std::string& line : messageLines) {
    std::cout << "    " << line << "\n";
}

// Blank line separator
std::cout << "\n";
```

## Example Output

```
commit 789abc123def456ghi789jkl012mno345pqr678st
Author: John Doe <john@example.com>
Date:   Fri Oct 31 15:30:45 2025 +0000

    Fix authentication bug
    
    This commit addresses the security vulnerability
    discovered in the login flow.

commit 456def789abc012ghi345jkl678mno901pqr234st
Author: Jane Smith <jane@example.com>
Date:   Thu Oct 30 10:20:15 2025 +0000

    Add user registration feature
    
    Implements the registration form with validation
    and email confirmation.

commit 123abc456def789ghi012jkl345mno678pqr901st
Author: John Doe <john@example.com>
Date:   Wed Oct 29 09:15:30 2025 +0000

    Initial commit
```

## Command Usage

### Basic Usage

```bash
gitter log
```

**Output:**
- Displays up to 10 most recent commits
- Shows full commit hash, author, date, and message
- Stops at root commit or after 10 commits

### Empty Repository

```bash
gitter log
# Output: No commits yet
```

### Single Commit

```bash
gitter log
# Output:
# commit abc123...
# Author: User <email>
# Date:   ...
#
#     Initial commit
```

### Multiple Commits

```bash
# Create commit history
gitter commit -m "First"
gitter commit -m "Second"
gitter commit -m "Third"

gitter log
# Output: Shows Third, Second, First (newest to oldest)
```

## Technical Details

### Commit Chain Traversal

```
HEAD → refs/heads/main → commit C
                            ↓
                         parent
                            ↓
                         commit B
                            ↓
                         parent
                            ↓
                         commit A (root, no parent)
```

**Traversal:**
1. Start at HEAD commit (C)
2. Display commit C
3. Follow parent pointer to B
4. Display commit B
5. Follow parent pointer to A
6. Display commit A
7. No more parents, stop

### Handling Merge Commits

Merge commits have **multiple parent hashes**:

```
commit <size>\0tree abc...
parent def456...  ← First parent (main branch)
parent ghi789...  ← Second parent (merged branch)
author ...
```

**Current Implementation:**
- Follows only first parent (`parentHashes[0]`)
- Ignores merge branches (simplified history)

**Future Enhancement:**
- Display merge commit with both parents
- Option to show full DAG
- Graphical branch visualization

### Timestamp Handling

**Git Format:** Unix timestamp (seconds since epoch)

```cpp
int64_t timestamp = 1698765432;
```

**Conversion to Human-Readable:**

```cpp
std::time_t t = static_cast<std::time_t>(timestamp);
std::tm* timeinfo = std::localtime(&t);  // Convert to local time
std::strftime(buffer, sizeof(buffer), "%a %b %d %H:%M:%S %Y", timeinfo);
// Result: "Fri Oct 31 15:30:45 2025"
```

**Timezone:**
- Stored as string: `"+0000"`, `"-0800"`, etc.
- Appended to formatted date

### Color Coding

Uses ANSI escape codes for terminal colors:

```cpp
"\033[33m"  // Yellow text
"\033[0m"   // Reset to default
```

**Commit hash in yellow:**
```cpp
std::cout << "\033[33mcommit " << hash << "\033[0m\n";
```

## Error Handling

### Missing Commit Object

```cpp
try {
    CommitObject commit = store.readCommit(hash);
} catch (const std::exception& e) {
    std::cerr << "Error reading commit " << hash << ": " << e.what() << "\n";
    break;  // Stop traversal
}
```

### Corrupted Commit

If commit object is malformed:
- Throws `std::runtime_error`
- Error message displayed
- Traversal stops

### No HEAD File

```bash
gitter log
# Output: No commits yet
```

### HEAD Points to Non-Existent Branch

```bash
gitter log
# Output: No commits yet
```

## Future Enhancements

### 1. Limit Flag

```bash
gitter log -n 20  # Show last 20 commits
gitter log --max-count=5  # Show last 5 commits
```

### 2. One-Line Format

```bash
gitter log --oneline
# Output:
# 789abc1 Fix authentication bug
# 456def7 Add user registration
# 123abc4 Initial commit
```

### 3. Pretty Formats

```bash
gitter log --pretty=short   # Shorter format
gitter log --pretty=full    # More details
gitter log --pretty=format:"%h %s"  # Custom format
```

### 4. Graph Visualization

```bash
gitter log --graph
# Output:
# * commit 789abc...
# |     Fix bug
# * commit 456def...
# |     Add feature
# * commit 123abc...
#       Initial commit
```

### 5. Filter by Author

```bash
gitter log --author="John Doe"
```

### 6. Filter by Date

```bash
gitter log --since="2025-10-01"
gitter log --until="2025-10-31"
```

### 7. Filter by File

```bash
gitter log -- src/main.cpp
# Show commits that modified src/main.cpp
```

### 8. Search in Message

```bash
gitter log --grep="bug fix"
# Show commits with "bug fix" in message
```

## Performance Considerations

### Object Reading

Each commit requires:
1. Read compressed file from disk
2. Decompress with zlib
3. Parse commit content

**Optimization:**
- Cache recently read commits
- Lazy load (only read what's displayed)

### Large Repositories

With thousands of commits:
- Only reads 10 commits (limit)
- No need to traverse entire history
- Fast even with deep history

### Compression Impact

zlib decompression is fast:
- ~100-200 MB/s on modern CPUs
- Commit objects are small (~1-5 KB compressed)
- Negligible overhead

## Summary

The log implementation:

✅ **Parses commit objects** - Extracts all metadata  
✅ **Traverses commit chain** - Follows parent pointers  
✅ **Displays formatted output** - Git-style format with colors  
✅ **Limits to 10 commits** - Prevents overwhelming output  
✅ **Handles root commits** - Stops when no parent  
✅ **Error handling** - Graceful failure on corrupt objects  
✅ **Timezone support** - Displays author timezone  
✅ **Multi-line messages** - Properly formats commit messages  

The implementation provides a functional Git-style log viewer! 🎉

