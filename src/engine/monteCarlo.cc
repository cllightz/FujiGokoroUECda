#include "../settings.h"
#include "estimation.hpp"
#include "simulation.hpp"
#include "monteCarlo.hpp"

using namespace std;

namespace Settings {
    const double valuePerClock = 5.0 / (THINKING_LEVEL * THINKING_LEVEL) / pow(10.0, 10);
    // 時間の価値(1秒あたり),3191は以前のPCのクロック周波数(/microsec)なので意味は無い
    const double valuePerSec = valuePerClock * 3191 * pow(10.0, 6); 
}

int selectBanditAction(const RootInfo& root, Dice& dice, std::vector<bool>& pruned, int *prunedCandidates) {
    // バンディット手法により次に試す行動を選ぶ
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
                    // printf("DEBUG PRUNE: !pruned[c]\n");
                    if(a[c].simulations >= HYPER_PARAM_PRUNE_SIMS_THRE){
                        // printf("DEBUG PRUNE: %d >= %d\n", a[c].simulations, HYPER_PARAM_PRUNE_SIMS_THRE);
                        // printf("DEBUG PRUNE: %f\n", tmpMean);
                        if(tmpMean < HYPER_PARAM_PRUNE_MEAN_THRE){
                            // printf("DEBUG PRUNE: %f < %f\n", tmpMean, HYPER_PARAM_PRUNE_MEAN_THRE);
                            if(tmpMean < worstScore){
                                // printf("DEBUG PRUNE: %f < %f\n", tmpMean, worstScore);
                                worstScore = tmpMean;
                                worstIndex = c;
                            }
                        }
                    }
                }
                // if(!pruned[c] && a[c].simulations >= HYPER_PARAM_PRUNE_SIMS_THRE && tmpMean < HYPER_PARAM_PRUNE_MEAN_THRE && tmpMean <>> worstScore){
                //     worstScore = tmpMean;
                //     worstIndex = c;
                // }
            }

            // 枝刈りが発生
            if(worstIndex >= 0){
                cout << "DEBUG PRUNE!: " << a[worstIndex].mean() << '\t' << a[worstIndex].simulations << endl;
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

bool finishCheck(const RootInfo& root, double simuTime, Dice& dice) {
    // Regretによる打ち切り判定

    const int candidates = root.candidates; // 候補数
    auto& child = root.child;
    double rewardScale = root.rewardGap;

    struct Dist { double mean, sem, reg; };
    double regretThreshold = 1600.0 * double(2 * simuTime * Settings::valuePerSec) / rewardScale;

    // regret check
    Dist d[N_MAX_MOVES];
    for (int i = 0; i < candidates; i++) {
        d[i] = {child[i].mean(), sqrt(child[i].mean_var()), 0};
    }
    for (int t = 0; t < 1600; t++) {
        double bestValue = -100;
        double value[N_MAX_MOVES];
        for (int i = 0; i < candidates; i++) {
            const Dist& td = d[i];
            std::normal_distribution<double> nd(td.mean, td.sem);
            value[i] = nd(dice);
            bestValue = max(bestValue, value[i]);
        }
        for (int i = 0; i < candidates; i++) {
            d[i].reg += bestValue - value[i];
        }
    }
    for (int i = 0; i < candidates; i++) {
        if (d[i].reg < regretThreshold) return true;
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
    std::vector<bool> pruned(proot->candidates, false);
    int prunedCandidates = 0;

    uint64_t simuTime = 0ULL; // プレイアウトと雑多な処理にかかった時間
    uint64_t estTime = 0ULL; // 局面推定にかかった時間

    // 諸々の準備が終わったので時間計測開始
    ClockMicS clock(0);

    while (!proot->exitFlag) { // 最大で最高回数までプレイアウトを繰り返す

        int world = 0;
        int action = selectBanditAction(*proot, dice, pruned, &prunedCandidates);          

        if (numSimulations[action] < numWorlds) {
            // まだ全ての世界でこの着手を検討していない
            world = numSimulations[action];
        } else if (numThreads * numWorlds + threadId < (int)worlds.size()) {
            // 新しい世界を作成
            simuTime += clock.restart();
            worlds[numWorlds] = estimator.create(DealType::REJECTION, record, *pshared, ptools);
            estTime += clock.restart();
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

        simuTime += clock.restart();

        // 終了判定
        if (Settings::fixedSimulationCount < 0
            && threadId == 0
            && numSimulationsSum % max(4, 32 / numThreads) == 0
            && proot->allSimulations > proot->candidates * 4) {
            if (finishCheck(*proot, double(simuTime) / pow(10, 6), dice)) {
                proot->exitFlag = 1;
                return;
            }
        }
    }
}
