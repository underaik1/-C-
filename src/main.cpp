#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <wrl.h>
#include <WebView2.h>

#include "../saper logic/b.cpp"
#include "resource.h"

using Microsoft::WRL::ComPtr;

namespace {
constexpr wchar_t kClassName[] = L"SaperMiniWindow";
constexpr wchar_t kTitle[] = L"Saper";
ComPtr<ICoreWebView2Controller> g_controller;
ComPtr<ICoreWebView2> g_webview;

class GameBridge : public MinesweeperCore {
public:
    GameBridge() : MinesweeperCore(Difficulty::EASY) {
        loadRecords();
    }

    void setMode(Difficulty d) {
        mode_ = d;
        setDifficulty(d);
        initField();
    }

    void restart() { initField(); }

    void reveal(int row, int col) {
        revealCell(row, col);
        if (isGameWon) saveRecords();
    }

    void flag(int row, int col) {
        if (row < 0 || row >= rows || col < 0 || col >= cols || isGameOver || isGameWon) return;
        if (grid[row][col].state == CellState::REVEALED) return;
        if (grid[row][col].state == CellState::HIDDEN && countFlags() >= minesCount) return;
        toggleFlag(row, col);
        finishWinIfAllMinesFlagged();
    }

    std::string toJson() const {
        std::ostringstream out;
        const int flagsCount = countFlags();
        const int correctFlags = countCorrectFlags();
        const int wrongFlags = isFirstMove ? 0 : flagsCount - correctFlags;
        const int minesLeft = isFirstMove ? minesCount : minesCount - correctFlags;

        out << '{'
            << "\"rows\":" << rows << ','
            << "\"cols\":" << cols << ','
            << "\"minesCount\":" << minesCount << ','
            << "\"flagsCount\":" << flagsCount << ','
            << "\"correctFlags\":" << correctFlags << ','
            << "\"wrongFlags\":" << wrongFlags << ','
            << "\"minesLeft\":" << minesLeft << ','
            << "\"gameOver\":" << (isGameOver ? "true" : "false") << ','
            << "\"gameWon\":" << (isGameWon ? "true" : "false") << ','
            << "\"difficulty\":\"" << diffText() << "\","
            << "\"highScores\":["
            << "{\"difficulty\":\"easy\",\"time\":\"" << formatTime(bestTimes[0]) << "\",\"seconds\":" << bestTimes[0] << "},"
            << "{\"difficulty\":\"medium\",\"time\":\"" << formatTime(bestTimes[1]) << "\",\"seconds\":" << bestTimes[1] << "},"
            << "{\"difficulty\":\"hard\",\"time\":\"" << formatTime(bestTimes[2]) << "\",\"seconds\":" << bestTimes[2] << "}"
            << "],"
            << "\"cells\":[";

        for (int r = 0; r < rows; ++r) {
            if (r) out << ',';
            out << '[';
            for (int c = 0; c < cols; ++c) {
                if (c) out << ',';
                const Cell& cell = grid[r][c];
                const bool mineVisible = cell.hasMine && (isGameOver || isGameWon);
                const bool wrongFlag = isGameOver && cell.state == CellState::FLAGGED && !cell.hasMine;
                const int adjacent = (cell.state == CellState::REVEALED && !cell.hasMine) ? cell.adjacentMines : 0;
                out << '{'
                    << "\"state\":\"" << stateText(cell.state) << "\","
                    << "\"mine\":" << (mineVisible ? "true" : "false") << ','
                    << "\"adjacent\":" << adjacent << ','
                    << "\"wrongFlag\":" << (wrongFlag ? "true" : "false")
                    << '}';
            }
            out << ']';
        }
        out << "]}";
        return out.str();
    }

private:
    int countFlags() const {
        int total = 0;
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                if (grid[r][c].state == CellState::FLAGGED) ++total;
            }
        }
        return total;
    }

    int countCorrectFlags() const {
        if (isFirstMove) return 0;
        int total = 0;
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                if (grid[r][c].state == CellState::FLAGGED && grid[r][c].hasMine) ++total;
            }
        }
        return total;
    }

    void finishWinIfAllMinesFlagged() {
        if (isFirstMove || isGameOver || isGameWon) return;
        if (countFlags() != minesCount || countCorrectFlags() != minesCount) return;

        isGameWon = true;
        const auto endTime = std::chrono::steady_clock::now();
        const int totalSeconds = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count();
        const int diffIndex = static_cast<int>(currentLevel);
        if (bestTimes[diffIndex] == -1 || totalSeconds < bestTimes[diffIndex]) {
            bestTimes[diffIndex] = totalSeconds;
        }
        saveRecords();
    }

    void loadRecords() {
        std::ifstream in("saper_records.txt");
        int easy = -1;
        int medium = -1;
        int hard = -1;
        if (in >> easy >> medium >> hard) {
            bestTimes[0] = easy >= 0 ? easy : -1;
            bestTimes[1] = medium >= 0 ? medium : -1;
            bestTimes[2] = hard >= 0 ? hard : -1;
        }
    }

    void saveRecords() const {
        std::ofstream out("saper_records.txt", std::ios::trunc);
        if (!out) return;
        out << bestTimes[0] << ' ' << bestTimes[1] << ' ' << bestTimes[2];
    }

    const char* diffText() const {
        if (mode_ == Difficulty::MEDIUM) return "medium";
        if (mode_ == Difficulty::HARD) return "hard";
        return "easy";
    }

    static const char* stateText(CellState s) {
        if (s == CellState::REVEALED) return "revealed";
        if (s == CellState::FLAGGED) return "flagged";
        return "hidden";
    }

    Difficulty mode_ = Difficulty::EASY;
};

GameBridge g_game;

std::wstring lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return s;
}

std::vector<std::wstring> split(const std::wstring& s, wchar_t delim) {
    std::vector<std::wstring> parts;
    size_t start = 0;
    while (start <= s.size()) {
        const size_t pos = s.find(delim, start);
        if (pos == std::wstring::npos) { parts.push_back(s.substr(start)); break; }
        parts.push_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return parts;
}

int toInt(const std::wstring& s, int fallback = -1) {
    try {
        size_t idx = 0;
        const int v = std::stoi(s, &idx);
        return idx == s.size() ? v : fallback;
    } catch (...) { return fallback; }
}

Difficulty parseDiff(const std::wstring& s) {
    const auto v = lower(s);
    if (v == L"medium") return Difficulty::MEDIUM;
    if (v == L"hard") return Difficulty::HARD;
    return Difficulty::EASY;
}

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), len);
    return w;
}

std::string loadTextResource(int id) {
    HRSRC resource = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!resource) return {};
    HGLOBAL loaded = LoadResource(nullptr, resource);
    if (!loaded) return {};
    const DWORD size = SizeofResource(nullptr, resource);
    if (size == 0) return {};
    const auto* data = reinterpret_cast<const char*>(LockResource(loaded));
    if (!data) return {};
    return std::string(data, data + size);
}

std::string replaceAll(std::string text, const std::string& from, const std::string& to) {
    if (from.empty()) return text;
    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
    return text;
}

std::string inlineScriptSafe(std::string script) {
    return replaceAll(std::move(script), "</script", "<\\/script");
}

std::string buildInlineHtml() {
    std::string html = loadTextResource(IDR_UI_INDEX_HTML);
    const std::string css = loadTextResource(IDR_UI_STYLES_CSS);
    const std::string app = inlineScriptSafe(loadTextResource(IDR_UI_APP_JS));
    const std::string anime = inlineScriptSafe(loadTextResource(IDR_UI_ANIME_JS));

    if (html.empty() || css.empty() || app.empty() || anime.empty()) {
        return "<!doctype html><html><body style='font-family:Segoe UI;padding:24px'>"
               "<h2>UI resources missing</h2><p>Rebuild the application.</p></body></html>";
    }

    const std::string styleTag = "<style>\n" + css + "\n</style>\n";
    const std::string scriptTags = "<script>\n" + anime + "\n</script>\n<script>\n" + app + "\n</script>\n";
    html = replaceAll(std::move(html), "<link rel=\"stylesheet\" href=\"styles.css\">", styleTag);

    const std::string externalScriptsLf = "<script src=\"vendor/anime.min.js\"></script>\n    <script src=\"app.js\"></script>";
    const std::string externalScriptsCrlf = "<script src=\"vendor/anime.min.js\"></script>\r\n    <script src=\"app.js\"></script>";
    if (html.find(externalScriptsLf) != std::string::npos) {
        html = replaceAll(std::move(html), externalScriptsLf, scriptTags);
    } else if (html.find(externalScriptsCrlf) != std::string::npos) {
        html = replaceAll(std::move(html), externalScriptsCrlf, scriptTags);
    } else {
        html = replaceAll(std::move(html), "</body>", scriptTags + "</body>");
    }
    return html;
}

void postState() {
    if (!g_webview) return;
    const std::wstring json = utf8ToWide(g_game.toJson());
    if (!json.empty()) g_webview->PostWebMessageAsJson(json.c_str());
}

void onCommand(const std::wstring& raw) {
    const auto parts = split(raw, L'|');
    if (parts.empty()) { postState(); return; }
    const auto cmd = lower(parts[0]);

    if (cmd == L"init") {
        g_game.setMode(parts.size() > 1 ? parseDiff(parts[1]) : Difficulty::EASY);
    } else if (cmd == L"difficulty" && parts.size() > 1) {
        g_game.setMode(parseDiff(parts[1]));
    } else if (cmd == L"restart") {
        g_game.restart();
    } else if (cmd == L"reveal" && parts.size() > 2) {
        g_game.reveal(toInt(parts[1]), toInt(parts[2]));
    } else if (cmd == L"flag" && parts.size() > 2) {
        g_game.flag(toInt(parts[1]), toInt(parts[2]));
    }

    postState();
}

void resizeWebview(HWND hwnd) {
    if (!g_controller) return;
    RECT r = {};
    GetClientRect(hwnd, &r);
    g_controller->put_Bounds(r);
}

class WebMessageReceivedHandler final : public ICoreWebView2WebMessageReceivedEventHandler {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** obj) override {
        if (!obj) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2WebMessageReceivedEventHandler) {
            *obj = static_cast<ICoreWebView2WebMessageReceivedEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        *obj = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&refCount_)); }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG count = static_cast<ULONG>(InterlockedDecrement(&refCount_));
        if (!count) delete this;
        return count;
    }

    HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) override {
        LPWSTR raw = nullptr;
        if (SUCCEEDED(args->TryGetWebMessageAsString(&raw)) && raw) {
            onCommand(raw);
            CoTaskMemFree(raw);
        } else {
            postState();
        }
        return S_OK;
    }

private:
    LONG refCount_ = 1;
};

class ControllerCompletedHandler final : public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler {
public:
    explicit ControllerCompletedHandler(HWND hwnd) : hwnd_(hwnd) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** obj) override {
        if (!obj) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler) {
            *obj = static_cast<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        *obj = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&refCount_)); }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG count = static_cast<ULONG>(InterlockedDecrement(&refCount_));
        if (!count) delete this;
        return count;
    }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT hr, ICoreWebView2Controller* ctrl) override {
        if (FAILED(hr) || !ctrl) return E_FAIL;
        g_controller = ctrl;
        g_controller->get_CoreWebView2(&g_webview);
        resizeWebview(hwnd_);

        ComPtr<ICoreWebView2Settings> settings;
        if (SUCCEEDED(g_webview->get_Settings(&settings)) && settings) {
            settings->put_AreDefaultContextMenusEnabled(FALSE);
            settings->put_IsStatusBarEnabled(FALSE);
            settings->put_IsZoomControlEnabled(FALSE);
        }

        EventRegistrationToken token{};
        g_webview->add_WebMessageReceived(new WebMessageReceivedHandler(), &token);

        const std::wstring html = utf8ToWide(buildInlineHtml());
        g_webview->NavigateToString(html.c_str());
        return S_OK;
    }

private:
    LONG refCount_ = 1;
    HWND hwnd_ = nullptr;
};

class EnvironmentCompletedHandler final : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler {
public:
    explicit EnvironmentCompletedHandler(HWND hwnd) : hwnd_(hwnd) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** obj) override {
        if (!obj) return E_POINTER;
        if (riid == IID_IUnknown || riid == IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler) {
            *obj = static_cast<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*>(this);
            AddRef();
            return S_OK;
        }
        *obj = nullptr;
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&refCount_)); }

    ULONG STDMETHODCALLTYPE Release() override {
        const ULONG count = static_cast<ULONG>(InterlockedDecrement(&refCount_));
        if (!count) delete this;
        return count;
    }

    HRESULT STDMETHODCALLTYPE Invoke(HRESULT hr, ICoreWebView2Environment* env) override {
        if (FAILED(hr) || !env) return E_FAIL;
        return env->CreateCoreWebView2Controller(hwnd_, new ControllerCompletedHandler(hwnd_));
    }

private:
    LONG refCount_ = 1;
    HWND hwnd_ = nullptr;
};

HRESULT initWebView(HWND hwnd) {
    using CreateEnvironmentWithOptionsFn = HRESULT(STDAPICALLTYPE*)(
        PCWSTR,
        PCWSTR,
        ICoreWebView2EnvironmentOptions*,
        ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);

    HMODULE loader = LoadLibraryW(L"WebView2Loader.dll");
    if (!loader) return HRESULT_FROM_WIN32(GetLastError());

    const auto createEnvironment = reinterpret_cast<CreateEnvironmentWithOptionsFn>(
        GetProcAddress(loader, "CreateCoreWebView2EnvironmentWithOptions"));
    if (!createEnvironment) return HRESULT_FROM_WIN32(GetLastError());

    return createEnvironment(
        nullptr, nullptr, nullptr,
        new EnvironmentCompletedHandler(hwnd));
}

LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: initWebView(hwnd); return 0;
        case WM_SIZE: resizeWebview(hwnd); return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
        default: return DefWindowProcW(hwnd, msg, wp, lp);
    }
}
}  // namespace

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow) {
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return -1;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kClassName;
    if (!RegisterClassExW(&wc)) return -1;

    HWND hwnd = CreateWindowExW(0, kClassName, kTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 980, 760, nullptr, nullptr, hInst, nullptr);
    if (!hwnd) return -1;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    CoUninitialize();
    return static_cast<int>(msg.wParam);
}
