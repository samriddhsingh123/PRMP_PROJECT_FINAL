#include "argolib_dec.hpp"
using namespace std;
int fib(int n) {
    if(n<2) return n;
    int x, y;
    argolib::Task_handle task1 = argolib::fork([&]() { x = fib(n-1); });
    argolib::Task_handle task2 = argolib::fork([&]() { y = fib(n-2); });
    argolib::join(task1, task2);
    return x+y;
}
int main(int argc, char **argv) {
    argolib::init(argc, argv);
    int result;
    int n=40;
    argolib::kernel([&]() {
    result = fib(n);
    });
    cout<<"Fib of "<<n<< " = "<<result<<endl;
    argolib::finalize();
    return 0;
}