#!/bin/bash
client_program="/home/xx/client/build/client"
shutdown_program="/home/xx/shutdown/build/shutdown"
addr_file="/home/xx/addr.txt"
res_file="/home/xx/res.txt"
client_log="/home/xx/client_log.txt"
server_log="/home/xx/server_log.txt"
shutdown_log="/home/xx/shutdown_log.txt"

touch $res_file
touch $client_log
touch $server_log
touch $shutdown_log

dataset=macos
chunk_size=16kb
node_num=32
feature_num=8
exp_seg_size=512
feature_method=FINESSE_FEATURE
route_method=GUIDEPOST
hit_thres=4
skew_thres=1.05
ssdedup=OFF
lcache_size=2048
lc_hit_thres=1
gp_size=(128 256 512 1024 2048)
gp_part=(1 2 4 8 16)

function wait_and_shut() {
    while true
    do
        if test -z "$(ps -u dc|grep client)"; then
            break
        else
            sleep 30
        fi
    done
    cd /home/xx/shutdown/build
    rm CMakeCache.txt
    cmake .. -DNODE_NUM=$1
    make
    echo "===========$(date +%Y-%m-%d\ %H:%M:%S)===========" >> $shutdown_log
    echo "===========$(date +%Y-%m-%d\ %H:%M:%S)===========" >> $res_file
    $shutdown_program $addr_file $res_file >> $shutdown_log
}

for ((m=0; m<${#gp_size[@]}; m++))
do
    for ((n=0; n<${#gp_part[@]}; n++))
    do
        echo "===========$(date +%Y-%m-%d\ %H:%M:%S)===========" >> $server_log
        ssh xx@219.xxx.xxx.67 "cd /home/xx/server/build ; rm CMakeCache.txt ; /snap/bin/cmake .. -DFEATURE_NUM=$feature_num -DROUTE_METHOD=$route_method -DHIT_THRES=$hit_thres -DGP_SIZE=${gp_size[$m]} ; make ; bash run.sh $node_num" >> $server_log &
        sleep 5
        cd /home/xx/client/build
        rm CMakeCache.txt
        cmake .. -DFEATURE_NUM=$feature_num -DROUTE_METHOD=$route_method -DHIT_THRES=$hit_thres -DEXP_SEGMENT_SIZE=$exp_seg_size -DFEATURE_METHOD=$feature_method -DNODE_NUM=$node_num -DSKEW_THRES=$skew_thres -DSSDEDUP=$ssdedup -DLCACHE_SIZE=$lcache_size -DLC_HIT_THRES=$lc_hit_thres -DGP_SIZE=${gp_size[$m]} -DGP_PART=${gp_part[$n]}
        make
        echo "===========$(date +%Y-%m-%d\ %H:%M:%S)===========" >> $client_log
        $client_program $addr_file /data/fsl/$dataset/ $res_file >> $client_log &
        
        sleep 10
        echo "[dataset]         :   $dataset" >> $res_file
        echo "[chunk_size]      :   $chunk_size" >> $res_file
        echo "[seg_size]        :   $exp_seg_size" >> $res_file
        echo "[node_num]        :   $node_num" >> $res_file
        echo "[feature_num]     :   $feature_num" >> $res_file
        echo "[feature_method]  :   $feature_method" >> $res_file
        echo "[route_method]    :   $route_method" >> $res_file
        echo "[hit_thres]       :   $hit_thres" >> $res_file
        echo "[skew_thres]      :   $skew_thres" >> $res_file
        echo "[ssdedup]         :   $ssdedup" >> $res_file
        # echo "[lcache_size]     :   $lcache_size" >> $res_file
        # echo "[lc_hit_thres]    :   $lc_hit_thres" >> $res_file
        echo "[gp_size]         :   ${gp_size[$m]}" >> $res_file
        echo "[gp_part]         :   ${gp_part[$n]}" >> $res_file
        wait_and_shut $node_num
        sleep 5
    done
done
