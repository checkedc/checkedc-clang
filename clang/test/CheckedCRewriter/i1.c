// RUN: CConvertStandalone %s -- | wc -l | grep '^0$'

int *foo() {
  int x = 1;
  int y = 2;
  int z = 3;
  int *ret;
  for (int i = 0; i < 4; i++) {
    switch(i) {
    case 0:
      ret = &x;
      break;
    case 1:
      ret = &y;
      break;
    case 2:
      ret = &z;
      break;
    case 3:
      ret = (int *)5;
      break;
    }
  }
  return ret;
}
