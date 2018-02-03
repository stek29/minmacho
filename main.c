#include <unistd.h>

int main(void) {
  unsigned long hi = 0x646e617246206948;
  write(1, (char*)&hi, 8);
  return 0;
}
