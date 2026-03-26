#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include <wrl.h>
#include <WebView2.h>

#include "../saper logic/b.cpp"

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace {
constexpr wchar_t kClassName[] = L"SaperMiniWindow";
constexpr wchar_t kTitle[] = L"Saper";
constexpr wchar_t kHost[] = L"saper.local";
ComPtr<ICoreWebView2Controller> g_controller;
ComPtr<ICoreWebView2> g_webview;

class GameBridge : public MinesweeperCore {
public:
    GameBridge() : MinesweeperCore(Difficulty::EASY) {}

    void setMode(Difficulty d) {
        mode_ = d;
        setDifficulty(d);
        initField();
    }

    void restart() { initField(); }

    void reveal(int row, int col) { revealCell(row, col); }

    void flag(int row, int col) { toggleFlag(row, col); }

    std::string toJson() const {
        std::ostringstream out;
        int flagsCount = 0;
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                if (grid[r][c].state == CellState::FLAGGED) ++flagsCount;
            }
        }

        out << '{'
            << "\"rows\":" << rows << ','
            << "\"cols\":" << cols << ','
            << "\"minesCount\":" << minesCount << ','
            << "\"flagsCount\":" << flagsCount << ','
            << "\"gameOver\":" << (isGameOver ? "true" : "false") << ','
            << "\"gameWon\":" << (isGameWon ? "true" : "false") << ','
            << "\"difficulty\":\"" << diffText() << "\","
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

std::wstring exeDir() {
    wchar_t path[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameW(nullptr, path, MAX_PATH);
    return len ? std::filesystem::path(std::wstring(path, len)).parent_path().wstring() : L".";
}

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
        g_game.toggleFlag(toInt(parts[1]), toInt(parts[2]));
    }

    postState();
}

void resizeWebview(HWND hwnd) {
    if (!g_controller) return;
    RECT r = {};
    GetClientRect(hwnd, &r);
    g_controller->put_Bounds(r);
}

HRESULT initWebView(HWND hwnd) {
    return CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hwnd](HRESULT hr, ICoreWebView2Environment* env) -> HRESULT {
                if (FAILED(hr) || !env) return E_FAIL;
                return env->CreateCoreWebView2Controller(
                    hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hwnd](HRESULT hr2, ICoreWebView2Controller* ctrl) -> HRESULT {
                            if (FAILED(hr2) || !ctrl) return E_FAIL;
                            g_controller = ctrl;
                            g_controller->get_CoreWebView2(&g_webview);
                            resizeWebview(hwnd);

                            ComPtr<ICoreWebView2Settings> settings;
                            if (SUCCEEDED(g_webview->get_Settings(&settings)) && settings) {
                                settings->put_AreDefaultContextMenusEnabled(FALSE);
                                settings->put_IsStatusBarEnabled(FALSE);
                                settings->put_IsZoomControlEnabled(FALSE);
                            }

                            EventRegistrationToken token{};
                            g_webview->add_WebMessageReceived(
                                Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                        LPWSTR raw = nullptr;
                                        if (SUCCEEDED(args->TryGetWebMessageAsString(&raw)) && raw) {
                                            onCommand(raw);
                                            CoTaskMemFree(raw);
                                        } else {
                                            postState();
                                        }
                                        return S_OK;
                                    }).Get(),
                                &token);

                            const auto uiPath = (std::filesystem::path(exeDir()) / "ui").wstring();
                            ComPtr<ICoreWebView2_3> webview3;
                            if (SUCCEEDED(g_webview.As(&webview3)) && webview3) {
                                webview3->SetVirtualHostNameToFolderMapping(
                                    kHost, uiPath.c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
                                g_webview->Navigate(L"https://saper.local/index.html");
                            }
                            return S_OK;
                        }).Get());
            }).Get());
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
