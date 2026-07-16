#include "component_identity.h"

#include <cstdlib>
#include <iostream>

namespace {

bool check(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

} // namespace

int main()
{
    bool ok = true;
    ok &= check(foocrate::kComponentName == "FooCrate", "Unexpected component name");
    ok &= check(foocrate::kComponentVersion == "1.0.2", "Unexpected component version");
    ok &= check(foocrate::kComponentFileName == "foo_crate.dll", "Unexpected component filename");
    ok &= check(foocrate::kComponentGuid.Data1 == 0x5aeac28d, "Unexpected component GUID Data1");
    ok &= check(foocrate::kComponentGuid.Data2 == 0xed01, "Unexpected component GUID Data2");
    ok &= check(foocrate::kComponentGuid.Data3 == 0x4574, "Unexpected component GUID Data3");
    ok &= check(foocrate::kComponentGuid.Data4[0] == 0xa9, "Unexpected component GUID Data4");
    ok &= check(foocrate::kComponentGuid.Data4[7] == 0x15, "Unexpected component GUID tail");
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
