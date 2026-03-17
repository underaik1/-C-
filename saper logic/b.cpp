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
};