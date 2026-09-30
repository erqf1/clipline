// Windows: Bildschirme (DXGI), Hotkey (RegisterHotKey), Autostart (Run-Key), Systemton (WASAPI-Loopback)
#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QDir>
#include <QKeySequence>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QUuid>
#include "platform.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <audioclient.h>
#include <dxgi.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>
#include <atomic>
#include <cmath>
#include <thread>
#include <vector>

// ---------------------------------------------------------------- Bildschirme
namespace {
struct EnumCtx { QList<Monitor> list; };

BOOL CALLBACK monitorProc(HMONITOR hm, HDC, LPRECT, LPARAM lp) {
    auto* ctx = reinterpret_cast<EnumCtx*>(lp);
    MONITORINFOEXW mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(hm, &mi)) return TRUE;
    Monitor m;
    m.id = QString::fromWCharArray(mi.szDevice);
    m.rect = QRect(mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                   mi.rcMonitor.bottom - mi.rcMonitor.top);
    m.primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    if (EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm) && dm.dmDisplayFrequency > 1)
        m.refresh = dm.dmDisplayFrequency;
    ctx->list.append(m);
    return TRUE;
}
}  // namespace

QList<Monitor> listMonitors() {
    EnumCtx ctx;
    EnumDisplayMonitors(nullptr, nullptr, monitorProc, reinterpret_cast<LPARAM>(&ctx));

    // ddagrab nutzt die Ausgänge des Standard-Adapters (Adapter 0)
    IDXGIFactory1* factory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory)))) {
        IDXGIAdapter1* adapter = nullptr;
        if (SUCCEEDED(factory->EnumAdapters1(0, &adapter))) {
            IDXGIOutput* out = nullptr;
            for (UINT i = 0; adapter->EnumOutputs(i, &out) != DXGI_ERROR_NOT_FOUND; ++i) {
                DXGI_OUTPUT_DESC desc{};
                if (SUCCEEDED(out->GetDesc(&desc))) {
                    const QString dev = QString::fromWCharArray(desc.DeviceName);
                    for (Monitor& m : ctx.list)
                        if (m.id == dev) m.ddaIndex = int(i);
                }
                out->Release();
            }
            adapter->Release();
        }
        factory->Release();
    }

    std::sort(ctx.list.begin(), ctx.list.end(), [](const Monitor& a, const Monitor& b) {
        if (a.primary != b.primary) return a.primary;
        return a.rect.x() < b.rect.x();
    });
    for (int i = 0; i < ctx.list.size(); ++i) {
        Monitor& m = ctx.list[i];
        m.name = QString("%1 %2 · %3×%4 · %5 Hz").arg(i + 1).arg(m.primary ? "★" : "").arg(m.rect.width())
                     .arg(m.rect.height()).arg(int(m.refresh));
    }
    return ctx.list;
}

// ---------------------------------------------------------------- Mikrofone (DirectShow via ffmpeg)
QStringList listMicrophones() {
    QProcess p;
    p.start(ffmpegPath(), {"-hide_banner", "-list_devices", "true", "-f", "dshow", "-i", "dummy"});
    p.waitForFinished(8000);
    QStringList out;
    const QString e = QString::fromUtf8(p.readAllStandardError());
    for (const QString& line : e.split('\n')) {
        auto m = QRegularExpression("\"([^\"]+)\"\\s*\\(audio\\)").match(line);
        if (m.hasMatch()) out << m.captured(1);
    }
    return out;
}

bool systemAudioSupported() { return true; }

// ---------------------------------------------------------------- Autostart
bool setAutostart(bool on) {
    QSettings run("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    if (on) {
        const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        run.setValue("Clipline", QString("\"%1\" --background").arg(exe));
    } else {
        run.remove("Clipline");
    }
    run.sync();
    return run.status() == QSettings::NoError;
}

// ---------------------------------------------------------------- Ton beim Speichern
// Kurzer, weicher Zwei-Ton (synthetisiert, als WAV im Speicher)
void playSaveSound() {
    static std::vector<char> wav = [] {
        const int rate = 44100;
        const int n = rate * 18 / 100;
        std::vector<int16_t> s(n);
        for (int i = 0; i < n; ++i) {
            const double t = double(i) / rate;
            const double f = t < 0.07 ? 880.0 : 1318.5;
            const double env = std::exp(-t * 18.0) * std::min(1.0, t * 400.0);
            s[i] = int16_t(std::sin(2 * 3.14159265 * f * t) * env * 9000);
        }
        std::vector<char> w(44 + n * 2);
        auto put32 = [&](int off, uint32_t v) { memcpy(&w[off], &v, 4); };
        auto put16 = [&](int off, uint16_t v) { memcpy(&w[off], &v, 2); };
        memcpy(&w[0], "RIFF", 4); put32(4, uint32_t(36 + n * 2)); memcpy(&w[8], "WAVEfmt ", 8);
        put32(16, 16); put16(20, 1); put16(22, 1); put32(24, rate); put32(28, rate * 2); put16(32, 2); put16(34, 16);
        memcpy(&w[36], "data", 4); put32(40, uint32_t(n * 2));
        memcpy(&w[44], s.data(), n * 2);
        return w;
    }();
    PlaySoundA(wav.data(), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}

// ---------------------------------------------------------------- Globaler Hotkey
namespace {
UINT vkFor(int key) {
    if (key >= Qt::Key_A && key <= Qt::Key_Z) return UINT('A' + (key - Qt::Key_A));
    if (key >= Qt::Key_0 && key <= Qt::Key_9) return UINT('0' + (key - Qt::Key_0));
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) return UINT(VK_F1 + (key - Qt::Key_F1));
    switch (key) {
    case Qt::Key_Space: return VK_SPACE;
    case Qt::Key_Print: return VK_SNAPSHOT;
    case Qt::Key_Pause: return VK_PAUSE;
    case Qt::Key_Insert: return VK_INSERT;
    case Qt::Key_Delete: return VK_DELETE;
    case Qt::Key_Home: return VK_HOME;
    case Qt::Key_End: return VK_END;
    case Qt::Key_PageUp: return VK_PRIOR;
    case Qt::Key_PageDown: return VK_NEXT;
    case Qt::Key_ScrollLock: return VK_SCROLL;
    default: return 0;
    }
}

class HotkeyFilter : public QAbstractNativeEventFilter {
public:
    std::function<void(int)> onHotkey;
    bool nativeEventFilter(const QByteArray&, void* message, qintptr*) override {
        MSG* msg = static_cast<MSG*>(message);
        if (msg->message == WM_HOTKEY && onHotkey) { onHotkey(int(msg->wParam)); return true; }
        return false;
    }
};
}  // namespace

struct GlobalHotkey::Impl {
    HotkeyFilter filter;
    int id = 0;
    bool registered = false;
};

GlobalHotkey::GlobalHotkey(QObject* parent) : QObject(parent), d(new Impl) {
    static int counter = 0x4C50;
    d->id = ++counter;
    d->filter.onHotkey = [this](int id) { if (id == d->id) emit activated(); };
    QCoreApplication::instance()->installNativeEventFilter(&d->filter);
}

GlobalHotkey::~GlobalHotkey() {
    clear();
    QCoreApplication::instance()->removeNativeEventFilter(&d->filter);
}

bool GlobalHotkey::supported() { return true; }

bool GlobalHotkey::set(const QString& sequence) {
    clear();
    const QKeySequence seq(sequence);
    if (seq.isEmpty()) return false;
    const QKeyCombination kc = seq[0];
    const UINT vk = vkFor(kc.key());
    if (!vk) return false;
    UINT mods = MOD_NOREPEAT;
    const Qt::KeyboardModifiers m = kc.keyboardModifiers();
    if (m & Qt::ControlModifier) mods |= MOD_CONTROL;
    if (m & Qt::AltModifier) mods |= MOD_ALT;
    if (m & Qt::ShiftModifier) mods |= MOD_SHIFT;
    if (m & Qt::MetaModifier) mods |= MOD_WIN;
    d->registered = RegisterHotKey(nullptr, d->id, mods, vk) != 0;
    return d->registered;
}

void GlobalHotkey::clear() {
    if (d->registered) UnregisterHotKey(nullptr, d->id);
    d->registered = false;
}

// ---------------------------------------------------------------- Systemton: WASAPI-Loopback -> Named Pipe -> ffmpeg
struct SystemAudioPipe::Impl {
    QString name;
    std::thread thread;
    std::atomic<bool> stop{false};
    HANDLE pipe = INVALID_HANDLE_VALUE;
    int rate = 48000, channels = 2, bytesPerFrame = 8;
};

SystemAudioPipe::SystemAudioPipe() : d(new Impl) {}
SystemAudioPipe::~SystemAudioPipe() { stop(); }

QString SystemAudioPipe::pipePath() const { return d->name; }

bool SystemAudioPipe::prepare(QString* fmt, int* rate, int* channels) {
    const bool comInit = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    bool ok = false;
    IMMDeviceEnumerator* en = nullptr;
    IMMDevice* dev = nullptr;
    IAudioClient* client = nullptr;
    WAVEFORMATEX* wf = nullptr;
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                   reinterpret_cast<void**>(&en))) &&
        SUCCEEDED(en->GetDefaultAudioEndpoint(eRender, eConsole, &dev)) &&
        SUCCEEDED(dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client))) &&
        SUCCEEDED(client->GetMixFormat(&wf))) {
        bool isFloat = wf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
        if (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
            isFloat = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(wf)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        if (isFloat && wf->wBitsPerSample == 32) *fmt = "f32le";
        else if (wf->wBitsPerSample == 16) *fmt = "s16le";
        else if (wf->wBitsPerSample == 24) *fmt = "s24le";
        else *fmt = "s32le";
        d->rate = int(wf->nSamplesPerSec);
        d->channels = wf->nChannels;
        d->bytesPerFrame = wf->nBlockAlign;
        *rate = d->rate;
        *channels = d->channels;
        ok = true;
    }
    if (wf) CoTaskMemFree(wf);
    if (client) client->Release();
    if (dev) dev->Release();
    if (en) en->Release();
    if (comInit) CoUninitialize();
    if (ok) d->name = QString("\\\\.\\pipe\\clipline_audio_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    return ok;
}

void SystemAudioPipe::start() {
    stop();
    if (d->name.isEmpty()) return;
    d->stop = false;
    d->pipe = CreateNamedPipeW(reinterpret_cast<const wchar_t*>(d->name.utf16()), PIPE_ACCESS_OUTBOUND,
                               PIPE_TYPE_BYTE | PIPE_WAIT, 1, 1 << 20, 0, 0, nullptr);
    if (d->pipe == INVALID_HANDLE_VALUE) return;
    Impl* s = d.get();
    d->thread = std::thread([s] {
        if (!ConnectNamedPipe(s->pipe, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) return;
        if (s->stop) return;
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        IMMDeviceEnumerator* en = nullptr;
        IMMDevice* dev = nullptr;
        IAudioClient* client = nullptr;
        IAudioCaptureClient* cap = nullptr;
        WAVEFORMATEX* wf = nullptr;
        bool ok = SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                             __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&en))) &&
                  SUCCEEDED(en->GetDefaultAudioEndpoint(eRender, eConsole, &dev)) &&
                  SUCCEEDED(dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client))) &&
                  SUCCEEDED(client->GetMixFormat(&wf)) &&
                  SUCCEEDED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 2000000, 0, wf, nullptr)) &&
                  SUCCEEDED(client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&cap))) &&
                  SUCCEEDED(client->Start());
        const int bpf = wf ? wf->nBlockAlign : s->bytesPerFrame;
        const int rate = wf ? int(wf->nSamplesPerSec) : s->rate;
        std::vector<char> zeros(size_t(bpf) * rate / 10, 0);
        LARGE_INTEGER freq, t0, now;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&t0);
        long long written = 0;
        auto put = [&](const void* data, size_t bytes) {
            DWORD w = 0;
            return WriteFile(s->pipe, data, DWORD(bytes), &w, nullptr) != 0;
        };
        while (!s->stop) {
            bool alive = true;
            if (ok) {
                UINT32 next = 0;
                while (alive && SUCCEEDED(cap->GetNextPacketSize(&next)) && next > 0) {
                    BYTE* data = nullptr;
                    UINT32 frames = 0;
                    DWORD flags = 0;
                    if (FAILED(cap->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) break;
                    const size_t bytes = size_t(frames) * bpf;
                    if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                        for (size_t left = bytes; alive && left > 0;) {
                            size_t c = std::min(left, zeros.size());
                            alive = put(zeros.data(), c);
                            left -= c;
                        }
                    } else {
                        alive = put(data, bytes);
                    }
                    written += frames;
                    cap->ReleaseBuffer(frames);
                }
            }
            // Loopback liefert nichts, solange nichts abgespielt wird: mit Stille auffüllen (hält den Ton synchron)
            QueryPerformanceCounter(&now);
            const long long expected = (now.QuadPart - t0.QuadPart) * rate / freq.QuadPart;
            if (alive && written + rate / 20 < expected) {
                long long missing = expected - written;
                while (alive && missing > 0) {
                    const long long c = std::min<long long>(missing, (long long)(zeros.size() / bpf));
                    alive = put(zeros.data(), size_t(c) * bpf);
                    missing -= c;
                    written += c;
                }
            }
            if (!alive) break;
            Sleep(10);
        }
        if (client) client->Stop();
        if (cap) cap->Release();
        if (wf) CoTaskMemFree(wf);
        if (client) client->Release();
        if (dev) dev->Release();
        if (en) en->Release();
        CoUninitialize();
    });
}

void SystemAudioPipe::stop() {
    if (!d->thread.joinable() && d->pipe == INVALID_HANDLE_VALUE) return;
    d->stop = true;
    // Falls der Thread noch auf ffmpeg wartet: selbst verbinden, damit ConnectNamedPipe zurückkehrt
    HANDLE h = CreateFileW(reinterpret_cast<const wchar_t*>(d->name.utf16()), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    if (d->pipe != INVALID_HANDLE_VALUE) {
        CancelIoEx(d->pipe, nullptr);
        DisconnectNamedPipe(d->pipe);
    }
    if (d->thread.joinable()) d->thread.join();
    if (d->pipe != INVALID_HANDLE_VALUE) CloseHandle(d->pipe);
    d->pipe = INVALID_HANDLE_VALUE;
}
