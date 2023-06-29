#pragma once

#include <atomic>
#include <condition_variable>
#include <shared_mutex>

namespace rwlock {

class ReadWriteLock {
public:
    void lock_shared();
    void unlock_shared();
    void lock();
    void unlock();

private:
    std::shared_mutex m_mutex;
    std::condition_variable_any m_waitCv;
    std::atomic<int32_t> m_readerCount{};
    std::atomic_flag m_writer{};
};
}  // namespace rwlock