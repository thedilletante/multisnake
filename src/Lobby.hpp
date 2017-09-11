#ifndef Speechkit_Lobby_H
#define Speechkit_Lobby_H

#include "util.hpp"

#include <list>
#include <map>
#include <algorithm>
#include <iterator>
#include <string>
#include <vector>
#include <thread>

#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

#include <experimental/thread_pool>
#include <experimental/future>

namespace executors = std::experimental;

using serverpp = websocketpp::server<websocketpp::config::asio>;

class INetwork
    : virtual public Printable {
public:
    struct TextMessage final
            : public Printable {
        void print(std::ostream &os) const override {
            os << message;
        }
        std::string message;
        explicit TextMessage(const std::string& message) : message {message} {}
    };
    struct BinaryMessage final {
        std::vector<uint8_t> data;

        template <class InputIterator>
        explicit BinaryMessage(InputIterator begin, InputIterator end)
            : data(begin, end) {}
    };
    struct Endpoint final
        : public Printable {
        void print(std::ostream &os) const override {
            os << "Endpoint{" << address << "," << hdl << "}";
        }

        std::string address;
        websocketpp::connection_hdl hdl;

        explicit Endpoint(serverpp::connection_ptr con) {
            address = con->get_remote_endpoint();
            hdl = con->get_handle();
        }
    };

    class Listener
        : public Printable {
    public:
        virtual ~Listener() = default;
        virtual void onConnected(sp<INetwork> network, sp<Endpoint> endpoint) {}
        virtual void onDisconnected(sp<INetwork> network, sp<Endpoint> endpoint) {}
        virtual void onMessage(sp<INetwork> network, sp<Endpoint> endpoint, sp<TextMessage> message) {}
        virtual void onMessage(sp<INetwork> network, sp<Endpoint> endpoint, sp<BinaryMessage> message) {}

        void print(std::ostream &os) const override {
            os << "NetworkListener{" << this << "}";
        }
    };
public:
    virtual ~INetwork() = default;

    virtual void send(sp<Endpoint> endpoint, sp<TextMessage> message) = 0;
    virtual void send(sp<Endpoint> endpoint, sp<BinaryMessage> message) = 0;
    virtual void disconnect(sp<Endpoint> endpoint) = 0;
    virtual void run() = 0;
    virtual void stop() = 0;

    void print(std::ostream &os) const override {
        os << "INetwork{" << this << "}";
    }
};

class NetworkObservable
    : public INetwork
    , public Observable<INetwork::Listener>{
public:
    static sp<NetworkObservable> create();
};

class Network
    : public NetworkObservable
    , public std::enable_shared_from_this<Network> {
public:
    ~Network() {
        LOGF();
    }

    void send(sp<Endpoint> endpoint, sp<TextMessage> message) override {
        LOGF();

        websocketpp::lib::error_code err;
        server->send(endpoint->hdl, message->message, websocketpp::frame::opcode::text, err);

        LOGI() << "Sent " << *message << " to " << *endpoint;
    }

    void send(sp<Endpoint> endpoint, sp<BinaryMessage> message) override {
    }

    void disconnect(sp<Endpoint> endpoint) override {
    }

    void stop() override {
        LOGF();
    }

    void run() override {
        LOGF();

        wp<Network> weak = shared_from_this();

        server->set_open_handler([weak](websocketpp::connection_hdl hdl){
            if (auto self = weak.lock()) self->onConnected(hdl);
        });

        server->set_close_handler([weak](websocketpp::connection_hdl hdl){
            if (auto self = weak.lock()) self->onDisconnected(hdl);
        });
        server->set_message_handler([weak](websocketpp::connection_hdl hdl, serverpp::message_ptr msg){
            if (auto self = weak.lock()) self->onMessage(hdl, msg);
        });

        server->init_asio();
        server->set_reuse_addr(true);

        server->listen(9012);

        server->start_accept();

        server->run();
    }

private:
    sp<serverpp> server = std::make_shared<serverpp>();

    void onConnected(websocketpp::connection_hdl hdl) {
        LOGF() << " with " << hdl;

        auto found = clients.find(hdl);
        if (found != clients.end()) {
            LOGE() << "Client " << found->second << " is already connected";
            return;
        }

        auto endpoint = std::make_shared<Endpoint>(server->get_con_from_hdl(hdl));
        auto inserted = clients.insert(std::make_pair(hdl, endpoint)).second;
        ASSERT(inserted) << "Fail to add " << *endpoint;

        LOGI() << "Added endpoint " << *endpoint;
        notify([this, endpoint](sp<Listener> listener) {
            listener->onConnected(shared_from_this(), endpoint);
        });
    }

    void onDisconnected(websocketpp::connection_hdl hdl) {
        LOGF() << " with " << hdl;

        auto found = clients.find(hdl);
        if (found == clients.end()) {
            LOGW() << hdl << " is already removed somehow";
            return;
        }

        auto endpoint = found->second;
        notify([this, endpoint](sp<Listener> listener){ listener->onDisconnected(shared_from_this(), endpoint); });

        LOGI() << "Removing " << *endpoint << " from the endpoints map";
        clients.erase(found);
    }

    void onMessage(websocketpp::connection_hdl hdl, serverpp::message_ptr msg) {
        LOGF();

        auto found = clients.find(hdl);
        if (found == clients.end()) {
            LOGW() << "Client " << hdl << " should be here";
            return;
        }

        auto endpoint = found->second;
        if (msg->get_opcode() == websocketpp::frame::opcode::text) {
            auto message = std::make_shared<TextMessage>(msg->get_payload());
            notify([this, endpoint, message](sp<Listener>listener){
                listener->onMessage(shared_from_this(), endpoint, message);
            });
        } else {
            auto message = std::make_shared<BinaryMessage>(
                std::begin(msg->get_payload()), std::end(msg->get_payload()));
            notify([this, endpoint, message](sp<Listener>listener){
                listener->onMessage(shared_from_this(), endpoint, message);
            });
        }
    }

    std::map<websocketpp::connection_hdl, sp<Endpoint>, std::owner_less<websocketpp::connection_hdl>> clients;
};

inline sp<NetworkObservable> NetworkObservable::create() {
    LOGF();
    auto net = std::make_shared<Network>();
    return net;
}

class IClient
    : virtual public Printable {
public:
    class Request;
    class Message;

    class Listener
        : virtual public Printable {
    public:
        virtual ~Listener() = default;
        virtual void onRequest(sp<IClient>, sp<Request> request) = 0;
        virtual void onGone(sp<IClient>) = 0;


        void print(std::ostream &os) const override {
            os << "ClientListener{" << this << "}";
        }
    };
public:
    virtual ~IClient() = default;
    virtual void send(sp<Message> message) = 0;
    virtual void close() = 0;
    void print(std::ostream &os) const override {
        os << "IClient{" << this << "}";
    }
};

class Client
    : public IClient
    , public Observable<IClient::Listener>
    , virtual public Printable {
public:
    void print(std::ostream &os) const override {
        os << "Client{" << this << "}";
    }

    void send(sp<Message> message) override {}

    void close() override {

    }

};

class Game;

class Lobby {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void onGameCreated(sp<Game> game);
        virtual void onClientGone(sp<Client> client);
    };

    virtual ~Lobby() = default;

    virtual void setClient(sp<Client> client);
    virtual void removeClient(sp<Client> client);

protected:
    explicit Lobby(wp<Listener> listener)
        : listener { listener } {}

    const wp<Listener> listener;
};

template <class Listener, class Callable>
inline void notify(wp<Listener> listener, Callable callback) {
    LOGF();

    if (auto slistener = listener.lock()) {
        LOGD() << "Notifying the listener " << *slistener;
        callback(slistener);
    } else {
        LOGW() << "Tring to notify expired listener" << *slistener;
    }
}

class Player
    : public IClient::Listener
    , public std::enable_shared_from_this<Player>
    , virtual public Printable {
public:
    using body_t = std::list<std::pair<unsigned, unsigned>>;

    class Message {
    public:
        virtual ~Message() = default;

        virtual body_t apply(body_t body) = 0;
    };

    class Listener
        : virtual public Printable {
    public:
        virtual ~Listener() = default;
        virtual void onGone(sp<Player> player) = 0;
        virtual void onMessage(sp<Player> player, up<Message> message) = 0;
        void print(std::ostream &os) const override {
            os << "PlayerListener{" << this << "}";
        }
    };

    virtual ~Player() = default;

    static sp<Player> create(wp<Listener> listener, sp<Client> client) {
        auto player = std::make_shared<Player>(listener);
        player->subscribeOn(client);
        return player;
    }

public:
    explicit Player(wp<Listener> listener)
        : listener {listener} {}

    void subscribeOn(sp<Client> client) {
        client->subscribe(shared_from_this());
    }

    wp<Listener> listener;

    void print(std::ostream &os) const override {
        os << "Player{" << this << "}";
    }

private:

    void onRequest(sp<IClient> client, sp<IClient::Request> request) override {
        LOGF();

        // parse request

    }

    void onGone(sp<IClient> client) override {
        LOGF();
        LOGD() << "Client " << *client << " is gone";
        notify(listener, [this](sp<Listener> listener) { listener->onGone(shared_from_this()); });
    }
};

inline std::ostream &operator<<(std::ostream &os, const Player &player) {
    return os << "Player{" << &player << "}";
}

class Timer {
public:
    class Listener {
    public:
        virtual ~Listener() = default;
        virtual void onTimeout(sp<Timer> timer) = 0;
    };

    virtual ~Timer() = default;
};


class GameBoard {
public:
    virtual ~GameBoard() = default;
    static sp<GameBoard> create(std::list<sp<Client>> clients);
};

class GameBoardImpl
    : public GameBoard
    , public Timer::Listener
    , public Player::Listener
    , public std::enable_shared_from_this<GameBoardImpl>
    , virtual public Printable {
public:
    explicit GameBoardImpl() = default;

    void init(const std::list<sp<Client>> &clients) {
        LOGF();

        for (auto client : clients) {
            auto player = Player::create(shared_from_this(), client);
            board[player] = placeNewPlayer();
        }

        adjustBoardState();
    }

    void onGone(sp<Player> player) override {
        LOGF();
        auto p = board.find(player);
        if (p == board.end()) {
            LOGD() << "there is no such player: " << *player;
            return;
        }

        LOGI() << *player << " has gone";
        board.erase(p);

        adjustBoardState();
    }

    void onMessage(sp<Player> player, up<Player::Message> message) override {
        LOGF();
        auto p = board.find(player);
        if (p == board.end()) {
            LOGD() << "there is no such player";
            return;
        }

        adjustBoardState();

        auto &body = p->second;
        body = message->apply(body);

        // recalculate the next timer
    }

    void onTimeout(sp<Timer> timer) override {
        LOGF();
        for (auto &task : timer_tasks) {
            task();
        }
    }

    void print(std::ostream &os) const override {
        os << "GameBoard{" << this << "}";
    }

private:


    Player::body_t placeNewPlayer() {
        return {};
    }

    using board_t = std::map<sp<Player>, Player::body_t>;
    using timer_tasks_t = std::list<std::function<void()>>;

    board_t board;
    timer_tasks_t timer_tasks;

    // TODO: to not recalculate the state after timer is expired
    // we could have a set of callbacks and push specific actions to it while calculating
    // for instance (send everybody they are done, close game, etc.)
    // and execute it after timer expired
    void adjustBoardState() {
        LOGF();
    }
};


inline sp<GameBoard> GameBoard::create(std::list<sp<Client>> clients) {
    LOGF();
    ASSERT(!clients.empty()) << "Somebody created GameBoard with empty list of client";

    auto board = std::make_shared<GameBoardImpl>();
    board->init(clients);

    return board;
}




#endif // Speechkit_Lobby_H
