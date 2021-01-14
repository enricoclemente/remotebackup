#pragma once

#include <condition_variable>
#include <exception>
#include <map>
#include <shared_mutex>

template <typename K, typename V>
class atomic_map {
public:
    atomic_map<K, V>(size_t limit) : limit(limit){};

    void add(K key, V value) {
        std::unique_lock<std::shared_mutex> lock(m);
        if (map.find(key) == map.end()) {
            cv.wait(lock, [&] { return map.size() < limit; });
        }
        map[key] = value;
    }

    void remove(K key) {
        std::unique_lock<std::shared_mutex> lock(m);
        map.erase(key);
        cv.notify_all();
    }

    bool has(K key) {
        std::shared_lock<std::shared_mutex> slock(m);
        return map.find(key) != map.end();
    }

    V get(K key) {
        std::shared_lock<std::shared_mutex> slock(m);
        return map[key];
    }

    size_t size() {
        std::shared_lock<std::shared_mutex> slock(m);
        return map.size();
    }

    class key_already_present : public std::exception {
        const char *what() const throw() {
            return "atomic_map::key_already_present";
        }
    };

private:
    size_t limit;
    std::map<K, V> map;
    std::shared_mutex m;
    std::condition_variable_any cv;
};

template <typename K, typename V>
class atomic_map_guard {
public:
    atomic_map_guard<K, V>(atomic_map<K, V> &m, K &key, V &value)
        : m(m), key(key) {
        if (m.has(key)) throw typename atomic_map<K, V>::key_already_present();
        m.add(key, value);
    }

    ~atomic_map_guard<K, V>() {
        m.remove(key);
    }

private:
    atomic_map<K, V> &m;
    K &key;
};
