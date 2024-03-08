#ifndef RANDOMGENERATOR_H
#define RANDOMGENERATOR_H


#include <QCoreApplication>
#include <random>
#include <cstdint>

#ifdef Q_OS_LINUX
#include <x86intrin.h>
#elif Q_OS_WIN
#include <intrin.h>
#endif


class RandomGenerator
{
private:  // Variables
    uint64_t m_state[2];    // Xorshiftで使用

private:  // Methods
    unsigned long long getTSC();                         // CPUのタイムスタンプカウンタ(TSC)を取得
    unsigned long long hashTSC(unsigned long long tsc);  // CPUのタイムスタンプカウンタ(TSC)の値にハッシュ処理を施す
    uint64_t           next();                           // シード用Xorshift

public:   // Methods
    explicit RandomGenerator();
    virtual  ~RandomGenerator();
    int      Generate(int maxValue);                     // 乱数の生成 (一様分布)
};

#endif // RANDOMGENERATOR_H
