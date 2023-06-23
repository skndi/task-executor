#include "ReadWriteLock.hpp"

#include <iostream>

using namespace rwlock;

void ReadWriteLock::lock_shared() {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    m_waitCv.wait(lock, [this]() { return !m_writer.test(); });

    m_readerCount.fetch_add(1, std::memory_order_relaxed);
}

void ReadWriteLock::unlock_shared() {
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    if (1 == m_readerCount.fetch_sub(1, std::memory_order_relaxed)) {
        lock.unlock();
        m_waitCv.notify_all();
        return;
    }
}

void ReadWriteLock::lock() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    m_waitCv.wait(lock, [this]() { return !m_writer.test(std::memory_order_relaxed); });

    m_writer.test_and_set(std::memory_order_acq_rel);

    m_waitCv.wait(lock, [this]() { return !m_readerCount.load(std::memory_order_relaxed); });
}

void ReadWriteLock::unlock() {
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_writer.clear(std::memory_order_acq_rel);
    lock.unlock();
    m_waitCv.notify_all();
}