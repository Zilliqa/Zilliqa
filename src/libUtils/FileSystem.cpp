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

#include <algorithm>
#include <iostream>

void recursive_copy_dir(const std::filesystem::path& src, const std::filesystem::path& dst) {
  if (!std::filesystem::exists(src)) {
    throw std::runtime_error("Source path: " + src.generic_string() +
                             " does not exist");
  }
  if (std::filesystem::is_directory(src)) {
    if (!std::filesystem::exists(dst)) {
      std::filesystem::create_directories(dst);
    }
    for (const auto& item : std::filesystem::directory_iterator(src)) {
      recursive_copy_dir(item.path(), dst / item.path().filename());
    }
  } else if (std::filesystem::is_regular_file(src)) {
    std::filesystem::copy_file(src, dst, std::filesystem::copy_options::overwrite_existing);
  } else {
    throw std::runtime_error(dst.generic_string() + " not dir or file");
  }
}

/*
 * Get the list of all files in given directory and its sub directories.
 *
 * Arguments
 * 	dirPath : Path of directory to be traversed
 * 	dirSkipList : List of folder names to be skipped
 *
 * Returns:
 * 	vector containing paths of all the files in given directory and its sub
 * directories
 *
 */
std::vector<std::string> getAllFilesInDir(
    const std::filesystem::path& dirPath, const std::vector<std::string>& dirSkipList) {
  // Create a vector of string
  std::vector<std::string> listOfFiles;
  try {
    // Check if given path exists and points to a directory
    if (std::filesystem::exists(dirPath) && std::filesystem::is_directory(dirPath)) {
      // Create a Recursive Directory Iterator object and points to the starting
      // of directory
      std::filesystem::recursive_directory_iterator iter(dirPath);

      // Create a Recursive Directory Iterator object pointing to end.
      std::filesystem::recursive_directory_iterator end;

      // Iterate till end
      while (iter != end) {
        // Check if current entry is a directory and if exists in skip list
        if (std::filesystem::is_directory(iter->path()) &&
            (std::find(dirSkipList.begin(), dirSkipList.end(),
                       iter->path().filename()) != dirSkipList.end())) {
          // Skip the iteration of current directory pointed by iterator
          iter.disable_recursion_pending();
        } else {
          // Add the name in vector
          listOfFiles.push_back(iter->path().string());
        }

        std::error_code ec;
        // Increment the iterator to point to next entry in recursive iteration
        iter.increment(ec);
        if (ec) {
          std::cerr << "Error While Accessing : " << iter->path().string()
                    << " :: " << ec.message() << '\n';
        }
      }
    }
  } catch (std::system_error& e) {
    std::cerr << "Exception :: " << e.what();
  }
  return listOfFiles;
}
