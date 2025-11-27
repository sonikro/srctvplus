# Quick Start Guide - 64-bit Build

## Building

```bash
# Clone and setup
git clone https://github.com/sonikro/srctvplus.git
cd srctvplus
git submodule init
git submodule update

# Build both architectures
./build.sh
```

## Installation

### For 64-bit Servers (default)
```bash
# srctvplus.so is already a symlink to the 64-bit version
cp srctvplus.so <TF2>/tf/addons/
cp srctvplus.vdf <TF2>/tf/addons/
```

### For 32-bit Servers
```bash
# Switch to 32-bit version
ln -sf srctvplus_i486.so srctvplus.so

# Then copy
cp srctvplus.so <TF2>/tf/addons/
cp srctvplus.vdf <TF2>/tf/addons/
```

## Files Generated

After running `./build.sh`:

| File | Size | Architecture | Purpose |
|------|------|--------------|---------|
| `srctvplus.so` | 19 bytes | Symlink | Points to active version |
| `srctvplus_i486.so` | ~803 KB | 32-bit | For 32-bit servers |
| `srctvplus_x86_64.so` | ~785 KB | 64-bit | For 64-bit servers (default) |

## Switching Between Architectures

```bash
# Switch to 32-bit
ln -sf srctvplus_i486.so srctvplus.so

# Switch to 64-bit
ln -sf srctvplus_x86_64.so srctvplus.so

# Verify which is active
ls -l srctvplus.so
```

## Verify Binary Architecture

```bash
# Check 64-bit
readelf -h srctvplus_x86_64.so | grep Machine
# Output: Machine: Advanced Micro Devices X86-64

# Check 32-bit
readelf -h srctvplus_i486.so | grep Machine
# Output: Machine: Intel 80386
```

## What Was Changed

See `docs/64BIT_IMPLEMENTATION.md` for detailed technical information about:
- Source SDK updates
- FunctionRoute 64-bit implementation
- Build system changes
- Architecture-specific compiler flags
- Troubleshooting

## Configuration

After installation, configure TF2 with appropriate settings:

```
tv_maxrate 20000          # Increase from default 8000
tv_snapshotrate 16        # Adjust as needed
```

## Support

For issues or questions, refer to:
- `README.md` - Installation and usage
- `docs/64BIT_IMPLEMENTATION.md` - Technical details
- Main repository issues page
