#ifndef NVM_CHANNEL_BASE_H
#define NVM_CHANNEL_BASE_H

namespace SSD_Components
{
	enum class BusChannelStatus { BUSY, IDLE };
	class NVM_Channel_Base { public: virtual ~NVM_Channel_Base() = default; };
}

#endif // !NVM_CHANNEL_BASE_H
