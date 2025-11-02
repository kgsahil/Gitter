# Docker Support for Gitter

Run Gitter in a containerized environment without installing dependencies locally!

---

## 🚀 Quick Start

### Option 1: Using Docker Compose (Recommended)

```bash
# Build the image first
docker-compose build

# Run interactively with welcome message!
docker-compose run --rm gitter

# You're now in a full Linux Ubuntu bash shell with welcome message displayed
# Demo project is in /demo directory
gitter help
cd /demo
gitter init demo-repo
cd demo-repo
echo "Hello World" > file.txt
gitter add file.txt
gitter commit -m "Initial commit"
gitter log

# You have full Linux environment - try:
ls -la
whoami
uname -a
which gitter

# Exit (container is automatically removed with --rm)
exit
```

### Option 2: Using Docker Directly

```bash
# Build the image
docker build -t gitter-cli:latest .

# Run interactively
docker run -it --name gitter-demo -v $(pwd)/docker-demo:/demo -w /demo gitter-cli:latest

# Inside container
gitter help

# Exit and cleanup
exit
docker rm gitter-demo
```

---

## 📦 Image Details

**Base Image:** Ubuntu 22.04  
**Includes:**
- GCC 11+ (C++20 compiler)
- CMake 3.20+
- Ninja build system
- zlib development libraries
- Git

**Build Configuration:**
- Release build (optimized)
- All tests enabled
- ~50MB final image size

---

## 🎯 Use Cases

### 1. Quick Demo
Showcase Gitter without installing anything:

```bash
docker-compose up -d
docker-compose exec gitter bash -c "cd /demo && gitter init test && cd test && echo '# Gitter Demo' > README.md && gitter add . && gitter commit -m 'Demo commit' && gitter log"
```

### 2. CI/CD Testing
Run tests in a clean environment:

```bash
docker build -t gitter-cli:latest .
docker run --rm gitter-cli:latest bash -c "cmake --preset linux-debug && cmake --build --preset linux-debug-build && cd build/linux-debug && ./gitter_tests"
```

### 3. Development Environment
Mount your source for active development:

```bash
docker run -it -v $(pwd):/workspace -w /workspace gitter-cli:latest bash
```

### 4. Cross-Platform Testing
Test on Ubuntu/Linux from any OS:

```bash
docker build -t gitter-cli:latest .
docker run -it gitter-cli:latest bash
```

---

## 🔧 Customization

### Build with Different Configuration

**Debug build:**
```bash
docker build --build-arg BUILD_TYPE=Debug -t gitter-cli:debug .
```

**With tests:**
```bash
docker build --build-arg BUILD_TESTS=ON -t gitter-cli:with-tests .
```

### Mount Your Repository

```bash
docker run -it \
  -v /path/to/your/repo:/workspace \
  -w /workspace \
  gitter-cli:latest \
  bash
```

---

## 📊 Image Information

```bash
# Check image size
docker images gitter-cli:latest

# Inspect contents
docker run --rm gitter-cli:latest gitter help

# Build with verbose output
docker build --progress=plain -t gitter-cli:latest .
```

---

## 🐛 Troubleshooting

### Build fails
```bash
# Clean build
docker build --no-cache -t gitter-cli:latest .
```

### Permission issues
```bash
# On Linux, ensure Docker has permissions
sudo docker-compose up
```

### Cache issues
```bash
# Clear Docker build cache
docker builder prune -af
```

---

## 📝 Dockerfile Customization

Edit `Dockerfile` to:
- Change base image (e.g., `alpine` for smaller size)
- Adjust compiler flags
- Add additional tools
- Modify build configuration

Example Alpine-based (smaller but longer build):

```dockerfile
FROM alpine:latest
RUN apk add --no-cache cmake ninja build-base git zlib-dev
WORKDIR /app
COPY . .
RUN cmake --preset linux-release && cmake --build --preset linux-release-build
CMD ["/bin/sh"]
```

---

## ✅ Verification

```bash
# Build image
docker-compose build

# Verify it works
docker run --rm gitter-cli:latest gitter --help

# Run full demo
docker-compose up -d
docker-compose exec gitter bash -c "cd /demo && gitter help"
docker-compose down
```

---

**See also:** [README.md](../README.md) for full Gitter documentation

