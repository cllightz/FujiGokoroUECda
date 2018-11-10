# Blauweregenをベースにしたコンピュータ大貧民プログラム

UECコンピュータ大貧民大会（UECda）[1] 無差別級参加を想定した大貧民プログラム bmod（仮称）です。
@YuriCat 氏の開発した、UECda-2017 無差別級優勝クライアント [2] をベースにしています。
Blauweregen が GPL-3.0 なので、bmod も GPL-3.0 です。

[1]: UECコンピュータ大貧民大会 公式ホームページ http://www.tnlab.inf.uec.ac.jp/daihinmin

[2]: YuriCat/FujiGokoroUECda リポジトリの record_base2 ブランチ https://github.com/YuriCat/FujiGokoroUECda/tree/record_base2

# 現時点でのBlauweregenとの差異

- モンテカルロ木探索における提出手選択時のバンディットアルゴリズムを、UCB-rootからThompson Sampling（報酬値はベータ分布に従うと仮定）に変更
