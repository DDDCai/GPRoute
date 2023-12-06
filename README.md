# GPRoute: A Prediction-based Method to Accelerate Stateful Routing for Cluster Deduplication


## Workload


## Build

### Server
cd server  
cmake . -DFEATURE_NUM=$feature_num -DROUTE_METHOD=$route_method -DHIT_THRES=$hit_thres -DGP_SIZE=$gp_size  
make  

__--feature_num__: the number of features for each super-chunk.  
__--route_method__: "GUIDEPOST" for the Guidepost-based method; "BOAFFT" for other polling-based methods.  
__--hit_thres__: the prediction threshold.  
__--gp_size__: the number of features a feature container (Guidepost slice) contains.  

e.g., cmake . -DFEATURE_NUM=8 -DROUTE_METHOD=GUIDEPOST -DHIT_THRES=4 -DGP_SIZE=1024  

### Client
cd client  
cmake . -DFEATURE_NUM=$feature_num -DROUTE_METHOD=$route_method -DHIT_THRES=$hit_thres -DEXP_SEGMENT_SIZE=$exp_seg_size -DFEATURE_METHOD=$feature_method -DNODE_NUM=$node_num -DSKEW_THRES=$skew_thres -DSSDEDUP=$ssdedup -DLCACHE_SIZE=$lcache_size -DLC_HIT_THRES=$lc_hit_thres -DGP_SIZE=$gp_size -DGP_PART=$gp_part  
make  

__--exp_seg_size__: the expected number of chunks each super-chunk contains (512 in our experiments).  
__--feature_method__: "FINESSE_FEATURE" for Guidepost/Boafft/SS-Dedup methods; "SIGMA_FEATURE" for Σ-Dedup.  
__--node_num__: the number of servers in this cluster.
__--skew_thres__: the upper limit of acceptable Data Skew (1.05 in our experiments).  
__--ssdedup__: whether to turn on SS-Dedup's LRU cache (ON/OFF).  
__--lcache_size__: the size of SS-Dedup's LRU cache.  
__--lc_hit_thres__: the prediction threshold of SS-Dedup's LRU cache.  
__--gp_part__: the number of Guidepost slices.  

e.g., cmake . -DFEATURE_NUM=8 -DROUTE_METHOD=GUIDEPOST -DHIT_THRES=4 -DEXP_SEGMENT_SIZE=512 -DFEATURE_METHOD=FINESSE -DNODE_NUM=32 -DSKEW_THRES=1.05 -DSSDEDUP=ON -DLCACHE_SIZE=2048 -DLC_HIT_THRES=4 -DGP_SIZE=1024 -DGP_PART=2  

### Shutdown
cd shutdown  
cmake . -DNODE_NUM=$node_num  
make  


## Run
./server [ipv4_address]:[port]  
./client [server_address_file] [workload_path] [result_file]  
./shutdown [server_address_file] [result_file]  

You should pass an [ipv4_address] and a [port] to the server program. The program will bind and monitor them to receive requests.  
To simulate a multi-server cluster, you should run multiple programs with different combinations of [ipv4_address] and [port].  
e.g., to simulate a 2-server cluster:  
./server 219.xxx.xxx.67:6400  
./server 219.xxx.xxx.67:6401  

__[server_address_file]__ is a file recording all server's [$ipv4_address:$port], with which the client knows where the requests should be sent to.  
__[workload_path]__ is the folder where the trace files are stored.  
__[result_file]__ is the file used to record some experiment results.  

When the deduplication process completes, run shutdown program to get necessary experiment results and shutdown all servers.  
e.g., to start a deduplication task:  
./client /home/xx/address.txt /data/fsl/macos/ home/xx/res.txt  
to end the experiment:  
./shutdown /home/xx/address.txt home/xx/res.txt  

We also provided the script we used to study Guidepost size as an example (gp_size.sh).
