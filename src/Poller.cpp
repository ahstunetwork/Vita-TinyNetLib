//
// Created by vitanmc on 2023/3/5.
//

#include "Poller.h"
#include "Channel.h"

namespace Vita {


    Poller::Poller(EventLoop *loop)
            : ownerLoop_(loop) {}

    bool Poller::hasChannel(Channel *channel) const {
        auto it = channels_.find(channel->get_fd());
        return it != channels_.end() && it->second == channel;
    }


} // Vita