#include "Random.h"
#include <ctime>
#include <random>


static uint32_t s[4]; // State

static inline uint32_t rotl(const uint32_t x, int k)
{
  return (x << k) | (x >> (32 - k));
}

static uint32_t next(void)
{
  const uint32_t result = s[0] + s[3];

  const uint32_t t = s[1] << 9;

  s[2] ^= s[0];
  s[3] ^= s[1];
  s[1] ^= s[2];
  s[0] ^= s[3];

  s[2] ^= t;

  s[3] = rotl(s[3], 11);

  return result;
}

static inline double to_double(uint64_t x)
{
  union {
    uint64_t i;
    double d;
  } u;

  u.i = UINT64_C(0x3FF) << 52 | x >> 12;
  return u.d - 1.0;
}

static inline float to_float(uint32_t x)
{
  union {
    uint32_t i;
    float d;
  } u;

  u.i = UINT32_C(0x7F) << 23 | x >> 9;
  return u.d - 1.0f;
}

void rng::Init()
{
  // Init RNG using splitmix64
  std::srand((unsigned int)std::time(nullptr)); // use current time as seed for random generator
  uint64_t seed = ((uint64_t)std::rand() << 32) | std::rand();

  auto nextgen = [&]() {
    uint64_t z = (seed += 0x9e3779b97f4a7c15);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
    return z ^ (z >> 31);
  };

  uint64_t* ptr = (uint64_t*)s;
  ptr[0] = nextgen();
  ptr[1] = nextgen();
}

float rng::Float()
{
  return to_float(next());
}

int rng::Int()
{
  return next();
}
