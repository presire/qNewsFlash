// ARMまたはRISC-V64アーキテクチャの場合、/dev/urandomを使用するため
// ファイル操作と時間計測のヘッダーをインクルード
#if defined(ARM_NEON) || defined(ARM_WMMX) || (defined(__riscv) && (__riscv_xlen == 64))
    #include <chrono>
    #include <fstream>
#endif

#include <iostream>
#include "RandomGenerator.h"


RandomGenerator::RandomGenerator() = default;


RandomGenerator::~RandomGenerator() = default;


// シード値を取得
// アーキテクチャに応じて最適な方法でシード値を生成します
unsigned long long RandomGenerator::getTSC()
{
#if defined(__x86_64__) || defined(__i386__)
    // x86/x64アーキテクチャの場合
    // CPUのタイムスタンプカウンタ(TSC)を取得する
    // TSCは、CPUが提供する高精度のタイマであり、プログラムの実行中に継続的に増加するカウンタの値を取得する
    // 非常に高速であり、実行するたびに異なる値を提供する
    return __rdtsc();
#elif defined(ARM_NEON) || defined(ARM_WMMX) || (defined(__riscv) && (__riscv_xlen == 64))
    // ARMまたはRISC-V64アーキテクチャの場合
    // これらのアーキテクチャにはx86のような高速TSC命令がないため
    // システムが提供する暗号論的に安全な乱数生成器(CSPRNG)から値を取得
    return csprng();
#endif
}


#if defined(ARM_NEON) || defined(ARM_WMMX) || (defined(__riscv) && (__riscv_xlen == 64))
// ARMまたはRISC-V64アーキテクチャ用の乱数生成実装
// /dev/urandomからシード値を読み取ります
unsigned long long RandomGenerator::csprng()
{
    // /dev/urandomはLinuxカーネルが提供する暗号論的に安全な疑似乱数生成器
    // ブロックせずに高品質な乱数を提供します
    std::ifstream urandom("/dev/urandom", std::ios::in | std::ios::binary);
    if (!urandom) {
        // /dev/urandomのオープンに失敗した場合のフォールバック処理
        std::cerr << QString("警告 : /dev/urandomのオープンに失敗").toStdString() << std::endl;

        // 代替手段として、高解像度クロックの現在時刻を使用
        // std::chrono::high_resolution_clockの現在の時刻を取得
        auto now = std::chrono::high_resolution_clock::now();

        // 時刻を時間の原点からの経過時間 (duration) に変換
        auto duration = now.time_since_epoch();

        // 経過時間をナノ秒としてキャスト
        auto nano = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
        unsigned long long seed = static_cast<unsigned long long>(nano);

        return seed;
    }

    // /dev/urandomから8バイト（64ビット）のランダムデータを読み取る
    unsigned long long seed = 0;
    urandom.read(reinterpret_cast<char*>(&seed), sizeof(seed));
    urandom.close();

    return seed;
}
#endif


// ハードウェアやOSに依存するAPIや関数を使用するため、移植性、セキュリティ、プライバシーに関する考慮が必要となる
// これらの値を直接シードとして使用するのではなく、ハッシュ処理を施した上で使用する
// ハッシュ化することで、元の値から予測不可能な値を生成し、乱数の品質を向上させます
unsigned long long RandomGenerator::hashTSC(unsigned long long tsc)
{
    std::hash<unsigned long long> hasher;
    return hasher(tsc);
}


// Xorshift128+アルゴリズムを使用した高速な疑似乱数生成
// この関数は内部状態を更新しながら次の乱数を生成します
uint64_t RandomGenerator::next()
{
    uint64_t       x = m_state[0];
    const uint64_t y = m_state[1];

    m_state[0] = y;
    x ^= x << 23;                                // a
    m_state[1] = x ^ y ^ (x >> 17) ^ (y >> 26);  // b, c

    return m_state[1] + y;
}


// 0からmaxValue-1までの範囲で一様分布の乱数を生成する
// この関数は、アーキテクチャに依存しない高品質な乱数を提供します
int RandomGenerator::Generate(int maxValue)
{
    // ステップ1: アーキテクチャに応じた方法でシード値を取得
    // x86/x64ならTSC、ARM/RISC-V64なら/dev/urandomから取得
    auto tsc = getTSC();

    // ステップ2: 取得したシード値をハッシュ化
    // ハッシュ化により、元の値から予測困難な値を生成
    auto hashedValue = hashTSC(tsc);

    // ステップ3: Xorshift128+の内部状態を初期化
    // 2つの状態変数を異なる値で初期化することで、より良い乱数列を生成
    m_state[0] = hashedValue;
    m_state[1] = hashedValue << 1;

    // ステップ4: Xorshift128+を使用してシード値を生成
    auto seed = next();

    // ステップ5: メルセンヌツイスター（MT19937）を初期化
    // MT19937は高品質な疑似乱数生成器として広く使用されています
    std::mt19937 mt(seed);

    // ステップ6: 0から(maxValue - 1)までの一様分布として乱数を生成
    // std::uniform_int_distributionを使用することで、偏りのない乱数を得られます
    std::uniform_int_distribution<int> dist(0, maxValue - 1);
    auto randomValue = dist(mt);

    return randomValue;
}
