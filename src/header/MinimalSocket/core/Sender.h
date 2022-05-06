/**
 * Author:    Andrea Casalino
 * Created:   01.28.2020
 *
 * report any bug to andrecasa91@gmail.com.
 **/

#pragma once

#include <MinimalSocket/core/Address.h>
#include <MinimalSocket/core/Socket.h>
#include <mutex>

namespace MinimalSocket {
class Sender : public virtual Socket {
public:
  /**
   * @return true if the message was completely sent
   * @param[in] the message to send
   */
  bool send(const Buffer &message);

private:
  std::mutex send_mtx;
};

class SenderTo : public virtual Socket {
public:
  /**
   * @return true if the message was completely sent
   * @param[in] the message to send
   */
  bool sendTo(const Buffer &message, const Address &recipient);

private:
  std::mutex send_mtx;
};
} // namespace MinimalSocket
