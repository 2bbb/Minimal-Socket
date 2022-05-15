/**
 * Author:    Andrea Casalino
 * Created:   16.05.2019
 *
 * report any bug to andrecasa91@gmail.com.
 **/

#include <MinimalSocket/udp/UdpSocket.h>

#include <Args.h>
#include <Respond.h>

using namespace std;

int main(const int argc, const char **argv) {
  cout << "-----------------------  Udp responder  -----------------------"
       << endl;
  PARSE_ARGS

  const auto port_this = static_cast<MinimalSocket::Port>(
      std::atoi(options->getValue("port_this").c_str()));
  const auto family =
      MinimalSocket::samples::to_family(options->getValue("family", "v4"));
  const bool connect = options->getValue("connect", "no") == "yes";

  MinimalSocket::udp::UdpBinded responder(port_this, family);

  if (!responder.open()) {
    cout << "Failed to reserve port " << port_this << endl;
    return EXIT_FAILURE;
  }

  if (connect) {
    // connect to first sending a request
    std::string first_request;
    auto connected_responder = responder.connect(&first_request);

    const auto &first_response =
        MinimalSocket::samples::NamesCircularIterator::NAMES_SURNAMES
            .find(first_request)
            ->second;
    connected_responder.send(first_response);

    MinimalSocket::samples::respond_forever(connected_responder);
  } else {
    // use as un-connected udp
    MinimalSocket::samples::respond_forever(responder);
  }

  return EXIT_SUCCESS;
}
