rm -rf build

mkdir build

cd build

cmake .. -DCMAKE_C_COMPILER=arm-linux-gcc

make 

scp myapp root@192.168.33.54:/root/workspace

scp -r ../resource root@192.168.33.57:/root/workspace/

scp myapp root@192.168.33.57:/root/workspace/

cd ..
