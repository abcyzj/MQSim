#include "../sim/Sim_Defs.h"
#include "../sim/Engine.h"
#include "PCIe_Link.h"
#include "PCIe_Message.h"
#include <stdexcept>
#include <limits>
#include <memory>
#include <cmath>

namespace Host_Components
{
	PCIe_Link::PCIe_Link(const sim_object_id_type& id, PCIe_Root_Complex* root_complex, PCIe_Switch* pcie_switch,
		double lane_bandwidth_GBPs, int lane_count, int tlp_header_size,
		int tlp_max_payload_size, int dllp_ovehread, int ph_overhead) :
		Sim_Object(id), root_complex(root_complex), pcie_switch(pcie_switch),
		lane_bandwidth_GBPs(lane_bandwidth_GBPs), lane_count(lane_count),
		tlp_header_size(tlp_header_size), tlp_max_payload_size(tlp_max_payload_size), dllp_ovehread(dllp_ovehread), ph_overhead(ph_overhead)
	{
		if (!std::isfinite(lane_bandwidth_GBPs) || lane_bandwidth_GBPs <= 0 || lane_count <= 0 ||
            tlp_max_payload_size <= 0 || tlp_header_size < 0 || dllp_ovehread < 0 || ph_overhead < 0 ||
            int64_t(tlp_header_size) + dllp_ovehread + ph_overhead > INT32_MAX - 4)
            throw std::invalid_argument("invalid PCIe link configuration");
        packet_overhead = ph_overhead + dllp_ovehread + tlp_header_size;
	}

	void PCIe_Link::Set_root_complex(PCIe_Root_Complex* root_complex)
	{
		this->root_complex = root_complex;
	}

	void PCIe_Link::Set_pcie_switch(PCIe_Switch* pcie_switch) 
	{
		this->pcie_switch = pcie_switch;
	}

    PCIe_Link::~PCIe_Link() {
        // Native payload ownership is unchanged. Extended envelopes own only callbacks.
        for (auto* queue : {&Message_buffer_toward_root_complex, &Message_buffer_toward_ssd_device}) {
            while (!queue->empty()) {
                auto* message = queue->front(); queue->pop();
                if (message->Type == PCIe_Message_Type::PLATFORM) delete message;
            }
        }
    }

    void PCIe_Link::notify(const char* stage, PCIe_Message* message) {
        if (observe) observe(stage, {message->Link_id, message->Platform_id, Simulator->Time(),
            message->Payload_size, Wire_bytes(*message), estimate_transfer_time(message),
            message->Destination, message->Type});
    }

    void PCIe_Link::start_transfer(PCIe_Message* message) {
        auto service = estimate_transfer_time(message);
        if (service > std::numeric_limits<sim_time_type>::max() - Simulator->Time())
            throw std::overflow_error("PCIe delivery time overflow");
        notify("start", message);
        Simulator->Register_sim_event(Simulator->Time() + service, this,
            (void*)(intptr_t)message->Destination, static_cast<int>(PCIe_Link_Event_Type::DELIVER));
    }

    void PCIe_Link::Deliver(PCIe_Message* message) {
        if (message->Destination != PCIe_Destination_Type::HOST && message->Destination != PCIe_Destination_Type::DEVICE)
            throw std::invalid_argument("invalid PCIe destination");
        if (message->Type == PCIe_Message_Type::PLATFORM) {
            if (!message->Platform_arrived || Wire_bytes(*message) > INT32_MAX)
                throw std::invalid_argument("invalid or oversized PCIe platform envelope");
            // Preserve native truncation, but reject unrepresentable extended service times.
            auto wire = Wire_bytes(*message);
            double delay = double(wire / lane_count + (wire % lane_count != 0)) / lane_bandwidth_GBPs;
            if (!std::isfinite(delay) || delay >= double(UINT64_MAX) || delay > double(UINT64_MAX - Simulator->Time()))
                throw std::overflow_error("PCIe platform delivery time overflow");
        }
        if (next_link_id == UINT64_MAX) throw std::overflow_error("PCIe message ID overflow");
        message->Link_id = ++next_link_id;
        notify("enqueue", message);
        auto& queue = message->Destination == PCIe_Destination_Type::HOST ?
            Message_buffer_toward_root_complex : Message_buffer_toward_ssd_device;
        auto delivering = message->Destination == PCIe_Destination_Type::HOST ? delivering_host : delivering_device;
        queue.push(message);
        if (queue.size() == 1 && !delivering) start_transfer(message);
    }

	void PCIe_Link::Start_simulation() {}

	void PCIe_Link::Validate_simulation_config() {}

    void PCIe_Link::Execute_simulator_event(MQSimEngine::Sim_Event* event) {
        auto destination = (PCIe_Destination_Type)(intptr_t)event->Parameters;
        auto& queue = destination == PCIe_Destination_Type::HOST ?
            Message_buffer_toward_root_complex : Message_buffer_toward_ssd_device;
        auto& delivering = destination == PCIe_Destination_Type::HOST ? delivering_host : delivering_device;
        if (queue.empty()) throw std::logic_error("PCIe delivery without message");
        auto* message = queue.front(); queue.pop();
        // Endpoint callbacks may append to this same FIFO. Schedule the new head exactly once.
        delivering = true;
        notify("arrive", message);
        if (message->Type == PCIe_Message_Type::PLATFORM) {
            std::unique_ptr<PCIe_Message> owned(message);
            if (!message->Platform_arrived) throw std::logic_error("missing platform endpoint");
            message->Platform_arrived();
        } else if (destination == PCIe_Destination_Type::HOST) root_complex->Consume_pcie_message(message);
        else pcie_switch->Deliver_to_device(message);
        delivering = false;
        if (!queue.empty()) start_transfer(queue.front());
    }
}
