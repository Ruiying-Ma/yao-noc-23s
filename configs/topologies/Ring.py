from math import remainder
from m5.params import *
from m5.objects import *

from common import FileSystemConfig

from topologies.BaseTopology import SimpleTopology

# Create a ring assuming an equal number of cache and directory controllers.


class Ring(SimpleTopology):
    description = "Ring"

    def __init__(self, controllers):
        self.nodes = controllers

    def makeTopology(self, options, network, IntLink, ExtLink, Router):
        nodes = self.nodes

        num_routers = options.num_cpus

        link_latency = options.link_latency
        router_latency = options.router_latency

        cntrls_per_router, remainder = divmod(len(nodes), num_routers)

        # Create the routers in the mesh
        routers = [
            Router(router_id=i, latency=router_latency)
            for i in range(num_routers)
        ]
        network.routers = routers

        # Set unique link ids using `link_count`
        link_count = 0

        # Add all but the remainder nodes to the list of nodes to the list of nodes to be uniformly distributed across the network
        network_nodes = []
        remainder_nodes = []
        for node_index in range(len(nodes)):
            if node_index < (len(nodes) - remainder):
                network_nodes.append(nodes[node_index])
            else:
                remainder_nodes.append(nodes[node_index])

        # Connect each nodes to the appropriate router
        ext_links = []
        for (i, node) in enumerate(network_nodes):
            cntrl_level, router_id = divmod(i, num_routers)
            assert cntrl_level < cntrls_per_router
            ext_links.append(
                ExtLink(
                    link_id=link_count,
                    ext_node=node,
                    int_node=routers[router_id],
                    latency=link_latency,
                )
            )
            link_count += 1

        # Connect the remainding nodes to router 0.
        # These should only be DMA nodes.
        for (i, node) in enumerate(remainder_nodes):
            assert node.type == "DMA_Controller"
            assert i < remainder
            ext_links.append(
                ExtLink(
                    link_id=link_count,
                    ext_node=node,
                    int_node=routers[0],
                    latency=link_latency,
                )
            )
            link_count += 1

        network.ext_links = ext_links

        # Create the ring's links
        int_links = []

        # Right output to Left input links (weight = 1)
        for item in range(num_routers):
            right_out = item
            left_in = (item + 1) % num_routers
            int_links.append(
                IntLink(
                    link_id=link_count,
                    src_node=routers[right_out],
                    dst_node=routers[left_in],
                    src_outport="Right",
                    dst_inport="Left",
                    latency=link_latency,
                    weight=1,
                )
            )
            link_count += 1

        # Left output to Right input links (weight = 1)
        for item in range(num_routers):
            left_out = (item + 1) % num_routers
            right_in = item
            int_links.append(
                IntLink(
                    link_id=link_count,
                    src_node=routers[left_out],
                    dst_node=routers[right_in],
                    src_outport="Left",
                    dst_inport="Right",
                    latency=link_latency,
                    weight=1,
                )
            )
            link_count += 1

        network.int_links = int_links

    def registerTopology(self, options):
        for i in range(options.num_cpus):
            FileSystemConfig.register_node(
                [i], MemorySize(options.mem_size) // options.num_cpus, i
            )
