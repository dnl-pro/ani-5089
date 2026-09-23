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

Vecteur appliquerPose(Pose pose, Vecteur point) {
    double w = pose.rotation.w;
    double x = pose.rotation.x;
    double y = pose.rotation.y;
    double z = pose.rotation.z;

    // Rotation du point par le quaternion
    double xx = x * x;
    double yy = y * y;
    double zz = z * z;

    Vecteur resultat;

    resultat.x = (1 - 2 * yy - 2 * zz) * point.x
               + (2 * x * y - 2 * z * w) * point.y
               + (2 * x * z + 2 * y * w) * point.z;

    resultat.y = (2 * x * y + 2 * z * w) * point.x
               + (1 - 2 * xx - 2 * zz) * point.y
               + (2 * y * z - 2 * x * w) * point.z;

    resultat.z = (2 * x * z - 2 * y * w) * point.x
               + (2 * y * z + 2 * x * w) * point.y
               + (1 - 2 * xx - 2 * yy) * point.z;

    // Translation
    resultat.x += pose.position.x;
    resultat.y += pose.position.y;
    resultat.z += pose.position.z;

    return resultat;
}

int main() {
    Pose pose;
    Vecteur point;

    cin >> pose.position.x >> pose.position.y >> pose.position.z;
    cin >> pose.rotation.w >> pose.rotation.x
        >> pose.rotation.y >> pose.rotation.z;
    cin >> point.x >> point.y >> point.z;

    Vecteur resultat = appliquerPose(pose, point);

    cout << resultat.x << " "
         << resultat.y << " "
         << resultat.z << endl;

    return 0;
}
```
