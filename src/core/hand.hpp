#pragma once

#include "daifugo.hpp"

/**************************手札表現**************************/

// 手札表現の基本
// 単に集合演算がしたい場合はCards型のみでやるが
// 着手生成や支配、必勝判定等行う場合はこちらが便利
// 用途によってどの部分を更新するかは要指定
// 枚数指定をミスると最後まで改善されないので注意

// TODO: pqr の算出は qr からの計算の方が高速と思ったが要検証

struct Hand {
    Cards cards; // 通常型
    uint32_t qty; // 総枚数
    uint32_t jk; // ジョーカー枚数
    Cards seq; // 3枚階段型
    
    CardArray qr; // ランク枚数型
    Cards pqr; // ランク枚数位置型
    Cards sc; // 圧縮型
    Cards nd[2]; // 無支配型(通常、革命)

    uint64_t key; // ハッシュ値
    
    // 情報を使う早さにより、前後半(ハッシュ値だけは例外としてその他)に分ける。
    // allが付く進行はhash値も含めて更新する。
    
    // 前半
    // cards, qty, jk, seq, qr, pqr
    // 後半
    // sc, nd[2]
    // その他
    // key
    
    // (未実装)必勝判定のとき
    // cards, qty, jk, seq, pqrを更新
    // qrは使わなさそう?
    
    constexpr operator Cards() const { return cards; }
    bool holds(Cards c) const { return cards.holds(c); }
    constexpr bool any() const { return cards.any(); }
    void setKey(uint64_t k) { key = k; }

    void set1stHalf(Cards c, unsigned q) {
        assert(c.count() == q);
        cards = c;
        jk = c.joker();
        qty = q;
        BitCards plain = c.plain();
        qr = CardsToQR(plain);
        seq = polymRanks(plain, jk, 3);
        pqr = QRToPQR(qr);
        assert(exam1stHalf());
    }
    void set(Cards c, uint32_t q) {
        // ハッシュ値はsetKeyにて別に設定
        set1stHalf(c, q);
        sc = PQRToSC(pqr);
        PQRToND(pqr, jk, nd);
        assert(exam());
    }
    void setAll(Cards c, uint32_t q, uint64_t k) {
        set(c, q);
        setKey(k);
        assert(exam_key());
    }
    void set1stHalf(Cards c) { set1stHalf(c, c.count()); }
    void set(Cards c) { set(c, c.count()); }
    void setAll(Cards c) { setAll(c, c.count(), CardsToHashKey(c)); }
    void init() {
        cards = seq = CARDS_NULL;
        qr = 0ULL;
        pqr = sc = CARDS_NULL;
        qty = jk = 0;
        key = 0ULL;
    }
    void makeMove(Move m);
    void makeMoveAll(Move m);
    void makeMoveAll(Move m, Cards dc, int dq, uint64_t dk);
    void makeMove1stHalf(Move m);
    void makeMove1stHalf(Move m, Cards dc, int dq);
    void unmakeMove(Move m, Cards dc, uint32_t dq) {
        // カードが増えない時は入らない
        // 更新するものは最初にチェック
        assert(exam());
        assert(!m.isPASS());
        ASSERT(isExclusiveCards(cards, dc),
               cerr << "Hand::unmakeMove : inclusive unmaking-move. " << dc << " to " << cards << endl; );
        
        int djk = dc.joker();
        int r = m.rank();
        
        cards += dc; // 通常型は足せば良い
        qty += dq; // 枚数進行
        
        assert(cards != CARDS_NULL);
        assert(qty > 0);
        
        Cards plain = cards.plain();
        jk = cards.joker();
        seq = polymRanks(plain, jk, 3);

        // 無支配型(共通処理)
        if (djk) {
            // ジョーカーが増えた事で、1枚分ずれる
            // 1枚のところは全て無支配
            nd[0] = ((nd[0] & PQR_123) << 1) | PQR_1;
            nd[1] = ((nd[1] & PQR_123) << 1) | PQR_1;
        }

        if (dc != CARDS_JOKER) { // ジョーカーだけ増えた場合はこれで終わり
            if (!m.isSeq()) {
                // ジョーカーの存在により少し処理が複雑に
                dq -= djk; // ジョーカーの分引く
                
                Cards mask = RankToCards(r); // 当該ランクのマスク
                
                // 枚数型は当該ランクの枚数を足す
                qr = qr.data() + (BitCards(dq) << (r << 2));
                uint32_t nq = qr[r]; // 当該ランクの新しい枚数
                
                // 枚数位置型、圧縮型ともに新しい枚数に入れ替える
                pqr = ((Cards(1U << (nq - 1U))) << (r << 2)) | (pqr & ~mask);
                sc |= Cards((1U << nq) -1U) << (r << 2);
                
                Cards npqr = pqr & mask; // 当該ランクの新しいpqr
                
                // 無支配型
                // グループは増やすのが簡単(ただしテーブル参照あり)
                if (jk) {
                    // ジョーカーありの場合には1ビット枚数を増やして判定
                    if (npqr & PQR_4) {
                        npqr = (npqr & PQR_4) | ((npqr & PQR_123) << 1);
                    } else {
                        npqr <<= 1;
                    }
                    nq += jk;
                }
                
                // 通常オーダー
                if (!(npqr & nd[0])) { // 増分が無支配ゾーンに関係するので更新の必要あり
                    nd[0] |= ORQ_NDTable[0][r - 1][nq - 1];
                }
                // 逆転オーダー
                if (!(npqr & nd[1])) { // 増分が無支配ゾーンに関係するので更新の必要あり
                    nd[1] |= ORQ_NDTable[1][r + 1][nq - 1];
                }
                
            } else {
                // 階段
                Cards mask = RankRangeToCards(r, r + dq - 1); // 当該ランクのマスク
                Cards dqr = dc >> SuitToSuitNum(m.suits()); // スートの分だけずらすと枚数を表すようになる

                if (djk) {
                    Cards jkmask = RankToCards(m.jokerRank()); // ジョーカーがある場合のマスク
                    mask ^= jkmask; // ジョーカー部分はマスクに関係ないので外す
                    dqr &= ~jkmask;
                }
                
                // 枚数型はジョーカー以外の差分を足す
                qr = qr.data() + dqr;
                
                // 枚数位置型は当該ランクを1枚分シフトし、元々無かった所には1枚をうめる
                // 圧縮型は当該ランクを1枚分シフトし、1枚のところを埋める
                pqr = ((pqr & mask) << 1) | (~sc & mask & PQR_1) | (pqr & ~mask); // 昔のscを使う
                sc |= ((sc  & mask) << 1) | (mask & PQR_1);
                PQRToND(pqr, jk, nd);
            }
        }
        assert(exam());
    }
    // カード集合単位(役の形をなしていない)の場合
    void add(Cards dc, const int dq) {
        set(addCards(cards, dc), qty + dq);
    }
    void add(Cards dc) {
        add(dc, dc.count());
    }
    void addAll(Cards dc, const int dq, const uint64_t dk) {
        setAll(addCards(cards, dc), qty + dq, addCardKey(key, dk));
    }
    void addAll(Cards dc) {
        addAll(dc, dc.count(), CardsToHashKey(dc));
    }
    
    void subtr(Cards dc, const int dq) {
        set(subtrCards(cards, dc), qty - dq);
    }
    void subtr(Cards dc) {
        subtr(dc, dc.count());
    }
    void subtrAll(Cards dc, const int dq, const uint64_t dk) {
        setAll(subtrCards(cards, dc), qty - dq, subCardKey(key, dk));
    }
    void subtrAll(Cards dc) {
        subtrAll(dc, dc.count(), CardsToHashKey(dc));
    }
    
    // validator
    // 無視する部分もあるので、その場合は部分ごとにチェックする
    // 当然だが基本のcardsがおかしかったらどうにもならない
    bool exam_cards() const {
        if (!holdsCards(CARDS_ALL, cards)) {
            cerr << "Hand : exam_cards() <<" << cards << endl;
            return false;
        }
        return true;
    }
    bool exam_key() const {
        if (key != CardsToHashKey(cards)) {
            cerr << "Hand : exam_key()" << cards << " <-> ";
            cerr << std::hex << key << std::dec << endl;
            return false;
        }
        return true;
    }
    bool exam_qty() const {
        if (qty != cards.count()) {
            cerr << "Hand : exam_qty() " << cards << " <-> " << qty << endl;
            return false;
        }
        return true;
    }
    bool exam_jk() const {
        if (jk != cards.joker()) {
            cerr << "Hand : exam_jk()" << endl;
            return false;
        }
        return true;
    }
    bool exam_qr() const {
        for (int r = RANK_IMG_MIN; r <= RANK_IMG_MAX; r++) {
            Cards rc = RankToCards(r) & cards;
            uint32_t rq = rc.count();
            uint32_t rqr = qr[r];
            if (rqr != rq) {
                cerr << "Hand : exam_qr()" << cards << " -> " << BitArray64<4>(qr) << endl;
                return false;
            } // 枚数型があってない
        }
        if (qr & ~CARDS_ALL) {
            cerr << "Hand : exam_qr()" << cards << " -> " << BitArray64<4>(qr) << endl;
            return false;
        }
        return true;
    }
    bool exam_pqr() const {
        for (int r = RANK_IMG_MIN; r <= RANK_IMG_MAX; r++) {
            Cards rc = RankToCards(r) & cards;
            uint32_t rq = rc.count();
            uint32_t rpqr = pqr[r];
            if (anyCards(rc)) {
                if (1U << (rq - 1) != rpqr) {
                    cerr << "Hand : exam_pqr()" << cards << " -> " << pqr << endl;
                    return false;
                } // pqrの定義
            } else {
                if (rpqr) {
                    cerr << "Hand : exam_pqr()" << cards << " -> " << pqr << endl;
                    return false;
                }
            }
        }
        if (pqr & ~CARDS_PLAIN_ALL) {
            cerr << "Hand : exam_pqr()" << cards << " -> " << pqr << endl;
            return false;
        }
        return true;
    }
    bool exam_sc() const {
        for (int r = RANK_IMG_MIN; r <= RANK_IMG_MAX; r++) {
            Cards rc = RankToCards(r) & cards;
            uint32_t rq = rc.count();
            uint32_t rsc = sc[r];
            if ((1U << rq) - 1U != rsc) {
                cerr << "Hand : exam_sc()" << endl;
                return false;
            } // scの定義
        }
        if (sc & ~CARDS_PLAIN_ALL) {
            cerr << "Hand : exam_sc()" << endl;
            return false;
        }
        return true;
    }
    bool exam_seq() const {
        if (seq != polymRanks(cards.plain(), cards.joker(), 3)) {
            cerr << "Hand : exam_seq()" << endl;
            return false;
        }
        return true;
    }
    bool exam_nd_by_pqr() const {
        // 無支配型をpqrからの変形によって確かめる
        // pqr -> nd は正確と仮定
        Cards tmpnd[2];
        PQRToND(pqr, containsJOKER(cards) ? 1 : 0, tmpnd);
        if (nd[0] != tmpnd[0]) {
            cerr << "Hand : exam_nd_by_pqr() nd[0]" << endl;
            return false;
        }
        if (nd[1] != tmpnd[1]) {
            cerr << "Hand : exam_nd_by_pqr() nd[1]" << endl;
            return false;
        }
        return true;
    }
    bool exam_nd() const {
        if (!exam_pqr()) return false;
        if (!exam_nd_by_pqr()) return false;
        return true;
    }
    
    bool exam1stHalf() const {
        if (!exam_cards()) return false;
        if (!exam_jk()) return false;
        if (!exam_qty()) return false;
        if (!exam_pqr()) return false;
        if (!exam_seq()) return false;
        if (!exam_qr()) return false;
        return true;
    }
    bool exam2ndHalf() const {
        if (!exam_sc()) return false;
        if (!exam_nd()) return false;
        return true;
    }
    bool exam() const {
        if (!exam1stHalf()) return false;
        if (!exam2ndHalf()) return false;
        return true;
    }
    bool examAll() const {
        if (!exam()) return false;
        if (!exam_key()) return false;
        return true;
    }
    
    std::string toDebugString() const {
        std::ostringstream oss;
        oss << "cards = " << cards << endl;
        oss << "qty = " << qty << endl;
        oss << "jk = " << jk << endl;
        oss << "seq = " << seq << endl;
        oss << "qr = " << CardArray(qr) << endl;
        oss << "pqr = " << CardArray(pqr) << endl;
        oss << "sc = " << CardArray(sc) << endl;
        oss << "nd[0] = " << CardArray(nd[0]) << endl;
        oss << "nd[1] = " << CardArray(nd[1]) << endl;
        oss << std::hex << key << std::dec << endl;
        
        oss << "correct data : " << endl;
        Hand tmp;
        tmp.setAll(cards);
        oss << "qty = " << tmp.qty << endl;
        oss << "jk = " << tmp.jk << endl;
        oss << "seq = " << tmp.seq << endl;
        oss << "qr = " << CardArray(tmp.qr) << endl;
        oss << "pqr = " << CardArray(tmp.pqr) << endl;
        oss << "sc = " << CardArray(tmp.sc) << endl;
        oss << "nd[0] = " << CardArray(tmp.nd[0]) << endl;
        oss << "nd[1] = " << CardArray(tmp.nd[1]) << endl;
        oss << std::hex << tmp.key << std::dec << endl;
        
        return oss.str();
    }
};

inline std::ostream& operator <<(std::ostream& out, const Hand& hand) { // 出力
    out << hand.cards << "(" << hand.qty << ")";
    return out;
}

template <bool HALF = false>
inline void makeMove(const Hand& arg, Hand *const dst, Move m, Cards dc, uint32_t dq) {
    // 普通、パスやカードが0枚になるときはこの関数には入らない。
    
    // 更新するものは最初にチェック
    if (HALF) assert(arg.exam1stHalf());
    else assert(arg.exam());
    assert(!m.isPASS());
    assert(arg.cards.holds(dc));
    
    int djk = dc.joker();
    int r = m.rank();
    
    dst->cards = subtrCards(arg.cards, dc); // 通常型は引けば良い
    dst->qty = arg.qty - dq; // 枚数進行
    dst->jk = arg.jk - djk; // ジョーカー枚数進行
    
    BitCards plain = dst->cards.plain();
    dst->seq = polymRanks(plain, dst->jk, 3);
    
    if (!HALF) {
        // 無支配型(共通処理)
        dst->nd[0] = arg.nd[0];
        dst->nd[1] = arg.nd[1];
        
        if (djk) {
            // ジョーカーが無くなった事で、1枚分ずれる
            // ただしもともと同ランク4枚があった場合にはそこだけ変化しない
            dst->nd[0] = (dst->nd[0] & PQR_234) >> 1;
            dst->nd[1] = (dst->nd[1] & PQR_234) >> 1;

            Cards quad = arg.pqr & PQR_4;
            if (quad) {
                IntCard ic0 = quad.highest();
                dst->nd[0] |= ORQ_NDTable[0][IntCardToRank(ic0) - 1][3];
                IntCard ic1 = quad.lowest();
                dst->nd[1] |= ORQ_NDTable[1][IntCardToRank(ic1) + 1][3];
            }
        }
    }

    if (dc & CARDS_PLAIN_ALL) {
        if (!m.isSeq()) {
            // ジョーカーの存在により少し処理が複雑に
            dq -= djk; // ジョーカーの分引く
            
            Cards mask = RankToCards(r); // 当該ランクのマスク
            Cards opqr = arg.pqr & mask; // 当該ランクの元々のpqr
            
            // 枚数型は当該ランクの枚数を引く
            dst->qr = arg.qr - (BitCards(dq) << (r << 2));
            
            // 枚数位置型、圧縮型は当該ランクのみ枚数分シフト
            // 0枚になったときに、シフトだけでは下のランクにはみ出す事に注意
            dst->pqr = (((arg.pqr & mask) >> dq) & mask) | (arg.pqr & ~mask);

            if (!HALF) {
                BitCards orgsc = arg.sc;
                dst->sc  = (((arg.sc  & mask) >> dq) & mask) | (arg.sc  & ~mask);

                // 無支配型
                if (dst->jk) { // まだジョーカーが残っている
                    // ジョーカーなしに戻す
                    dst->nd[0] &= PQR_234;
                    dst->nd[0] >>= 1;
                    dst->nd[1] &= PQR_234;
                    dst->nd[1] >>= 1;
                }
                BitCards dmask = orgsc ^ dst->sc; // 取り去る分のマスク

                // 通常オーダー
                if (!(opqr & dst->nd[0])) { // 元々そのランクが無支配ゾーンに関係しているので更新の必要あり
                    BitCards dmask0 = dmask & ~dst->nd[0];
                    while (1) {
                        dmask0 >>= 4;
                        dst->nd[0] ^= dmask0;
                        dmask0 &= ~dst->sc; // 現にあるカード集合の分はもう外れない
                        if (!(dmask0 & CARDS_ALL)) break;
                    }
                }
                // 逆転オーダー
                if (!(opqr & dst->nd[1])) { // 元々そのランクが無支配ゾーンに関係しているので更新の必要あり
                    BitCards dmask1 = dmask & ~dst->nd[1];
                    while (1) {
                        dmask1 <<= 4;
                        dst->nd[1] ^= dmask1;
                        dmask1 &= ~dst->sc; // 現にあるカード集合の分はもう外れない
                        if (!(dmask1 & CARDS_ALL)) break;
                    }
                }
                if (dst->jk) {
                    dst->nd[0] <<= 1;
                    dst->nd[0] |= PQR_1;
                    dst->nd[1] <<= 1;
                    dst->nd[1] &= PQR_234;
                    dst->nd[1] |= PQR_1;
                }
            }
        } else {
            // 階段
            Cards mask = RankRangeToCards(r, r + dq - 1); // 当該ランクのマスク
            Cards dqr = dc;

            if (djk) {
                Cards jkmask = RankToCards(m.jokerRank()); // ジョーカーがある場合のマスク
                mask &= ~jkmask; // ジョーカー部分はマスクに関係ないので外す
                dqr = maskJOKER(dqr);
            }

            dqr >>= SuitToSuitNum(m.suits()); // スートの分だけずらすと枚数を表すようになる

            // 枚数型はジョーカー以外の差分を引く
            dst->qr = arg.qr - dqr;
            // 枚数位置型、圧縮型は当該ランクを1枚分シフト
            // ただしグループと違って元々1枚の場合に注意
            dst->pqr = ((arg.pqr & mask & PQR_234) >> 1) | (arg.pqr & ~mask);

            if (!HALF) {
                dst->sc  = ((arg.sc  & mask & PQR_234) >> 1) | (arg.sc & ~mask);
                // めんどいのでその場計算
                PQRToND(dst->pqr, dst->jk, dst->nd);
            }
        }
    } else { // ジョーカーだけ無くなった場合のコピー処理
        dst->qr = arg.qr;
        dst->pqr = arg.pqr;
        if (!HALF) dst->sc = arg.sc;
    }
    if (HALF) assert(dst->exam1stHalf());
    else assert(dst->exam());
}

inline void makeMove(const Hand& arg, Hand *const dst, Move m) {
    makeMove(arg, dst, m, m.cards(), m.qty());
}
inline void makeMove1stHalf(const Hand& arg, Hand *const dst, Move m, Cards dc, int dq) {
    makeMove<true>(arg, dst, m, dc, dq);
}
inline void makeMove1stHalf(const Hand& arg, Hand *const dst, Move m) {
    makeMove1stHalf(arg, dst, m, m.cards(), m.qty());
}
inline void makeMoveAll(const Hand& arg, Hand *const dst, Move m, Cards dc, int dq, uint64_t dk) {
    makeMove(arg, dst, m, dc, dq);
    dst->key = subCardKey(arg.key, dk);
    assert(dst->exam_key());
}
inline void makeMoveAll(const Hand& arg, Hand *const dst, Move m) {
    Cards dc = m.cards();
    makeMoveAll(arg, dst, m, dc, m.qty(), CardsToHashKey(dc));
}
inline void Hand::makeMove1stHalf(Move m, Cards dc, int dq) {
    ::makeMove1stHalf(*this, this, m, dc, dq);
}
inline void Hand::makeMove1stHalf(Move m) {
    ::makeMove1stHalf(*this, this, m);
}
inline void Hand::makeMove(Move m) {
    ::makeMove(*this, this, m);
}
inline void Hand::makeMoveAll(Move m, Cards dc, int dq, uint64_t dk) {
    ::makeMoveAll(*this, this, m, dc, dq, dk);
}
inline void Hand::makeMoveAll(Move m) {
    ::makeMoveAll(*this, this, m);
}