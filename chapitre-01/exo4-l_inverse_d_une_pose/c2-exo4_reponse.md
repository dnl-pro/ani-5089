```cpp
#include <iostream>
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
    Vecteur r;

    r.x = (1 - 2*q.y*q.y - 2*q.z*q.z) * v.x
        + (2*q.x*q.y - 2*q.z*q.w) * v.y
        + (2*q.x*q.z + 2*q.y*q.w) * v.z;

    r.y = (2*q.x*q.y + 2*q.z*q.w) * v.x
        + (1 - 2*q.x*q.x - 2*q.z*q.z) * v.y
        + (2*q.y*q.z - 2*q.x*q.w) * v.z;

    r.z = (2*q.x*q.z - 2*q.y*q.w) * v.x
        + (2*q.y*q.z + 2*q.x*q.w) * v.y
        + (1 - 2*q.x*q.x - 2*q.y*q.y) * v.z;

    return r;
}

Vecteur appliquer(Pose pose, Vecteur point) {
    Vecteur r = tourner(pose.rotation, point);

    r.x += pose.position.x;
    r.y += pose.position.y;
    r.z += pose.position.z;

    return r;
}

Pose Inverser(Pose pose) {
    Quaternion q = {
        pose.rotation.w,
        -pose.rotation.x,
        -pose.rotation.y,
        -pose.rotation.z
    };

    Vecteur p = {
        -pose.position.x,
        -pose.position.y,
        -pose.position.z
    };

    p = tourner(q, p);

    return {p, q};
}

int main() {
    Pose pose;
    Vecteur point;

    cin >> pose.position.x >> pose.position.y >> pose.position.z;
    cin >> pose.rotation.w >> pose.rotation.x
        >> pose.rotation.y >> pose.rotation.z;
    cin >> point.x >> point.y >> point.z;

    Vecteur resultat = appliquer(pose, point);
    Vecteur retour = appliquer(Inverser(pose), resultat);

    cout << retour.x - point.x << " "
         << retour.y - point.y << " "
         << retour.z - point.z << endl;

    return 0;
}
```
