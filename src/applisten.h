// Per-app listening (Windows 10 2004+ process loopback): capture the audio of one program and its child
// processes, e.g. only Spotify or only Discord. Also lists the programs that are playing sound right now.
#pragma once
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiopolicy.h>
#include <tlhelp32.h>
#include <propidl.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cwctype>

namespace applisten {

// not in the MinGW headers
typedef enum { ACT_TYPE_DEFAULT = 0, ACT_TYPE_PROCESS_LOOPBACK = 1 } ActType;
typedef enum { LOOPBACK_INCLUDE_TREE = 0, LOOPBACK_EXCLUDE_TREE = 1 } LoopbackMode;
struct LoopbackParams { DWORD TargetProcessId; LoopbackMode ProcessLoopbackMode; };
struct ActivationParams { ActType ActivationType; LoopbackParams ProcessLoopbackParams; };
static const wchar_t* kVirtualDevice = L"VAD\\Process_Loopback";
static const GUID kIID_Handler = {0x41D949AB, 0x9862, 0x444A, {0x80, 0xF6, 0xC2, 0x61, 0x33, 0x4D, 0xA5, 0xEB}};  // IActivateAudioInterfaceCompletionHandler
static const GUID kIID_Agile = {0x94EA2B94, 0xE9CC, 0x49E0, {0xC0, 0xFF, 0xEE, 0x64, 0xCA, 0x8F, 0x5B, 0x90}};    // IAgileObject

typedef HRESULT(WINAPI* ActivateFn)(LPCWSTR, REFIID, PROPVARIANT*, IActivateAudioInterfaceCompletionHandler*, IActivateAudioInterfaceAsyncOperation**);

// process loopback needs build 19041 (Windows 10 2004)
static bool Supported() {
    typedef LONG(WINAPI * RtlGetVersionFn)(OSVERSIONINFOW*);
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    RtlGetVersionFn f = nt ? (RtlGetVersionFn)GetProcAddress(nt, "RtlGetVersion") : NULL;
    OSVERSIONINFOW v = {sizeof(v)};
    if (!f || f(&v) != 0) return false;
    if (v.dwMajorVersion > 10 || (v.dwMajorVersion == 10 && v.dwBuildNumber >= 19041)) return GetProcAddress(LoadLibraryW(L"mmdevapi.dll"), "ActivateAudioInterfaceAsync") != NULL;
    return false;
}

static std::wstring Lower(std::wstring s) { for (auto& c : s) c = (wchar_t)towlower(c); return s; }
static std::wstring ExeOf(DWORD pid) {
    std::wstring r; HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid); if (!h) return r;
    wchar_t p[MAX_PATH]; DWORD n = MAX_PATH;
    if (QueryFullProcessImageNameW(h, 0, p, &n)) { const wchar_t* b = wcsrchr(p, L'\\'); r = Lower(b ? b + 1 : p); }
    CloseHandle(h); return r;
}
// programs with an active audio session on the default output, lowercase exe names
static std::vector<std::wstring> PlayingNow() {
    std::vector<std::wstring> out;
    IMMDeviceEnumerator* en = NULL; IMMDevice* dev = NULL; IAudioSessionManager2* mgr = NULL; IAudioSessionEnumerator* se = NULL;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&en))) return out;
    if (SUCCEEDED(en->GetDefaultAudioEndpoint(eRender, eConsole, &dev)) &&
        SUCCEEDED(dev->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, NULL, (void**)&mgr)) &&
        SUCCEEDED(mgr->GetSessionEnumerator(&se))) {
        int n = 0; se->GetCount(&n);
        for (int i = 0; i < n; i++) {
            IAudioSessionControl* c = NULL; IAudioSessionControl2* c2 = NULL;
            if (FAILED(se->GetSession(i, &c))) continue;
            AudioSessionState st; DWORD pid = 0;
            if (SUCCEEDED(c->GetState(&st)) && st == AudioSessionStateActive && SUCCEEDED(c->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&c2))
                && SUCCEEDED(c2->GetProcessId(&pid)) && pid) {
                std::wstring e = ExeOf(pid);
                if (!e.empty() && std::find(out.begin(), out.end(), e) == out.end()) out.push_back(e);
            }
            if (c2) c2->Release();
            c->Release();
        }
    }
    if (se) se->Release();
    if (mgr) mgr->Release();
    if (dev) dev->Release();
    en->Release();
    std::sort(out.begin(), out.end());
    return out;
}
// the top process of a program (its parent is something else), so INCLUDE_TREE catches all its children
static DWORD RootPid(const std::wstring& exe) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); if (snap == INVALID_HANDLE_VALUE) return 0;
    struct P { DWORD pid, parent; std::wstring exe; }; std::vector<P> all;
    PROCESSENTRY32W pe = {sizeof(pe)};
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) all.push_back({pe.th32ProcessID, pe.th32ParentProcessID, Lower(pe.szExeFile)});
    CloseHandle(snap);
    for (auto& p : all) {
        if (p.exe != exe) continue;
        bool parentSame = false;
        for (auto& q : all) if (q.pid == p.parent && q.exe == exe) { parentSame = true; break; }
        if (!parentSame) return p.pid;
    }
    return 0;
}

class Handler final : public IActivateAudioInterfaceCompletionHandler {
public:
    HANDLE done = CreateEventW(NULL, TRUE, FALSE, NULL);
    ~Handler() { CloseHandle(done); }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID r, void** p) override {
        if (IsEqualGUID(r, IID_IUnknown) || IsEqualGUID(r, kIID_Handler) || IsEqualGUID(r, kIID_Agile)) { *p = this; AddRef(); return S_OK; }
        *p = NULL; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&ref); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r = InterlockedDecrement(&ref); if (!r) delete this; return r; }
    HRESULT STDMETHODCALLTYPE ActivateCompleted(IActivateAudioInterfaceAsyncOperation*) override { SetEvent(done); return S_OK; }
private:
    LONG ref = 1;
};

// Activate a capture client on one process tree. The format is ours to choose: float stereo 48 kHz.
static IAudioClient* Open(DWORD pid, WAVEFORMATEX* wf) {
    static ActivateFn act = (ActivateFn)GetProcAddress(LoadLibraryW(L"mmdevapi.dll"), "ActivateAudioInterfaceAsync");
    if (!act) return NULL;
    ActivationParams ap = {}; ap.ActivationType = ACT_TYPE_PROCESS_LOOPBACK;
    ap.ProcessLoopbackParams.TargetProcessId = pid; ap.ProcessLoopbackParams.ProcessLoopbackMode = LOOPBACK_INCLUDE_TREE;
    PROPVARIANT pv; PropVariantInit(&pv); pv.vt = VT_BLOB; pv.blob.cbSize = sizeof(ap); pv.blob.pBlobData = (BYTE*)&ap;
    // on the heap: if activation answers after we gave up waiting, the callback still has a live object
    Handler* h = new Handler(); IActivateAudioInterfaceAsyncOperation* op = NULL;
    if (FAILED(act(kVirtualDevice, __uuidof(IAudioClient), &pv, h, &op)) || !op) { h->Release(); return NULL; }
    IAudioClient* ac = NULL;
    if (WaitForSingleObject(h->done, 5000) == WAIT_OBJECT_0) {
        HRESULT hr = E_FAIL; IUnknown* u = NULL;
        if (SUCCEEDED(op->GetActivateResult(&hr, &u)) && SUCCEEDED(hr) && u) { u->QueryInterface(__uuidof(IAudioClient), (void**)&ac); u->Release(); }
    }
    op->Release(); h->Release();
    if (!ac) return NULL;
    memset(wf, 0, sizeof(*wf));
    wf->wFormatTag = WAVE_FORMAT_IEEE_FLOAT; wf->nChannels = 2; wf->nSamplesPerSec = 48000; wf->wBitsPerSample = 32;
    wf->nBlockAlign = 8; wf->nAvgBytesPerSec = 48000 * 8;
    if (FAILED(ac->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM,
                              2000000, 0, wf, NULL))) { ac->Release(); return NULL; }
    return ac;
}

// apps where the guy talks along instead of nodding
static bool IsVoiceApp(const std::wstring& e) {
    static const wchar_t* v[] = {L"discord.exe", L"discordptb.exe", L"discordcanary.exe", L"telegram.exe", L"ts3client_win64.exe", L"teamspeak.exe",
                                 L"skype.exe", L"zoom.exe", L"ms-teams.exe", L"teams.exe", L"slack.exe", L"whatsapp.exe", L"viber.exe", L"mumble.exe"};
    for (auto s : v) if (e == s) return true;
    return false;
}

} // namespace applisten
