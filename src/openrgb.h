// Minimal OpenRGB SDK client (protocol 0, TCP 127.0.0.1:6742): puts every device into direct mode and paints
// all its LEDs one color, ~40 times a second. The color comes from a callback, so the beat logic stays in main.cpp.
// OpenRGB must be running with "SDK Server" started.
#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <cstring>
#include <cstdint>

namespace orgb {

enum { ST_OFF = 0, ST_NOSERVER = 1, ST_CONNECTED = 2 };
static volatile LONG g_enabled = 0, g_state = ST_OFF, g_devices = 0;
static uint32_t (*g_color)() = nullptr;   // 0xRRGGBB to show right now
static volatile LONG* g_running = nullptr;

enum { PKT_COUNT = 0, PKT_DATA = 1, PKT_NAME = 50, PKT_UPDATELEDS = 1050, PKT_CUSTOM = 1100 };

static bool sendAll(SOCKET s, const void* p, int n) {
    const char* c = (const char*)p;
    while (n > 0) { int k = send(s, c, n, 0); if (k <= 0) return false; c += k; n -= k; }
    return true;
}
static bool recvAll(SOCKET s, void* p, int n) {
    char* c = (char*)p;
    while (n > 0) { int k = recv(s, c, n, 0); if (k <= 0) return false; c += k; n -= k; }
    return true;
}
static bool sendPkt(SOCKET s, uint32_t dev, uint32_t id, const void* data, uint32_t size) {
    char h[16]; memcpy(h, "ORGB", 4); memcpy(h + 4, &dev, 4); memcpy(h + 8, &id, 4); memcpy(h + 12, &size, 4);
    return sendAll(s, h, 16) && (size == 0 || sendAll(s, data, (int)size));
}
static bool recvPkt(SOCKET s, uint32_t want, std::vector<char>& out) {
    for (;;) {
        char h[16]; if (!recvAll(s, h, 16) || memcmp(h, "ORGB", 4)) return false;
        uint32_t id, size; memcpy(&id, h + 8, 4); memcpy(&size, h + 12, 4);
        if (size > (16u << 20)) return false;
        out.resize(size); if (size && !recvAll(s, out.data(), (int)size)) return false;
        if (id == want) return true;           // skip unrelated packets (device list updates)
    }
}

// number of LEDs from a protocol-0 controller data block
struct Rd {
    const char* p; size_t n, i = 0; bool ok = true;
    template <class T> T get() { T v{}; if (i + sizeof(T) > n) { ok = false; return v; } memcpy(&v, p + i, sizeof(T)); i += sizeof(T); return v; }
    void skip(size_t k) { if (i + k > n) { ok = false; i = n; } else i += k; }
    void str() { skip(get<uint16_t>()); }
};
static int ledCount(const std::vector<char>& d) {
    Rd r{d.data(), d.size()};
    r.get<uint32_t>(); r.get<int32_t>();                       // data_size, type
    for (int i = 0; i < 5; i++) r.str();                        // name, description, version, serial, location
    int modes = r.get<uint16_t>(); r.get<int32_t>();            // num_modes, active_mode
    for (int m = 0; m < modes && r.ok; m++) {
        r.str(); r.skip(4 + 4 * 8);                             // name, value, flags .. color_mode
        r.skip(4 * (size_t)r.get<uint16_t>());                  // mode colors
    }
    int zones = r.get<uint16_t>();
    for (int z = 0; z < zones && r.ok; z++) {
        r.str(); r.skip(4 + 4 * 3);                             // name, type, leds min/max/count
        r.skip(r.get<uint16_t>());                              // matrix (height, width, data) or nothing
    }
    int leds = r.get<uint16_t>();
    return r.ok ? leds : -1;
}

static DWORD WINAPI Thread(LPVOID) {
    WSADATA w; if (WSAStartup(MAKEWORD(2, 2), &w)) return 0;
    while (*g_running) {
        if (!g_enabled) { InterlockedExchange(&g_state, ST_OFF); Sleep(300); continue; }
        SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in a = {}; a.sin_family = AF_INET; a.sin_port = htons(6742); a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        DWORD tmo = 2000; setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tmo, sizeof(tmo));
        int one = 1; setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof(one));
        std::vector<int> leds; std::vector<char> buf;
        bool ok = s != INVALID_SOCKET && connect(s, (sockaddr*)&a, sizeof(a)) == 0;
        if (ok) { const char nm[] = "Headphones Guy"; ok = sendPkt(s, 0, PKT_NAME, nm, sizeof(nm)); }
        if (ok) ok = sendPkt(s, 0, PKT_COUNT, nullptr, 0) && recvPkt(s, PKT_COUNT, buf) && buf.size() >= 4;
        if (ok) {
            uint32_t n; memcpy(&n, buf.data(), 4);
            for (uint32_t d = 0; d < n && ok; d++) {
                ok = sendPkt(s, d, PKT_DATA, nullptr, 0) && recvPkt(s, PKT_DATA, buf);
                int c = ok ? ledCount(buf) : -1; leds.push_back(c);
                if (ok && c > 0) ok = sendPkt(s, d, PKT_CUSTOM, nullptr, 0);
            }
        }
        if (!ok) {
            if (s != INVALID_SOCKET) closesocket(s);
            InterlockedExchange(&g_state, ST_NOSERVER);
            for (int i = 0; i < 30 && *g_running && g_enabled; i++) Sleep(100);   // retry every 3 s
            continue;
        }
        int nDev = 0; for (int c : leds) nDev += c > 0;
        InterlockedExchange(&g_devices, nDev); InterlockedExchange(&g_state, ST_CONNECTED);
        uint32_t last = 0xFFFFFFFF; DWORD lastSend = 0; std::vector<char> pkt;
        while (*g_running && g_enabled && ok) {
            uint32_t rgb = g_color ? g_color() & 0xFFFFFF : 0;
            DWORD t = GetTickCount();
            if (rgb != last || t - lastSend > 1000) {   // resend now and then so other apps can't keep the LEDs
                uint32_t c = ((rgb >> 16) & 255) | (rgb & 0xFF00) | ((rgb & 255) << 16);   // RGBColor = 0x00BBGGRR
                for (size_t d = 0; d < leds.size() && ok; d++) {
                    if (leds[d] <= 0) continue;
                    uint16_t cnt = (uint16_t)leds[d]; uint32_t size = 4 + 2 + 4u * cnt;
                    pkt.resize(size); memcpy(pkt.data(), &size, 4); memcpy(pkt.data() + 4, &cnt, 2);
                    for (int i = 0; i < cnt; i++) memcpy(pkt.data() + 6 + 4 * i, &c, 4);
                    ok = sendPkt(s, (uint32_t)d, PKT_UPDATELEDS, pkt.data(), size);
                }
                last = rgb; lastSend = t;
            }
            Sleep(25);
        }
        closesocket(s);
        InterlockedExchange(&g_state, g_enabled ? ST_NOSERVER : ST_OFF);
    }
    WSACleanup();
    return 0;
}

} // namespace orgb
