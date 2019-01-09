/*
 * Copyright (C) 2019 Zilliqa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef __SINGLETON_H__
#define __SINGLETON_H__

template <typename T>
class Singleton {
 protected:
  Singleton() noexcept = default;

  Singleton(const Singleton&) = delete;

  Singleton& operator=(const Singleton&) = delete;

  virtual ~Singleton() = default;  // to silence base class Singleton<T> has a
                                   // non-virtual destructor [-Weffc++]

 public:
  static T& GetInstance() noexcept(std::is_nothrow_constructible<T>::value) {
    // Guaranteed to be destroyed.
    // Instantiated on first use.
    // Thread safe in C++11
    static T instance;

    return instance;
  }
};

#endif  // __SINGLETON_H__
