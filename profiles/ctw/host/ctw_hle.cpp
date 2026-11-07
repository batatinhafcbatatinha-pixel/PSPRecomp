#include "ctw_hle.hpp"

#include <array>
#include <cstdint>
#include <utility>

namespace ctw {
namespace {

// Network and DRM libraries only back the multiplayer modes, which have no
// host implementation. Every call fails the way an absent WLAN does, so the
// game reports "no connection" instead of stalling.
constexpr std::uint32_t kNetUnavailable = 0x80410D00u;

constexpr std::array<std::pair<const char *, std::uint32_t>, 59> kUnavailableNetImports{{
    {"sceNet", 0x0BF0A3AEu}, {"sceNet", 0x281928A9u}, {"sceNet", 0x39AF39A6u}, {"sceNet", 0x89360950u},
    {"sceNetInet", 0x05038FC7u}, {"sceNetInet", 0x162E6FD5u}, {"sceNetInet", 0x17943399u},
    {"sceNetInet", 0x1A33F9AEu}, {"sceNetInet", 0x2FE71FE7u}, {"sceNetInet", 0x410B34AAu},
    {"sceNetInet", 0x4A114C7Cu}, {"sceNetInet", 0x4CFE4E56u}, {"sceNetInet", 0x5BE8D595u},
    {"sceNetInet", 0x7AA671BCu}, {"sceNetInet", 0x8B7B220Fu}, {"sceNetInet", 0x8D7284EAu},
    {"sceNetInet", 0xA9ED66B9u}, {"sceNetInet", 0xB75D5B0Au}, {"sceNetInet", 0xC91142E4u},
    {"sceNetInet", 0xCDA85C99u}, {"sceNetInet", 0xD0792666u}, {"sceNetInet", 0xFBABE411u},
    {"sceNetResolver", 0x224C5F44u}, {"sceNetResolver", 0x244172AFu}, {"sceNetResolver", 0x6138194Au},
    {"sceNetResolver", 0x94523E09u}, {"sceNetResolver", 0xF3370E61u},
    {"sceNetApctl", 0x2BEFDF23u}, {"sceNetApctl", 0x5963991Bu}, {"sceNetApctl", 0x8ABADD51u},
    {"sceNetApctl", 0xB3EDD0ECu}, {"sceNetApctl", 0xE2F91F9Bu},
    {"sceNetAdhoc", 0x6F92741Bu}, {"sceNetAdhoc", 0x7F27BB5Eu}, {"sceNetAdhoc", 0xA62C6F57u},
    {"sceNetAdhoc", 0xABED3790u}, {"sceNetAdhoc", 0xDFE53E03u}, {"sceNetAdhoc", 0xE1D621D7u},
    {"sceNetAdhocctl", 0x08FFF7A0u}, {"sceNetAdhocctl", 0x0AD043EDu}, {"sceNetAdhocctl", 0x20B317A0u},
    {"sceNetAdhocctl", 0x34401D65u}, {"sceNetAdhocctl", 0x5E7F79C9u}, {"sceNetAdhocctl", 0x81AEE1BEu},
    {"sceNetAdhocctl", 0x8916C003u}, {"sceNetAdhocctl", 0x9D689E13u}, {"sceNetAdhocctl", 0xE26F226Eu},
    {"sceNetAdhocctl", 0xEC0635C1u},
    {"sceNetAdhocMatching", 0x2A2A1E07u}, {"sceNetAdhocMatching", 0x32B156B3u},
    {"sceNetAdhocMatching", 0x5E3D4B79u}, {"sceNetAdhocMatching", 0x7945ECDAu},
    {"sceNetAdhocMatching", 0x93EF3843u}, {"sceNetAdhocMatching", 0xB58E61B7u},
    {"sceNetAdhocMatching", 0xCA5EDA6Fu}, {"sceNetAdhocMatching", 0xEA3C6108u},
    {"sceNetAdhocMatching", 0xF16EAF4Fu},
    {"scePspNpDrm_user", 0x08D98894u}, {"scePspNpDrm_user", 0xA1336091u},
}};


} // namespace

void install_hle(psprecomp::Runtime &runtime) {
    for (const auto &[library, nid] : kUnavailableNetImports) {
        runtime.register_hle(library, nid, [](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
            ctx.set_gpr(2, kNetUnavailable);
        });
    }
}

} // namespace ctw
