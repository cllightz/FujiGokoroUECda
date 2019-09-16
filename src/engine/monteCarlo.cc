#include <bitset>
#include "../settings.h"
#include "estimation.hpp"
#include "simulation.hpp"
#include "monteCarlo.hpp"

namespace Settings {
    constexpr double valuePerClock = 5.0 / (THINKING_LEVEL * THINKING_LEVEL) / std::pow(10.0, 10);

    // 時間の価値(1秒あたり),3191は以前のPCのクロック周波数(/microsec)なので意味は無い
    constexpr double valuePerSec = valuePerClock * 3191 * std::pow(10, 6); 
}

void MonteCarloThread(const int threadId, const int numThreads,
                      RootInfo *const proot, const Field *const pfield,
                      SharedData *const pshared, ThreadTools *const ptools) {
    const int myPlayerNum = proot->myPlayerNum;
    auto& dice = ptools->dice;

    // 腕ごとのプレイアウト回数
    // TODO: intじゃなくてもいいか？
    std::array<int, N_MAX_MOVES> numSimulations = {0};
    
    // 全腕の総プレイアウト回数
    int numSimulationsSum = 0;

    // 作成した世界の数
    int numWorlds = 0;

    // プールした生成済みの世界
    std::array<World, 128> worlds;

    #pragma region 世界生成のためのデータを初期化
    // 今の試合のプレイ履歴
    const auto& record = pshared->record.latestGame();

    // 世界生成器(手札推定器)
    RandomDealer estimator(*pfield, myPlayerNum);
    #pragma endregion 世界生成のためのデータを初期化

    // 現在の盤面情報の複製
    Field pf = *pfield;

    // 客観視点に変更
    pf.myPlayerNum = -1;
    pf.addAttractedPlayer(myPlayerNum);
    pf.setMoveBuffer(ptools->mbuf);
    if (proot->rivalPlayerNum >= 0) {
        pf.attractedPlayers.set(proot->rivalPlayerNum);
    }

    // 枝刈り用
    std::bitset<N_MAX_MOVES> pruned;
    int prunedCandidates = 0;

    // 各腕に同じ世界を選択するためのカウンタ
    int worldSelectingCounter = 0;

    // 使用する世界の番号
    int world = -1;

    // 腕の削除の間隔を管理するカウンタ
    int pruningCounter = 0;
    constexpr int pruningInterval = 10;

    // 選択する腕のインデックス
    int action = -1;

    uint64_t simuTime = 0ULL; // プレイアウトと雑多な処理にかかった時間
    uint64_t estTime = 0ULL; // 局面推定にかかった時間

    // 諸々の準備が終わったので時間計測開始（スレッド0でのみ計測を行う）
    ClockMicS clock;
    if (threadId == 0) {
        clock.start();
    }

    // TODO: 時間計測ロジックを排除できれば、もっと綺麗に書ける
    for (int loop = 0; !proot->exitFlag; loop++) { // 最大で最高回数までプレイアウトを繰り返す
        #pragma region 着手の選択
        const auto& a = proot->child;

        if (proot->candidates == 2) {
            // 2つの時は同数(分布サイズ単位)に割り振る
            action = loop % 2;
        } else {
            if (pruningCounter >= pruningInterval) {
                pruningCounter = 0;

                // 最も μ^-3σ^ が高い腕を検索
                double bestScore = -DBL_MAX;
                for (int c = 0; c < proot->candidates; c++) {
                    if (!pruned[c]) {
                        double tmpScore = a[c].mean() - 3 * std::sqrt(a[c].mean_var());
                        if (tmpScore > bestScore) {
                            bestScore = tmpScore;
                        }
                    }
                }

                // μ^+3σ^ < μ^*-3σ^* となる腕を枝刈り
                for(int c = 0; c < proot->candidates; c++){
                    if (!pruned[c]) {
                        double tmpScore = a[c].mean() + 3 * std::sqrt(a[c].mean_var());
                        if (tmpScore < bestScore) {
                            // 枝刈りが発生
                            if (threadId == 1) {
                                std::cout << "DEBUG_PRUNE" << std::endl;
                            }
                            pruned[c] = true;
                            prunedCandidates++;
                        }
                    }
                }
            }

            // 次に探索する腕の決定
            while (true) {
                action++;
                action %= proot->candidates;
                if (!pruned[action]) {
                    break;
                }
            }
        }
        #pragma endregion 着手の選択
        
        #pragma region 世界の生成(手札推定)
        if (worldSelectingCounter == 0) {
            if (numThreads * numWorlds < (int)worlds.size()) {
                // まだ十分な数だけ世界を生成していない場合、新しい世界を作成する

                if (threadId == 0) {
                    simuTime += clock.restart();
                    worlds[numWorlds] = estimator.create(DealType::REJECTION, record, *pshared, ptools);
                    estTime += clock.restart();
                } else {
                    worlds[numWorlds] = estimator.create(DealType::REJECTION, record, *pshared, ptools);
                }

                numWorlds++;
            }

            world++;
            world %= numWorlds;
            pruningCounter++;
        }

        worldSelectingCounter++;

        if (worldSelectingCounter >= proot->candidates - prunedCandidates) {
            // 世界を固定して全腕に対して1回ずつ試行したら世界を生成する
            worldSelectingCounter = 0;
        }
        #pragma endregion 世界の生成(手札推定)

        #pragma region シミュレーション実行
        Field f;
        copyField(pf, &f);
        setWorld(worlds[world], &f);
        if (proot->isChange) {
            startChangeSimulation(f, myPlayerNum, proot->child[action].changeCards, pshared, ptools);
        } else {
            startPlaySimulation(f, proot->child[action].move, pshared, ptools);
        }

        proot->feedSimulationResult(action, f, pshared); // 結果をセット(排他制御は関数内で)
        numSimulations[action]++;
        numSimulationsSum++;
        #pragma endregion シミュレーション実行

        #pragma region Regretによる打ち切り判定
        // 終了判定はスレッド0でのみ判定する
        if (threadId == 0) {
            simuTime += clock.restart();

            if (Settings::fixedSimulationCount < 0 // プレイアウト回数が固定値で指定されていないこと
                && worldSelectingCounter == 0 // あまりにも頻繁に終了判定しないように
                && proot->allSimulations > proot->candidates * 4) { // 腕の数の4倍以上プレイアウトしていること
                auto uSecSimuTime = simuTime / std::pow(10, 6);
                
                const double rewardScale = proot->rewardGap;
                constexpr int tryCount = 1600;
                const double regretThreshold = tryCount * double(2 * uSecSimuTime * Settings::valuePerSec) / rewardScale;

                struct Dist {
                    double mean; // 平均
                    double sem; // 標準偏差
                    double reg; // 損失
                };

                // 腕毎の損失のバッファ
                std::array<Dist, N_MAX_MOVES> d;

                // 腕毎の損失のバッファの初期化
                for (int i = 0; i < proot->candidates; i++) {
                    d[i] = {
                        proot->child[i].mean(),
                        std::sqrt(proot->child[i].mean_var()),
                        0
                    };
                }

                // TODO: tryCount回ループさせなくても損失は求まりそう
                // TODO: ループさせるにしてもfor文の構造を工夫すればバッファの配列は不要そう
                for (int t = 0; t < tryCount; t++) {
                    // 経験最高報酬
                    double bestValue = -100;

                    // 腕毎の評価値のバッファ
                    std::array<double, N_MAX_MOVES> value;

                    // 全腕に対して腕を引いてみる
                    for (int i = 0; i < proot->candidates; i++) {
                        // 正規分布に従う乱数生成器
                        std::normal_distribution<double> nd(d[i].mean, d[i].sem);

                        // 腕を引いた結果
                        value[i] = nd(dice);

                        // 経験最高報酬の更新
                        bestValue = std::max(bestValue, value[i]);
                    }

                    // 全腕の損失を加算
                    for (int i = 0; i < proot->candidates; i++) {
                        d[i].reg += bestValue - value[i];
                    }
                }

                // 損失が大きい場合は十分に最適腕が求まったとして終了させる
                for (int i = 0; i < proot->candidates; i++) {
                    if (d[i].reg < regretThreshold) {
                        proot->exitFlag = true;
                        return;
                    }
                }
            }
        }
        #pragma endregion Regretによる打ち切り判定
    }

    if (threadId == 1)
    {
        for (auto i = 0; i < proot->candidates; i++) {
            std::cout << "DEBUG_THREAD_1," << pshared->record.getLatestGameNum() << ',' << pf.turnCount() << ',' << numSimulationsSum << ',' << i << ',' << proot->child[i].mean() << ',' << std::sqrt(proot->child[i].mean_var()) << std::endl;
        }
    }
}
