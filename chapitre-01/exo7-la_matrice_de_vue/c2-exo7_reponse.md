#include <iostream>
#include <cmath>
using namespace std;

struct Vec3 { double x, y, z; };
struct Quat { double w, x, y, z; };
struct Pose { Vec3 p; Quat q; };

void quatToMat3(const Quat& q, double R[3][3]) {
    double w = q.w, x = q.x, y = q.y, z = q.z;

    R[0][0] = 1 - 2*y*y - 2*z*z;  R[0][1] = 2*x*y - 2*z*w;      R[0][2] = 2*x*z + 2*y*w;
    R[1][0] = 2*x*y + 2*z*w;      R[1][1] = 1 - 2*x*x - 2*z*z;  R[1][2] = 2*y*z - 2*x*w;
    R[2][0] = 2*x*z - 2*y*w;      R[2][1] = 2*y*z + 2*x*w;      R[2][2] = 1 - 2*x*x - 2*y*y;
}

Vec3 mulMat3Vec(const double R[3][3], const Vec3& v) {
    return {
        R[0][0]*v.x + R[0][1]*v.y + R[0][2]*v.z,
        R[1][0]*v.x + R[1][1]*v.y + R[1][2]*v.z,
        R[2][0]*v.x + R[2][1]*v.y + R[2][2]*v.z
    };
}

void poseToMatrix(const Pose& p, double M[4][4]) {
    double R[3][3];
    quatToMat3(p.q, R);

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) M[i][j] = R[i][j];
        M[i][3] = (&p.p.x)[i];
    }
    M[3][0] = M[3][1] = M[3][2] = 0;
    M[3][3] = 1;
}

void invertMatrix4(const double M[4][4], double out[4][4]) {
    double a[4][8];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            a[i][j] = M[i][j];
            a[i][j + 4] = (i == j);
        }

    for (int i = 0; i < 4; i++) {
        int piv = i;
        for (int j = i + 1; j < 4; j++)
            if (abs(a[j][i]) > abs(a[piv][i])) piv = j;

        if (abs(a[piv][i]) < 1e-12) {
            cout << "Matrice non inversible" << endl;
            return;
        }
        for (int j = 0; j < 8; j++) swap(a[i][j], a[piv][j]);

        double d = a[i][i];
        for (int j = 0; j < 8; j++) a[i][j] /= d;

        for (int k = 0; k < 4; k++) {
            if (k == i) continue;
            double f = a[k][i];
            for (int j = 0; j < 8; j++) a[k][j] -= f * a[i][j];
        }
    }

    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) out[i][j] = a[i][j + 4];
}

Pose invertPose(const Pose& p) {
    Quat qi = { p.q.w, -p.q.x, -p.q.y, -p.q.z };

    double Ri[3][3];
    quatToMat3(qi, Ri);
    Vec3 pi = mulMat3Vec(Ri, { -p.p.x, -p.p.y, -p.p.z });

    return { pi, qi };
}

int main() {
    Pose p;
    cin >> p.p.x >> p.p.y >> p.p.z;
    cin >> p.q.w >> p.q.x >> p.q.y >> p.q.z;

    double M[4][4], Minv[4][4];
    poseToMatrix(p, M);
    invertMatrix4(M, Minv);

    double Manalytic[4][4];
    poseToMatrix(invertPose(p), Manalytic);

    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            cout << Minv[i][j] - Manalytic[i][j] << (j == 3 ? '\n' : ' ');

    return 0;
}
