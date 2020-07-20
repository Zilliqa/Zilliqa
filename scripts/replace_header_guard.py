#!/usr/bin/env python
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

import os
import re
headerFiles=[".h",".hh",".hpp"]
exclude=["depends"]


for subdir, dirs, files in os.walk("./src"):
	skip = False
	for exclusion in exclude:
		if subdir.find(exclusion) != -1:
			print(subdir)
			skip = True
			break
	if skip:
		continue
	for filename in files:
		for i in headerFiles:
			if filename.endswith(i):			
			#print filename
				print(subdir)
				filepath = subdir + os.sep + filename
				#print filepath
				guard_body = re.sub('[/\\.]','_',filepath)
				guard_body = "zilliqa"+guard_body[1:]+"_"
				#print guard_body
				with open(filepath) as f:
					text = f.readlines()
					#print text
				with open(filepath,"w") as f:
					for line in text:
						if line.find('#ifndef') != -1 and line.rstrip().endswith('_'):
							print(line)
							line = "#ifndef "+guard_body.upper()+"\n"
						if line.find('#define') != -1 and line.rstrip().endswith('_'):
							line = "#define "+guard_body.upper()+'\n'
						if line.find("#endif  //") != -1 and line.rstrip().endswith('_'):
							line = "#endif  // "+guard_body.upper()+'\n'
						f.write(line)




