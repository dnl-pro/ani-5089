#include <iostream>
#include <cmath>
using namespace std;

struct Vec3 { double x, y, z; };
struct Quat { double w, x, y, z; };
struct Pose { Vec3 p; Quat q; };

Quat quatMul(const Quat& a, const Quat& b) {
    return {
        a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z,
        a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
        a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
        a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w
    };
}

Quat quatNormalize(const Quat& q) {
    double n = sqrt(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z);
    return { q.w/n, q.x/n, q.y/n, q.z/n };
}

Pose integratePose(const Pose& pose, const Vec3& v, const Vec3& w, double dt) {
    Vec3 p = {
        pose.p.x + v.x * dt,
        pose.p.y + v.y * dt,
        pose.p.z + v.z * dt
    };

    double wn = sqrt(w.x*w.x + w.y*w.y + w.z*w.z);
    Quat dq;

    if (wn < 1e-12) {
        dq = {1.0, 0.0, 0.0, 0.0};
    } else {
        double half = wn * dt / 2.0;
        double s = sin(half) / wn;
        dq = { cos(half), w.x * s, w.y * s, w.z * s };
    }

    Quat q = quatNormalize(quatMul(dq, pose.q));

    return { p, q };
}

int main() {
    Pose pose;
    cin >> pose.p.x >> pose.p.y >> pose.p.z;
    cin >> pose.q.w >> pose.q.x >> pose.q.y >> pose.q.z;

    Vec3 v, w;
    cin >> v.x >> v.y >> v.z;
    cin >> w.x >> w.y >> w.z;

    double dt;
    cin >> dt;

    Pose result = integratePose(pose, v, w, dt);

    cout << result.p.x << " " << result.p.y << " " << result.p.z << "\n";
    cout << result.q.w << " " << result.q.x << " " << result.q.y << " " << result.q.z << "\n";

    return 0;
}
