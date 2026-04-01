#include <vector>
#include <random>

// Состояния клетки: закрыта, открыта или помечена флагом.
enum class CellState { HIDDEN, REVEALED, FLAGGED };

// Уровни сложности определяют размер поля и число мин.
enum class Difficulty { EASY, MEDIUM, HARD };

// Данные одной клетки поля.
struct Cell {
    bool hasMine = false;
    int adjacentMines = 0;
    CellState state = CellState::HIDDEN;
};

class MinesweeperCore {
protected:
    // Параметры поля (зависят от выбранной сложности).
    int rows;
    int cols;
    int minesCount;

    int revealedCellsCount = 0;
    bool isGameOver = false;
    bool isGameWon = false;
    bool isFirstMove = true;

    std::vector<std::vector<Cell>> grid;

public:
    // Создает игру с выбранной сложностью и пустым полем.
    MinesweeperCore(Difficulty level = Difficulty::EASY) {
        setDifficulty(level);
        initField();
    }

    // Применяет размеры поля и число мин для выбранного режима.
    void setDifficulty(Difficulty level) {
        switch (level) {
            case Difficulty::EASY:   rows = 8;  cols = 8;  minesCount = 10; break;
            case Difficulty::MEDIUM: rows = 12; cols = 12; minesCount = 25; break;
            case Difficulty::HARD:   rows = 16; cols = 16; minesCount = 40; break;
        }
    }

    // Полный сброс игрового состояния перед новой партией.
    void initField() {
        grid.assign(rows, std::vector<Cell>(cols));
        revealedCellsCount = 0;
        isGameOver = false;
        isGameWon = false;
        isFirstMove = true;
    }

    // Случайно расставляет мины по свободным клеткам.
    void placeMines() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> disR(0, rows - 1);
        std::uniform_int_distribution<> disC(0, cols - 1);

        int placedMines = 0;
        while (placedMines < minesCount) {
            int r = disR(gen);
            int c = disC(gen);
            if (!grid[r][c].hasMine) {
                grid[r][c].hasMine = true;
                placedMines++;
            }
        }
    }

    // Считает количество мин вокруг каждой безопасной клетки.
    void calculateAdjacentMines() {
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                if (grid[r][c].hasMine) continue;
                int count = 0;
                // Проверяем соседей в окне 3x3 с учетом границ поля.
                for (int i = -1; i <= 1; ++i) {
                    for (int j = -1; j <= 1; ++j) {
                        int nr = r + i, nc = c + j;
                        if (nr >= 0 && nr < rows && nc >= 0 && nc < cols) {
                            if (grid[nr][nc].hasMine) count++;
                        }
                    }
                }
                grid[r][c].adjacentMines = count;
            }
        }
    }

    // Открывает клетку, обрабатывает поражение/победу и автодорисовку пустот.
    void revealCell(int r, int c) {
        // Игнорируем выход за границы, завершенную игру и уже открытую клетку.
        if (r < 0 || r >= rows || c < 0 || c >= cols) return;
        if (isGameOver || isGameWon || grid[r][c].state != CellState::HIDDEN) return;

        // Мины ставим только на первом ходе.
        if (isFirstMove) {
            placeMines();
            calculateAdjacentMines();
            isFirstMove = false;
        }

        grid[r][c].state = CellState::REVEALED;

        // Открыли мину: игра окончена.
        if (grid[r][c].hasMine) {
            isGameOver = true;
            return;
        }

        revealedCellsCount++;

        // Если рядом нет мин, рекурсивно открываем соседние клетки.
        if (grid[r][c].adjacentMines == 0) {
            for (int i = -1; i <= 1; ++i) {
                for (int j = -1; j <= 1; ++j) {
                    if (i != 0 || j != 0) revealCell(r + i, c + j);
                }
            }
        }

        // Победа: открыты все клетки, кроме мин.
        if (revealedCellsCount == (rows * cols) - minesCount) {
            isGameWon = true;
        }
    }

    // Ставит или снимает флаг на закрытой клетке.
    void toggleFlag(int r, int c) {
        if (r < 0 || r >= rows || c < 0 || c >= cols || isGameOver || isGameWon) return;
        if (grid[r][c].state == CellState::REVEALED) return;
        grid[r][c].state = (grid[r][c].state == CellState::FLAGGED) ? CellState::HIDDEN : CellState::FLAGGED;
    }

    // Возвращает состояние конкретной клетки.
    const Cell& getCell(int r, int c) const { return grid[r][c]; }

    // Возвращает флаг проигрыша.
    bool getIsGameOver() const { return isGameOver; }

    // Возвращает флаг победы.
    bool getIsGameWon() const { return isGameWon; }

    // Возвращает количество строк поля.
    int getRows() const { return rows; }

    // Возвращает количество столбцов поля.
    int getCols() const { return cols; }
};
