#pragma once

#include <cassert>

namespace UECda {
    
    // ハッシュ値生成
    // とりあえず生成関数を毎回呼べば動くだろうが、
    // １からの計算は出来る限り避けるように
    
    /**************************カード整数**************************/
    
    constexpr uint64_t cardsHashKeyTable[64] = {
        // インデックスがIntCard番号に対応
        // generated by SFMT
        0x15cc5ec4cae423e2, 0xa1373ceae861f22a, 0x7b60ee1280de0951, 0x970b602e9f0a831a,
        0x9c2d0e84fa38fd7b, 0xf8e8f5de24c6613c, 0x59e1e0ec5c2dcf0f, 0xee5236f6cc5ecd7c,
        0x955cdae1107b0a6f, 0x664c969fef782110, 0x131d24cfbc6cc542, 0x4747206ff1446e2c,
        0x02ea232067f62eec, 0xb53c73b144873900, 0x62623a5213bdae74, 0x655c7b3f43d2ea77,
        0x4e4f49ed97504cd0, 0x62e37cdd7416e4d1, 0x8d82596514f50486, 0x85eeb4e5f361ad26,
        0xca376df878e7b568, 0x012caaf9c1c68d82, 0x9ae28611b76ac1d6, 0xe8b42904d7ac4688,
        0x50ebe782f7343538, 0xf2876e2b5a0d5da5, 0xf308e93cd29a1fb5, 0x3e58ae2a9e1fb64a,
        0x143a9b63f5128d58, 0xd7e31ea845745bf5, 0xcc59315a5031ae64, 0x77591890cfbe493a,
        0xea239dd1932bfc0b, 0xbb4a9b581dc50a58, 0xd7640b6cb72a9798, 0x537b3fcac53dcefc,
        0xa52fb140c73cc931, 0xd123cf73f9aab466, 0x6eed725d80ead216, 0x151b7aa1f03f0532,
        0xfba74ec660ed2e46, 0x8aa22769ccf87343, 0x1896000f642b41ac, 0x97b0de139c5b487c,
        0x20a10996d700d1b3, 0x76e8529b3f3d425e, 0xf48b294add39ea07, 0x1abdb74a2202e8ea,
        0xff502998b9aed7e7, 0x6629aa61eb40d7e0, 0x87e72aef918d27b7, 0xf1b25fdda49f70bd,
        0xb10abbc401dd1e03, 0xb9ddbad67b370949, 0xefa07417c6906e38, 0x1616cec390c9db9f,
        0xb048124c6ef48ff5, 0x65978b47dbc1debb, 0x925e60277ee19bbf, 0xed776c6b664087e8,
        0x29bf249af2b02a7b, 0xc64ed74ce9ea7c77, 0xc05774752bed93f3, 0x5fc31db82af16d07,
    };
    inline uint64_t IntCardToHashKey(IntCard ic) {
        assert(0 <= ic && ic < 64);
        return cardsHashKeyTable[ic];
    }
    
    /**************************カード集合**************************/
    
    // 一枚一枚に乱数をあてたゾブリストハッシュ値を用いる
    // 線形のため合成や進行が楽
    // 一方0からの計算は枚数に対して線形時間なので末端処理では厳禁
    
    constexpr uint64_t HASH_CARDS_NULL = 0ULL;
    constexpr uint64_t HASH_CARDS_ALL = 0xe59ef9b1d4fe1c44ULL; // 先に計算してある

    inline uint64_t CardsToHashKey(Cards c) {
        uint64_t key = HASH_CARDS_NULL;
        for (IntCard ic : c) key ^= IntCardToHashKey(ic);
        return key;
    }

    /**************************複数のカード集合**************************/
    
    // 交換不可能なもの
    
    // 支配保証判定など、複数のカード集合間に成立する関係を保存する場合に使うかも
    inline uint64_t CardsCardsToHashKey(Cards c0, Cards c1) {
        return cross64(CardsToHashKey(c0), CardsToHashKey(c1));
    }
    inline uint64_t knitCardsCardsHashKey(uint64_t key0, uint64_t key1) {
        return cross64(key0, key1);
    }
    
    // 交換可能なもの
    // 最小のハッシュ値を採用すれば良いだろう
    
    /**************************場**************************/
    
    // 場役の情報 + 誰がパスをしたかの情報
    
    // 空場
    // 空場のときは、場の変数はオーダー関連だけである事が多いのでそのまま
    constexpr uint64_t NullBoardToHashKey(Board bd) {
        return bd.order();
    }
    uint64_t BoardToHashKey(Board bd) {
        return bd.toInt();
    }
    
    /**************************局面:L2**************************/
    
    // 完全情報局面なのでシンプルな後退ハッシュ値
    // 先手、後手の順番でカード集合ハッシュ値をクロスして場のハッシュ値を線形加算
    // ただしNFでない場合パスと場主も考慮が必要なので、現在はやってない
    
    constexpr uint64_t L2PassHashKeyTable[2] = {
        0x9a257a985d22921b, 0xe8237fa57f5d50ed,
    };
    
    inline uint64_t L2NullFieldToHashKey(Cards c0, Cards c1, Board bd) {
        return CardsCardsToHashKey(c0, c1) ^ NullBoardToHashKey(bd);
    }
    
    // すでにハッシュ値が部分的に計算されている場合
    inline uint64_t knitL2NullFieldHashKey(uint64_t ckey0, uint64_t ckey1, uint64_t boardKey) {
        return knitCardsCardsHashKey(ckey0, ckey1) ^ boardKey;
    }
}