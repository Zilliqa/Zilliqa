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

#ifndef ZILLIQA_SRC_LIBUTILS_SETTHREADNAME_H_
#define ZILLIQA_SRC_LIBUTILS_SETTHREADNAME_H_

#include <thread>

namespace utility {

#ifdef __APPLE__

inline void SetThreadName(const char* threadName) {
  pthread_setname_np(threadName);
}
#elif defined _WIN32 || defined _WIN64 || defined __linux__

inline void SetThreadName(const char* threadName) {
  pthread_setname_np(pthread_self(), threadName);
}
#else
inline void SetThreadName(const char*) {}
#endif

}  // namespace utility

#endif  // ZILLIQA_SRC_LIBUTILS_SETTHREADNAME_H_