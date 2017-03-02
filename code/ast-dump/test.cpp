
void f(int x) {
  x += 1;
}

struct X {
  int g() {
    f(66666);
  }
};
