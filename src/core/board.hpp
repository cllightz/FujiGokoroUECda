#pragma once

#include "daifugo.hpp"
#include "card.hpp"
#include "move.hpp"

namespace UECda {
    
    /**************************場表現**************************/
    
    // 各プレーヤーの情報を持たない場表現
    // 32ビット着手表現と同一の系列で扱えるようにする
    // ジョーカー情報などを残すかは難しいが、現在はほとんどの情報を残したまま
    
    struct Board {
        uint32_t b;
        
        constexpr Board() :b() {}
        constexpr Board(uint32_t arg) : b(arg) {}
        constexpr Board(const Board& arg) : b(arg.b) {}
        
        constexpr operator uint32_t() const { return b; }
        constexpr operator uint64_t() const { return (uint64_t)b; }
        
        void init() { b = 0U; }
        
        // set, fix
        
        void setTmpOrder(const uint32_t ord) { b |= ord << MOVE_LCT_TMPORD; }
        void setPrmOrder(const uint32_t ord) { b |= ord << MOVE_LCT_PRMORD; }
        
        void fixTmpOrder(const uint32_t ord) { b = (b & ~MOVE_FLAG_TMPORD) | (ord << MOVE_LCT_TMPORD); }
        void fixPrmOrder(const uint32_t ord) { b = (b & ~MOVE_FLAG_PRMORD) | (ord << MOVE_LCT_PRMORD); }
        
        void flipTmpOrder() { b ^= MOVE_FLAG_TMPORD; }
        void flipPrmOrder() { b ^= MOVE_FLAG_PRMORD; }
        
        void setExceptOrder(const uint32_t info) { b |= info; }
        void fixExceptOrder(const uint32_t info) { b = (b & MOVE_FLAG_ORD) | info; }
        
        void resetDom() { b &= ~MOVE_FLAG_DOM; }
        
        // 2体情報をメンバ関数で返す関数
        // 半マスク化みたいな感じ
        constexpr uint32_t domConditionally(Move m) const { return isSingleJOKER() && m.isS3Flush(); }
        
        constexpr bool locksSuits(Move m) const { return suitsPart() == m.suitsPart(); }
        constexpr bool locksRank(Move m) const { return false; } // ルールにない
        
        constexpr uint32_t afterPrmOrder(Move m) const { return ((b ^ m) >> MOVE_LCT_PRMORD) & 1U; }
        constexpr uint32_t afterTmpOrder(Move m) const { return ((b ^ m) >> MOVE_LCT_TMPORD) & 1U; }
        
        constexpr uint32_t isAfterTmpOrderReversed(Move m) const { return (b ^ m) & (1U << MOVE_LCT_TMPORD); }
        constexpr uint32_t isAfterPrmOrderReversed(Move m) const { return (b ^ m) & (1U << MOVE_LCT_PRMORD); }
            
        constexpr bool afterSuitsLocked(Move m) const {
            return suitsLocked() || locksSuits(m);
        }
        
        // get
        constexpr uint32_t prmOrder()   const { return (b >> MOVE_LCT_PRMORD) & 1U; }
        constexpr uint32_t tmpOrder()   const { return (b >> MOVE_LCT_TMPORD) & 1U; }
        constexpr uint32_t suits()      const { return (b >> MOVE_LCT_SUITS) & 15U; }
        constexpr int qty()        const { return (b >> MOVE_LCT_QTY) & 15U; }
        constexpr int rank()       const { return (b >> MOVE_LCT_RANK) & 15U; }
        constexpr int rank4x()     const { return (b >> MOVE_LCT_RANK4X) & (15U << 2); } // 4倍型
        constexpr int jokerRank()  const { return (b >> MOVE_LCT_JKRANK) & 15U; }
        constexpr int jokerRank4x() const { return (b >> MOVE_LCT_JKRANK4X) & (15U << 2); } // 4倍型
        constexpr uint32_t jokerSuits() const { return (b >> MOVE_LCT_JKSUITS) & 15U; }
        
        // 部分に着目する
        constexpr uint32_t orderPart()     const { return b & MOVE_FLAG_ORD; }
        constexpr uint32_t exceptOrderPart() const { return b & ~MOVE_FLAG_ORD; }
        constexpr uint32_t suitsPart()     const { return b & MOVE_FLAG_SUITS; }
        constexpr uint32_t rankPart()      const { return b & MOVE_FLAG_RANK; }
        constexpr uint32_t qtyPart()       const { return b & MOVE_FLAG_QTY; }
        constexpr uint32_t typePart()      const { return b & MOVE_FLAG_TYPE; } // サイズ＋形式
        constexpr uint32_t jokerPart()     const { return b & MOVE_FLAG_JK; } // ジョーカー関連
        constexpr uint32_t exeptJokerPart() const { return b & ~MOVE_FLAG_JK; } // ジョーカー関連以外
        
        // true or false
        constexpr uint32_t isNotNull() const { return b & (MOVE_FLAG_SINGLE | MOVE_FLAG_GROUP | MOVE_FLAG_SEQ); }
        constexpr bool isNull() const { return !isNotNull(); }
        constexpr bool isNF() const { return !isNotNull(); }
        constexpr uint32_t suitsLocked() const { return b & MOVE_FLAG_SUITSLOCK; }
        constexpr uint32_t rankLocked() const { return b & MOVE_FLAG_RANKLOCK; }
        
        constexpr uint32_t isTmpOrderRev() const { return b & MOVE_FLAG_TMPORD; }
        constexpr uint32_t isPrmOrderRev() const { return b & MOVE_FLAG_PRMORD; }
        
        constexpr uint32_t containsJOKER() const { return b & MOVE_FLAG_JK; }
        
        constexpr bool isSingleJOKER() const { return (b & (MOVE_FLAG_SINGLE | MOVE_FLAG_RANK)) == MOVE_FLAG_SINGLE; }
        constexpr bool isS3Flush() const { return holdsBits(b, (MOVE_FLAG_SINGLE | MOVE_FLAG_CONDDOM)); }
        constexpr uint32_t domInevitably() const { return b & MOVE_FLAG_INEVITDOM; }
        
        constexpr uint32_t isSeq() const { return b & MOVE_FLAG_SEQ; }
        constexpr uint32_t isGroup() const { return b & MOVE_FLAG_GROUP; }
        constexpr uint32_t isSingle() const { return b & MOVE_FLAG_SINGLE; }
        constexpr uint32_t isSingleOrGroup() const { return b & (MOVE_FLAG_SINGLE | MOVE_FLAG_GROUP); }
        bool isQuintuple() const {
            return typePart() == (MOVE_FLAG_GROUP | (5U << MOVE_LCT_QTY));
        }
        
        bool isOver5Seq() const { // 5枚以上階段
            return isSeq() && (qtyPart() > (4U << MOVE_LCT_QTY));
        }
        
        template <int IS_SEQ = _BOTH>
        bool isSpecialRankSeq() const {
            if ((IS_SEQ == _NO) || ((IS_SEQ != _YES) && !isSeq())) return false;
            uint32_t r = rank();
            uint32_t q = qty();
            return (r < RANK_MIN) || (RANK_MAX < (r + q - 1));
        }
        
        int typeNum() const {
            uint32_t q = qty();
            if (isSeq()) {
                if (q >= 6) return 8;
                return 2 + q;
            } else {
                if (q >= 5) return 8;
                return q;
            }
        }
        
        // 進行
        void procOrder(Move m) { b ^= m.orderPart(); } // オーダーフリップのみ
        
        void flush() {
            // 一時オーダーを永続オーダーに合わせる
            // TODO: ...現ルールではやらなくてよいので未実装
            b &= MOVE_FLAG_ORD;
        }
        
        void lockSuits() { b |= MOVE_FLAG_SUITSLOCK; }
        
        void procPASS() const {}//何もしない
        
        template <int IS_NF = _BOTH, int IS_PASS = _BOTH>
        void proc(Move m) { // プレーヤー等は関係なく局面のみ進める
            if (IS_PASS == _YES || ((IS_PASS != _NO) && m.isPASS())) {
                procPASS();
            } else {
                if (IS_NF == _YES) {
                    procOrder(m);
                    if (m.domInevitably()) { // 無条件完全支配
                        flush();
                    } else {
                        setExceptOrder(m.exceptOrderPart()); // 一時情報入れ替え
                    }
                } else if (IS_NF == _NO) {
                    procOrder(m);
                    if (m.domInevitably()) { // 無条件完全支配
                        flush();
                    } else {
                        if (domConditionally(m)) { // 条件付完全支配(Joker->S3のみ)
                            flush();
                        } else {
                            // スートロック
                            if (!suitsLocked()) {
                                // スートが一緒だったらロック処理
                                if (locksSuits(m)) { lockSuits(); }
                            }
                            // 一時情報入れ替え
                            b = (b & (MOVE_FLAG_LOCK | MOVE_FLAG_ORD))
                            | (m & ~(MOVE_FLAG_LOCK | MOVE_FLAG_ORD));
                        }
                    }
                } else { // IS_NF不明
                    procOrder(m);
                    if (m.domInevitably()) { // 無条件完全支配
                        flush();
                    } else {
                        if (isNF()) {
                            setExceptOrder(m.exceptOrderPart()); // 一時情報入れ替え
                        } else {
                            if (domConditionally(m)) { // 条件付完全支配(Joker->S3のみ)
                                flush();
                            } else {
                                // スートロック
                                if (!suitsLocked()) {
                                    // スートが一緒だったらロック処理
                                    if (locksSuits(m)) { lockSuits(); }
                                }
                                // 一時情報入れ替え
                                b = (b & (MOVE_FLAG_LOCK | MOVE_FLAG_ORD))
                                | (m & ~(MOVE_FLAG_LOCK | MOVE_FLAG_ORD));
                            }
                        }
                    }
                }
            }
        }
        
        template <int IS_NF = _BOTH, int IS_PASS = _BOTH>
        void procAndFlush(Move m) {
            // 局面を更新し、強引に場を流す
            if (IS_PASS == _NO || ((IS_PASS != _YES) && (!m.isPASS()))) { // パスならオーダーフリップいらず
                procOrder(m); // オーダーフリップ
            }
            if (IS_NF == _NO || ((IS_NF != _YES) && (!isNF()))) {
                flush();
            }
        }
        
        void procExceptFlush(Move m) {
            // 局面を更新するが場を流さない
            procOrder(m);
            
            // スートロック
            if (!suitsLocked()) {
                // スートが一緒だったらロック処理
                if (locksSuits(m)) { lockSuits(); }
            }
            
            if (domConditionally(m)) { // Joker->S3のみ
                b = ((b & (MOVE_FLAG_LOCK | MOVE_FLAG_ORD))
                      | (m & ~(MOVE_FLAG_LOCK | MOVE_FLAG_ORD)));
                // ８切りと同じように無条件支配フラグをたてておく
                b |= MOVE_FLAG_INEVITDOM;
                b &= ~MOVE_FLAG_CONDDOM; // 代わりに条件フラグは外す
            }
            else{
                b = (b & (MOVE_FLAG_LOCK | MOVE_FLAG_ORD))
                      | (m & ~(MOVE_FLAG_LOCK | MOVE_FLAG_ORD));
            }
        }
    };
    
    constexpr Board OrderToNullBoard(int o) {
        return Board(o << MOVE_LCT_PRMORD | o << MOVE_LCT_TMPORD);
    }
    
    constexpr Move BoardToMove(const Board& b) {
        // 場->場役へと変化させる
        // 場役へとコンバート出来ない部分は変えない
        return Move(b.b);
    }
    constexpr Board MoveToBoard(const Move& mv) {
        // 場->場役へと変化させる
        return Board(mv.m_);
    }
    
    std::ostream& operator <<(std::ostream& out, const Board& b) { // Board出力
        if (b.isNull()) {
            out << "NULL";
        } else {
            Move m = BoardToMove(b); // 場役へ変化
            out << m;
        }
        // オーダー...一時オーダーのみ
        out << "  Order : ";
        if (b.tmpOrder() == ORDER_NORMAL) {
            out << "NORMAL";
        } else {
            out << "REVERSED";
        }
        out << "  Suits : ";
        if (b.suitsLocked()) {
            out << "LOCKED";
        } else {
            out << "FREE";
        }
        return out;
    }
    
    bool isSubjectivelyValid(Board b, Move mv, const Cards& c, const uint32_t q) {
        // 不完全情報の上での合法性判定
        // c はそのプレーヤーが所持可能なカード
        // q はそのプレーヤーの手札枚数（公開されている情報）
        if (mv.isPASS()) {
            return true;
        }
        // 枚数オーバー
        if (mv.qty() > q) return false;
        // 持っていないはずの札を使った場合
        if (!holdsCards(c, mv.cards())) return false;
        if (b.isNF()) {
        } else {
            if (b.typePart() != mv.typePart()) return false; // 型違い
            if (b.isSeq()) {
                if (!isValidSeqRank(mv.rank(), b.tmpOrder(), b.rank(), mv.qty())) {
                    return false;
                }
                if (b.suitsLocked()) {
                    if (b.suits() != mv.suits()) return false;
                }
            } else {
                if (b.isSingleJOKER()) {
                    if (!mv.isS3Flush()) { // ジョーカー->S3でなかった
                        return false;
                    } else {
                        return true;
                    }
                }
                if (mv.isSingleJOKER()) {
                    if (!b.isSingle()) return false;
                } else {
                    if (!isValidGroupRank(mv.rank(), b.tmpOrder(), b.rank())) {
                        return false;
                    }
                    if (b.suitsLocked()) {
                        if (b.suits() != mv.suits()) return false;
                    }
                }
            }
        }
        return true;
    }
}