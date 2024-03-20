#if defined(ARM_NEON) || defined(ARM_WMMX)
#include <chrono>
#include <fstream>
#endif

#include <iostream>
#include "RandomGenerator.h"


RandomGenerator::RandomGenerator() = default;


RandomGenerator::~RandomGenerator() = default;


// シード値を取得
unsigned long long RandomGenerator::getTSC()
{
#if defined(__x86_64__) || defined(__i386__)
    // CPUのタイムスタンプカウンタ(TSC)を取得する
    // TSCは、CPUが提供する高精度のタイマであり、プログラムの実行中に継続的に増加するカウンタの値を取得する
    // 非常に高速であり、実行するたびに異なる値を提供する
    return __rdtsc();
#elif defined(ARM_NEON) || defined(ARM_WMMX)
    // システムが提供する暗号論的に安全な乱数生成器(CSPRNG)から値を取得
    return csprng();
#endif
}


#if defined(ARM_NEON) || defined(ARM_WMMX)
unsigned long long RandomGenerator::csprng()
{
    std::ifstream urandom("/dev/urandom", std::ios::in | std::ios::binary);
    if (!urandom) {
        std::cerr << QString("警告 : /dev/urandomのオープンに失敗").toStdString() << std::endl;

        // std::chrono::high_resolution_clockの現在の時刻を取得
        auto now = std::chrono::high_resolution_clock::now();

        // 時刻を時間の原点からの経過時間 (duration) に変換
        auto duration = now.time_since_epoch();

        // 経過時間ををナノ秒としてキャスト
        auto nano = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
        unsigned long long seed = static_cast<unsigned long long>(nano);

        return 0;
    }

    unsigned long long seed = 0;
    urandom.read(reinterpret_cast<char*>(&seed), sizeof(seed));
    urandom.close();

    return seed;
}
#endif


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
    // シード値を取得
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
