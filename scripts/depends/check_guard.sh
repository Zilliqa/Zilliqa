#!/bin/bash
#!/usr/bin/env bash
#
# Copyright 2017 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Modification Copyright (C) 2019 Zilliqa

#Source
#https://github.com/googleapis/google-cloud-cpp/commit/b6600156830efed42615b4ee4c388acb6ede09c1


set -eu

find src -name "*.h" -not -path "*src/depends/*"  -print0 \
| xargs -0 awk 'BEGINFILE {
	# The guard must begin with the name of the project.
	guard_prefix="ZILLIQA_"
	# The guard must end with "_"
	guard_suffix="_"
	# The guard name is the filename (including path from the root of the
	# project), with "/" and "." characters replaced with "_", and all
	# characters converted to uppercase:
	guard_body=toupper(FILENAME)
	gsub("[/\\.]", "_", guard_body)
	guard_lower=(guard_prefix guard_body guard_suffix)
	guard=toupper(guard_lower)
	matches=0
	}
	/^#ifndef / {
	# Ignore lines that do not look like guards at all.
	if ($0 !~ "_H_$") { next; }
	if (index($0, guard) == 9) {
	  matches++;
	} else {
	 printf("%s:\nexpected: #ifndef %s\n   found: %s\n", FILENAME, guard, $0);
	}
	}
	/^#define / {
	# Ignore lines that do not look like guards at all.
	if ($0 !~ "_H_$") { next; }
	if (index($0, guard) == 9) {
	  matches++;
	} else {
	  printf("%s:\nexpected: #define %s\n   found: %s\n", FILENAME, guard, $0);
	}
	}
	/^#endif / {
	# Ignore lines that do not look like guards at all.
	if ($0 !~ "_H_$") { next; }
	if (index($0, "// " guard) == 9) {
	  matches++;
	} else {
	  printf("%s:\nexpected: #endif  // %s\n   found: %s\n",
	         FILENAME, guard, $0);
	}
	}
	ENDFILE {
	if (matches != 3) {
	  printf("%s has invalid include guards\n", FILENAME);
	  exit(1);
	}
}'