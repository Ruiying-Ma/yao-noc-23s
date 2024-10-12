#!/bin/bash
./build/NULL/gem5.opt \
configs/example/garnet_synth_traffic.py \
--network=garnet --num-cpus=1 --num-dirs=1 \
--topology=Torus_XYZ --torus-xs=1 --torus-ys=1 --routing-algorithm=3 \
--inj-vnet=0 --synthetic=neighbor \
--sim-cycles=10000 --injectionrate=1 \
--vcs-per-vnet=16

echo > network_stats.txt
grep "packets_injected::total" m5out/stats.txt | sed 's/system.ruby.network.packets_injected::total\s*/packets_injected = /' | sed 's/Unspecified/Count/' >> network_stats.txt
grep "packets_received::total" m5out/stats.txt | sed 's/system.ruby.network.packets_received::total\s*/packets_received = /' | sed 's/Unspecified/Count/' >> network_stats.txt
grep "average_packet_queueing_latency" m5out/stats.txt | sed 's/system.ruby.network.average_packet_queueing_latency\s*/average_packet_queueing_latency = /' | sed 's#Unspecified#Tick\/Count#' >> network_stats.txt
grep "average_packet_network_latency" m5out/stats.txt | sed 's/system.ruby.network.average_packet_network_latency\s*/average_packet_network_latency = /' | sed 's#Unspecified#Tick\/Count#' >> network_stats.txt
grep "average_packet_latency" m5out/stats.txt | sed 's/system.ruby.network.average_packet_latency\s*/average_packet_latency = /' | sed 's#Unspecified#Tick\/Count#' >> network_stats.txt
grep "average_hops" m5out/stats.txt | sed 's/system.ruby.network.average_hops\s*/average_hops = /' | sed 's/Unspecified/Count/' >> network_stats.txt
packets_received=$(grep "packets_received" network_stats.txt | awk -F ' ' '{print $3}')
num_cpus=$(grep -c "^\[system.cpu[0-9]*\]" m5out/config.ini)
num_ticks=$(grep "simTicks" m5out/stats.txt | awk -F ' ' '{print $2}')
sys_cycle_period=$(grep "system.clk_domain.clock" m5out/stats.txt | awk -F ' ' '{print $2}')
num_sys_cycles=$(expr $num_ticks / $sys_cycle_period)
reception_rate=`echo $packets_received $num_cpus $num_sys_cycles|awk '{printf "%0.6f\n", $1/$2/$3}'`
echo $reception_rate | sed "s/^/reception_rate = /" >> network_stats.txt
