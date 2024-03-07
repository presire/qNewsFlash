#ifndef SEED_H
#define SEED_H


#include <QCoreApplication>
#include <random>
#include <cstdint>

#ifdef Q_OS_LINUX
#include <x86intrin.h>
#elif Q_OS_WIN
#include <intrin.h>
#endif


class Seed
{
private:
    uint64_t m_state[2];    // Xorshift用

private:
    unsigned long long getTSC();                         // CPUのタイムスタンプカウンタ(TSC)を取得
    unsigned long long hashTSC(unsigned long long tsc);  // CPUのタイムスタンプカウンタ(TSC)の値にハッシュ処理を施す

public:
    explicit Seed();
    virtual  ~Seed();
    uint64_t next();        // Xorshift用
};

#endif // SEED_H
