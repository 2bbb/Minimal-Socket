#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <list>
#include <omp.h>
#include <thread>

#include <MinimalSocket/Error.h>

#include "ConnectionsUtils.h"
#include "Parallel.h"
#include "PortFactory.h"
#include "SlicedOps.h"

using namespace MinimalSocket;
using namespace MinimalSocket::tcp;
using namespace MinimalSocket::test;

namespace {
static const std::string request = "Hello";
static const std::string response = "Welcome";

struct SenderReceiver {
  Sender &sender;
  Receiver &receiver;
};
template <typename T> SenderReceiver makeSenderReceiver(T &subject) {
  Sender &as_sender = subject;
  Receiver &as_receiver = subject;
  return SenderReceiver{as_sender, as_receiver};
}

void send_response(const SenderReceiver &requester,
                   const SenderReceiver &responder) {
  std::size_t cycles = 5;

  parallel(
      [&]() {
        for (std::size_t c = 0; c < cycles; ++c) {
          CHECK(requester.sender.send(request));
          auto received_response = requester.receiver.receive(response.size());
          CHECK(received_response == response);
        }
      },
      [&]() {
        for (std::size_t c = 0; c < cycles; ++c) {
          auto received_request = responder.receiver.receive(request.size());
          CHECK(received_request == request);
          CHECK(responder.sender.send(response));
        }
      });
}
} // namespace

TEST_CASE("Establish tcp connection", "[tcp]") {
  const auto port = PortFactory::makePort();
  const auto family = GENERATE(AddressFamily::IP_V4, AddressFamily::IP_V6);

#if !defined(_WIN32)
  SECTION("expected failure") {
    TcpClient client(Address(port, family));
    CHECK_THROWS_AS(client.open(), Error);
    CHECK_FALSE(client.wasOpened());
  }
#endif

  SECTION("expected success") {
    test::TcpPeers peers(port, family);
    auto &server_side = peers.getServerSide();
    auto &client_side = peers.getClientSide();

    REQUIRE(client_side.wasOpened());

    const std::size_t cycles = 5;
    const std::string request = "Hello";
    const std::string response = "Welcome";

    SECTION("client send, server respond") {
      send_response(makeSenderReceiver(client_side),
                    makeSenderReceiver(server_side));
    }

    SECTION("server send, client respond") {
      send_response(makeSenderReceiver(server_side),
                    makeSenderReceiver(client_side));
    }

    SECTION("receive with timeout") {
      const auto timeout = Timeout{500};

      SECTION("expect fail within timeout") {
        auto received_request = server_side.receive(request.size(), timeout);
        CHECK(received_request.empty());
      }

      SECTION("expect success within timeout") {
        const auto wait = Timeout{250};
        parallel(
            [&]() {
#pragma omp barrier
              std::this_thread::sleep_for(wait);
              client_side.send(request);
            },
            [&]() {
#pragma omp barrier
              auto received_request =
                  server_side.receive(request.size(), timeout);
              CHECK(received_request == request);
            });
      }
    }
  }
}

TEST_CASE("Establish many tcp connections to same server", "[tcp]") {
  const auto port = PortFactory::makePort();
  const auto family = GENERATE(AddressFamily::IP_V4, AddressFamily::IP_V6);

  TcpServer server(port, family);
  server.open();

  const std::size_t clients_numb = 5;

  SECTION("sequencial connnections") {
    std::list<TcpConnection> accepted_clients;
    std::list<TcpClient> clients;
    parallel(
        [&]() {
          for (std::size_t c = 0; c < clients_numb; ++c) {
            accepted_clients.emplace_back(server.acceptNewClient());
          }
        },
        [&]() {
          for (std::size_t c = 0; c < clients_numb; ++c) {
            auto &client = clients.emplace_back(Address(port, family));
            CHECK(client.open());
          }
        });
  }

  SECTION("concurrent connnections") {
    std::vector<Task> tasks;
    tasks.emplace_back([&]() {
      for (std::size_t c = 0; c < clients_numb; ++c) {
        auto accepted = server.acceptNewClient();
      }
    });
    Task ask_connection = [&]() {
      TcpClient client(Address(port, family));
      CHECK(client.open());
    };
    for (std::size_t c = 0; c < clients_numb; ++c) {
      tasks.push_back(ask_connection);
    }
    parallel(tasks);
  }
}

TEST_CASE("Open multiple times tcp clients", "[tcp]") {
  const auto port = PortFactory::makePort();
  const auto family = GENERATE(AddressFamily::IP_V4, AddressFamily::IP_V6);

  TcpServer server(port, family);
  server.open();

  std::size_t cycles = 5;

  TcpClient client(Address(port, family));

  for (std::size_t c = 0; c < cycles; ++c) {
    parallel([&]() { server.acceptNewClient(); },
             [&]() {
               CHECK(client.open());
               TcpClient{std::move(client)};
               CHECK_FALSE(client.wasOpened());
             });
  }
}

TEST_CASE("Open tcp client with timeout", "[tcp]") {
  const auto port = PortFactory::makePort();
  const auto family = GENERATE(AddressFamily::IP_V4, AddressFamily::IP_V6);

  const auto timeout = Timeout{500};

  TcpClient client(Address(port, family));

  SECTION("expect fail within timeout") {
#ifdef _WIN32
    CHECK_FALSE(client.open(timeout));
#else
    CHECK_THROWS_AS(
        client.open(timeout),
        Error); // linux throw if no server tcp were previously created, while
                // windows seems to does not have this check
#endif
    CHECK_FALSE(client.wasOpened());
  }

  SECTION("expect success within timeout") {
    const auto wait = Timeout{250};
    TcpServer server(port, family);
    REQUIRE(server.open());
    parallel(
        [&]() {
#pragma omp barrier
          std::this_thread::sleep_for(wait);
          TcpConnection conn = server.acceptNewClient();
          auto received_request = conn.receive(request.size());
          CHECK(received_request == request);
        },
        [&]() {
#pragma omp barrier
          CHECK(client.open(timeout));
          client.send(request);
        });
  }
}

TEST_CASE("Reserve random port for tcp server", "[tcp]") {
  const auto family = GENERATE(AddressFamily::IP_V4, AddressFamily::IP_V6);

  TcpServer server(ANY_PORT, family);
  REQUIRE(server.open());
  const auto port = server.getPortToBind();
  REQUIRE(port != 0);

  parallel(
      [&]() {
#pragma omp barrier
        auto accepted = server.acceptNewClient();
        auto received_request = accepted.receive(request.size());
        CHECK(received_request == request);
      },
      [&]() {
        // client
        TcpClient client(Address(port, family));
#pragma omp barrier
        REQUIRE(client.open());
        REQUIRE(client.wasOpened());
        client.send(request);
      });
}

TEST_CASE("Accept client with timeout", "[tcp]") {
  const auto family = GENERATE(AddressFamily::IP_V4, AddressFamily::IP_V6);
  const auto port = PortFactory::makePort();

  TcpServer server(port, family);
  REQUIRE(server.open());
  const auto server_address = Address(port, family);

  const auto timeout = Timeout{500};

  SECTION("expect fail within timeout") {
    // connect first client
    TcpClient client_first = TcpClient{server_address};
    std::unique_ptr<TcpConnection> server_side_first;
    parallel(
        [&]() {
#pragma omp barrier
          CHECK(client_first.open());
        },
        [&]() {
#pragma omp barrier
          auto accepted = server.acceptNewClient();
          server_side_first =
              std::make_unique<TcpConnection>(std::move(accepted));
        });

    // expect second accept to fail
    CHECK_FALSE(server.acceptNewClient(timeout));
    CHECK(server.wasOpened());

    // check first accepted connection is still valid
    parallel(
        [&]() {
#pragma omp barrier
          auto received_request = server_side_first->receive(request.size());
          CHECK(received_request == request);
        },
        [&]() {
    // client
#pragma omp barrier
          client_first.send(request);
        });

    // connect second client after accept unsuccess and check they can exchange
    // messages
    parallel(
        [&]() {
          TcpClient client_second = TcpClient{server_address};
#pragma omp barrier
          CHECK(client_second.open());
          client_second.send(request);
        },
        [&]() {
#pragma omp barrier
          auto server_side_second = server.acceptNewClient();
          auto received_request = server_side_second.receive(request.size());
          CHECK(received_request == request);
        });
  }

  SECTION("expect success within timeout") {
    const auto wait = Timeout{250};
    parallel(
        [&]() {
          TcpClient client = TcpClient{server_address};
#pragma omp barrier
          std::this_thread::sleep_for(wait);
          CHECK(client.open());
        },
        [&]() {
#pragma omp barrier
          CHECK(server.acceptNewClient(timeout));
        });
  }
}

TEST_CASE("Send Receive messages split into multiple pieces (tcp)",
#ifdef __APPLE__
          "[tcp][!mayfail]"
#else
          "[tcp]"
#endif
) {
  const auto port = PortFactory::makePort();
  const auto family = GENERATE(AddressFamily::IP_V4, AddressFamily::IP_V6);

  TcpPeers peers(port, family);
  auto &server_side = peers.getServerSide();
  auto &client_side = peers.getClientSide();

  const std::string request = "This is a simulated long message";

  const std::size_t delta = 4;

  SECTION("split receive") {
    parallel([&]() { client_side.send(request); },
             [&]() {
               auto received_request =
                   sliced_receive(server_side, request.size(), 4);
               CHECK(received_request == request);
             });
  }

  SECTION("split send") {
    parallel(
        [&]() {
          sliced_send(client_side, request, 4);
#pragma omp barrier
        },
        [&]() {
#pragma omp barrier
          auto received_request = server_side.receive(request.size());
          CHECK(received_request == request);
        });
  }
}
