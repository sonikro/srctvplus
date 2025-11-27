#!/bin/bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

SDKPATH="${SDKPATH:-${DIR}/vendor/sdk}"
TFTRUEPATH="${TFTRUEPATH:-${DIR}/vendor/tftrue}"

# Function to build for a specific architecture
build_arch() {
    local ARCH=$1
    local ARCH_NAME=$2
    local M_FLAG=$3
    
    echo "Building for $ARCH_NAME ($ARCH)..."
    
    # Use appropriate march flags for the architecture
    local MARCH_FLAGS
    if [ "$ARCH" = "x86_64" ]; then
        # For 64-bit, use a more appropriate march flag
        MARCH_FLAGS="-march=x86-64"
    else
        # For 32-bit, keep the original flags
        MARCH_FLAGS="-march=pentium3 -mmmx -msse"
    fi
    
    local CXXFLAGS_ARCH="$CXXFLAGS $MARCH_FLAGS $M_FLAG -Wall -fvisibility=hidden -fPIC"
    CXXFLAGS_ARCH="$CXXFLAGS_ARCH -fvisibility-inlines-hidden -fno-strict-aliasing"
    CXXFLAGS_ARCH="$CXXFLAGS_ARCH -Wno-delete-non-virtual-dtor -Wno-unused -Wno-reorder"
    CXXFLAGS_ARCH="$CXXFLAGS_ARCH -Wno-overloaded-virtual -Wno-unknown-pragmas -Wno-invalid-offsetof"
    CXXFLAGS_ARCH="$CXXFLAGS_ARCH -Wno-sign-compare -std=c++11 -Dtypeof=decltype"
    CXXFLAGS_ARCH="$CXXFLAGS_ARCH -O3 -pthread -lpthread"
    
    CXXFLAGS_ARCH="$CXXFLAGS_ARCH -DDEBUG"
    CXXFLAGS_ARCH="$CXXFLAGS_ARCH -DGNUC -DRAD_TELEMETRY_DISABLED -DLINUX -D_LINUX -DPOSIX"
    CXXFLAGS_ARCH="$CXXFLAGS_ARCH -DNO_MALLOC_OVERRIDE -D_FORTIFY_SOURCE=0"
    CXXFLAGS_ARCH="$CXXFLAGS_ARCH -DVPROF_LEVEL=1 -DSWDS -D_finite=finite -Dstricmp=strcasecmp"
    CXXFLAGS_ARCH="$CXXFLAGS_ARCH -D_stricmp=strcasecmp -D_strnicmp=strncasecmp -Dstrnicmp=strncasecmp"
    CXXFLAGS_ARCH="$CXXFLAGS_ARCH -D_vsnprintf=vsnprintf -D_alloca=alloca -Dstrcmpi=strcasecmp"
    
    CXXFLAGS_ARCH="$CXXFLAGS_ARCH -I${SDKPATH}/common"
    CXXFLAGS_ARCH="$CXXFLAGS_ARCH -I${SDKPATH}/public"
    CXXFLAGS_ARCH="$CXXFLAGS_ARCH -I${SDKPATH}/public/tier0"
    CXXFLAGS_ARCH="$CXXFLAGS_ARCH -I${SDKPATH}/public/tier1"
    CXXFLAGS_ARCH="$CXXFLAGS_ARCH -I${SDKPATH}/game/shared"
    CXXFLAGS_ARCH="$CXXFLAGS_ARCH -I${SDKPATH}/game/server"
    CXXFLAGS_ARCH="$CXXFLAGS_ARCH -I${TFTRUEPATH}/FunctionRoute/"
    
    local LDFLAGS_ARCH="$LDFLAGS -lrt -lm -ldl $M_FLAG -flto -shared -static-libgcc -static-libstdc++"
    LDFLAGS_ARCH="$LDFLAGS_ARCH -Wl,--version-script=version-script"
    
    # Determine FunctionRoute library path
    local FUNCTIONROUTE_LIB
    local FUNCTIONROUTE_FOUND=0
    
    # Look for architecture-specific FunctionRoute first
    if [ -f "${TFTRUEPATH}/FunctionRoute/FunctionRoute_${ARCH}.a" ]; then
        FUNCTIONROUTE_LIB="${TFTRUEPATH}/FunctionRoute/FunctionRoute_${ARCH}.a"
        FUNCTIONROUTE_FOUND=1
    # Fallback to generic FunctionRoute (only works for 32-bit)
    elif [ -f "${TFTRUEPATH}/FunctionRoute/FunctionRoute.a" ] && [ "$ARCH" = "i486" ]; then
        FUNCTIONROUTE_LIB="${TFTRUEPATH}/FunctionRoute/FunctionRoute.a"
        FUNCTIONROUTE_FOUND=1
    fi
    
    # Determine library paths based on architecture
    local LIB_PATH
    local TIER1_LIB
    local TIER2_LIB
    local MATHLIB_LIB
    
    if [ "$ARCH" = "x86_64" ]; then
        LIB_PATH="${SDKPATH}/lib/public/linux64"
        TIER1_LIB="${LIB_PATH}/tier1.a"
        TIER2_LIB="${LIB_PATH}/tier2.a"
        MATHLIB_LIB="${LIB_PATH}/mathlib.a"
    else
        LIB_PATH="${SDKPATH}/lib/public/linux"
        TIER1_LIB="${LIB_PATH}/tier1_i486.a"
        TIER2_LIB="${LIB_PATH}/tier2.a"  # Fallback: check if tier2 exists
        MATHLIB_LIB="${LIB_PATH}/mathlib_i486.a"
    fi
    
    # Check if required libraries exist
    if [ ! -f "${TIER1_LIB}" ]; then
        echo "Warning: tier1 library not found at ${TIER1_LIB}. Skipping $ARCH_NAME build."
        return 1
    fi
    
    # For 32-bit, tier2 might not exist in this SDK version
    if [ ! -f "${TIER2_LIB}" ]; then
        TIER2_LIB=""
    fi
    
    local OBJS_ARCH="-L${LIB_PATH} -ltier0_srv -lvstdlib_srv"
    OBJS_ARCH="$OBJS_ARCH ${TIER1_LIB}"
    if [ -n "${TIER2_LIB}" ]; then
        OBJS_ARCH="$OBJS_ARCH ${TIER2_LIB}"
    fi
    OBJS_ARCH="$OBJS_ARCH ${MATHLIB_LIB}"
    if [ $FUNCTIONROUTE_FOUND -eq 1 ]; then
        OBJS_ARCH="$OBJS_ARCH ${FUNCTIONROUTE_LIB}"
    else
        if [ "$ARCH" = "x86_64" ]; then
            echo "Warning: FunctionRoute not found for 64-bit. Function hooking will not be available."
            echo "To enable 64-bit function hooking, a 64-bit version of FunctionRoute is needed:"
            echo "  ${TFTRUEPATH}/FunctionRoute/FunctionRoute_x86_64.a"
        fi
    fi
    
    set -x
    g++ $CXXFLAGS_ARCH -c -o srctvplus_${ARCH}.o srctvplus.cpp
    gcc $CXXFLAGS_ARCH $LDFLAGS_ARCH -o srctvplus_${ARCH}.so srctvplus_${ARCH}.o $OBJS_ARCH
    set +x
    
    echo "✓ Built srctvplus_${ARCH}.so"
}

# Build 32-bit version
build_arch "i486" "32-bit" "-m32" || true

# Build 64-bit version (if libraries available)
build_arch "x86_64" "64-bit" "-m64" || true

echo ""
echo "Build complete! Output files:"
ls -lh srctvplus_*.so 2>/dev/null || echo "No binaries built"

# Create symlinks for default plugin names
if [ -f "srctvplus_i486.so" ] && [ -f "srctvplus_x86_64.so" ]; then
    echo ""
    echo "Both architectures built. To use a specific version:"
    echo "  ln -sf srctvplus_i486.so srctvplus.so     # For 32-bit servers"
    echo "  ln -sf srctvplus_x86_64.so srctvplus.so   # For 64-bit servers"
    echo ""
    echo "Creating symlink to 64-bit version by default:"
    rm -f srctvplus.so
    ln -sf srctvplus_x86_64.so srctvplus.so
    echo "✓ srctvplus.so -> srctvplus_x86_64.so"
elif [ -f "srctvplus_i486.so" ]; then
    rm -f srctvplus.so
    ln -sf srctvplus_i486.so srctvplus.so
    echo "✓ Created srctvplus.so -> srctvplus_i486.so (32-bit)"
elif [ -f "srctvplus_x86_64.so" ]; then
    rm -f srctvplus.so
    ln -sf srctvplus_x86_64.so srctvplus.so
    echo "✓ Created srctvplus.so -> srctvplus_x86_64.so (64-bit)"
fi
