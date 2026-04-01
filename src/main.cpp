#include <windows.h>

#include <algorithm>
#include <cwctype>
#include <sstream>
#include <string>
#include <vector>

#include <wrl.h>
#include <WebView2.h>

#include "../saper logic/b.cpp"
#include "resource.h"

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

namespace {
constexpr wchar_t kClassName[] = L"SaperMiniWindow";
constexpr wchar_t kTitle[] = L"Saper";
ComPtr<ICoreWebView2Controller> g_controller;
ComPtr<ICoreWebView2> g_webview;

class GameBridge : public MinesweeperCore {
public:
    // Создает мост между UI и ядром игры со стартовой простой сложностью.
    GameBridge() : MinesweeperCore(Difficulty::EASY) {}

    // Меняет уровень сложности и полностью перезапускает поле.
    void setMode(Difficulty d) {
        mode_ = d;
        setDifficulty(d);
        initField();
    }

    // Перезапускает текущую игру без смены сложности.
    void restart() { initField(); }

    // Делегирует открытие клетки в игровое ядро.
    void reveal(int row, int col) { revealCell(row, col); }

    // Делегирует установку/снятие флага в игровое ядро.
    void flag(int row, int col) { toggleFlag(row, col); }

    // Сериализует текущее состояние игры в JSON для фронтенда.
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
    // Возвращает строковое имя текущей сложности для JSON.
    const char* diffText() const {
        if (mode_ == Difficulty::MEDIUM) return "medium";
        if (mode_ == Difficulty::HARD) return "hard";
        return "easy";
    }

    // Преобразует внутреннее состояние клетки в текстовый статус.
    static const char* stateText(CellState s) {
        if (s == CellState::REVEALED) return "revealed";
        if (s == CellState::FLAGGED) return "flagged";
        return "hidden";
    }

    Difficulty mode_ = Difficulty::EASY;
};

GameBridge g_game;

// Переводит строку в нижний регистр для устойчивого парсинга команд.
std::wstring lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return s;
}

// Делит строку по разделителю и возвращает все части.
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

// Безопасно превращает строку в число, иначе отдает fallback.
int toInt(const std::wstring& s, int fallback = -1) {
    try {
        size_t idx = 0;
        const int v = std::stoi(s, &idx);
        return idx == s.size() ? v : fallback;
    } catch (...) { return fallback; }
}

// Преобразует текст сложности из команды в enum.
Difficulty parseDiff(const std::wstring& s) {
    const auto v = lower(s);
    if (v == L"medium") return Difficulty::MEDIUM;
    if (v == L"hard") return Difficulty::HARD;
    return Difficulty::EASY;
}

// Конвертирует UTF-8 строку в UTF-16 для WinAPI/WebView2.
std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), len);
    return w;
}

// Читает текстовый ресурс, встроенный в .exe по идентификатору.
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

// Заменяет все вхождения подстроки в тексте.
std::string replaceAll(std::string text, const std::string& from, const std::string& to) {
    if (from.empty()) return text;
    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
    return text;
}

// Экранирует закрывающий </script> для безопасной встройки JS внутрь HTML.
std::string inlineScriptSafe(std::string script) {
    return replaceAll(std::move(script), "</script", "<\\/script");
}

// Собирает итоговый HTML с инлайн-CSS/JS из ресурсов.
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

// Отправляет текущее состояние игры в WebView как JSON-сообщение.
void postState() {
    if (!g_webview) return;
    const std::wstring json = utf8ToWide(g_game.toJson());
    if (!json.empty()) g_webview->PostWebMessageAsJson(json.c_str());
}

// Обрабатывает команду от UI и применяет ее к игре.
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

// Подгоняет размеры WebView под клиентскую область окна.
void resizeWebview(HWND hwnd) {
    if (!g_controller) return;
    RECT r = {};
    GetClientRect(hwnd, &r);
    g_controller->put_Bounds(r);
}

// Инициализирует WebView2, подписывает обработчики и загружает UI.
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

                            const std::wstring html = utf8ToWide(buildInlineHtml());
                            g_webview->NavigateToString(html.c_str());
                            return S_OK;
                        }).Get());
            }).Get());
}

// Оконная процедура WinAPI: маршрутизирует сообщения окна.
LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: initWebView(hwnd); return 0;
        case WM_SIZE: resizeWebview(hwnd); return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
        default: return DefWindowProcW(hwnd, msg, wp, lp);
    }
}
}  // namespace

// Точка входа приложения: COM, окно, цикл сообщений.
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
