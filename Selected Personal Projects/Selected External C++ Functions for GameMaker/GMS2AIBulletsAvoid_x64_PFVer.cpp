/*
- External function call for use in GameMaker
- Uses a 2D weighted vector to direct where the AI should move in order to avoid nearby rectangular projectiles
- In cases where bullets are moving directly towards the AI, they will attempt to dodge perpendicular to the bullet's path
- Nearer bullets are weighted more strongly than other ones
- Works so good that it's genuinely frustrating to try and land a hit on them
*/

#include <vector>
#include <cmath>
#include <algorithm>

#define func extern "C" __declspec(dllexport)

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct Bullet {
  double x0, y0, x1, y1, angle, speedX, speedY; // Rectangular bounds and speed of the bullet
};

struct Vector {
  double x, y;
  Vector(double x_ = 0, double y_ = 0) : x(x_), y(y_) {}
  void normalize() {
    double length = std::sqrt(x * x + y * y);
    if (length > 0) {
      x /= length;
      y /= length;
    }
  }
  Vector perpendicular() const { return Vector(-y, x); }
};

// Function to compute the closest point on the bullet's rectangle to the AI
Vector closestPointOnBullet(double ai_x, double ai_y, const Bullet& bullet) {
  double closestX = std::clamp(ai_x, bullet.x0, bullet.x1);
  double closestY = std::clamp(ai_y, bullet.y0, bullet.y1);
  return Vector(closestX, closestY);
}

// Function to compute the dot product of two vectors
double dotProduct(double x1, double y1, double x2, double y2) {
  return x1 * x2 + y1 * y2;
}

func double ai_movement_avoid_bullets(double* bulletBuffer, double numBullets, double ai_x, double ai_y) {

  if (numBullets == 0.0) {
    return 0.0;
  }

  double totalWeightedX = 0.0;
  double totalWeightedY = 0.0;

  Bullet* bullets = reinterpret_cast<Bullet*>(bulletBuffer);

  for (int i = 0; i < numBullets; ++i) {
    Bullet& bullet = bullets[i];

    // Compute the closest point on the bullet to the AI
    Vector closestPoint = closestPointOnBullet(ai_x, ai_y, bullet);

    double distanceX = ai_x - closestPoint.x;
    double distanceY = ai_y - closestPoint.y;
    double distance = std::sqrt(distanceX * distanceX + distanceY * distanceY);

    // Calculate weight for bullet
    double weight = 1.0 / (distance * distance + 1.0);

    double bulletActualAngle = -(bullet.angle) * M_PI / 180.0;
    double dirX = distanceX;
    double dirY = distanceY;

    double bulletToAIAngle = atan2(dirY, dirX);
    double angleDifference = std::fmod(bulletToAIAngle - bulletActualAngle + 3 * M_PI, 2 * M_PI) - M_PI;
    double angleDifferenceAbs = fabs(angleDifference);

    // Check if bullet is directly coming or close to AI
    if (angleDifferenceAbs < 0.5) {

      weight *= 10000.0;

      // Direction of bullet
      Vector bulletDirection(bullet.speedX, bullet.speedY);
      bulletDirection.normalize();

      // Calculate potential dodge directions
      Vector dodgePerpendicular1(-dirY, dirX);  // Perpendicular in one direction
      Vector dodgePerpendicular2(dirY, -dirX);  // Perpendicular in the other direction

      // Choose the dodge direction that's more against the bullet's direction
      if (dotProduct(bulletDirection.x, bulletDirection.y, dodgePerpendicular1.x, dodgePerpendicular1.y) < 
        dotProduct(bulletDirection.x, bulletDirection.y, dodgePerpendicular2.x, dodgePerpendicular2.y)) {
        dodgePerpendicular1.normalize();
        totalWeightedX += (dodgePerpendicular1.x + bulletDirection.x) * weight;  // Combine with opposite of bullet direction
        totalWeightedY += (dodgePerpendicular1.y + bulletDirection.y) * weight;
      } else {
        dodgePerpendicular2.normalize();
        totalWeightedX += (dodgePerpendicular2.x + bulletDirection.x) * weight;  // Combine with opposite of bullet direction
        totalWeightedY += (dodgePerpendicular2.y + bulletDirection.y) * weight;
      }

    }
    else {
      totalWeightedX += dirX * weight;
      totalWeightedY += dirY * weight;
    }
  }

  bulletBuffer[0] = -totalWeightedX;
  bulletBuffer[1] =  totalWeightedY;

  return 0.0;
}