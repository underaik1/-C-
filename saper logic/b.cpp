#include <vector>
#include <random>
#include <chrono>
#include <string>

//состояния ячеек: скрыта, открыта или под флагом
enum class CellState { HIDDEN, REVEALED, FLAGGED };

//уровни сложности: меняют размер поля и количество мин
enum class Difficulty { EASY, MEDIUM, HARD };

//структура одной клетки поля
struct Cell {
    bool hasMine = false;
    int adjacentMines = 0;
    CellState state = CellState::HIDDEN;
};

class MinesweeperCore {
protected:
    //параметры поля (меняются в зависимости от сложности)
    int rows;
    int cols;
    int minesCount;

    int revealedCellsCount = 0;
    bool isGameOver = false;
    bool isGameWon = false;
    bool isFirstMove = true;

    std::vector<std::vector<Cell>> grid;

    //переменные для времени и рекордов
    Difficulty currentLevel; 
    int bestTimes[3]; // массив для хранения лучших результатов (в секундах)
    std::chrono::steady_clock::time_point startTime; //т очка отсчета времени

public:
    //конструктор с выбором сложности
    MinesweeperCore(Difficulty level = Difficulty::EASY) {
        // Заполняем рекорды маркерами "пусто" (-1)
        bestTimes[0] = -1;
        bestTimes[1] = -1;
        bestTimes[2] = -1;
        
        setDifficulty(level);
        initField();
    }

    //настройка параметров поля по уровню сложности
    void setDifficulty(Difficulty level) {
        currentLevel = level; // Запоминаем текущую сложность для таблицы
        switch (level) {
            case Difficulty::EASY:   rows = 8;  cols = 8;  minesCount = 10; break;
            case Difficulty::MEDIUM: rows = 12; cols = 12; minesCount = 25; break;
            case Difficulty::HARD:   rows = 16; cols = 16; minesCount = 40; break;
        }
    }

    //инициализация или сброс поля
    void initField() {
        grid.assign(rows, std::vector<Cell>(cols));
        revealedCellsCount = 0;
        isGameOver = false;
        isGameWon = false;
        isFirstMove = true;
    }

    //расстановка мин (с защитой первого клика)
    void placeMines(int startR, int startC) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> disR(0, rows - 1);
        std::uniform_int_distribution<> disC(0, cols - 1);

        int placedMines = 0;
        while (placedMines < minesCount) {
            int r = disR(gen);
            int c = disC(gen);
            
            //если мины тут нет и это не та клетка, куда мы нажали в первый раз
            if (!grid[r][c].hasMine && (r != startR || c != startC)) {
                grid[r][c].hasMine = true;
                placedMines++;
            }
        }
    }

    //подсчет мин вокруг каждой клетки(окно 3х3)
    void calculateAdjacentMines() {
        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                if (grid[r][c].hasMine) continue;
                int count = 0;
                //проверка соседей с защитой границ поля
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

    //основная логика: открытие клетки при нажатии
    void revealCell(int r, int c) {
        //проверка границ и состояния ячейки
        if (r < 0 || r >= rows || c < 0 || c >= cols) return;
        if (isGameOver || isGameWon || grid[r][c].state != CellState::HIDDEN) return;

        //расставляем мины только при первом ходе
        if (isFirstMove) {
            placeMines(r, c); //передаем координаты клика, чтобы туда не попала мина
            calculateAdjacentMines();
            isFirstMove = false;
            startTime = std::chrono::steady_clock::now(); // запускаем таймер
        }

        grid[r][c].state = CellState::REVEALED;

        //попал на мину — проигрыш
        if (grid[r][c].hasMine) {
            isGameOver = true;
            return;
        }

        revealedCellsCount++;

        //автооткрытие соседей, если рядом 0 мин
        if (grid[r][c].adjacentMines == 0) {
            for (int i = -1; i <= 1; ++i) {
                for (int j = -1; j <= 1; ++j) {
                    if (i != 0 || j != 0) revealCell(r + i, c + j);
                }
            }
        }

        //проверка на победу
        if (revealedCellsCount == (rows * cols) - minesCount) {
            isGameWon = true;
            
            //считаем время
            auto endTime = std::chrono::steady_clock::now();
            int totalSeconds = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count();
            
            int diffIndex = static_cast<int>(currentLevel);
            
            //если рекорда еще нет или мы побили старый
            if (bestTimes[diffIndex] == -1 || totalSeconds < bestTimes[diffIndex]) {
                bestTimes[diffIndex] = totalSeconds;
            }
        }
    }

    // поставить или снять флаг
    void toggleFlag(int r, int c) {
        if (r < 0 || r >= rows || c < 0 || c >= cols || isGameOver || isGameWon) return;
        if (grid[r][c].state == CellState::REVEALED) return;
        grid[r][c].state = (grid[r][c].state == CellState::FLAGGED) ? CellState::HIDDEN : CellState::FLAGGED;
    }

    //форматирование времени и вывод таблицы

    std::string formatTime(int totalSeconds) const {
        if (totalSeconds == -1) return "--:--"; 
        
        int mins = totalSeconds / 60;
        int secs = totalSeconds % 60;
        
        std::string timeStr = std::to_string(mins) + ":";
        if (secs < 10) timeStr += "0"; 
        timeStr += std::to_string(secs);
        
        return timeStr;
    }

    //готовый текст для отображения рекордов
    std::string getHighScoreTable() const {
        return "Лучшее время прохождения на уровне easy: " + formatTime(bestTimes[0]) + "\n" +
               "Лучшее время прохождения на уровне medium: " + formatTime(bestTimes[1]) + "\n" +
               "Лучшее время прохождения на уровне hard: " + formatTime(bestTimes[2]);
    }

    // геттеры для отображения в интерфейсе
    const Cell& getCell(int r, int c) const { return grid[r][c]; }
    bool getIsGameOver() const { return isGameOver; }
    bool getIsGameWon() const { return isGameWon; }
    int getRows() const { return rows; }
    int getCols() const { return cols; }
};