# Gitter CLI Tool 

Build your own Git-like command line tool that mimics core Git functionality.

## Objective

Create a CLI tool with behavior similar to Git, supporting commands like `init`, `add`, `commit`, `status`, etc. The goal is to understand the internal workings of Git and build a minimal yet extensible version of it.


## Supported Commands

| Command                  | Description                                                                 |
|--------------------------|-----------------------------------------------------------------------------|
| `gitter help`            | Lists all supported commands and their usage                                |
| `gitter init`            | Initializes a new Gitter repository (default branch: `main`)                |
| `gitter checkout branch` | Switches branches                                         |
| `gitter add <files>`     | Adds file(s) to the staging index                                           |
| `gitter status`          | Displays current working tree state (staged, unstaged, untracked)          |
| `gitter commit -m "msg"` | Commits staged files with a message                                         |
| `gitter commit -am "msg"` |                                         |
| `gitter log`             | Displays commit history in reverse order                                    |


## Important Notes
- Root file for the project is the gitter file. Do not rename or modify its filename.
- While uploading your project as a zip, do not alter the folder structure. Keep everything as it is.
