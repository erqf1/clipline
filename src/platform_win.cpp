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
#include <propsys.h>
#include <ks.h>
#include <ksmedia.h>
#include <atomic>
#include <cmath>
#include <thread>
#include <vector>

static const PROPERTYKEY PKEY_Device_FriendlyName = {{0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14};

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
    QStringList out;
    const bool comInit = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    IMMDeviceEnumerator* en = nullptr;
    IMMDeviceCollection* list = nullptr;
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                   reinterpret_cast<void**>(&en))) &&
        SUCCEEDED(en->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &list))) {
        UINT n = 0;
        list->GetCount(&n);
        for (UINT i = 0; i < n; ++i) {
            IMMDevice* dev = nullptr;
            IPropertyStore* props = nullptr;
            PROPVARIANT v;
            PropVariantInit(&v);
            if (SUCCEEDED(list->Item(i, &dev)) && SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &props)) &&
                SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &v)) && v.vt == VT_LPWSTR)
                out << QString::fromWCharArray(v.pwszVal);
            PropVariantClear(&v);
            if (props) props->Release();
            if (dev) dev->Release();
        }
    }
    if (list) list->Release();
    if (en) en->Release();
    if (comInit) CoUninitialize();
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

// ---------------------------------------------------------------- Ton: WASAPI (Systemton + Mikrofon) -> Mischer -> Named Pipe -> ffmpeg
namespace {
constexpr int kRate = 48000;

template <class T> void release(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

// Mikrofon über den Anzeigenamen finden (derselbe Name wie früher bei DirectShow)
IMMDevice* findCaptureDevice(IMMDeviceEnumerator* en, const QString& name) {
    IMMDeviceCollection* list = nullptr;
    if (FAILED(en->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &list))) return nullptr;
    IMMDevice* found = nullptr;
    IMMDevice* prefix = nullptr;  // alte DirectShow-Namen waren manchmal gekürzt
    UINT n = 0;
    list->GetCount(&n);
    for (UINT i = 0; i < n && !found; ++i) {
        IMMDevice* dev = nullptr;
        if (FAILED(list->Item(i, &dev))) continue;
        IPropertyStore* props = nullptr;
        PROPVARIANT v;
        PropVariantInit(&v);
        QString devName;
        if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &props)) && SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &v)) &&
            v.vt == VT_LPWSTR)
            devName = QString::fromWCharArray(v.pwszVal);
        PropVariantClear(&v);
        release(props);
        if (devName == name) {
            found = dev;
        } else if (!prefix && !devName.isEmpty() && (devName.startsWith(name) || name.startsWith(devName))) {
            prefix = dev;
        } else {
            dev->Release();
        }
    }
    list->Release();
    if (found) {
        release(prefix);
        return found;
    }
    return prefix;
}

// Eine WASAPI-Aufnahme (Loopback oder Mikrofon), gewandelt in Stereo-float mit 48 kHz
struct Capture {
    IAudioClient* client = nullptr;
    IAudioCaptureClient* cap = nullptr;
    int ch = 2, rate = kRate, bits = 32, bpf = 8;
    bool isFloat = true;
    std::vector<float> in;  // gewandelte Stereo-Frames vor dem Umrechnen der Abtastrate
    double pos = 0;         // Leseposition für die Abtastraten-Umrechnung

    bool open(IMMDevice* dev, bool loopback) {
        WAVEFORMATEX* wf = nullptr;
        bool ok = SUCCEEDED(dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client))) &&
                  SUCCEEDED(client->GetMixFormat(&wf));
        if (ok) {
            ch = wf->nChannels;
            rate = int(wf->nSamplesPerSec);
            bits = wf->wBitsPerSample;
            bpf = wf->nBlockAlign;
            isFloat = wf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                      (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                       reinterpret_cast<WAVEFORMATEXTENSIBLE*>(wf)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
            ok = SUCCEEDED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, loopback ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0, 2000000, 0,
                                              wf, nullptr)) &&
                 SUCCEEDED(client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&cap))) &&
                 SUCCEEDED(client->Start());
        }
        if (wf) CoTaskMemFree(wf);
        if (!ok) close();
        return ok;
    }
    void close() {
        if (client) client->Stop();
        release(cap);
        release(client);
    }
    float sample(const BYTE* p) const {
        if (isFloat && bits == 32) return *reinterpret_cast<const float*>(p);
        if (bits == 16) return *reinterpret_cast<const int16_t*>(p) / 32768.f;
        if (bits == 24) return float(int32_t((uint32_t(p[0]) << 8) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 24))) / 2147483648.f;
        return *reinterpret_cast<const int32_t*>(p) / 2147483648.f;
    }
    // Mehrkanal -> Stereo: vorne links/rechts, Mitte zu beiden, hintere/seitliche Kanäle leiser dazu (LFE weglassen)
    void toStereo(const BYTE* data, UINT32 frames, bool silent) {
        const int step = bits / 8;
        const size_t base = in.size();
        in.resize(base + size_t(frames) * 2, 0.f);
        if (silent) return;
        for (UINT32 f = 0; f < frames; ++f) {
            const BYTE* fr = data + size_t(f) * bpf;
            float l = sample(fr), r = ch > 1 ? sample(fr + step) : l;
            if (ch >= 3) {
                const float c = sample(fr + 2 * step) * 0.707f;
                l += c;
                r += c;
            }
            for (int k = 4; k < ch; ++k) (k % 2 == 0 ? l : r) += sample(fr + k * step) * 0.5f;
            in[base + f * 2] = l;
            in[base + f * 2 + 1] = r;
        }
    }
    // Alles Verfügbare lesen und als Stereo 48 kHz an out anhängen
    bool read(std::vector<float>& out) {
        if (!cap) return false;
        UINT32 next = 0;
        while (SUCCEEDED(cap->GetNextPacketSize(&next)) && next > 0) {
            BYTE* data = nullptr;
            UINT32 frames = 0;
            DWORD flags = 0;
            if (FAILED(cap->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) return false;
            toStereo(data, frames, flags & AUDCLNT_BUFFERFLAGS_SILENT);
            cap->ReleaseBuffer(frames);
        }
        if (rate == kRate) {
            out.insert(out.end(), in.begin(), in.end());
            in.clear();
            return true;
        }
        // Lineare Umrechnung der Abtastrate (z. B. 44,1 kHz Mikrofon)
        const double step = double(rate) / kRate;
        const size_t n = in.size() / 2;
        while (pos + 1 < double(n)) {
            const size_t i = size_t(pos);
            const float f = float(pos - double(i));
            out.push_back(in[i * 2] * (1 - f) + in[i * 2 + 2] * f);
            out.push_back(in[i * 2 + 1] * (1 - f) + in[i * 2 + 3] * f);
            pos += step;
        }
        const size_t used = std::min(size_t(pos), n);
        in.erase(in.begin(), in.begin() + used * 2);
        pos -= double(used);
        return true;
    }
};

float rmsDb(const float* s, size_t frames) {
    if (!frames) return -90.f;
    double sum = 0;
    for (size_t i = 0; i < frames * 2; ++i) sum += double(s[i]) * s[i];
    const double rms = std::sqrt(sum / double(frames * 2));
    return float(std::max(-90.0, 20.0 * std::log10(rms + 1e-9)));
}

// Rauschsperre fürs Mikrofon: unter der Schwelle wird weich stummgeschaltet (mit kurzer Haltezeit,
// damit Wortenden nicht abgeschnitten werden)
struct NoiseGate {
    float thresholdDb = -45.f, volume = 1.f, gain = 0.f;
    int hold = 0;
    std::atomic<float>* level = nullptr;
    void process(float* s, size_t frames) {
        const bool off = thresholdDb <= -80.f;
        const float attack = 1.f - std::exp(-1.f / (0.005f * kRate)), rel = 1.f - std::exp(-1.f / (0.08f * kRate));
        for (size_t o = 0; o < frames; o += 480) {  // 10-ms-Blöcke
            const size_t n = std::min<size_t>(480, frames - o);
            const float db = rmsDb(s + o * 2, n);
            if (level) level->store(db);
            if (off || db > thresholdDb) hold = kRate / 4;
            const float target = hold > 0 ? 1.f : 0.f;
            hold -= int(n);
            for (size_t i = 0; i < n; ++i) {
                gain += (target - gain) * (target > gain ? attack : rel);
                s[(o + i) * 2] *= gain * volume;
                s[(o + i) * 2 + 1] *= gain * volume;
            }
        }
    }
};

void dropFront(std::vector<float>& buf, size_t frames) {
    buf.erase(buf.begin(), buf.begin() + std::min(buf.size(), frames * 2));
}
}  // namespace

struct SystemAudioPipe::Impl {
    QString name;
    std::thread thread;
    std::atomic<bool> stop{false};
    HANDLE pipe = INVALID_HANDLE_VALUE;
    bool system = false;
    QString mic;
    MicTuning tuning;
};

SystemAudioPipe::SystemAudioPipe() : d(new Impl) {}
SystemAudioPipe::~SystemAudioPipe() { stop(); }

QString SystemAudioPipe::pipePath() const { return d->name; }

bool SystemAudioPipe::prepare(bool systemSound, const QString& mic, const MicTuning& tuning) {
    const bool comInit = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    IMMDeviceEnumerator* en = nullptr;
    bool haveSys = false, haveMic = false;
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                   reinterpret_cast<void**>(&en)))) {
        if (systemSound) {
            IMMDevice* dev = nullptr;
            haveSys = SUCCEEDED(en->GetDefaultAudioEndpoint(eRender, eConsole, &dev));
            release(dev);
        }
        if (!mic.isEmpty()) {
            IMMDevice* dev = findCaptureDevice(en, mic);
            haveMic = dev != nullptr;
            release(dev);
        }
    }
    release(en);
    if (comInit) CoUninitialize();
    d->system = haveSys;
    d->mic = haveMic ? mic : QString();
    d->tuning = tuning;
    if (!haveSys && !haveMic) return false;
    d->name = QString("\\\\.\\pipe\\clipline_audio_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    return true;
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
        Capture sys, mic;
        bool haveSys = false, haveMic = false;
        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                       reinterpret_cast<void**>(&en)))) {
            if (s->system) {
                IMMDevice* dev = nullptr;
                if (SUCCEEDED(en->GetDefaultAudioEndpoint(eRender, eConsole, &dev))) haveSys = sys.open(dev, true);
                release(dev);
            }
            if (!s->mic.isEmpty()) {
                IMMDevice* dev = findCaptureDevice(en, s->mic);
                if (dev) haveMic = mic.open(dev, false);
                release(dev);
            }
        }
        NoiseGate gate;
        gate.thresholdDb = float(s->tuning.gateDb);
        gate.volume = float(std::clamp(s->tuning.volume, 0, 400)) / 100.f;

        // Ausgabe im Takt der Uhr, ~60 ms hinter der Echtzeit: so reicht der Puffer für beide Quellen,
        // fehlende Daten werden zu Stille (Loopback liefert nichts, solange nichts abgespielt wird)
        const long long delay = kRate * 60 / 1000, maxBuffered = kRate * 250 / 1000;
        std::vector<float> sysBuf, micBuf, micNew, out;
        LARGE_INTEGER freq, t0, now;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&t0);
        long long written = 0;
        while (!s->stop) {
            if (haveSys) sys.read(sysBuf);
            if (haveMic) {
                micNew.clear();
                mic.read(micNew);
                gate.process(micNew.data(), micNew.size() / 2);
                micBuf.insert(micBuf.end(), micNew.begin(), micNew.end());
            }
            // Uhren der Geräte laufen minimal auseinander: zu viel Gepuffertes verwerfen
            if (static_cast<long long>(sysBuf.size() / 2) > maxBuffered) dropFront(sysBuf, sysBuf.size() / 2 - delay);
            if (static_cast<long long>(micBuf.size() / 2) > maxBuffered) dropFront(micBuf, micBuf.size() / 2 - delay);

            QueryPerformanceCounter(&now);
            const long long due = (now.QuadPart - t0.QuadPart) * kRate / freq.QuadPart - delay - written;
            if (due > 0) {
                out.assign(size_t(due) * 2, 0.f);
                const size_t ns = std::min(out.size(), sysBuf.size()), nm = std::min(out.size(), micBuf.size());
                for (size_t i = 0; i < ns; ++i) out[i] += sysBuf[i];
                for (size_t i = 0; i < nm; ++i) out[i] += micBuf[i];
                for (float& v : out) v = std::clamp(v, -1.f, 1.f);
                sysBuf.erase(sysBuf.begin(), sysBuf.begin() + ns);
                micBuf.erase(micBuf.begin(), micBuf.begin() + nm);
                DWORD w = 0;
                if (!WriteFile(s->pipe, out.data(), DWORD(out.size() * sizeof(float)), &w, nullptr)) break;
                written += due;
            }
            Sleep(5);
        }
        sys.close();
        mic.close();
        release(en);
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

// ---------------------------------------------------------------- Mikrofon-Pegel (Einstellungen)
struct MicLevelMeter::Impl {
    std::thread thread;
    std::atomic<bool> stop{false};
    std::atomic<float> level{-90.f};
};

MicLevelMeter::MicLevelMeter() : d(new Impl) {}
MicLevelMeter::~MicLevelMeter() { stop(); }

bool MicLevelMeter::start(const QString& micName) {
    stop();
    if (micName.isEmpty()) return false;
    d->stop = false;
    d->level = -90.f;
    Impl* s = d.get();
    d->thread = std::thread([s, micName] {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        IMMDeviceEnumerator* en = nullptr;
        Capture mic;
        bool ok = false;
        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                       reinterpret_cast<void**>(&en)))) {
            IMMDevice* dev = findCaptureDevice(en, micName);
            if (dev) ok = mic.open(dev, false);
            release(dev);
        }
        std::vector<float> buf;
        while (ok && !s->stop) {
            buf.clear();
            mic.read(buf);
            if (buf.size() >= 2) {
                // schnell hoch, langsam runter - wie eine Pegelanzeige im Mischpult
                const float db = rmsDb(buf.data(), buf.size() / 2), cur = s->level;
                s->level = db > cur ? db : cur + (db - cur) * 0.25f;
            }
            Sleep(40);
        }
        mic.close();
        release(en);
        CoUninitialize();
    });
    return true;
}

void MicLevelMeter::stop() {
    d->stop = true;
    if (d->thread.joinable()) d->thread.join();
}

float MicLevelMeter::levelDb() const { return d->level; }
bool micMeterSupported() { return true; }
