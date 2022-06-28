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

#ifndef ZILLIQA_SRC_COMMON_SINGLETON_H_
#define ZILLIQA_SRC_COMMON_SINGLETON_H_

#include <functional>
#include <memory>
#include <type_traits>

template <typename T>
class Singleton {
 protected:
  Singleton() noexcept = default;

  Singleton(const Singleton&) = delete;
  Singleton& operator=(const Singleton&) = delete;

  Singleton(Singleton&&) = delete;
  Singleton& operator=(Singleton&&) = delete;

  virtual ~Singleton() = default;  // to silence base class Singleton<T> has a
                                   // non-virtual destructor [-Weffc++]

 public:
  static T& GetInstance(const std::function<std::shared_ptr<T>()>& _allocator =
                            []() { return std::make_shared<T>(); }) {
    static std::shared_ptr<T> instance;
    if (!instance) {
      if (_allocator) {
        instance = _allocator();
      }
    }
    return *instance.get();
  }
};

#endif  // ZILLIQA_SRC_COMMON_SINGLETON_H_
