#pragma once

// 藤心基本設定
// 設定パターンによっては動かなくなるかもしれないので注意

#include <string>
#include <cfloat>
#include <cmath>
#include <algorithm>

// プロフィール
const std::string MY_NAME = "bmod";
const std::string MY_POL_NAME = "Sbmod";
const std::string MY_VERSION = "20190818";

// 教師用ビルドでは1スレッドでルートでの方策の利用はなし
#ifdef TEACHER

// 戦略設定

// 思考レベル(0~＋∞)
constexpr int THINKING_LEVEL = 40;

// 最大並列スレッド数
constexpr int N_THREADS = 1;

#ifdef USE_POLICY_TO_ROOT
#undef USE_POLICY_TO_ROOT
#endif

#else

// 戦略設定

// 思考レベル(0~＋∞)
constexpr int THINKING_LEVEL = 9;

// 最大並列スレッド数
constexpr int N_THREADS = 8;

#endif

// 末端報酬を階級リセットから何試合前まで計算するか
constexpr int N_REWARD_CALCULATED_GAMES = 32;

// 方策の計算設定
using policy_value_t = float;

// プレーヤー人数
#define N_NORMAL_PLAYERS (5)

namespace Settings {
    extern bool policyMode;
    extern int numPlayThreads;
    extern int numChangeThreads;
    extern int fixedSimulationCount;
    extern bool maximizePosition;
}

extern std::string DIRECTORY_PARAMS_IN;
extern std::string DIRECTORY_PARAMS_OUT;
extern std::string DIRECTORY_LOGS;

struct ConfigReader {
    ConfigReader(std::string cfile);
};
extern ConfigReader configReader;

#define Dice XorShift64

// ルール設定
#ifndef _PLAYERS
// 標準人数に設定
#define _PLAYERS (N_NORMAL_PLAYERS)
#endif
