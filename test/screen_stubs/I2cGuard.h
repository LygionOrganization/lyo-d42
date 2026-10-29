#pragma once
inline bool i2cAvailable=true;
struct I2cGuard { bool acquired; explicit I2cGuard(unsigned=20):acquired(i2cAvailable){} };
