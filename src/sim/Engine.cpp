#include <stdexcept>
#include "Engine.h"
#include "../utils/Logical_Address_Partitioning_Unit.h"

namespace MQSimEngine
{
	Engine* Engine::_instance = NULL;

	Engine* Engine::Instance() {
		if (_instance == 0) {
			_instance = new Engine;
		}
		return _instance;
	}

	void Engine::Reset()
	{
		queue_changed = {};
        dispatching = false;
		_EventList->Clear();
		_ObjectList.clear();
		_sim_time = 0;
		stop = false;
		started = false;
		Utils::Logical_Address_Partitioning_Unit::Reset();
	}


	//Add an object to the simulator object list
	void Engine::AddObject(Sim_Object* obj)
	{
		if (_ObjectList.find(obj->ID()) != _ObjectList.end()) {
			throw std::invalid_argument("Duplicate object key: " + obj->ID());
		}
		_ObjectList.insert(std::pair<sim_object_id_type, Sim_Object*>(obj->ID(), obj));
	}
	
	Sim_Object* Engine::GetObject(sim_object_id_type object_id)
	{
		auto itr = _ObjectList.find(object_id);
		if (itr == _ObjectList.end()) {
			return NULL;
		}

		return (*itr).second;
	}

	void Engine::RemoveObject(Sim_Object* obj)
	{
		std::unordered_map<sim_object_id_type, Sim_Object*>::iterator it = _ObjectList.find(obj->ID());
		if (it == _ObjectList.end()) {
			throw std::invalid_argument("Removing an unregistered object.");
		}
		_ObjectList.erase(it);
	}

	/// This is the main method of simulator which starts simulation process.
	void Engine::Initialize_simulation()
	{
		if (started) {
			throw std::logic_error("MQSim engine already initialized");
		}
		started = true;

		for(std::unordered_map<sim_object_id_type, Sim_Object*>::iterator obj = _ObjectList.begin();
			obj != _ObjectList.end();
			++obj) {
			if (!obj->second->IsTriggersSetUp()) {
				obj->second->Setup_triggers();
			}
		}

		for (std::unordered_map<sim_object_id_type, Sim_Object*>::iterator obj = _ObjectList.begin();
			obj != _ObjectList.end();
			++obj) {
			obj->second->Validate_simulation_config();
		}
		
		for (std::unordered_map<sim_object_id_type, Sim_Object*>::iterator obj = _ObjectList.begin();
			obj != _ObjectList.end();
			++obj) {
			obj->second->Start_simulation();
		}
		
	}

	void Engine::Start_simulation()
	{
		Initialize_simulation();
		Sim_Event* ev = NULL;
		while (true) {
			if (_EventList->Count == 0 || stop) {
				break;
			}

			EventTreeNode* minNode = _EventList->Get_min_node();
			ev = minNode->FirstSimEvent;

			_sim_time = ev->Fire_time;

			while (ev != NULL) {
				if(!ev->Ignore) {
					ev->Target_sim_object->Execute_simulator_event(ev);
				}
				Sim_Event* consumed_event = ev;
				ev = ev->Next_event;
				delete consumed_event;
			}
			_EventList->Remove(minNode);
		}
	}

	void Engine::Stop_simulation()
	{
		stop = true;
	}

	bool Engine::Has_started()
	{
		return started;
	}

	sim_time_type Engine::Time()
	{
		return _sim_time;
	}

	Sim_Event* Engine::Register_sim_event(sim_time_type fireTime, Sim_Object* targetObject, void* parameters, int type)
	{
        if (fireTime < _sim_time) throw std::logic_error("MQSim event before now");
		Sim_Event* ev = new Sim_Event(fireTime, targetObject, parameters, type);
		DEBUG("RegisterEvent " << fireTime << " " << targetObject)
		_EventList->Insert_sim_event(ev);
        if (queue_changed) queue_changed();
		return ev;
	}

	void Engine::Ignore_sim_event(Sim_Event* ev)
	{
		ev->Ignore = true;
        if (queue_changed) queue_changed();
	}

    std::optional<sim_time_type> Engine::Peek_next_time()
    {
        // Ignored events own only their event envelope, never Parameters.
        while (_EventList->Count) {
            auto* node = _EventList->Get_min_node();
            auto* ev = node->FirstSimEvent;
            if (!ev->Ignore) return ev->Fire_time;
            node->FirstSimEvent = ev->Next_event;
            if (!node->FirstSimEvent) _EventList->Remove(node);
            delete ev;
        }
        return std::nullopt;
    }

    void Engine::Synchronize_now(sim_time_type now)
    {
        if (!started) throw std::logic_error("MQSim synchronize before initialize");
        if (now < _sim_time) throw std::logic_error("MQSim time reversal");
        const auto next = Peek_next_time();
        if (next && *next < now) throw std::logic_error("MQSim unprocessed event before SST now");
        _sim_time = now;
    }

    size_t Engine::Dispatch_current_time(sim_time_type now, size_t limit)
    {
        if (dispatching) throw std::logic_error("MQSim recursive dispatch");
        Synchronize_now(now);
        dispatching = true;
        size_t count = 0;
        try {
            while (!stop) {
                const auto next = Peek_next_time();
                if (!next || *next != now) break;
                if (count == limit) {
                    auto* pending = _EventList->Get_min_node()->FirstSimEvent;
                    throw std::logic_error("MQSim same-time dispatch limit exceeded time=" + std::to_string(now)
                        + " target=" + pending->Target_sim_object->ID() + " type=" + std::to_string(pending->Type));
                }
                auto* node = _EventList->Get_min_node();
                auto* ev = node->FirstSimEvent;
                node->FirstSimEvent = ev->Next_event;
                if (!node->FirstSimEvent) _EventList->Remove(node);
                // Pop before callback: same-time additions append after existing FIFO.
                try { ev->Target_sim_object->Execute_simulator_event(ev); }
                catch (...) { delete ev; throw; }
                delete ev;
                ++count;
            }
        } catch (...) { dispatching = false; throw; }
        dispatching = false;
        return count;
    }

    void Engine::Set_event_queue_changed_callback(std::function<void()> callback)
    {
        queue_changed = std::move(callback);
    }

	bool Engine::Is_integrated_execution_mode()
	{
		return false;
	}
}