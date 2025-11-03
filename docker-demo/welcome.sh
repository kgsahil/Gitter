#!/bin/bash
cat << 'EOF'
╔═══════════════════════════════════════════════════════════════╗
║                                                               ║
║     🚀 GITTER - Git-like Version Control System               ║
║                                                               ║
║     Your Gitter Docker environment is ready!                  ║
║                                                               ║
╚═══════════════════════════════════════════════════════════════╝

📋 Quick Start:
   gitter help                    Show all commands
   gitter init [name]             Initialize new repository
   gitter status                  Show working tree status
   gitter log                     Display commit history

📚 Full Documentation:
   See README.md in /app or visit:
   https://github.com/kgsahil/Gitter

💡 Demo Directory:
   cd /demo                        Try examples in this directory

┌───────────────────────────────────────────────────────────────┐
│ Try it now: gitter help                                       │
└───────────────────────────────────────────────────────────────┘

EOF

# Change to demo directory if it exists
cd /demo 2>/dev/null || true

# Start interactive bash
exec /bin/bash

