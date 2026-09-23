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

Vecteur appliquer(Pose p, Vecteur v) {
    v = tourner(p.rotation, v);
    return {v.x+p.position.x, v.y+p.position.y, v.z+p.position.z};
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
    Quaternion q = multiplier(a.rotation, b.rotation);
    Vecteur p = tourner(a.rotation, b.position);

    p.x += a.position.x;
    p.y += a.position.y;
    p.z += a.position.z;

    return {p, q};
}

int main() {
    Pose a, b;
    Vecteur point;

    cin >> a.position.x >> a.position.y >> a.position.z;
    cin >> a.rotation.w >> a.rotation.x >> a.rotation.y >> a.rotation.z;

    cin >> b.position.x >> b.position.y >> b.position.z;
    cin >> b.rotation.w >> b.rotation.x >> b.rotation.y >> b.rotation.z;

    cin >> point.x >> point.y >> point.z;

    Pose c = composer(a, b);

    Vecteur p1 = appliquer(c, point);
    Vecteur p2 = appliquer(a, appliquer(b, point));

    cout << p1.x << " " << p1.y << " " << p1.z << endl;
    cout << p2.x << " " << p2.y << " " << p2.z << endl;

    cout << p1.x-p2.x << " "
         << p1.y-p2.y << " "
         << p1.z-p2.z << endl;

    return 0;
}
```
