#include <vector>
#include <random>

//состояния ячейки: скрыта, открыта или под флагом
enum class CellState { HIDDEN, REVEALED, FLAGGED };

// структура одной клетки поля
struct Cell {
    bool hasMine = false;
    int adjacentMines = 0;
    CellState state = CellState::HIDDEN;
};

class MinesweeperCore {
protected:
    // параметры поля (потом можно вынести в конструктор)
    inline static constexpr int ROWS = 5;
    inline static constexpr int COLS = 5;
    inline static constexpr int MINES_COUNT = 3;

    int revealedCellsCount = 0;
    bool isGameOver = false;
    bool isGameWon = false;
    bool isFirstMove = true;

    std::vector<std::vector<Cell>> grid;

public:
    MinesweeperCore() {
        initField();
    }

    // инициализация или сброс поля
    void initField() {
        grid.assign(ROWS, std::vector<Cell>(COLS));
        revealedCellsCount = 0;
        isGameOver = false;
        isGameWon = false;
        isFirstMove = true;
    }

    // расстановка мин
    void placeMines() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> disR(0, ROWS - 1);
        std::uniform_int_distribution<> disC(0, COLS - 1);

        int placedMines = 0;
        while (placedMines < MINES_COUNT) {
            int r = disR(gen);
            int c = disC(gen);
            
            if (!grid[r][c].hasMine) {
                grid[r][c].hasMine = true;
                placedMines++;
            }
        }
    }

    //подсчет мин вокруг
    void calculateAdjacentMines() {
        for (int r = 0; r < ROWS; ++r) {
            for (int c = 0; c < COLS; ++c) {
                if (grid[r][c].hasMine) continue;
                
                int count = 0;
                
                //проверка соседей (окно 3х3)
                for (int i = -1; i <= 1; ++i) {
                    for (int j = -1; j <= 1; ++j) {
                        int nr = r + i;
                        int nc = c + j;
                        
                        //проверка границ
                        if (nr >= 0 && nr < ROWS && nc >= 0 && nc < COLS) {
                            if (grid[nr][nc].hasMine) {
                                count++;
                            }
                        }
                    }
                }
                grid[r][c].adjacentMines = count;
            }
        }
    }
};