// Tests del VFS (src/platform/VFS.{h,cpp}). Cubren la resolucion basica y el
// rechazo de path traversal — invariante del que depende el AssetManager
// para garantizar sandbox.

#include <doctest/doctest.h>

#include "platform/VFS.h"

using namespace Mood;

TEST_CASE("VFS::isSafeLogicalPath acepta paths relativos simples") {
    CHECK(VFS::isSafeLogicalPath("textures/grid.png"));
    CHECK(VFS::isSafeLogicalPath("foo/bar/baz.ext"));
    CHECK(VFS::isSafeLogicalPath("singlefile.txt"));
}

TEST_CASE("VFS::isSafeLogicalPath rechaza componentes peligrosos") {
    CHECK_FALSE(VFS::isSafeLogicalPath(""));
    CHECK_FALSE(VFS::isSafeLogicalPath(".."));
    CHECK_FALSE(VFS::isSafeLogicalPath("../secret"));
    CHECK_FALSE(VFS::isSafeLogicalPath("a/../b"));
    CHECK_FALSE(VFS::isSafeLogicalPath("a/b/../c"));
    // Tambien rechazamos "." explicito.
    CHECK_FALSE(VFS::isSafeLogicalPath("./foo"));
    CHECK_FALSE(VFS::isSafeLogicalPath("."));
}

TEST_CASE("VFS::isSafeLogicalPath rechaza paths absolutos") {
    // Unix-style.
    CHECK_FALSE(VFS::isSafeLogicalPath("/etc/passwd"));
    // Windows con drive letter.
    CHECK_FALSE(VFS::isSafeLogicalPath("C:/Windows/system32"));
    CHECK_FALSE(VFS::isSafeLogicalPath("C:\\Users\\Public"));
}

TEST_CASE("VFS::resolve concatena con la raiz para paths seguros") {
    VFS vfs{"/tmp/assets"};
    const auto p = vfs.resolve("textures/grid.png");
    CHECK_FALSE(p.empty());
    CHECK(p.generic_string().find("/tmp/assets/") != std::string::npos);
    CHECK(p.generic_string().find("textures/grid.png") != std::string::npos);
}

TEST_CASE("VFS::resolve devuelve path vacio para paths inseguros") {
    VFS vfs{"assets"};
    CHECK(vfs.resolve("../outside").empty());
    CHECK(vfs.resolve("/etc/passwd").empty());
    CHECK(vfs.resolve("").empty());
    CHECK(vfs.resolve("a/../b").empty());
}
