from os import link
from m5.params import *
from m5.objects import *

from common import FileSystemConfig

from topologies.BaseTopology import SimpleTopology

# Create a 3D Torus assuming an equal number of cache and directory controllers
# XYZ routing is enforced to guarantee deadlock freedom


class Torus_XYZ(SimpleTopology):
    description = "Torus_XYZ"

    def __init__(self, controllers):
        self.nodes = controllers

    # Make a 3D Torus assuming an equal number of cache and directory cntrls

    def makeTopology(self, options, network, IntLink, ExtLink, Router):
        nodes = self.nodes

        num_routers = options.num_cpus
        num_xs = options.torus_xs
        num_ys = options.torus_ys

        link_latency = options.link_latency  # used by simple and garnet
        router_latency = options.router_latency  # only used by garnet

        cntrls_per_router, remainder = divmod(len(nodes), num_routers)
        assert num_xs > 0
        assert num_ys > 0
        assert num_xs * num_ys <= num_routers
        num_zs = int(num_routers / (num_xs * num_ys))
        assert num_xs * num_ys * num_zs == num_routers

        # Create the routers in the mesh
        routers = [
            Router(router_id=i, latency=router_latency)
            for i in range(num_routers)
        ]
        network.routers = routers

        # link counter to set unique link ids
        link_count = 0

        # Add all but the remainder nodes to the list of nodes
        # to be unifomrly distributed across the nework.
        network_nodes = []
        remainder_nodes = []
        for node_index in range(len(nodes)):
            if node_index < (len(nodes) - remainder):
                network_nodes.append(nodes[node_index])
            else:
                remainder_nodes.append(nodes[node_index])

        # Connect each node to the appropriate router
        ext_links = []
        for (i, n) in enumerate(network_nodes):
            cntrl_level, router_id = divmod(i, num_routers)
            assert cntrl_level < cntrls_per_router
            ext_links.append(
                ExtLink(
                    link_id=link_count,
                    ext_node=n,
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

        # Create the torus links
        int_links = []

        # Front output to Back input links (weight = 1)
        for z in range(num_zs):
            for y in range(num_ys):
                for x in range(num_xs):
                    front_out = z * num_xs * num_ys + y * num_xs + x
                    back_in = (
                        z * num_xs * num_ys + y * num_xs + ((x + 1) % num_xs)
                    )
                    int_links.append(
                        IntLink(
                            link_id=link_count,
                            src_node=routers[front_out],
                            dst_node=routers[back_in],
                            src_outport="Front",
                            dst_inport="Back",
                            latency=link_latency,
                            weight=1,
                        )
                    )
                    link_count += 1

        # Back output to Front input links (weight = 1)
        for z in range(num_zs):
            for y in range(num_ys):
                for x in range(num_xs):
                    back_out = (
                        z * num_xs * num_ys + y * num_xs + ((x + 1) % num_xs)
                    )
                    front_in = z * num_xs * num_ys + y * num_xs + x
                    int_links.append(
                        IntLink(
                            link_id=link_count,
                            src_node=routers[back_out],
                            dst_node=routers[front_in],
                            src_outport="Back",
                            dst_inport="Front",
                            latency=link_latency,
                            weight=1,
                        )
                    )
                    link_count += 1

        # Left output to Right input links (weight = 1)
        for z in range(num_zs):
            for x in range(num_xs):
                for y in range(num_ys):
                    left_out = (
                        z * num_xs * num_ys + ((y + 1) % num_ys) * num_xs + x
                    )
                    right_in = z * num_xs * num_ys + y * num_xs + x
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

        # Right output to Left input links (weight = 1)
        for z in range(num_zs):
            for x in range(num_xs):
                for y in range(num_ys):
                    right_out = z * num_xs * num_ys + y * num_xs + x
                    left_in = (
                        z * num_xs * num_ys + ((y + 1) % num_ys) * num_xs + x
                    )
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

        # Up output to Down input links (weight = 1)
        for y in range(num_ys):
            for x in range(num_xs):
                for z in range(num_zs):
                    up_out = z * num_xs * num_ys + y * num_xs + x
                    down_in = (
                        ((z + 1) % num_zs) * num_xs * num_ys + y * num_xs + x
                    )
                    int_links.append(
                        IntLink(
                            link_id=link_count,
                            src_node=routers[up_out],
                            dst_node=routers[down_in],
                            src_outport="Up",
                            dst_inport="Down",
                            latency=link_latency,
                            weight=1,
                        )
                    )
                    link_count += 1

        # Down output to Up input links (weight = 1)
        for y in range(num_ys):
            for x in range(num_xs):
                for z in range(num_zs):
                    down_out = (
                        ((z + 1) % num_zs) * num_xs * num_ys + y * num_xs + x
                    )
                    up_in = z * num_xs * num_ys + y * num_xs + x
                    int_links.append(
                        IntLink(
                            link_id=link_count,
                            src_node=routers[down_out],
                            dst_node=routers[up_in],
                            src_outport="Down",
                            dst_inport="Up",
                            latency=link_latency,
                            weight=1,
                        )
                    )
                    link_count += 1

        network.int_links = int_links

    # Register nodes with filesystem
    def registerTopology(self, options):
        for i in range(options.num_cpus):
            FileSystemConfig.register_node(
                [i], MemorySize(options.mem_size) // options.num_cpus, i
            )
