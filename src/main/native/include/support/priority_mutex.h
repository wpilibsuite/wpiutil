/*----------------------------------------------------------------------------*/
/* Copyright (c) 2016-2017 FIRST. All Rights Reserved.                        */
/* Open Source Software - may be modified and shared by FRC teams. The code   */
/* must be accompanied by the FIRST BSD license file in the root directory of */
/* the project.                                                               */
/*----------------------------------------------------------------------------*/

#pragma once

// Allows usage with std::lock_guard without including <mutex> separately
#include <mutex>

#ifdef __linux__
#include <bits/wordsize.h>
#include <pthread.h>
#endif

namespace wpi {

#ifdef __linux__

#define WPI_HAVE_PRIORITY_MUTEX 1

class priority_recursive_mutex {
 public:
  typedef pthread_mutex_t* native_handle_type;

  constexpr priority_recursive_mutex() noexcept = default;
  priority_recursive_mutex(const priority_recursive_mutex&) = delete;
  priority_recursive_mutex& operator=(const priority_recursive_mutex&) = delete;

  // Lock the mutex, blocking until it's available.
  void lock() { pthread_mutex_lock(&m_mutex); }

  // Unlock the mutex.
  void unlock() { pthread_mutex_unlock(&m_mutex); }

  // Tries to lock the mutex.
  bool try_lock() noexcept { return !pthread_mutex_trylock(&m_mutex); }

  pthread_mutex_t* native_handle() { return &m_mutex; }

 private:
// Do the equivalent of setting PTHREAD_PRIO_INHERIT and
// PTHREAD_MUTEX_RECURSIVE_NP.
#if __WORDSIZE == 64
  pthread_mutex_t m_mutex = {
      {0, 0, 0, 0, 0x20 | PTHREAD_MUTEX_RECURSIVE_NP, 0, 0, {0, 0}}};
#else
  pthread_mutex_t m_mutex = {
      {0, 0, 0, 0x20 | PTHREAD_MUTEX_RECURSIVE_NP, 0, {0}}};
#endif
};

class priority_mutex {
 public:
  typedef pthread_mutex_t* native_handle_type;

  constexpr priority_mutex() noexcept = default;
  priority_mutex(const priority_mutex&) = delete;
  priority_mutex& operator=(const priority_mutex&) = delete;

  // Lock the mutex, blocking until it's available.
  void lock() { pthread_mutex_lock(&m_mutex); }

  // Unlock the mutex.
  void unlock() { pthread_mutex_unlock(&m_mutex); }

  // Tries to lock the mutex.
  bool try_lock() noexcept { return !pthread_mutex_trylock(&m_mutex); }

  pthread_mutex_t* native_handle() { return &m_mutex; }

 private:
// Do the equivalent of setting PTHREAD_PRIO_INHERIT.
#if __WORDSIZE == 64
  pthread_mutex_t m_mutex = {{0, 0, 0, 0, 0x20, 0, 0, {0, 0}}};
#else
  pthread_mutex_t m_mutex = {{0, 0, 0, 0x20, 0, {0}}};
#endif
};

#endif  // __linux__

}  // namespace wpi
