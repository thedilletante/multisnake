#ifndef MULTISNAKE_UTIL_HPP
#define MULTISNAKE_UTIL_HPP

#include <memory>
#include <iostream>
#include <cassert>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <thread>
#include <list>

template <class T>
using up = std::unique_ptr<T>;
template <class T>
using sp = std::shared_ptr<T>;
template <class T>
using wp = std::weak_ptr<T>;

class LOG {
public:
    enum class Level {
        ERROR,
        WARNING,
        INFO,
        DEBUG
    };

private:
    char getLevelLetter(Level level) {
        switch (level) {
            case Level::ERROR:
                return 'E';
            case Level::WARNING:
                return 'W';
            case Level::INFO:
                return 'I';
            case Level::DEBUG:
                return 'D';
        }
        return 'U';
    }
public:
    explicit LOG(Level level) {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);

        stream << std::put_time(std::localtime(&in_time_t), "[%Y-%m-%d %X] ");
        stream << "[" << getLevelLetter(level) << "] ";
        stream << "[" << std::this_thread::get_id() << "] ";
    }

    ~LOG() {
        impl().os << stream.str() << std::endl;
    }

    template <class T>
    LOG &operator<<(T&& obj) {
        stream << obj;
        return *this;
    }

private:
    struct LogImpl {
        std::ostream &os;
    };

    static LogImpl &impl() {
        static LogImpl impl_ { std::cout };
        return impl_;
    }

    std::stringstream stream;
};

class Assert {
public:
    explicit Assert(bool assertion)
        : assertion {assertion} {}

    ~Assert() {
        if (!assertion) {
            LOG(LOG::Level::ERROR) << stream.str();
            assert(false);
        }
    }

    template <class T>
    Assert &operator<<(T&& obj) {
        stream << obj;
        return *this;
    }

private:
    std::stringstream stream;
    bool assertion;
};

#define LOGD() LOG(LOG::Level::DEBUG)<< __PRETTY_FUNCTION__ << ": "
#define LOGI() LOG(LOG::Level::INFO)<< __PRETTY_FUNCTION__ << ": "
#define LOGW() LOG(LOG::Level::WARNING)<< __PRETTY_FUNCTION__ << ": "
#define LOGE() LOG(LOG::Level::ERROR)<< __PRETTY_FUNCTION__ << ": "
#define LOGF() LOG(LOG::Level::INFO)<< __PRETTY_FUNCTION__
#define ASSERT(x) Assert(x) << "ASSERTED: " << #x << " " << __PRETTY_FUNCTION__


namespace std {
    // to enable ADL
    inline ostream &operator<<(ostream &os, const weak_ptr<void> &hdl) {
        return os << "websocketpp::connection_hdl{" << hdl.lock().get() << "}";
    }
}

class Printable {
public:
    virtual ~Printable() = default;

    virtual void print(std::ostream &os) const {
        os << "Printable{" << this << "}";
    }
};

inline std::ostream &operator<<(std::ostream &os, const Printable &printable) {
    printable.print(os);
    return os;
}

template <class Listener>
class Observable {
private:
    class ListenerComparator
            : public std::unary_function<wp<Listener>, bool> {
    public:
        explicit ListenerComparator(sp<Listener> needed)
            : needed {needed} {}

        bool operator()(wp<Listener> inListListener) {
            if (auto strongInListListener = inListListener.lock()) {
                return strongInListListener == needed;
            }
            return false;
        }
    private:
        sp<Listener> needed;
    };
public:
    virtual ~Observable() = default;

    void subscribe(wp<Listener> listener) {
        LOGF();

        if (auto strongListener = listener.lock()) {
            auto found = std::find_if(std::begin(listeners), std::end(listeners), ListenerComparator(strongListener));
            if (found != std::end(listeners)) {
                LOGD() << "The listener " << *strongListener << " is already subscribed";
            } else {
                LOGD() << "Subscribing the listener " << *strongListener;
                listeners.push_back(listener);
            }
        } else {
            LOGW() << "Trying to subscribe expired listener";
        }
    }

    void unsubscibe(wp<Listener> listener) {
        LOGF();

        if (auto strongListener = listener.lock()) {
            LOGD() << "Removing the listener" << *strongListener;
            listeners.remove_if(ListenerComparator(strongListener));
        } else {
            LOGW() << "Tring to remove expired listener";
        }
    }

protected:
    using NotificationCallback = std::function<void(sp<Listener> listener)>;

    void notify(NotificationCallback callback) {
        LOGF();
        bool areExpiredHere = false;

        for (auto listener : listeners) {
            if (auto strongListener = listener.lock()) {
                LOGD() << "Notifying the listener " << *strongListener;
                callback(strongListener);
            } else {
                areExpiredHere = true;
            }
        }

        if (areExpiredHere) {
            removeExpired();
        }
    }

    bool areListenersHere() const {
        return !listeners.empty();
    }

private:
    void removeExpired() {
        LOGF();
        listeners.remove_if([](wp<Listener> listener){ return listener.expired(); });
    }

    std::list<wp<Listener>> listeners;
};

#endif //MULTISNAKE_UTIL_HPP
