const boardElement = document.getElementById("board");
const boardPanelElement = document.getElementById("board-panel");
const messageElement = document.getElementById("message");
const minesLeftElement = document.getElementById("mines-left");
const flagsCountElement = document.getElementById("flags-count");
const boardSizeElement = document.getElementById("board-size");
const modeSelectElement = document.getElementById("mode-select");
const difficultyTriggerElement = document.getElementById("difficulty-trigger");
const difficultyLabelElement = document.getElementById("difficulty-label");
const difficultyMenuElement = document.getElementById("difficulty-menu");
const difficultyItems = Array.from(document.querySelectorAll(".mode-item"));
const restartButton = document.getElementById("restart");

let state = null;
let previousRevealed = [];
let difficultyValue = "easy";
const difficultyTexts = { easy: "Простой", medium: "Средний", hard: "Сложный" };

// Отправляет команду в нативную часть через WebView2 bridge.
function sendCommand(command, ...args) {
    if (!(window.chrome && window.chrome.webview)) return;
    window.chrome.webview.postMessage([command, ...args].join("|"));
}

// Синхронизирует выбранную сложность в интерфейсе.
function setDifficultyUi(value) {
    difficultyValue = difficultyTexts[value] ? value : "easy";
    difficultyLabelElement.textContent = difficultyTexts[difficultyValue];
    difficultyItems.forEach((item) => {
        const selected = item.dataset.value === difficultyValue;
        item.classList.toggle("is-selected", selected);
        item.setAttribute("aria-selected", selected ? "true" : "false");
    });
}

// Открывает выпадающее меню выбора сложности.
function openModeMenu() {
    modeSelectElement.classList.add("is-open");
    difficultyMenuElement.hidden = false;
    difficultyTriggerElement.setAttribute("aria-expanded", "true");
}

// Закрывает меню выбора сложности.
function closeModeMenu() {
    modeSelectElement.classList.remove("is-open");
    difficultyMenuElement.hidden = true;
    difficultyTriggerElement.setAttribute("aria-expanded", "false");
}

// Переключает состояние меню сложности.
function toggleModeMenu() {
    if (difficultyMenuElement.hidden) openModeMenu();
    else closeModeMenu();
}

// Формирует карту уже открытых клеток для анимации только новых открытий.
function createRevealMap(rows, cols, sourceState) {
    return Array.from({ length: rows }, (_, row) =>
        Array.from({ length: cols }, (_, col) => sourceState.cells[row][col].state === "revealed"));
}

// Показывает текст статуса игры и подсвечивает тон сообщения.
function setMessage(text, tone = "") {
    messageElement.textContent = text;
    messageElement.classList.remove("state-win", "state-lose");
    if (tone === "win") messageElement.classList.add("state-win");
    if (tone === "lose") messageElement.classList.add("state-lose");
}

// Анимирует появление только что открытой клетки.
function animateReveal(node) {
    if (window.anime) {
        window.anime({
            targets: node,
            scale: [0.74, 1],
            opacity: [0.3, 1],
            duration: 220,
            easing: "easeOutQuad"
        });
    }
}

// Возвращает текст, который должен отображаться в клетке.
function getCellText(cell) {
    if (cell.state === "flagged") return "F";
    if (cell.state === "revealed" && cell.mine) return "*";
    return cell.state === "revealed" && cell.adjacent > 0 ? String(cell.adjacent) : "";
}

// Подбирает размер клеток под текущий размер панели, без скролла.
function updateBoardScale() {
    if (!state) return;
    const gap = 4;
    const width = boardPanelElement.clientWidth - 8;
    const height = boardPanelElement.clientHeight - 8;
    const fromWidth = Math.floor((width - (state.cols - 1) * gap) / state.cols);
    const fromHeight = Math.floor((height - (state.rows - 1) * gap) / state.rows);
    const size = Math.max(16, Math.min(40, fromWidth, fromHeight, 36));
    boardElement.style.setProperty("--cell-size", `${size}px`);
    boardElement.style.setProperty("--gap", `${gap}px`);
}

// Полностью перерисовывает поле по актуальному состоянию из C++.
function render(animateStart = false) {
    if (!state) return;
    boardElement.style.setProperty("--cols", state.cols);
    boardElement.innerHTML = "";
    updateBoardScale();

    for (let row = 0; row < state.rows; row += 1) {
        for (let col = 0; col < state.cols; col += 1) {
            const cell = state.cells[row][col];
            const button = document.createElement("button");
            button.type = "button";
            button.className = "cell";
            if (cell.state === "revealed") button.classList.add("revealed");
            if (cell.state === "flagged") button.classList.add("flagged");
            if (cell.mine && cell.state === "revealed") button.classList.add("mine");
            if (cell.wrongFlag) button.classList.add("wrong-flag");
            if (cell.state === "revealed" && cell.adjacent > 0) button.classList.add(`n${cell.adjacent}`);
            button.textContent = getCellText(cell);
            button.addEventListener("click", () => sendCommand("reveal", String(row), String(col)));
            button.addEventListener("contextmenu", (event) => {
                event.preventDefault();
                sendCommand("flag", String(row), String(col));
            });
            boardElement.appendChild(button);
            if (cell.state === "revealed" && !(previousRevealed[row] && previousRevealed[row][col])) {
                animateReveal(button);
            }
        }
    }

    if (animateStart && window.anime) {
        window.anime({
            targets: ".cell",
            opacity: [0, 1],
            scale: [0.95, 1],
            duration: 180,
            delay: window.anime.stagger(3),
            easing: "easeOutSine"
        });
    }

    previousRevealed = createRevealMap(state.rows, state.cols, state);
    minesLeftElement.textContent = String(state.minesCount - state.flagsCount);
    flagsCountElement.textContent = String(state.flagsCount);
    boardSizeElement.textContent = `${state.rows}x${state.cols}`;
    setDifficultyUi(state.difficulty || difficultyValue);

    if (state.gameWon) {
        setMessage("Победа. Все безопасные клетки открыты.", "win");
    } else if (state.gameOver) {
        setMessage("Поражение. Попали на мину.", "lose");
    } else {
        setMessage("ЛКМ: открыть клетку, ПКМ: поставить флаг.");
    }
}

// Принимает новое состояние игры и запускает рендер.
function receiveState(payload) {
    if (!payload || !payload.cells) return;
    const first = state === null;
    state = payload;
    if (first) previousRevealed = Array.from({ length: state.rows }, () => Array(state.cols).fill(false));
    render(first);
}

// Кнопка перезапуска текущей партии.
restartButton.addEventListener("click", () => sendCommand("restart"));

// Кнопка открытия/закрытия меню сложности.
difficultyTriggerElement.addEventListener("click", () => toggleModeMenu());

// Выбор конкретной сложности из выпадающего меню.
difficultyItems.forEach((item) => {
    item.addEventListener("click", () => {
        setDifficultyUi(item.dataset.value);
        closeModeMenu();
        sendCommand("difficulty", difficultyValue);
    });
});

// Закрывает меню, если клик был вне блока выбора сложности.
document.addEventListener("click", (event) => {
    if (!modeSelectElement.contains(event.target)) closeModeMenu();
});

// Закрывает меню по Esc.
document.addEventListener("keydown", (event) => {
    if (event.key === "Escape") closeModeMenu();
});

// Пересчитывает масштаб поля при изменении размера окна.
window.addEventListener("resize", () => {
    updateBoardScale();
});

setDifficultyUi(difficultyValue);

// Основной режим запуска: WebView2 доступен, слушаем сообщения из C++.
if (window.chrome && window.chrome.webview) {
    window.chrome.webview.addEventListener("message", (event) => {
        let payload = event.data;
        if (typeof payload === "string") {
            try { payload = JSON.parse(payload); } catch { return; }
        }
        receiveState(payload);
    });
    sendCommand("init", difficultyValue);
} else {
    // Режим отладки в браузере: сообщаем, что нужен запуск через .exe.
    setMessage("Интерфейс нужно запускать через WebView2 приложение.");
}
