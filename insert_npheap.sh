#Inserting our npheap
sudo insmod ../../p1/CSC501_NPHeap/kernel_module/npheap.ko
#Inserting Tseng's npheap
#sudo insmod ../../p2/CSC501_TNPHeap/NPHeap/npheap.ko
sudo chmod 777 /dev/npheap

#./configure
#make
#sudo make install
#nphfuse /dev/npheap testdir -s -d 

