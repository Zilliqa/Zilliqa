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
    const bfs::path& dirPath, const std::vector<std::string>& dirSkipList) {
  // Create a vector of string
  std::vector<std::string> listOfFiles;
  try {
    // Check if given path exists and points to a directory
    if (bfs::exists(dirPath) && bfs::is_directory(dirPath)) {
      // Create a Recursive Directory Iterator object and points to the starting
      // of directory
      bfs::recursive_directory_iterator iter(dirPath);

      // Create a Recursive Directory Iterator object pointing to end.
      bfs::recursive_directory_iterator end;

      // Iterate till end
      while (iter != end) {
        // Check if current entry is a directory and if exists in skip list
        if (bfs::is_directory(iter->path()) &&
            (std::find(dirSkipList.begin(), dirSkipList.end(),
                       iter->path().filename()) != dirSkipList.end())) {
          // Skip the iteration of current directory pointed by iterator
          iter.no_push();
        } else {
          // Add the name in vector
          listOfFiles.push_back(iter->path().string());
        }

        boost::system::error_code ec;
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
