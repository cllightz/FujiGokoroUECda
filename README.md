# Blauweregenをベースにしたコンピュータ大貧民プログラム

UECコンピュータ大貧民大会（UECda）[1] 無差別級参加を想定した大貧民プログラム bmod（仮称）です。
@YuriCat 氏が開発している YuriCat/FujiGokoroUECda [2] をベースにしています。
Blauweregen が GPL-3.0 なので、bmod も GPL-3.0 です。
YuriCat/FujiGokoroUECda の README.md は README_FujiGokoro.md にあります。

[1]:電気通信大学, "UECコンピュータ大貧民大会", http://www.tnlab.inf.uec.ac.jp/daihinmin (参照2019-08-18)

[2]: YuriCat, "Daifugo Program for UEC Computer Daihinmin Contest (UECda)", https://github.com/YuriCat/FujiGokoroUECda/tree/record_base2 (参照2019-08-18)

# 現時点での YuriCat/FujiGokoroUECda との差異

- モンテカルロ木探索における提出手選択時のバンディットアルゴリズムを、UCB-rootからThompson Sampling（報酬値はベータ分布に従うと仮定）に変更
