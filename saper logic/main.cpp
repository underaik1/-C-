#include "b.cpp" 

int main() {
    LogikaSapera igra;
    cout << "--- Генерируем поле после первого хода ---" << endl;
    igra.rasstanovkaMin(); 
    igra.podschetMin();
    cout << "Miny rasstavleny, oshibok net!" << endl;

    return 0;
}