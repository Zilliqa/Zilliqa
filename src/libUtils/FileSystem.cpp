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

#include "FileSystem.h"

void recursive_copy_dir(const bfs::path& src, const bfs::path& dst) {
  if (!bfs::exists(src)) {
    throw std::runtime_error("Source path: " + src.generic_string() +
                             " does not exist");
  }
  if (bfs::is_directory(src)) {
    if (!bfs::exists(dst)) {
      bfs::create_directories(dst);
    }
    for (bfs::directory_entry& item : bfs::directory_iterator(src)) {
      recursive_copy_dir(item.path(), dst / item.path().filename());
    }
  } else if (bfs::is_regular_file(src)) {
    bfs::copy_file(src, dst, bfs::copy_option::overwrite_if_exists);
  } else {
    throw std::runtime_error(dst.generic_string() + " not dir or file");
  }
}