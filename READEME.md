This is a coursework of *AI+X Computing Acceleration: From Algorithms Development, Analysis, to Deployment*. The code is based on [gem5](https://github.com/gem5/gem5). I implemented a torus with the routing algorithm proposed in [this paper](https://ieeexplore.ieee.org/abstract/document/5765951). Experiments were conducted with different network settings, and are shown in the [report](experiment_report.pdf).

# How to run
## 3D Torus
### Configure 3D Torus
`--topology=Torus_XYZ`

`--torus-xs=a`

`--torus-ys=b`

can create a $a\times b\times c$-Torus. We require that $a, b, c > 2$.

### Configure 3D Torus's customized routing
`--routing-algorithm=3`

### Example
```bash
./build/NULL/gem5.opt configs/example/garnet_synth_traffic.py \
--network=garnet --num-cpus=64 --num-dirs=64 \
--topology=Torus_XYZ --torus-xs=4 --torus-ys=4 --routing-algorithm=3 \
--inj-vnet=0 --synthetic=uniform_random \
--sim-cycles=10000 --injectionrate=0.01
```

### Test 3D Torus's performance
We use the following bash to test the performance of the 3D torus with its customized routing under different synthetic traffics and vcs-per-vnet:

```bash
# For --topology=Torus_XYZ and --routing-algorithm=3

## --synthetic=neighbor
### --vcs-per-vnet=2
inj_rate=0.020
rec_rate=1.000
echo "neighbor"
while [[ `echo "$inj_rate <= 1.000" | bc` -eq 1 && `echo "$rec_rate > 0.400" | bc` -eq 1 ]]
do
    echo $inj_rate
    ./build/NULL/gem5.opt \
    configs/example/garnet_synth_traffic.py \
    --network=garnet --num-cpus=64 --num-dirs=64 \
    --topology=Torus_XYZ --torus-xs=4 --torus-ys=4 --routing-algorithm=3 \
    --inj-vnet=0 --synthetic=neighbor \
    --sim-cycles=20000 --injectionrate=$inj_rate \
    --vcs-per-vnet=2 --router-latency=1

    ./network_stats.sh

    packets_injected=$(grep "packets_injected" network_stats.txt | awk -F ' ' '{print $3}')
    packets_received=$(grep "packets_received" network_stats.txt | awk -F ' ' '{print $3}')
    rec_rate=`echo $packets_received $packets_injected|awk '{printf "%0.6f\n", $1/$2}'`
    grep "average_packet_latency" network_stats.txt | awk -F ' ' '{print $3}' | sed "s/^/${rec_rate}\t/" | sed "s/^/${inj_rate}\t/" >> ../gem5_test/Torus4x4x4/neighbor/2-vcs-per-vnet.txt
    inj_rate=`echo $inj_rate |awk '{printf "%0.3f\n", $1 + 0.02}'` # 49 points for avg_latency-inj_rate graph
done

## --synthetic=neighbor
### --vcs-per-vnet=4
inj_rate=0.020
rec_rate=1.000
echo "neighbor"
while [[ `echo "$inj_rate <= 1.000" | bc` -eq 1 && `echo "$rec_rate > 0.400" | bc` -eq 1 ]]
do
    echo $inj_rate
    ./build/NULL/gem5.opt \
    configs/example/garnet_synth_traffic.py \
    --network=garnet --num-cpus=64 --num-dirs=64 \
    --topology=Torus_XYZ --torus-xs=4 --torus-ys=4 --routing-algorithm=3 \
    --inj-vnet=0 --synthetic=neighbor \
    --sim-cycles=20000 --injectionrate=$inj_rate \
    --vcs-per-vnet=4 --router-latency=1

    ./network_stats.sh

    packets_injected=$(grep "packets_injected" network_stats.txt | awk -F ' ' '{print $3}')
    packets_received=$(grep "packets_received" network_stats.txt | awk -F ' ' '{print $3}')
    rec_rate=`echo $packets_received $packets_injected|awk '{printf "%0.6f\n", $1/$2}'`
    grep "average_packet_latency" network_stats.txt | awk -F ' ' '{print $3}' | sed "s/^/${rec_rate}\t/" | sed "s/^/${inj_rate}\t/" >> ../gem5_test/Torus4x4x4/neighbor/4-vcs-per-vnet.txt
    inj_rate=`echo $inj_rate |awk '{printf "%0.3f\n", $1 + 0.02}'` # 49 points for avg_latency-inj_rate graph
done

## --synthetic=shuffle
### --vcs-per-vnet=2
inj_rate=0.020
rec_rate=1.000
echo "shuffle"
while [[ `echo "$inj_rate <= 1.000" | bc` -eq 1 && `echo "$rec_rate > 0.400" | bc` -eq 1 ]]
do
    echo $inj_rate
    ./build/NULL/gem5.opt \
    configs/example/garnet_synth_traffic.py \
    --network=garnet --num-cpus=64 --num-dirs=64 \
    --topology=Torus_XYZ --torus-xs=4 --torus-ys=4 --routing-algorithm=3 \
    --inj-vnet=0 --synthetic=shuffle \
    --sim-cycles=20000 --injectionrate=$inj_rate \
    --vcs-per-vnet=2 --router-latency=1

    ./network_stats.sh

    packets_injected=$(grep "packets_injected" network_stats.txt | awk -F ' ' '{print $3}')
    packets_received=$(grep "packets_received" network_stats.txt | awk -F ' ' '{print $3}')
    rec_rate=`echo $packets_received $packets_injected|awk '{printf "%0.6f\n", $1/$2}'`
    grep "average_packet_latency" network_stats.txt | awk -F ' ' '{print $3}' | sed "s/^/${rec_rate}\t/" | sed "s/^/${inj_rate}\t/" >> ../gem5_test/Torus4x4x4/shuffle/2-vcs-per-vnet.txt
    inj_rate=`echo $inj_rate |awk '{printf "%0.3f\n", $1 + 0.02}'` # 49 points for avg_latency-inj_rate graph
done

## --synthetic=shuffle
### --vcs-per-vnet=4
inj_rate=0.020
rec_rate=1.000
echo "shuffle"
while [[ `echo "$inj_rate <= 1.000" | bc` -eq 1 && `echo "$rec_rate > 0.400" | bc` -eq 1 ]]
do
    echo $inj_rate
    ./build/NULL/gem5.opt \
    configs/example/garnet_synth_traffic.py \
    --network=garnet --num-cpus=64 --num-dirs=64 \
    --topology=Torus_XYZ --torus-xs=4 --torus-ys=4 --routing-algorithm=3 \
    --inj-vnet=0 --synthetic=shuffle \
    --sim-cycles=20000 --injectionrate=$inj_rate \
    --vcs-per-vnet=4 --router-latency=1

    ./network_stats.sh

    packets_injected=$(grep "packets_injected" network_stats.txt | awk -F ' ' '{print $3}')
    packets_received=$(grep "packets_received" network_stats.txt | awk -F ' ' '{print $3}')
    rec_rate=`echo $packets_received $packets_injected|awk '{printf "%0.6f\n", $1/$2}'`
    grep "average_packet_latency" network_stats.txt | awk -F ' ' '{print $3}' | sed "s/^/${rec_rate}\t/" | sed "s/^/${inj_rate}\t/" >> ../gem5_test/Torus4x4x4/shuffle/4-vcs-per-vnet.txt
    inj_rate=`echo $inj_rate |awk '{printf "%0.3f\n", $1 + 0.02}'` # 49 points for avg_latency-inj_rate graph
done

## --synthetic=tornado
### --vcs-per-vnet=2
inj_rate=0.020
rec_rate=1.000
echo "tornado"
while [[ `echo "$inj_rate <= 1.000" | bc` -eq 1 && `echo "$rec_rate > 0.400" | bc` -eq 1 ]]
do
    echo $inj_rate
    ./build/NULL/gem5.opt \
    configs/example/garnet_synth_traffic.py \
    --network=garnet --num-cpus=64 --num-dirs=64 \
    --topology=Torus_XYZ --torus-xs=4 --torus-ys=4 --routing-algorithm=3 \
    --inj-vnet=0 --synthetic=tornado \
    --sim-cycles=20000 --injectionrate=$inj_rate \
    --vcs-per-vnet=2 --router-latency=1

    ./network_stats.sh

    packets_injected=$(grep "packets_injected" network_stats.txt | awk -F ' ' '{print $3}')
    packets_received=$(grep "packets_received" network_stats.txt | awk -F ' ' '{print $3}')
    rec_rate=`echo $packets_received $packets_injected|awk '{printf "%0.6f\n", $1/$2}'`
    grep "average_packet_latency" network_stats.txt | awk -F ' ' '{print $3}' | sed "s/^/${rec_rate}\t/" | sed "s/^/${inj_rate}\t/" >> ../gem5_test/Torus4x4x4/tornado/2-vcs-per-vnet.txt
    inj_rate=`echo $inj_rate |awk '{printf "%0.3f\n", $1 + 0.02}'` # 49 points for avg_latency-inj_rate graph
done

## --synthetic=tornado
### --vcs-per-vnet=4
inj_rate=0.020
rec_rate=1.000
echo "tornado"
while [[ `echo "$inj_rate <= 1.000" | bc` -eq 1 && `echo "$rec_rate > 0.400" | bc` -eq 1 ]]
do
    echo $inj_rate
    ./build/NULL/gem5.opt \
    configs/example/garnet_synth_traffic.py \
    --network=garnet --num-cpus=64 --num-dirs=64 \
    --topology=Torus_XYZ --torus-xs=4 --torus-ys=4 --routing-algorithm=3 \
    --inj-vnet=0 --synthetic=tornado \
    --sim-cycles=20000 --injectionrate=$inj_rate \
    --vcs-per-vnet=4 --router-latency=1

    ./network_stats.sh

    packets_injected=$(grep "packets_injected" network_stats.txt | awk -F ' ' '{print $3}')
    packets_received=$(grep "packets_received" network_stats.txt | awk -F ' ' '{print $3}')
    rec_rate=`echo $packets_received $packets_injected|awk '{printf "%0.6f\n", $1/$2}'`
    grep "average_packet_latency" network_stats.txt | awk -F ' ' '{print $3}' | sed "s/^/${rec_rate}\t/" | sed "s/^/${inj_rate}\t/" >> ../gem5_test/Torus4x4x4/tornado/4-vcs-per-vnet.txt
    inj_rate=`echo $inj_rate |awk '{printf "%0.3f\n", $1 + 0.02}'` # 49 points for avg_latency-inj_rate graph
done

# ## --synthetic=transpose
# ### --vcs-per-vnet=2
inj_rate=0.020
rec_rate=1.000
echo "transpose"
while [[ `echo "$inj_rate <= 1.000" | bc` -eq 1 && `echo "$rec_rate > 0.400" | bc` -eq 1 ]]
do
    echo $inj_rate
    ./build/NULL/gem5.opt \
    configs/example/garnet_synth_traffic.py \
    --network=garnet --num-cpus=64 --num-dirs=64 \
    --topology=Torus_XYZ --torus-xs=4 --torus-ys=4 --routing-algorithm=3 \
    --inj-vnet=0 --synthetic=transpose \
    --sim-cycles=20000 --injectionrate=$inj_rate \
    --vcs-per-vnet=2 --router-latency=1

    ./network_stats.sh

    packets_injected=$(grep "packets_injected" network_stats.txt | awk -F ' ' '{print $3}')
    packets_received=$(grep "packets_received" network_stats.txt | awk -F ' ' '{print $3}')
    rec_rate=`echo $packets_received $packets_injected|awk '{printf "%0.6f\n", $1/$2}'`
    grep "average_packet_latency" network_stats.txt | awk -F ' ' '{print $3}' | sed "s/^/${rec_rate}\t/" | sed "s/^/${inj_rate}\t/" >> ../gem5_test/Torus4x4x4/transpose/2-vcs-per-vnet.txt
    inj_rate=`echo $inj_rate |awk '{printf "%0.3f\n", $1 + 0.02}'` # 49 points for avg_latency-inj_rate graph
done

## --synthetic=transpose
### --vcs-per-vnet=4
inj_rate=0.020
rec_rate=1.000
echo "transpose"
while [[ `echo "$inj_rate <= 1.000" | bc` -eq 1 && `echo "$rec_rate > 0.400" | bc` -eq 1 ]]
do
    echo $inj_rate
    ./build/NULL/gem5.opt \
    configs/example/garnet_synth_traffic.py \
    --network=garnet --num-cpus=64 --num-dirs=64 \
    --topology=Torus_XYZ --torus-xs=4 --torus-ys=4 --routing-algorithm=3 \
    --inj-vnet=0 --synthetic=transpose \
    --sim-cycles=20000 --injectionrate=$inj_rate \
    --vcs-per-vnet=4 --router-latency=1

    ./network_stats.sh

    packets_injected=$(grep "packets_injected" network_stats.txt | awk -F ' ' '{print $3}')
    packets_received=$(grep "packets_received" network_stats.txt | awk -F ' ' '{print $3}')
    rec_rate=`echo $packets_received $packets_injected|awk '{printf "%0.6f\n", $1/$2}'`
    grep "average_packet_latency" network_stats.txt | awk -F ' ' '{print $3}' | sed "s/^/${rec_rate}\t/" | sed "s/^/${inj_rate}\t/" >> ../gem5_test/Torus4x4x4/transpose/4-vcs-per-vnet.txt
    inj_rate=`echo $inj_rate |awk '{printf "%0.3f\n", $1 + 0.02}'` # 49 points for avg_latency-inj_rate graph
done

## --synthetic=uniform_random
### --vcs-per-vnet=2
inj_rate=0.020
rec_rate=1.000
echo "uniform_random"
while [[ `echo "$inj_rate <= 1.000" | bc` -eq 1 && `echo "$rec_rate > 0.400" | bc` -eq 1 ]]
do
    echo $inj_rate
    ./build/NULL/gem5.opt \
    configs/example/garnet_synth_traffic.py \
    --network=garnet --num-cpus=64 --num-dirs=64 \
    --topology=Torus_XYZ --torus-xs=4 --torus-ys=4 --routing-algorithm=3 \
    --inj-vnet=0 --synthetic=uniform_random \
    --sim-cycles=20000 --injectionrate=$inj_rate \
    --vcs-per-vnet=2 --router-latency=1

    ./network_stats.sh

    packets_injected=$(grep "packets_injected" network_stats.txt | awk -F ' ' '{print $3}')
    packets_received=$(grep "packets_received" network_stats.txt | awk -F ' ' '{print $3}')
    rec_rate=`echo $packets_received $packets_injected|awk '{printf "%0.6f\n", $1/$2}'`
    grep "average_packet_latency" network_stats.txt | awk -F ' ' '{print $3}' | sed "s/^/${rec_rate}\t/" | sed "s/^/${inj_rate}\t/" >> ../gem5_test/Torus4x4x4/uniform_random/2-vcs-per-vnet.txt
    inj_rate=`echo $inj_rate |awk '{printf "%0.3f\n", $1 + 0.02}'` # 49 points for avg_latency-inj_rate graph
done

## --synthetic=uniform_random
### --vcs-per-vnet=4
inj_rate=0.020
rec_rate=1.000
echo "uniform_random"
while [[ `echo "$inj_rate <= 1.000" | bc` -eq 1 && `echo "$rec_rate > 0.400" | bc` -eq 1 ]]
do
    echo $inj_rate
    ./build/NULL/gem5.opt \
    configs/example/garnet_synth_traffic.py \
    --network=garnet --num-cpus=64 --num-dirs=64 \
    --topology=Torus_XYZ --torus-xs=4 --torus-ys=4 --routing-algorithm=3 \
    --inj-vnet=0 --synthetic=uniform_random \
    --sim-cycles=20000 --injectionrate=$inj_rate \
    --vcs-per-vnet=4 --router-latency=1

    ./network_stats.sh

    packets_injected=$(grep "packets_injected" network_stats.txt | awk -F ' ' '{print $3}')
    packets_received=$(grep "packets_received" network_stats.txt | awk -F ' ' '{print $3}')
    rec_rate=`echo $packets_received $packets_injected|awk '{printf "%0.6f\n", $1/$2}'`
    grep "average_packet_latency" network_stats.txt | awk -F ' ' '{print $3}' | sed "s/^/${rec_rate}\t/" | sed "s/^/${inj_rate}\t/" >> ../gem5_test/Torus4x4x4/uniform_random/4-vcs-per-vnet.txt
    inj_rate=`echo $inj_rate |awk '{printf "%0.3f\n", $1 + 0.02}'` # 49 points for avg_latency-inj_rate graph
done
```

## 2D Mesh_v3
### Configure 2D Mesh_v3
`--topology=Mesh_v3`


`--mesh-row=8`
### Configure 2D Mesh_v3's customized routing
`--routing-algorithm=6`

### Example
```bash
./build/NULL/gem5.opt configs/example/garnet_synth_traffic.py \
--network=garnet --num-cpus=64 --num-dirs=64 \
--topology=Mesh_v3 --mesh-rows=8 --routing-algorithm=6 \
--inj-vnet=0 --synthetic=unfiorm_random \
--sim-cycles=10000 --injectionrate=0.01
```
