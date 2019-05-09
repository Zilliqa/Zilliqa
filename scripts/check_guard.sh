#!/bin/bash
# Copyright (C) 2019 Zilliqa
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

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