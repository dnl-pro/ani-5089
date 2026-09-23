```cpp
#include <iostream>
#include <cmath>
using namespace std;

struct Vecteur {
    double x, y, z;
};

struct Quaternion {
    double w, x, y, z;
};

struct Pose {
    Vecteur position;
    Quaternion rotation;
};

Vecteur tourner(Quaternion q, Vecteur v) {
    return {
        (1-2*q.y*q.y-2*q.z*q.z)*v.x
        +(2*q.x*q.y-2*q.z*q.w)*v.y
        +(2*q.x*q.z+2*q.y*q.w)*v.z,

        (2*q.x*q.y+2*q.z*q.w)*v.x
        +(1-2*q.x*q.x-2*q.z*q.z)*v.y
        +(2*q.y*q.z-2*q.x*q.w)*v.z,

        (2*q.x*q.z-2*q.y*q.w)*v.x
        +(2*q.y*q.z+2*q.x*q.w)*v.y
        +(1-2*q.x*q.x-2*q.y*q.y)*v.z
    };
}

Quaternion multiplier(Quaternion a, Quaternion b) {
    return {
        a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z,
        a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
        a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
        a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w
    };
}

Pose composer(Pose a, Pose b) {
    Vecteur p = tourner(a.rotation, b.position);

    p.x += a.position.x;
    p.y += a.position.y;
    p.z += a.position.z;

    return {p, multiplier(a.rotation, b.rotation)};
}

int main() {
    double bras, avantBras, angle;

    cin >> bras >> avantBras >> angle;

    Pose epaule = {{0,0,0}, {cos(angle/2),0,0,sin(angle/2)}};
    Pose coude = {{bras,0,0}, {1,0,0,0}};
    Pose main = {{avantBras,0,0}, {1,0,0,0}};

    Pose coudeMonde = composer(epaule, coude);
    Pose mainMonde = composer(coudeMonde, main);

    cout << coudeMonde.position.x << " "
         << coudeMonde.position.y << " "
         << coudeMonde.position.z << endl;

    cout << mainMonde.position.x << " "
         << mainMonde.position.y << " "
         << mainMonde.position.z << endl;

    return 0;
}
```
