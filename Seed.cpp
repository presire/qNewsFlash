#include "Seed.h"

Seed::Seed()
{
    uint64_t tsc = getTSC();
    uint64_t hashedValue = hashTSC(tsc);
    m_state[0] = hashedValue;
    m_state[1] = hashedValue << 1;
}


Seed::~Seed()
{

}


// CPUのタイムスタンプカウンタ(TSC)を取得する
// TSCは、CPUが提供する高精度のタイマであり、プログラムの実行中に継続的に増加するカウンタの値を取得する
// 非常に高速であり、実行するたびに異なる値を提供する
unsigned long long Seed::getTSC()
{
    return __rdtsc();
}


// ハードウェアやOSに依存するAPIや関数を使用するため、移植性、セキュリティ、プライバシーに関する考慮が必要となる
// これらの値を直接シードとして使用するのではなく、ハッシュ処理を施した上で使用する
unsigned long long Seed::hashTSC(unsigned long long tsc)
{
    std::hash<unsigned long long> hasher;
    return hasher(tsc);
}


uint64_t Seed::next()
{
    uint64_t       x = m_state[0];
    const uint64_t y = m_state[1];

    m_state[0] = y;
    x ^= x << 23;                               // a
    m_state[1] = x ^ y ^ (x >> 17) ^ (y >> 26);   // b, c

    return m_state[1] + y;
}
