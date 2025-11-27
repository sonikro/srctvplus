# SrcTV+ 64-bit Support Implementation

This document describes the changes made to add 64-bit Linux server support to the SrcTV+ plugin.

## Overview

SrcTV+ was originally built exclusively for 32-bit servers. With Team Fortress 2 now supporting 64-bit Linux servers, this project has been updated to build for both 32-bit and 64-bit architectures from a single source.

## Table of Contents

- [Architecture Overview](#architecture-overview)
- [Changes Made](#changes-made)
- [Build System](#build-system)
- [FunctionRoute 64-bit Implementation](#functionroute-64-bit-implementation)
- [Usage](#usage)
- [Technical Details](#technical-details)

## Architecture Overview

The project now produces three binaries:

| File | Architecture | Purpose |
|------|--------------|---------|
| `srctvplus.so` | Symlink | Points to the appropriate architecture binary |
| `srctvplus_i486.so` | 32-bit (i486) | For legacy 32-bit TF2 servers |
| `srctvplus_x86_64.so` | 64-bit (x86-64) | For modern 64-bit TF2 servers |

## Changes Made

### 1. Source SDK Update

**File**: `vendor/sdk` (git submodule)

**Change**: Updated from the old hl2sdk commit to the latest AlliedModders hl2sdk tf2 branch.

**Reason**: The latest SDK includes pre-built 64-bit libraries.

**Libraries Added**:
```
vendor/sdk/lib/public/linux64/
├── tier1.a
├── tier2.a
├── mathlib.a
├── libtier0_srv.so
└── libvstdlib_srv.so
```

**Previous Libraries** (still used for 32-bit):
```
vendor/sdk/lib/public/linux/
├── tier1_i486.a
├── mathlib_i486.a
├── libtier0_srv.so
└── libvstdlib_srv.so
```

### 2. FunctionRoute 64-bit Implementation

**Files Added**:
- `vendor/tftrue/FunctionRoute/FunctionRoute_64bit.cpp` - 64-bit implementation
- `vendor/tftrue/FunctionRoute/build_64bit.sh` - Build script for 64-bit version

**File Modified**:
- `vendor/tftrue/FunctionRoute/FunctionRoute.h` - Added `#include <cstddef>` for NULL compatibility

**Output**: `vendor/tftrue/FunctionRoute/FunctionRoute_x86_64.a` (64-bit static library)

#### Why This Was Needed

The original FunctionRoute library (`FunctionRoute.a`) was only available as a 32-bit ELF object file. When linking 64-bit code, the linker would fail with architecture mismatch errors. A 64-bit compatible version was created.

### 3. Build System Enhancement

**File**: `build.sh` (completely rewritten)

**Changes**:
- Modularized into `build_arch()` function that handles both architectures
- Automatic detection of SDK libraries based on architecture
- Appropriate compiler flags for each architecture:
  - 32-bit: `-march=pentium3 -mmmx -msse -m32`
  - 64-bit: `-march=x86-64 -m64`
- Automatic symlink creation
- Better error handling and user feedback

**Build Flow**:
```
./build.sh
├─ build_arch i486 "32-bit" "-m32"
│  ├─ Compile: g++ ... -c -o srctvplus_i486.o
│  └─ Link: gcc ... -o srctvplus_i486.so
│
├─ build_arch x86_64 "64-bit" "-m64"
│  ├─ Compile: g++ ... -c -o srctvplus_x86_64.o
│  └─ Link: gcc ... -o srctvplus_x86_64.so
│
└─ Create Symlink: ln -sf srctvplus_x86_64.so srctvplus.so
```

### 4. Documentation Update

**File**: `README.md`

**Changes**:
- Added "Multi-architecture support" section
- Updated compilation instructions
- Added architecture-specific installation instructions
- Explained how to switch between architectures

### 5. Git Configuration

**File**: `.gitignore`

**Changes**:
- Added `srctvplus_*.o` - Architecture-specific object files
- Added `srctvplus_*.so` - Architecture-specific shared libraries

**Reason**: Build artifacts should not be committed to version control.

## Build System

### Prerequisites

The build system requires:
- `g++` compiler with 64-bit support
- `gcc` linker
- Standard build tools (`make`, `ar`)
- Git with submodule support
- `readelf` (for verification)

### Single Build Process

Running `./build.sh` now:

1. **Initializes submodules** (if needed)
2. **Builds 32-bit version**:
   - Compiles with `-m32` flag
   - Links against `lib/public/linux/` libraries
   - Produces `srctvplus_i486.so`
3. **Builds 64-bit version**:
   - Compiles with `-m64` flag
   - Links against `lib/public/linux64/` libraries
   - Produces `srctvplus_x86_64.so`
4. **Creates symlink** to 64-bit version by default

### Build Output

```
✓ srctvplus.so           → symlink to srctvplus_x86_64.so (64-bit default)
✓ srctvplus_i486.so      → 803 KB (32-bit ELF32)
✓ srctvplus_x86_64.so    → 785 KB (64-bit ELF64)
```

## FunctionRoute 64-bit Implementation

### Problem

The original FunctionRoute library from TFTrue was only available as a 32-bit compiled object file. The source code was never released, making it impossible to recompile for 64-bit.

### Solution

A 64-bit compatible implementation was created based on the header file API and understanding of how function hooking works on Linux x86-64.

### How It Works

The implementation uses x86-64 absolute jump sequences for function hooking:

```
Original Function:
48 83 EC 08          # sub rsp, 0x8
... (rest of function)

After Hooking (14 bytes):
48 B8 XX XX XX XX XX XX XX XX  # movabs rax, <64-bit address of hook>
FF E0                            # jmp rax
90 90                            # nop padding
```

### Key Features

1. **Memory Protection**: Uses `mprotect()` to make code sections writable
2. **Code Backup**: Saves original bytes before hooking
3. **Restoration**: Can restore original function on cleanup
4. **Compatibility**: Works with Position Independent Code (PIC)

### Implementation Details

**File**: `vendor/tftrue/FunctionRoute/FunctionRoute_64bit.cpp`

Key functions implemented:

- `CFunctionRoute::RouteFunction()` - Hook a direct function call
- `CFunctionRoute::RestoreFunction()` - Remove hook and restore original
- `CFunctionRoute::InternalCallOriginalFunction()` - Call original without triggering hook
- `CFunctionRoute::SetMemoryProtection()` - Change page permissions

**Limitations**:

- Virtual function routing is simplified (returns false)
- Only basic direct function hooking is implemented
- Requires function to be at least 14 bytes to hook

### Build Process for FunctionRoute

```bash
$ cd vendor/tftrue/FunctionRoute
$ ./build_64bit.sh
$ # Produces: FunctionRoute_x86_64.a
```

The build script:
1. Compiles `FunctionRoute_64bit.cpp` with 64-bit flags
2. Creates static library using `ar`
3. Outputs `FunctionRoute_x86_64.a`

## Usage

### Building

```bash
$ git submodule init
$ git submodule update
$ ./build.sh
```

### Deploying to 64-bit Server

```bash
$ cp srctvplus.so <TF2_SERVER>/tf/addons/
$ cp srctvplus.vdf <TF2_SERVER>/tf/addons/
```

### Deploying to 32-bit Server (if needed)

```bash
$ ln -sf srctvplus_i486.so srctvplus.so
$ cp srctvplus.so <TF2_SERVER>/tf/addons/
$ cp srctvplus.vdf <TF2_SERVER>/tf/addons/
```

### Switching Architectures After Build

```bash
# Switch to 32-bit
$ ln -sf srctvplus_i486.so srctvplus.so

# Switch back to 64-bit
$ ln -sf srctvplus_x86_64.so srctvplus.so
```

## Technical Details

### Compiler Flags

#### 32-bit Build
```
-march=pentium3         # Target Pentium 3 (compatible with older CPUs)
-mmmx                   # Enable MMX
-msse                   # Enable SSE
-m32                    # Force 32-bit compilation
-fPIC                   # Position Independent Code
-fvisibility=hidden     # Hide symbols by default
```

#### 64-bit Build
```
-march=x86-64           # Target x86-64 baseline
-m64                    # Force 64-bit compilation
-fPIC                   # Position Independent Code
-fvisibility=hidden     # Hide symbols by default
```

### Library Linking

#### 32-bit
```
-L vendor/sdk/lib/public/linux
-ltier0_srv -lvstdlib_srv (dynamic)
vendor/sdk/lib/public/linux/tier1_i486.a (static)
vendor/sdk/lib/public/linux/mathlib_i486.a (static)
vendor/tftrue/FunctionRoute/FunctionRoute.a (static)
```

#### 64-bit
```
-L vendor/sdk/lib/public/linux64
-ltier0_srv -lvstdlib_srv (dynamic)
vendor/sdk/lib/public/linux64/tier1.a (static)
vendor/sdk/lib/public/linux64/tier2.a (static)
vendor/sdk/lib/public/linux64/mathlib.a (static)
vendor/tftrue/FunctionRoute/FunctionRoute_x86_64.a (static)
```

### Binary Sizes

| Binary | Size | Architecture |
|--------|------|--------------|
| srctvplus_i486.so | 803 KB | ELF32 Intel 80386 |
| srctvplus_x86_64.so | 785 KB | ELF64 x86-64 |

The 64-bit version is actually slightly smaller due to more efficient instruction encoding in 64-bit mode.

## Verification

### Check Binary Architecture

```bash
$ readelf -h srctvplus_x86_64.so | grep -E "Class|Machine|Type"
  Class:                             ELF64
  Type:                              DYN (Shared object file)
  Machine:                           Advanced Micro Devices X86-64
```

### Check Symlink

```bash
$ ls -l srctvplus.so
lrwxrwxrwx  1 root root  19 srctvplus.so -> srctvplus_x86_64.so
```

## Future Improvements

1. **Virtual Function Routing**: Full implementation for 64-bit
2. **ModuleScanner Support**: 64-bit version of ModuleScanner library
3. **Build System**: Add CMake or Meson support
4. **Testing**: Automated testing on actual 32-bit and 64-bit TF2 servers
5. **Documentation**: Add architecture-specific debugging guide

## References

- [AlliedModders hl2sdk](https://github.com/alliedmodders/hl2sdk) - Latest SDK with 64-bit support
- [TFTrue Project](https://github.com/AnAkIn1/TFTrue) - Original FunctionRoute from TFTrue
- [x86-64 ABI](https://en.wikipedia.org/wiki/X86_calling_conventions#System_V_AMD64_ABI) - 64-bit calling conventions
- [ELF Format](https://en.wikipedia.org/wiki/Executable_and_Linkable_Format) - Binary format details

## Troubleshooting

### Build Fails with "Architecture mismatch"

This indicates you're trying to link 32-bit and 64-bit objects. Ensure:
- `FunctionRoute_x86_64.a` exists for 64-bit builds
- Correct SDK libraries are used (check library paths in build.sh)

### Server Won't Load Plugin

Check the server logs:
```bash
$ ./srcds_run -console +map ctf_2fort
# Plugin errors will appear in console
```

Ensure:
- Correct binary architecture matches server (use `readelf -h`)
- `srctvplus.vdf` is present and correctly formatted
- Plugin is in correct directory: `tf/addons/` or `tf/custom/srctvplus/addons/`

### Symbol Not Found Errors

This may indicate:
- Incompatible SDK version
- Missing shared libraries (libtier0_srv.so, libvstdlib_srv.so)
- Incorrect compiler flags during build

## Contact & Contributions

For issues, improvements, or questions about the 64-bit implementation, please refer to the main project repository.
