#include <bitset>
#include "../settings.h"
#include "estimation.hpp"
#include "simulation.hpp"
#include "monteCarlo.hpp"

namespace Settings {
    constexpr double valuePerClock = 5.0 / (THINKING_LEVEL * THINKING_LEVEL) / std::pow(10.0, 10);

    // 時間の価値(1秒あたり),3191は以前のPCのクロック周波数(/microsec)なので意味は無い
    constexpr double valuePerSec = valuePerClock * 3191 * std::pow(10.0, 6); 
}

// バンディット手法により次に試す行動を選ぶ
inline int selectBanditAction(const RootInfo& root, Dice& dice, std::bitset<N_MAX_MOVES>& pruned, int *prunedCandidates) {
    int actions = root.candidates;
    const auto& a = root.child;
    if (actions == 2) {
        // 2つの時は同数(分布サイズ単位)に割り振る
        if (a[0].size() == a[1].size()) return dice() % 2;
        else return a[0].size() < a[1].size() ? 0 : 1;
    } else {
        // 枝刈り処理
        constexpr int HYPER_PARAM_PRUNE_SIZE_THRE = 100;
        constexpr int HYPER_PARAM_PRUNE_CAND_MIN = 5;
        constexpr int HYPER_PARAM_PRUNE_SIMS_THRE = 50;
        constexpr double HYPER_PARAM_PRUNE_MEAN_THRE = 0.05;
        int index = 0;
        double allSize = root.monteCarloAllScore.size();
        if(allSize >= HYPER_PARAM_PRUNE_SIZE_THRE && actions - *prunedCandidates > HYPER_PARAM_PRUNE_CAND_MIN){
            // スコアが最低値の候補を検索
            double worstScore = DBL_MAX;
            int worstIndex = -1;
            for(int c = 0; c < actions; ++c){
                double tmpMean = a[c].mean();
                if(!pruned[c]){
                    if(a[c].simulations >= HYPER_PARAM_PRUNE_SIMS_THRE){
                        if(tmpMean < HYPER_PARAM_PRUNE_MEAN_THRE){
                            if(tmpMean < worstScore){
                                worstScore = tmpMean;
                                worstIndex = c;
                            }
                        }
                    }
                }
            }

            // 枝刈りが発生
            if(worstIndex >= 0){
                std::cout << "DEBUG PRUNE!: " << a[worstIndex].mean() << '\t' << a[worstIndex].simulations << endl;
                pruned[worstIndex] = true;
                prunedCandidates++;
            }
        }

        // 探索を進める候補を選ぶ
        double bestScore = -DBL_MAX;
        for(int c = 0; c < actions; ++c){
            if(!pruned[c]){
                // Thompson Sampling (報酬はベータ分布に従うと仮定)
                // バンディットcの推定報酬値をベータ分布に従って乱数で定める
                double tmpScore = a[c].monteCarloScore.rand(&dice);
                if(tmpScore > bestScore){
                    bestScore = tmpScore;
                    index = c;
                }
            }
        }
        return index;
    }
}

// Regretによる打ち切り判定
inline bool finishCheck(const RootInfo& root, double simuTime, Dice& dice) {
    const int candidates = root.candidates; // 候補数
    const auto& child = root.child;
    const double rewardScale = root.rewardGap;
    const double regretThreshold = 1600.0 * double(2 * simuTime * Settings::valuePerSec) / rewardScale;

    struct Dist {
        double mean; // 平均
        double sem; // 標準偏差
        double reg; // 損失
    };

    // 損失計算用
    std::array<Dist, N_MAX_MOVES> d;

    for (int i = 0; i < candidates; i++) {
        d[i] = {child[i].mean(), std::sqrt(child[i].mean_var()), 0};
    }
    for (int t = 0; t < 1600; t++) {
        double bestValue = -100;
        double value[N_MAX_MOVES];
        for (int i = 0; i < candidates; i++) {
            const Dist& td = d[i];
            std::normal_distribution<double> nd(td.mean, td.sem);
            value[i] = nd(dice);
            bestValue = std::max(bestValue, value[i]);
        }
        for (int i = 0; i < candidates; i++) {
            d[i].reg += bestValue - value[i];
        }
    }
    for (int i = 0; i < candidates; i++) {
        if (d[i].reg < regretThreshold) {
            return true;
        }
    }
    return false;
}

void MonteCarloThread(const int threadId, const int numThreads,
                      RootInfo *const proot, const Field *const pfield,
                      SharedData *const pshared, ThreadTools *const ptools) {
    const int myPlayerNum = proot->myPlayerNum;
    auto& dice = ptools->dice;

    int numSimulations[N_MAX_MOVES] = {0};
    int numSimulationsSum = 0;

    int numWorlds = 0; // 作成した世界の数
    std::array<World, 128> worlds;

    // 世界生成のためのクラスを初期化
    const auto& record = pshared->record.latestGame();
    RandomDealer estimator(*pfield, myPlayerNum);

    Field pf = *pfield;
    pf.myPlayerNum = -1; // 客観視点に変更
    pf.addAttractedPlayer(myPlayerNum);
    pf.setMoveBuffer(ptools->mbuf);
    if (proot->rivalPlayerNum >= 0) {
        pf.attractedPlayers.set(proot->rivalPlayerNum);
    }

    // 枝刈り用
    std::bitset<N_MAX_MOVES> pruned;
    int prunedCandidates = 0;

    uint64_t simuTime = 0ULL; // プレイアウトと雑多な処理にかかった時間
    uint64_t estTime = 0ULL; // 局面推定にかかった時間

    // 諸々の準備が終わったので時間計測開始（スレッド0でのみ計測を行う）
    ClockMicS clock;
    if (threadId == 0) {
        clock.start();
    }

    for (int i = 0; !proot->exitFlag; i++) { // 最大で最高回数までプレイアウトを繰り返す
        // 使用する世界の番号
        int world = 0;
        int action = selectBanditAction(*proot, dice, pruned, &prunedCandidates);          

        if (numSimulations[action] < numWorlds) {
            // まだ全ての世界でこの着手を検討していない
            world = numSimulations[action];
        } else if (numThreads * numWorlds + threadId < (int)worlds.size()) {
            // まだ十分な数だけ世界を生成していない場合、新しい世界を作成する

            if (threadId == 0) {
                simuTime += clock.restart();
                worlds[numWorlds] = estimator.create(DealType::REJECTION, record, *pshared, ptools);
                estTime += clock.restart();
            } else {
                worlds[numWorlds] = estimator.create(DealType::REJECTION, record, *pshared, ptools);
            }

            world = numWorlds++;
        } else {
            // ランダム選択
            world = dice() % numWorlds;
        }

        numSimulations[action]++;
        numSimulationsSum++;

        // シミュレーション実行
        Field f;
        copyField(pf, &f);
        setWorld(worlds[world], &f);
        if (proot->isChange) {
            startChangeSimulation(f, myPlayerNum, proot->child[action].changeCards, pshared, ptools);
        } else {
            startPlaySimulation(f, proot->child[action].move, pshared, ptools);
        }

        proot->feedSimulationResult(action, f, pshared); // 結果をセット(排他制御は関数内で)
        if (proot->exitFlag) return;

        // 終了判定
        if (threadId == 0) {
            simuTime += clock.restart();

            if (Settings::fixedSimulationCount < 0
                && numSimulationsSum % std::max(4, 32 / numThreads) == 0
                && proot->allSimulations > proot->candidates * 4) {
                if (finishCheck(*proot, double(simuTime) / std::pow(10, 6), dice)) {
                    proot->exitFlag = true;
                    return;
                }
            }
        }
    }
}
