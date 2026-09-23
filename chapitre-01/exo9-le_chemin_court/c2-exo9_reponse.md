#include <iostream>
#include <cmath>
using namespace std;

struct Vec3 { double x, y, z; };
struct Quat { double w, x, y, z; };

Quat quatMul(const Quat& a, const Quat& b) {
    return {
        a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z,
        a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
        a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
        a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w
    };
}

Quat quatConj(const Quat& q) {
    return { q.w, -q.x, -q.y, -q.z };
}

Vec3 angularVelocity(Quat q1, Quat q2, double dt, bool forceShortPath) {
    if (forceShortPath) {
        double dot = q1.w*q2.w + q1.x*q2.x + q1.y*q2.y + q1.z*q2.z;
        if (dot < 0.0) {
            q2.w = -q2.w; q2.x = -q2.x; q2.y = -q2.y; q2.z = -q2.z;
        }
    }

    Quat dq = quatMul(q2, quatConj(q1));

    double s = sqrt(dq.x*dq.x + dq.y*dq.y + dq.z*dq.z);
    double angle = 2.0 * atan2(s, dq.w);

    if (s < 1e-12) return {0.0, 0.0, 0.0};

    double k = angle / (s * dt);
    return { dq.x * k, dq.y * k, dq.z * k };
}

int main() {
    double dt = 0.01;
    double eps = 0.01;

    Quat q1 = {1.0, 0.0, 0.0, 0.0};
    Quat q2 = {-cos(eps/2.0), 0.0, 0.0, -sin(eps/2.0)};

    Vec3 forced = angularVelocity(q1, q2, dt, true);
    Vec3 unforced = angularVelocity(q1, q2, dt, false);

    cout << forced.x << " " << forced.y << " " << forced.z << "\n";
    cout << unforced.x << " " << unforced.y << " " << unforced.z << "\n";

    return 0;
}
