```cpp
#include <iostream>
using namespace std;

struct V{double x,y,z;};
struct Q{double w,x,y,z;};
struct Pose{V p;Q q;};

V r(Pose a,V v){
 double w=a.q.w,x=a.q.x,y=a.q.y,z=a.q.z;
 return {(1-2*y*y-2*z*z)*v.x+(2*x*y-2*z*w)*v.y+(2*x*z+2*y*w)*v.z+a.p.x,
 (2*x*y+2*z*w)*v.x+(1-2*x*x-2*z*z)*v.y+(2*y*z-2*x*w)*v.z+a.p.y,
 (2*x*z-2*y*w)*v.x+(2*y*z+2*x*w)*v.y+(1-2*x*x-2*y*y)*v.z+a.p.z};
}

V t(Pose a,V v){
 v.x+=a.p.x;v.y+=a.p.y;v.z+=a.p.z;
 a.p={0,0,0};
 return r(a,v);
}

int main(){
 Pose a;V v;
 cin>>a.p.x>>a.p.y>>a.p.z>>a.q.w>>a.q.x>>a.q.y>>a.q.z>>v.x>>v.y>>v.z;
 V x=r(a,v),y=t(a,v);
 cout<<x.x<<" "<<x.y<<" "<<x.z<<"\n"<<y.x<<" "<<y.y<<" "<<y.z<<"\n";
}
```
