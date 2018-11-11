time=`date '+%s'`
echo ${time}

# 設定
scripts_dir=`dirname $0`/
current_dir=${scripts_dir}../
learn_record_dir=${current_dir}learn_record_dir/
opponent_dir=${current_dir}../kou2/

# cd ..

# 保存ディレクトリ作成
save_dir=${current_dir}save${time}/
mkdir -p ${save_dir}

# backup_dir=${save_dir}backup/
# save_param_dir=${save_dir}param/
# mkdir -p ${backup_dir}
# mkdir -p ${save_param_dir}

current_param_dir=${current_dir}param/

# ビルド
# make release -j4

# データのバックアップを取る
# zip -r ${backup_dir}uecda.zip .

# 学習
echo policy_learner
# time ${current_dir}out/release/policy_learner -o ${save_param_dir} -th 4 -t -i 100000 -ld ${learn_record_dir} 2>${save_dir}policy_learn_log.txt

# 方策オンリーでの対戦実験
echo server
time ${current_dir}out/release/server -g 10000 1>${save_dir}policy_game.txt &

# rm -r ${current_param_dir}
# cp -r ${save_param_dir} ${current_dir}

sleep 1.0
echo policy_client
time ${current_dir}out/release/policy_client -i ${current_param_dir} 2>${save_dir}policy_client_output.txt &

# ""./blauweregen_config.txt" の読み込みはカレントディレクトリに依存するため、cdしておく。
cd ${opponent_dir}
sleep 0.5
echo opponent
time ./client &
sleep 0.5
echo opponent
time ./client &
sleep 0.5
echo opponent
time ./client &
sleep 0.5
echo opponent
time ./client &
cd ../bmod

# モンテカルロでの対戦実験

# cd scripts