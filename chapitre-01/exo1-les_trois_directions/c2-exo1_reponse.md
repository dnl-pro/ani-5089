#include <iostream>
#include <iomanip>
using namespace std;

struct Vecteur {
    double x, y, z;
};

Vecteur Avant() {
    return {0, 0, 1};
}

Vecteur Haut() {
    return {0, 1, 0};
}

Vecteur Droite() {
    return {1, 0, 0};
}

double scalaire(Vecteur a, Vecteur b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

int main() {
    Vecteur point;

    cin >> point.x >> point.y >> point.z;

    cout << fixed << setprecision(4);

    cout << scalaire(point, Avant()) << endl;
    cout << scalaire(point, Haut()) << endl;
    cout << scalaire(point, Droite()) << endl;

    return 0;
}
