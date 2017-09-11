#include <iostream>
#include "Lobby.hpp"

int main(int argc, char* argv[]) {


    auto network = NetworkObservable::create();

    class S : public INetwork::Listener {
    public:
        void
        onMessage(sp<INetwork> network, sp<INetwork::Endpoint> endpoint, sp<INetwork::TextMessage> message) override {
            LOGF() << ": endpoint " << *endpoint << ", message: " << *message;
        }

        void
        onMessage(sp<INetwork> network, sp<INetwork::Endpoint> endpoint, sp<INetwork::BinaryMessage> message) override {

        }

        void onConnected(sp<INetwork> network, sp<INetwork::Endpoint> endpoint) override {
            LOGF() << ": client " << *endpoint << " connected";
            network->send(endpoint, std::make_shared<INetwork::TextMessage>("hello"));
        }

        void onDisconnected(sp<INetwork> network, sp<INetwork::Endpoint> endpoint) override {
            LOGF() << ": client " << *endpoint << " disconnected";
        }

    };
    auto listener = std::make_shared<S>();
    network->subscribe(listener);

    network->run();

    return 0;
}