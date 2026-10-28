#ifndef PCIE_MESSAGE_H
#define PCIE_MESSAGE_H

#include<cstdint>
#include<functional>

namespace Host_Components
{
	enum class PCIe_Destination_Type {HOST, DEVICE};
	enum class PCIe_Message_Type {READ_REQ, WRITE_REQ, READ_COMP, PLATFORM};
	
	class PCIe_Message
	{
	public:
		PCIe_Destination_Type Destination;
		PCIe_Message_Type Type;
		void* Payload;
		unsigned int Payload_size;
		uint64_t Address;
		// Extended envelopes are consumed by the link endpoint, never by NVMe.
		uint64_t Link_id = 0, Platform_id = 0;
		std::function<void()> Platform_arrived;
	};
}

#endif //!PCIE_MESSAGE_H
