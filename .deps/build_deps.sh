#!/bin/bash
# Build script for Sentience MUD dependencies
# Run from the .deps directory: cd /sentience/src/.deps && ./build_deps.sh
#
# Dependencies managed here (git submodules):
#   - libbacktrace - stack trace file:line resolution
#   - libcotp      - TOTP/HOTP library
#
# System packages (not managed here, install separately):
#   - zlog, jansson, libquickmail

set -e  # Exit on error

DEPS_DIR="$(cd "$(dirname "$0")" && pwd)"
NPROC=$(nproc 2>/dev/null || echo 4)

echo "=== Building Sentience Dependencies ==="
echo "Dependencies directory: $DEPS_DIR"
echo "Parallel jobs: $NPROC"
echo ""

# Function to check if a library is already built
check_lib() {
    local libname="$1"
    local libdir="$2"
    if [ -f "$libdir/lib${libname}.so" ] || [ -f "$libdir/lib${libname}.a" ]; then
        return 0
    fi
    return 1
}

# Initialize git submodules if needed
init_submodules() {
    echo "=== Initializing git submodules ==="
    cd "$DEPS_DIR/.."
    git submodule update --init --recursive .deps/
    echo "Submodules initialized"
    echo ""
}

# Build libbacktrace (autotools)
build_libbacktrace() {
    echo "=== Building libbacktrace ==="
    cd "$DEPS_DIR/libbacktrace"

    if check_lib "backtrace" "$DEPS_DIR/libbacktrace/.libs"; then
        echo "libbacktrace already built, skipping..."
        return 0
    fi

    if [ ! -f "Makefile" ]; then
        ./configure
    fi

    make -j$NPROC

    echo "libbacktrace built successfully"
    echo ""
}

# Build libcotp (cmake)
build_libcotp() {
    echo "=== Building libcotp ==="
    cd "$DEPS_DIR/libcotp"

    if check_lib "cotp" "$DEPS_DIR/libcotp"; then
        echo "libcotp already built, skipping..."
        return 0
    fi

    if [ ! -f "Makefile" ]; then
        cmake -DBUILD_SHARED_LIBS=ON .
    fi

    make -j$NPROC

    echo "libcotp built successfully"
    echo ""
}

# Print usage
usage() {
    echo "Usage: $0 [all|libbacktrace|libcotp|clean|check]"
    echo ""
    echo "Commands:"
    echo "  all            Build all dependencies (default)"
    echo "  libbacktrace   Build only libbacktrace"
    echo "  libcotp        Build only libcotp"
    echo "  clean          Clean all builds"
    echo "  check          Check which libraries are built"
    echo ""
    echo "System packages (install separately): zlog, jansson, libquickmail"
}

# Clean all builds
clean_all() {
    echo "=== Cleaning all dependency builds ==="

    if [ -d "$DEPS_DIR/libbacktrace" ]; then
        cd "$DEPS_DIR/libbacktrace" && make clean 2>/dev/null || true
    fi
    if [ -d "$DEPS_DIR/libcotp" ]; then
        cd "$DEPS_DIR/libcotp" && make clean 2>/dev/null || true
    fi

    echo "Clean complete"
}

# Check build status
check_status() {
    echo "=== Dependency Build Status ==="

    echo -n "libbacktrace: "
    if check_lib "backtrace" "$DEPS_DIR/libbacktrace/.libs"; then
        echo "BUILT"
    else
        echo "NOT BUILT"
    fi

    echo -n "libcotp: "
    if check_lib "cotp" "$DEPS_DIR/libcotp"; then
        echo "BUILT"
    else
        echo "NOT BUILT"
    fi

    echo ""
    echo "System packages (not managed here): zlog, jansson, libquickmail"
}

# Main
case "${1:-all}" in
    all)
        init_submodules
        build_libbacktrace
        build_libcotp
        echo "=== All dependencies built successfully ==="
        ;;
    libbacktrace)
        build_libbacktrace
        ;;
    libcotp)
        build_libcotp
        ;;
    clean)
        clean_all
        ;;
    check)
        check_status
        ;;
    -h|--help|help)
        usage
        ;;
    *)
        echo "Unknown command: $1"
        usage
        exit 1
        ;;
esac
