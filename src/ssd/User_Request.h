#ifndef USER_REQUEST_H
#define USER_REQUEST_H

#include <string>
#include <list>
#include "SSD_Defs.h"
#include "../sim/Sim_Defs.h"
#include "Host_Interface_Defs.h"
#include "NVM_Transaction.h"

namespace SSD_Components
{
	enum class UserRequestType { READ, WRITE };
	enum class RequestCompletionRoute { HOST, DEVICE_LOCAL };
	class NVM_Transaction;
	class User_Request
	{
	public:
		User_Request();
		// HOST is the unchanged native path. Local requests are owned by the interface.
		RequestCompletionRoute Completion_route = RequestCompletionRoute::HOST;
		uint64_t Local_token = 0;
		bool Local_completion_queued = false;
		IO_Flow_Priority_Class::Priority Priority_class;
		io_request_id_type ID;
		LHA_type Start_LBA;

		sim_time_type STAT_InitiationTime;
		sim_time_type STAT_ResponseTime;
		std::list<NVM_Transaction*> Transaction_list;
		unsigned int Sectors_serviced_from_cache;

		unsigned int Size_in_byte;
		unsigned int SizeInSectors;
		UserRequestType Type;
		stream_id_type Stream_id;
		bool ToBeIgnored;
		void* IO_command_info;//used to store host I/O command info
		void* Data;
	private:
		static unsigned int lastId;
	};
}

#endif // !USER_REQUEST_H
