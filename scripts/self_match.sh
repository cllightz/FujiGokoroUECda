name=${1}
index=${2}
port=${3}
cd ..
mkdir ./dump/
echo ${name} ${index} ${port}
./out/release/server -l ./dump/record_${name}${index}.dat -g 10000 -p ${port} 1>>./dump/output_server_${name}${index}.txt &
sleep 2.5
./out/release/client -p ${port} 2>>./dump/output_client${name}${index}_0.txt &
./out/release/client -p ${port} 2>>./dump/output_client${name}${index}_1.txt &
./out/release/client -p ${port} 2>>./dump/output_client${name}${index}_2.txt &
./out/release/client -p ${port} 2>>./dump/output_client${name}${index}_3.txt &
./out/release/client -p ${port} 2>>./dump/output_client${name}${index}_4.txt
cd scripts
