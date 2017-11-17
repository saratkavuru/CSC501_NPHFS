#Inserting our npheap
sudo insmod ../../p1/CSC501_NPHeap/kernel_module/npheap.ko
#Inserting Tseng's npheap
#sudo insmod ../../p2/CSC501_TNPHEAP/NPHeap/npheap.ko
sudo chmod 777 /dev/npheap

#./configure
#make
#sudo make install
#nphfuse -s -d /dev/npheap testdir 
#fusermount -u testdir
