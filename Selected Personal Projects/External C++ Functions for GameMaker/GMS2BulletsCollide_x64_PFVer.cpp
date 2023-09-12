/*
- External function call for use in GameMaker
- More optimal way of fetching collisions between N rectangular projectiles
- Uses a simple grid partitioning scheme to reduce calculations
- Function call uses buffers to transfer data between GameMaker
*/

#define func extern "C" __declspec(dllexport)

#include <algorithm>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>

struct BulletData {
  double x0, y0, x1, y1, isActive, unitOwner;
  double bulletIndex;
};

const int CELL_SIZE = 160;

struct GridCell {
  std::vector<BulletData*> bullets;
};

using GridRow = std::unordered_map<int, GridCell>;
std::unordered_map<int, GridRow> grid;

int getGridX(double x) {
  return x / CELL_SIZE;
}

int getGridY(double y) {
  return y / CELL_SIZE;
}

void addBulletToGrid(BulletData& bullet) {
  int gx = getGridX(bullet.x0);
  int gy = getGridY(bullet.y0);
  grid[gx][gy].bullets.push_back(&bullet);
}

func double scr_entityGrid_bullets_collide(double* bulletBuffer, double* bulletCollisionsOut, double numBullets) {
  BulletData* bulletArray = reinterpret_cast<BulletData*>(bulletBuffer);

  // Reset grid
  grid.clear();

  // Populate the grid with bullets
  for (int i = 0; i < numBullets; ++i) {
    if (bulletArray[i].isActive) {
      addBulletToGrid(bulletArray[i]);
    }
  }

  // Estimate size and reserve space
  std::vector<double> collisionsList;
  collisionsList.reserve(numBullets * 2); // Assuming at most 2 collisions per bullet on average

  for (int i = 0; i < numBullets; ++i) {
    BulletData& currentBullet = bulletArray[i];
    if (!currentBullet.isActive) continue;

    int gx = getGridX(currentBullet.x0);
    int gy = getGridY(currentBullet.y0);

    // Check the current cell and neighboring cells
    for (int dx = -1; dx <= 1; dx++) {

      for (int dy = -1; dy <= 1; dy++) {

        auto rowIt = grid.find(gx + dx);
        if (rowIt == grid.end()) { continue; }

        auto cellIt = rowIt->second.find(gy + dy);
        if (cellIt == rowIt->second.end()) { continue; }

        GridCell& cell = cellIt->second;
        cell.bullets.reserve(2); // Assuming an average of 2 bullets per cell

        for (BulletData* targetBullet : cell.bullets) {

          if (targetBullet->bulletIndex <= currentBullet.bulletIndex) { continue; }
          if (!targetBullet->isActive) { continue; }

          if (
            currentBullet.unitOwner != targetBullet->unitOwner &&
            currentBullet.x1 >= targetBullet->x0 &&
            currentBullet.x0 <= targetBullet->x1 &&
            currentBullet.y1 >= targetBullet->y0 &&
            currentBullet.y0 <= targetBullet->y1
            ) {
            // Register the collision in the list
            collisionsList.push_back(currentBullet.bulletIndex);
            collisionsList.push_back(targetBullet->bulletIndex);

          }
        }
      }
    }
  }

  std::copy(collisionsList.begin(), collisionsList.end(), bulletCollisionsOut);
  return 0.0;
}
