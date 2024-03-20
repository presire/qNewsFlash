#ifndef RANDOMGENERATOR_H
#define RANDOMGENERATOR_H


#include <QCoreApplication>
#include <random>
#include <cstdint>

#ifdef Q_OS_LINUX
    #if defined(__x86_64__) || defined(__i386__)
    #include <x86intrin.h>
    #endif

    #ifdef ARM_NEON
    #include <arm_neon.h>
    #elif ARM_WMMX
    #include <mmintrin.h>
    #endif
#elif Q_OS_WIN
#include <intrin.h>
#endif


class RandomGenerator
{
private:  // Variables
    uint64_t m_state[2]{};    // Xorshiftで使用

private:  // Methods
    unsigned long long getTSC();                                // CPUのタイムスタンプカウンタ(TSC)を取得
    unsigned long long csprng();                                // システムが提供する暗号論的に安全な乱数生成器(CSPRNG)から値を取得
    static unsigned long long hashTSC(unsigned long long tsc);  // CPUのタイムスタンプカウンタ(TSC)の値にハッシュ処理を施す
    uint64_t           next();                                  // シード用Xorshift

public:   // Methods
    explicit RandomGenerator();
    virtual  ~RandomGenerator();
    int      Generate(int maxValue);                            // 乱数の生成 (一様分布)
};

#endif // RANDOMGENERATOR_H
