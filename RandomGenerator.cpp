#include "RandomGenerator.h"

RandomGenerator::RandomGenerator()
{

}


RandomGenerator::~RandomGenerator()
{

}


// CPUのタイムスタンプカウンタ(TSC)を取得する
// TSCは、CPUが提供する高精度のタイマであり、プログラムの実行中に継続的に増加するカウンタの値を取得する
// 非常に高速であり、実行するたびに異なる値を提供する
unsigned long long RandomGenerator::getTSC()
{
    return __rdtsc();
}


// ハードウェアやOSに依存するAPIや関数を使用するため、移植性、セキュリティ、プライバシーに関する考慮が必要となる
// これらの値を直接シードとして使用するのではなく、ハッシュ処理を施した上で使用する
unsigned long long RandomGenerator::hashTSC(unsigned long long tsc)
{
    std::hash<unsigned long long> hasher;
    return hasher(tsc);
}


uint64_t RandomGenerator::next()
{
    uint64_t       x = m_state[0];
    const uint64_t y = m_state[1];

    m_state[0] = y;
    x ^= x << 23;                                // a
    m_state[1] = x ^ y ^ (x >> 17) ^ (y >> 26);  // b, c

    return m_state[1] + y;
}


int RandomGenerator::Generate(int maxValue)
{
    // CPUタイムスタンプカウンタの値を取得
    auto tsc = getTSC();

    // 取得したCPUタイムスタンプカウンタの値をハッシュ化
    auto hashedValue = hashTSC(tsc);

    m_state[0] = hashedValue;
    m_state[1] = hashedValue << 1;

    // ハッシュ化した値をXorshiftしてシード値を生成
    auto seed = next();

    // 生成したシード値を使用してメルセンヌツイスターを初期化
    std::mt19937 mt(seed);

    // 0から(取得した記事群の数 - 1)までの一様分布として乱数を生成
    std::uniform_int_distribution<int> dist(0, maxValue - 1);
    auto randomValue = dist(mt);

    return randomValue;
}
