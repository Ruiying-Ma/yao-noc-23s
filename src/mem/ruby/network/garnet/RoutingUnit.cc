/*
 * Copyright (c) 2008 Princeton University
 * Copyright (c) 2016 Georgia Institute of Technology
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "mem/ruby/network/garnet/RoutingUnit.hh"

#include "base/cast.hh"
#include "base/compiler.hh"
#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/garnet/InputUnit.hh"
#include "mem/ruby/network/garnet/Router.hh"
#include "mem/ruby/slicc_interface/Message.hh"

namespace gem5
{

namespace ruby
{

namespace garnet
{

RoutingUnit::RoutingUnit(Router *router)
{
    m_router = router;
    m_routing_table.clear();
    m_weight_table.clear();
}

void
RoutingUnit::addRoute(std::vector<NetDest>& routing_table_entry)
{
    if (routing_table_entry.size() > m_routing_table.size()) {
        m_routing_table.resize(routing_table_entry.size());
    }
    for (int v = 0; v < routing_table_entry.size(); v++) {
        m_routing_table[v].push_back(routing_table_entry[v]);
    }
}

void
RoutingUnit::addWeight(int link_weight)
{
    m_weight_table.push_back(link_weight);
}

bool
RoutingUnit::supportsVnet(int vnet, std::vector<int> sVnets)
{
    // If all vnets are supported, return true
    if (sVnets.size() == 0) {
        return true;
    }

    // Find the vnet in the vector, return true
    if (std::find(sVnets.begin(), sVnets.end(), vnet) != sVnets.end()) {
        return true;
    }

    // Not supported vnet
    return false;
}

/*
 * This is the default routing algorithm in garnet.
 * The routing table is populated during topology creation.
 * Routes can be biased via weight assignments in the topology file.
 * Correct weight assignments are critical to provide deadlock avoidance.
 */
int
RoutingUnit::lookupRoutingTable(int vnet, NetDest msg_destination)
{
    // First find all possible output link candidates
    // For ordered vnet, just choose the first
    // (to make sure different packets don't choose different routes)
    // For unordered vnet, randomly choose any of the links
    // To have a strict ordering between links, they should be given
    // different weights in the topology file

    int output_link = -1;
    int min_weight = INFINITE_;
    std::vector<int> output_link_candidates;
    int num_candidates = 0;

    // Identify the minimum weight among the candidate output links
    for (int link = 0; link < m_routing_table[vnet].size(); link++) {
        if (msg_destination.intersectionIsNotEmpty(
            m_routing_table[vnet][link])) {

        if (m_weight_table[link] <= min_weight)
            min_weight = m_weight_table[link];
        }
    }

    // Collect all candidate output links with this minimum weight
    for (int link = 0; link < m_routing_table[vnet].size(); link++) {
        if (msg_destination.intersectionIsNotEmpty(
            m_routing_table[vnet][link])) {

            if (m_weight_table[link] == min_weight) {
                num_candidates++;
                output_link_candidates.push_back(link);
            }
        }
    }

    if (output_link_candidates.size() == 0) {
        fatal("Fatal Error:: No Route exists from this Router.");
        exit(0);
    }

    // Randomly select any candidate output link
    int candidate = 0;
    if (!(m_router->get_net_ptr())->isVNetOrdered(vnet))
        candidate = rand() % num_candidates;

    output_link = output_link_candidates.at(candidate);
    return output_link;
}


void
RoutingUnit::addInDirection(PortDirection inport_dirn, int inport_idx)
{
    m_inports_dirn2idx[inport_dirn] = inport_idx;
    m_inports_idx2dirn[inport_idx]  = inport_dirn;
}

void
RoutingUnit::addOutDirection(PortDirection outport_dirn, int outport_idx)
{
    m_outports_dirn2idx[outport_dirn] = outport_idx;
    m_outports_idx2dirn[outport_idx]  = outport_dirn;
}

// outportCompute() is called by the InputUnit
// It calls the routing table by default.
// A template for adaptive topology-specific routing algorithm
// implementations using port directions rather than a static routing
// table is provided here.

int
RoutingUnit::outportCompute(RouteInfo route, int inport,
                            PortDirection inport_dirn)
{
    int outport = -1;

    if (route.dest_router == m_router->get_id()) {

        // Multiple NIs may be connected to this router,
        // all with output port direction = "Local"
        // Get exact outport id from table
        outport = lookupRoutingTable(route.vnet, route.net_dest);
        return outport;
    }

    // Routing Algorithm set in GarnetNetwork.py
    // Can be over-ridden from command line using --routing-algorithm = 1
    RoutingAlgorithm routing_algorithm =
        (RoutingAlgorithm) m_router->get_net_ptr()->getRoutingAlgorithm();

    assert(routing_algorithm != XYZ_);

    switch (routing_algorithm) {
        case TABLE_:  outport =
            lookupRoutingTable(route.vnet, route.net_dest); break;
        case XY_:     outport =
            outportComputeXY(route, inport, inport_dirn); break;
        case RING_:   outport =
            outportComputeRing(route, inport, inport_dirn); break;
        // any custom algorithm
        case CUSTOM_: outport =
            outportComputeCustom(route, inport, inport_dirn); break;
        default: outport =
            lookupRoutingTable(route.vnet, route.net_dest); break;
    }

    assert(outport != -1);
    return outport;
}

// XY routing implemented using port directions
// Only for reference purpose in a Mesh
// By default Garnet uses the routing table
int
RoutingUnit::outportComputeXY(RouteInfo route,
                              int inport,
                              PortDirection inport_dirn)
{
    PortDirection outport_dirn = "Unknown";

    [[maybe_unused]] int num_rows = m_router->get_net_ptr()->getNumRows();
    int num_cols = m_router->get_net_ptr()->getNumCols();
    assert(num_rows > 0 && num_cols > 0);

    int my_id = m_router->get_id();
    int my_x = my_id % num_cols;
    int my_y = my_id / num_cols;

    int dest_id = route.dest_router;
    int dest_x = dest_id % num_cols;
    int dest_y = dest_id / num_cols;

    int x_hops = abs(dest_x - my_x);
    int y_hops = abs(dest_y - my_y);

    bool x_dirn = (dest_x >= my_x);
    bool y_dirn = (dest_y >= my_y);

    // already checked that in outportCompute() function
    assert(!(x_hops == 0 && y_hops == 0));

    if (x_hops > 0) {
        if (x_dirn) {
            assert(inport_dirn == "Local" || inport_dirn == "West");
            outport_dirn = "East";
        } else {
            assert(inport_dirn == "Local" || inport_dirn == "East");
            outport_dirn = "West";
        }
    } else if (y_hops > 0) {
        if (y_dirn) {
            // "Local" or "South" or "West" or "East"
            assert(inport_dirn != "North");
            outport_dirn = "North";
        } else {
            // "Local" or "North" or "West" or "East"
            assert(inport_dirn != "South");
            outport_dirn = "South";
        }
    } else {
        // x_hops == 0 and y_hops == 0
        // this is not possible
        // already checked that in outportCompute() function
        panic("x_hops == y_hops == 0");
    }

    return m_outports_dirn2idx[outport_dirn];
}

// Template for implementing custom routing algorithm
// using port directions. (Example adaptive)
int
RoutingUnit::outportComputeCustom(RouteInfo route,
                                 int inport,
                                 PortDirection inport_dirn)
{
    panic("%s placeholder executed", __FUNCTION__);
}

// Ring routing implemented using port directions
int
RoutingUnit::outportComputeRing(RouteInfo route,
                                int inport,
                                PortDirection inport_dirn)
{
    PortDirection outport_dirn = "Unknown";

    int num_nodes = m_router->get_net_ptr()->getNumRouters();

    int my_id = m_router->get_id();

    int dest_id = route.dest_router;

    // already checked that in `outportCompute()`
    assert(my_id != dest_id);

    if (dest_id - my_id != 0) {
        if (dest_id > my_id) {
            if ( (dest_id - my_id) <= (num_nodes / 2)) {
                // clockwise
                assert(inport_dirn == "Local" || inport_dirn == "Left");
                outport_dirn = "Right";
            } else {
                // counter-clockwise
                assert(inport_dirn == "Local" || inport_dirn == "Right");
                outport_dirn = "Left";
            }
        } else {
            if ( (my_id - dest_id) <= (num_nodes / 2)) {
                // counter-clockwise
                assert(inport_dirn == "Local" || inport_dirn == "Right");
                outport_dirn = "Left";
            } else {
                // clockwise
                assert(inport_dirn == "Local" || inport_dirn == "Left");
                outport_dirn = "Right";
            }
        }
    } else {
        panic("hops == 0");
    }

    return m_outports_dirn2idx[outport_dirn];
}


// 3D Torus routing impelemented using port directions
// The return value is the set of all possible (PortDirection, R1/R2)
std::set<std::pair<int, bool>>
RoutingUnit::outportComputeXYZ(RouteInfo route,
                               int inport,
                               PortDirection inport_dirn)
{
    int num_xs = m_router->get_net_ptr()->getNumXs();
    int num_ys = m_router->get_net_ptr()->getNumYs();
    int num_zs = m_router->get_net_ptr()->getNumZs();
    assert(num_xs > 0 && num_ys > 0 && num_zs > 0 && num_xs * num_ys * num_zs == m_router->get_net_ptr()->getNumRouters());

    int my_id = m_router->get_id(); // my_id = my_x + my_y * num_xs + my_z * num_xs * num_ys
    int my_x = my_id % num_xs;
    int my_y = ((int)(my_id / num_xs)) % num_ys;
    int my_z = (my_id - my_x - my_y * num_xs) / (num_xs * num_ys);
    assert(my_id == my_x + my_y * num_xs + my_z * num_xs * num_ys);
    //std::cout<<"my_router "<<my_id<<" = "<<" ( "<<my_x<<", "<<my_y<<", "<<my_z<<")\n";//////////////////

    int dest_id = route.dest_router;
    int dest_x = dest_id % num_xs;
    int dest_y = ((int)(dest_id / num_xs)) % num_ys;
    int dest_z = (dest_id- dest_x - dest_y * num_xs) / (num_xs * num_ys);
    assert(dest_id == dest_x + dest_y * num_xs + dest_z * num_xs * num_ys);
    //std::cout<<"dest_router "<<dest_id<<" = "<<" ( "<<dest_x<<", "<<dest_y<<", "<<dest_z<<")\n";//////////////////

    if (dest_id == my_id) {
        std::set<std::pair<int, bool>> output_ports;
        int outport = lookupRoutingTable(route.vnet, route.net_dest);
        output_ports.emplace(outport, 0);
        output_ports.emplace(outport, 1);
        return output_ports;
    }
    assert(dest_id != my_id);

    int x_hops = abs(dest_x - my_x);
    int y_hops = abs(dest_y - my_y);
    int z_hops = abs(dest_z - my_z);

    assert(!(x_hops == 0 && y_hops == 0 && z_hops == 0));

    bool x_dirn1_en = (dest_x != my_x);
    bool y_dirn1_en = (dest_y != my_y);
    bool z_dirn1_en = (dest_z != my_z);

    // Choose among R1 channels (R1x, R1y, Rz1):
    // x_dirn1 == true: Front outport ---> Back inport       | R1x+
    // x_dirn1 == false: Back outport ---> Front ourport     | R1x-
    // y_dirn1 == true: Right outport ---> Left inport       | R1y+
    // y_dirn1 == false: Left outport ---> Right inport      | R1y-
    // z_dirn1 == true: Up outport ---> Down inport          | R1z+
    // z_dirn1 == false: Down outport --> Up inport          | R1z-
    bool x_dirn1 = false;
    if ( ( (my_x > dest_x) && ((my_x - dest_x) > (num_xs / 2)) ) || ( (my_x < dest_x) && ((dest_x - my_x) <= (num_xs / 2)) ) ){
        x_dirn1 = true;
    }
    bool y_dirn1 = false;
    if ( ( (my_y > dest_y) && ((my_y - dest_y) > (num_ys / 2)) ) || ( (my_y < dest_y) && ((dest_y - my_y) <= (num_ys / 2)) ) ){
        y_dirn1 = true;
    }
    bool z_dirn1 = false;
    if ( ( (my_z > dest_z) && ((my_z - dest_z) > (num_zs / 2)) ) || ( (my_z < dest_z) && ((dest_z - my_z) <= (num_zs / 2)) ) ){
        z_dirn1 = true;
    }

    // Choose among R2 channels
    // x_dirn2 == true: Front outport ---> Back inport       | R2x+
    // x_dirn2 == false: Back outport ---> Front ourport     | R2x-
    // y_dirn2 == true: Right outport ---> Left inport       | R2y+
    // y_dirn2 == false: Left outport ---> Right inport      | R2y-
    // z_dirn2 == true: Up outport ---> Down inport          | R2z+
    // z_dirn2 == false: Down outport --> Up inport          | R2z-
    bool x_wraparoud = (x_hops > (num_xs / 2)) ? true : false;
    bool y_wraparound = (y_hops > (num_ys / 2)) ? true : false;
    bool z_wraparound = (z_hops > (num_zs / 2)) ? true : false;
    bool x_dirn2_en = false, y_dirn2_en = false, z_dirn2_en = false;
    bool x_dirn2 = false, y_dirn2 = false, z_dirn2 = false;
    if (x_wraparoud) {
        if (my_x == num_xs - 1) { // R2x+
            x_dirn2_en = true;
            x_dirn2 = true;
        } else if (my_x == 0) { // R2x-
            x_dirn2_en = true;
        }
    } else if (y_wraparound) {
        if (my_y == num_ys - 1) { // R2y+
            y_dirn2_en = true;
            y_dirn2 = true;
        } else if (my_y == 0) { // R2y-
            y_dirn2_en = true;
        }
    } else if (z_wraparound) {
        if (my_z == num_zs - 1) { // R2z+
            z_dirn2_en = true;
            z_dirn2 = true;
        } else if (my_z == 0) { // R2z-
            z_dirn2_en = true;
        }
    } else {
        if (x_hops != 0) {
            x_dirn2_en = true;
            x_dirn2 = (dest_x - my_x > 0) ? true : false;
        } else if (y_hops != 0) {
            y_dirn2_en = true;
            y_dirn2 = (dest_y - my_y > 0) ? true : false;
        } else if (z_hops != 0) {
            z_dirn2_en = true;
            z_dirn2 = (dest_z - my_z > 0) ? true : false;
        } else {
            panic("x_hops == y_hops == z_hops == 0");
        }
    }

    // Put possible directions into the set `output_ports`
    std::set<std::pair<int, bool>> output_ports;
    if (x_dirn1_en && x_dirn1) {output_ports.emplace(m_outports_dirn2idx["Front"], 1);}
    if (x_dirn2_en && x_dirn2) {output_ports.emplace(m_outports_dirn2idx["Front"], 0);}
    if (x_dirn1_en && (!x_dirn1)) {output_ports.emplace(m_outports_dirn2idx["Back"], 1);}
    if (x_dirn2_en && (!x_dirn2)) {output_ports.emplace(m_outports_dirn2idx["Back"], 0);}
    if (y_dirn1_en && y_dirn1) {output_ports.emplace(m_outports_dirn2idx["Right"], 1);}
    if (y_dirn2_en && y_dirn2) {output_ports.emplace(m_outports_dirn2idx["Right"], 0);}
    if (y_dirn1_en && (!y_dirn1)) {output_ports.emplace(m_outports_dirn2idx["Left"], 1);}
    if (y_dirn2_en && (!y_dirn2)) {output_ports.emplace(m_outports_dirn2idx["Left"], 0);}
    if (z_dirn1_en && z_dirn1) {output_ports.emplace(m_outports_dirn2idx["Up"], 1);}
    if (z_dirn2_en && z_dirn2) {output_ports.emplace(m_outports_dirn2idx["Up"], 0);}
    if (z_dirn1_en && (!z_dirn1)) {output_ports.emplace(m_outports_dirn2idx["Down"], 1);}
    if (z_dirn2_en && (!z_dirn2)) {output_ports.emplace(m_outports_dirn2idx["Down"], 0);}

    assert(output_ports.size() > 0 && output_ports.size() <= 4);

    return output_ports;
}

} // namespace garnet
} // namespace ruby
} // namespace gem5
