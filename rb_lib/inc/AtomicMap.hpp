#pragma once

#include <map>
#include <shared_mutex>
#include <condition_variable>

template<typename K, typename V>
class AtomicMap {
public:
  AtomicMap<K,V>(size_t limit) : limit(limit) {};

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

private:
  size_t limit;
  std::map<K,V> map;
  std::shared_mutex m;
  std::condition_variable_any cv;
};
