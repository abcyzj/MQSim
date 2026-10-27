#ifndef ENGINE_H
#define ENGINE_H

#include <iostream>
#include <optional>
#include <functional>
#include <cstddef>
#include <unordered_map>
#include "Sim_Defs.h"
#include "EventTree.h"
#include "Sim_Object.h"

namespace MQSimEngine {
	class Engine
	{
		friend class EventTree;
	public:
		Engine()
		{
			this->_EventList = new EventTree;
			started = false;
		}

		~Engine() {
			delete _EventList;
		}
		
		static Engine* Instance();
		sim_time_type Time();
		Sim_Event* Register_sim_event(sim_time_type fireTime, Sim_Object* targetObject, void* parameters = NULL, int type = 0);
		void Ignore_sim_event(Sim_Event*);
		void Reset();
		void AddObject(Sim_Object* obj);
		Sim_Object* GetObject(sim_object_id_type object_id);
		void RemoveObject(Sim_Object* obj);
		// Initialize objects at time zero without consuming backend events.
		void Initialize_simulation();
        // Embedded stepping API: caller supplies the authoritative current ns.
        // Peek may reclaim ignored envelopes. Event Parameters remain caller-owned.
        std::optional<sim_time_type> Peek_next_time();
        void Synchronize_now(sim_time_type now);
        size_t Dispatch_current_time(sim_time_type now, size_t limit = 100000);
        // Observer for insert/Ignore; may peek, but must not register, Ignore or dispatch.
        // Install after initialization; coalesce notifications during dispatch.
        void Set_event_queue_changed_callback(std::function<void()> callback);
		void Start_simulation();
		void Stop_simulation();
		bool Has_started();
		bool Is_integrated_execution_mode();
	private:
		sim_time_type _sim_time = 0;
		EventTree* _EventList;
		std::unordered_map<sim_object_id_type, Sim_Object*> _ObjectList;
		bool stop = false;
		bool started;
		static Engine* _instance;
        std::function<void()> queue_changed;
        bool dispatching = false;
	};
}

#define Simulator MQSimEngine::Engine::Instance()
#endif // !ENGINE_H
