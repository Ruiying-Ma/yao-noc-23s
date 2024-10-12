/*
 * Copyright (c) 2020 Inria
 * Copyright (c) 2016 Georgia Institute of Technology
 * Copyright (c) 2008 Princeton University
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


#include "mem/ruby/network/garnet/SwitchAllocator.hh"

#include "debug/RubyNetwork.hh"
#include "mem/ruby/network/garnet/GarnetNetwork.hh"
#include "mem/ruby/network/garnet/InputUnit.hh"
#include "mem/ruby/network/garnet/OutputUnit.hh"
#include "mem/ruby/network/garnet/Router.hh"

namespace gem5
{

namespace ruby
{

namespace garnet
{

SwitchAllocator::SwitchAllocator(Router *router)
    : Consumer(router)
{
    m_router = router;
    m_num_vcs = m_router->get_num_vcs();
    m_vc_per_vnet = m_router->get_vc_per_vnet();

    m_input_arbiter_activity = 0;
    m_output_arbiter_activity = 0;
}

void
SwitchAllocator::init()
{
    m_num_inports = m_router->get_num_inports();
    m_num_outports = m_router->get_num_outports();
    m_round_robin_inport.resize(m_num_outports);
    m_round_robin_invc.resize(m_num_inports);
    m_port_requests.resize(m_num_inports);
    m_vc_winners.resize(m_num_inports);

    for (int i = 0; i < m_num_inports; i++) {
        m_round_robin_invc[i] = 0;
        m_port_requests[i] = -1;
        m_vc_winners[i] = -1;
    }

    for (int i = 0; i < m_num_outports; i++) {
        m_round_robin_inport[i] = 0;
    }
}

/*
 * The wakeup function of the SwitchAllocator performs a 2-stage
 * seperable switch allocation. At the end of the 2nd stage, a free
 * output VC is assigned to the winning flits of each output port.
 * There is no separate VCAllocator stage like the one in garnet1.0.
 * At the end of this function, the router is rescheduled to wakeup
 * next cycle for peforming SA for any flits ready next cycle.
 */

void
SwitchAllocator::wakeup()
{
    arbitrate_inports(); // First stage of allocation
    arbitrate_outports(); // Second stage of allocation

    clear_request_vector();
    check_for_wakeup();
}

/*
 * SA-I (or SA-i) loops through all input VCs at every input port,
 * and selects one in a round robin manner.
 *    - For HEAD/HEAD_TAIL flits only selects an input VC whose output port
 *     has at least one free output VC.
 *    - For BODY/TAIL flits, only selects an input VC that has credits
 *      in its output VC.
 * Places a request for the output port from this input VC.
 */

void
SwitchAllocator::arbitrate_inports()
{
    bool wormhole = m_router->get_net_ptr()->isWormholeEnabled();
    bool torus = (m_router->get_net_ptr()->getRoutingAlgorithm() == XYZ_);
    // Select a VC from each input in a round robin manner
    // Independent arbiter at each input port
    for (int inport = 0; inport < m_num_inports; inport++) {
        int invc = m_round_robin_invc[inport];

        for (int invc_iter = 0; invc_iter < m_num_vcs; invc_iter++) {
            auto input_unit = m_router->getInputUnit(inport);

            if (input_unit->need_stage(invc, SA_, curTick())) {
                // This flit is in SA stage
                bool make_request;
                int outport;
                int outvc;
                if (!torus) {
                    if (wormhole) {
                        flit* t_flit = input_unit->peekTopFlit(invc);
                        input_unit->grant_outport(invc, t_flit->get_outport());
                        input_unit->grant_outvc(invc, -1);
                    }
                    outport = input_unit->get_outport(invc);
                    outvc = input_unit->get_outvc(invc);
                    // check if the flit in this InputVC is allowed to be sent
                    // send_allowed conditions described in that function.
                    assert(outport >= 0);
                    make_request = send_allowed(inport, invc, outport, outvc, wormhole, -1);
                } else {
                    // 3D Torus customed routing
                    outport = input_unit->get_outport(invc);
                    outvc = input_unit->get_outvc(invc);
                    if (outvc == -1) {
                        // HEAD_ / HEAD_TAIL_
                        std::set<std::pair<int, bool>> outports = input_unit->get_outports(invc);
                        assert(outports.size() > 0 && outports.size() <= 4);
                        make_request = torus_send_allowed(inport, invc, outports);
                        outport = input_unit->get_outport(invc);
                        outvc = input_unit->get_outvc(invc);
                        if (make_request) {assert(outport >= 0 && outvc == -1);}
                    } else {
                        assert(outport >= 0 && outvc >= 0);
                        int firsthalf = (input_unit->get_firsthalf(invc)) ? 1 : 0;
                        make_request = send_allowed(inport, invc, outport, outvc, wormhole, firsthalf);
                    }
                }

                if (make_request) {
                    m_input_arbiter_activity++;
                    m_port_requests[inport] = outport;
                    m_vc_winners[inport] = invc;

                    break; // got one vc winner for this port
                }
            }

            invc++;
            if (invc >= m_num_vcs)
                invc = 0;
        }
    }
}

/*
 * SA-II (or SA-o) loops through all output ports,
 * and selects one input VC (that placed a request during SA-I)
 * as the winner for this output port in a round robin manner.
 *      - For HEAD/HEAD_TAIL flits, performs simplified outvc allocation.
 *        (i.e., select a free VC from the output port).
 *      - For BODY/TAIL flits, decrement a credit in the output vc.
 * The winning flit is read out from the input VC and sent to the
 * CrossbarSwitch.
 * An increment_credit signal is sent from the InputUnit
 * to the upstream router. For HEAD_TAIL/TAIL flits, is_free_signal in the
 * credit is set to true.
 */

void
SwitchAllocator::arbitrate_outports()
{
    bool wormhole = m_router->get_net_ptr()->isWormholeEnabled();
    bool torus = (m_router->get_net_ptr()->getRoutingAlgorithm() == XYZ_);
    // Now there are a set of input vc requests for output vcs.
    // Again do round robin arbitration on these requests
    // Independent arbiter at each output port
    for (int outport = 0; outport < m_num_outports; outport++) {
        int inport = m_round_robin_inport[outport];

        for (int inport_iter = 0; inport_iter < m_num_inports;
                 inport_iter++) {

            // inport has a request this cycle for outport
            if (m_port_requests[inport] == outport) {
                auto output_unit = m_router->getOutputUnit(outport);
                auto input_unit = m_router->getInputUnit(inport);

                // grant this outport to this inport
                int invc = m_vc_winners[inport];

                int outvc = input_unit->get_outvc(invc);
                if (outvc == -1) {
                    // VC Allocation - select any free VC from outport
                    if (!torus) {outvc = vc_allocate(outport, inport, invc, wormhole, -1);}
                    else {
                        int firsthalf = (input_unit->get_firsthalf(invc))? 1 : 0;
                        outvc = vc_allocate(outport, inport, invc, wormhole, firsthalf);
                    }
                }

                // remove flit from Input VC
                flit *t_flit = input_unit->getTopFlit(invc);

                DPRINTF(RubyNetwork, "SwitchAllocator at Router %d "
                                     "granted outvc %d at outport %d "
                                     "to invc %d at inport %d to flit %s at "
                                     "cycle: %lld\n",
                        m_router->get_id(), outvc,
                        m_router->getPortDirectionName(
                            output_unit->get_direction()),
                        invc,
                        m_router->getPortDirectionName(
                            input_unit->get_direction()),
                            *t_flit,
                        m_router->curCycle());


                // Update outport field in the flit since this is
                // used by CrossbarSwitch code to send it out of
                // correct outport.
                // Note: post route compute in InputUnit,
                // outport is updated in VC, but not in flit
                if (wormhole) {assert(t_flit->get_outport() == outport);}
                else {t_flit->set_outport(outport);}

                // set outvc (i.e., invc for next hop) in flit
                // (This was updated in VC by vc_allocate, but not in flit)
                t_flit->set_vc(outvc);

                // decrement credit in outvc
                output_unit->decrement_credit(outvc);

                // flit ready for Switch Traversal
                t_flit->advance_stage(ST_, curTick());
                m_router->grant_switch(inport, t_flit);
                m_output_arbiter_activity++;

                if (!wormhole) {
                    if ((t_flit->get_type() == TAIL_) ||
                        t_flit->get_type() == HEAD_TAIL_) {

                        // This Input VC should now be empty
                        assert(!(input_unit->isReady(invc, curTick())));

                        // Free this VC
                        input_unit->set_vc_idle(invc, curTick());

                        // Send a credit back
                        // along with the information that this VC is now idle
                        input_unit->increment_credit(invc, true, curTick());
                    } else {
                        // Send a credit back
                        // but do not indicate that the VC is idle
                        input_unit->increment_credit(invc, false, curTick());
                    }
                } else {
                    if (!(input_unit->isReady(invc, curTick()))) {
                        // This Input VC is now empty
                        // Free this VC
                        input_unit->set_vc_idle(invc, curTick());

                        // Send a credit back
                        // along with the information that this VC is now idle
                        input_unit->increment_credit(invc, true, curTick());
                    } else {
                        // Send a credit back
                        // but do not indicate that the VC is idle
                        input_unit->increment_credit(invc, false, curTick());
                    }
                }

                // remove this request
                m_port_requests[inport] = -1;

                // Update Round Robin pointer
                m_round_robin_inport[outport] = inport + 1;
                if (m_round_robin_inport[outport] >= m_num_inports)
                    m_round_robin_inport[outport] = 0;

                // Update Round Robin pointer to the next VC
                // We do it here to keep it fair.
                // Only the VC which got switch traversal
                // is updated.
                m_round_robin_invc[inport] = invc + 1;
                if (m_round_robin_invc[inport] >= m_num_vcs)
                    m_round_robin_invc[inport] = 0;


                break; // got a input winner for this outport
            }

            inport++;
            if (inport >= m_num_inports)
                inport = 0;
        }
    }
}

/*
 * A flit can be sent only if
 * (1) there is at least one free output VC at the
 *     output port (for HEAD/HEAD_TAIL),
 *  or
 * (2) if there is at least one credit (i.e., buffer slot)
 *     within the VC for BODY/TAIL flits of multi-flit packets.
 * and
 * (3) pt-to-pt ordering is not violated in ordered vnets, i.e.,
 *     there should be no other flit in this input port
 *     within an ordered vnet
 *     that arrived before this flit and is requesting the same output port.
 */

bool
SwitchAllocator::send_allowed(int inport, int invc, int outport, int outvc, bool wormhole, int first_half)
{
    // Check if outvc needed
    // Check if credit needed (for multi-flit packet)
    // Check if ordering violated (in ordered vnet)

    int vnet = get_vnet(invc);
    bool has_outvc = (outvc != -1);
    bool has_credit = false;

    auto output_unit = m_router->getOutputUnit(outport);

    if (!wormhole) {
        if (!has_outvc) {
            // needs outvc
            // this is only true for HEAD and HEAD_TAIL flits.
            if (first_half == 1) {
                if (output_unit->first_has_free_vc(vnet)) {
                    has_outvc = true;
                    has_credit = true;
                }
            } else if (first_half == 0) {
                if (output_unit->second_has_free_vc(vnet)) {
                    has_outvc = true;
                    has_credit = true;
                }
            } else {
                assert(first_half == -1);
                if (output_unit->has_free_vc(vnet)) {
                    has_outvc = true;
                    // each VC has at least one buffer,
                    // so no need for additional credit check
                    has_credit = true;
                }
            }
        } else {
            has_credit = output_unit->has_credit(outvc);
        }

        // cannot send if no outvc or no credit.
        if (!has_outvc || !has_credit){
            return false;
        }
    } else {
        // outvc = -1, has_outvc = 0
        assert(outvc == -1 && (!has_outvc));
        if (!output_unit->has_vc_with_credits(vnet)) {
            // cannot send if the output port has no vc with credits
            return false;
        }
    }



    // protocol ordering check
    if ((m_router->get_net_ptr())->isVNetOrdered(vnet)) {
        auto input_unit = m_router->getInputUnit(inport);

        // enqueue time of this flit
        Tick t_enqueue_time = input_unit->get_enqueue_time(invc);

        // check if any other flit is ready for SA and for same output port
        // and was enqueued before this flit
        int vc_base = vnet*m_vc_per_vnet;
        for (int vc_offset = 0; vc_offset < m_vc_per_vnet; vc_offset++) {
            int temp_vc = vc_base + vc_offset;
            if (input_unit->need_stage(temp_vc, SA_, curTick()) &&
               (input_unit->get_outport(temp_vc) == outport) &&
               (input_unit->get_enqueue_time(temp_vc) < t_enqueue_time)) {
                return false;
            }
        }
    }

    return true;
}

bool
SwitchAllocator::torus_send_allowed(int inport, int invc, std::set<std::pair<int, bool>> outports)
{
    assert(outports.size() > 0 && outports.size() <= 4);
    std::set<std::pair<int, bool>> legal_outports;
    for(auto& outport_firsthalf : outports) {
        int outport = outport_firsthalf.first;
        int first_half = (outport_firsthalf.second) ? 1 : 0;
        if (send_allowed(inport, invc, outport, -1, false, first_half)) {
                legal_outports.emplace(outport, first_half);
        }

    }
    if (legal_outports.size() == 0) {
        return false;
    }
    // Randomly select a (outport, first_half) from `legal_outports`
    int random_index = rand() % (legal_outports.size());
    assert(random_index < legal_outports.size());
    auto it = legal_outports.begin();
    while (random_index > 0) {it++; assert(it != legal_outports.end()); random_index--;}
    auto input_unit = m_router->getInputUnit(inport);
    input_unit->grant_outport(invc, it->first);
    input_unit->grant_firsthalf(invc, it->second);
    return true;

}

// Assign a free VC to the winner of the output port.
int
SwitchAllocator::vc_allocate(int outport, int inport, int invc, bool wormhole, int firsthalf)
{
    int outvc;
    if (!wormhole) {
        // Select a free VC from the output port
        if (firsthalf == 1) {
            outvc = m_router->getOutputUnit(outport)->first_select_free_vc(get_vnet(invc));
        } else if (firsthalf == 0) {
            outvc = m_router->getOutputUnit(outport)->second_select_free_vc(get_vnet(invc));
        } else {
            assert(firsthalf == -1);
            outvc = m_router->getOutputUnit(outport)->select_free_vc(get_vnet(invc));
        }
    } else {
        // Select a VC with credits from the output port
        outvc = m_router->getOutputUnit(outport)->select_vc_with_credits(get_vnet(invc));
    }
    // has to get a valid VC since it checked before performing SA
    assert(outvc != -1);
    m_router->getInputUnit(inport)->grant_outvc(invc, outvc);
    return outvc;
}

// Wakeup the router next cycle to perform SA again
// if there are flits ready.
void
SwitchAllocator::check_for_wakeup()
{
    Tick nextCycle = m_router->clockEdge(Cycles(1));

    if (m_router->alreadyScheduled(nextCycle)) {
        return;
    }

    for (int i = 0; i < m_num_inports; i++) {
        for (int j = 0; j < m_num_vcs; j++) {
            if (m_router->getInputUnit(i)->need_stage(j, SA_, nextCycle)) {
                m_router->schedule_wakeup(Cycles(1));
                return;
            }
        }
    }
}

int
SwitchAllocator::get_vnet(int invc)
{
    int vnet = invc/m_vc_per_vnet;
    assert(vnet < m_router->get_num_vnets());
    return vnet;
}


// Clear the request vector within the allocator at end of SA-II.
// Was populated by SA-I.
void
SwitchAllocator::clear_request_vector()
{
    std::fill(m_port_requests.begin(), m_port_requests.end(), -1);
}

void
SwitchAllocator::resetStats()
{
    m_input_arbiter_activity = 0;
    m_output_arbiter_activity = 0;
}

} // namespace garnet
} // namespace ruby
} // namespace gem5
