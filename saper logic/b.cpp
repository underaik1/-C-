#include <iostream>
#include <vector>
#include <random>

using namespace std;

// ссостояния ячеек: скрыта, открыта или под флагом
enum class Sostoyanie { ZAKRITO, OTKRITO, FLAG };

// структура одной клетки поля
struct Kletka {
    bool estMina = false;
    int miniRyadom = 0;
    Sostoyanie status = Sostoyanie::ZAKRITO;
};

class LogikaSapera {
protected:
    // параметры поля 5 на 5, 3 мины
    inline static constexpr int RYADI = 5;
    inline static constexpr int KOLONKI = 5;
    inline static constexpr int MIN_V_IGRE = 3;

    int otkritieKletki = 0;
    bool proigrish = false;
    bool pobeda = false;
    bool perviyHod = true;

    vector<vector<Kletka>> pole;

public:
    LogikaSapera() {
        initField();
    }

    // инициализация или сброс поля
    void initField() {
        pole.assign(RYADI, vector<Kletka>(KOLONKI));
        otkritieKletki = 0;
        proigrish = false;
        pobeda = false;
        perviyHod = true;
    }
    // расстановка мин
    void rasstanovkaMin() {
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> disR(0, RYADI - 1);
        uniform_int_distribution<> disC(0, KOLONKI - 1);

        int rasstavleno = 0;
        while (rasstavleno < MIN_V_IGRE) {
            int r = disR(gen);
            int c = disC(gen);
            if (!pole[r][c].estMina) {
                pole[r][c].estMina = true;
                rasstavleno++;
            }
        }
    }

    // подсчет мин вокруг
    void podschetMin() {
        // перебор всех клеток поля
        for (int r = 0; r < RYADI; ++r) {
            for (int c = 0; c < KOLONKI; ++c) {
                if (pole[r][c].estMina) continue;
                int count = 0; // счетчик для текущей клетки
                // Проверка соседей
                for (int i = -1; i <= 1; ++i) {
                    for (int j = -1; j <= 1; ++j) {
                        int nr = r + i;
                        int nc = c + j;
                        // проверка границы поля
                        if (nr >= 0 && nr < RYADI && nc >= 0 && nc < KOLONKI) {
                            if (pole[nr][nc].estMina) {
                                count++;
                            }
                        }
                    }
                }
                pole[r][c].miniRyadom = count; // апись результа
            }
        }
    }
};
