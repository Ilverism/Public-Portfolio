/*
- External function call for use in GameMaker
- Optimal way of carrying velocities across the cape buffer
*/

#include <windows.h>

extern "C" __declspec(dllexport) double updateVelocities(double* velocitiesArray, double arraySizeIn, double reduceMulCur, double increaseMulCur) {

    double velocityPreceding;
    int arraySize = (int)arraySizeIn;

    const int updates = 1;
    for (int update = 0; update < updates; update++) {

      velocitiesArray[arraySize - 1] *= reduceMulCur;

      for (int i = arraySize - 1; i > 0; i--) {

        velocityPreceding = velocitiesArray[i - 1];
        velocitiesArray[i - 1] *= reduceMulCur;
        velocitiesArray[i] += (velocityPreceding * increaseMulCur);

        }

      }

    return 0.0;

    }