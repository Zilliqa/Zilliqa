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

import click
from xml.etree import ElementTree as et

class XMLCombiner(object):
    def __init__(self, filenames, new_file):
        assert len(filenames) > 0, 'No filenames!'
        # save all the roots, in order, to be processed later
        self.roots = [et.parse(f).getroot() for f in filenames]
        self.output = new_file

    def combine(self):
        for r in self.roots[1:]:
            # combine each element with the first one, and update that
            self.combine_element(self.roots[0], r)
        # return the string representation
        tree = et.ElementTree(self.roots[0])
        tree.write(self.output, encoding="utf-8", xml_declaration=True)
        return True

    def combine_element(self, one, other):

        mapping = {el.tag: el for el in one}
        for el in other:
            if len(el) == 0:
                # Not nested
                try:
                    # Update the text
                    mapping[el.tag].text = el.text
                except KeyError:
                    # An element with this name is not in the mapping
                    mapping[el.tag] = el
                    # Add it
                    one.append(el)
            else:
                try:
                    # Recursively process the element, and update it in the same way
                    self.combine_element(mapping[el.tag], el)
                except KeyError:
                    # Not in the mapping
                    mapping[el.tag] = el
                    # Just add it
                    one.append(el)


@click.command("merge-xml")
@click.option('--baseline', default='constants.xml', prompt='The baseline file',
              help='The name of the master file probably constants.xml')
@click.option('--update', default='update.xml', prompt='The file with the updates',
              help='The name of the update file probably just the difference from the baseline file')
@click.option('--output', default='constants_new.xml', prompt='The combined file',
              help='The name of the output file probably constants_new.xml')
@click.pass_context

def setup(ctx, baseline, update, output):

    source_xml_file = 'constants_v9.3.0rt1_latest.xml'
    target_xml_file = 'test.xml'
    new_file = 'constants_new.xml'

    if XMLCombiner((baseline, update), output).combine() :
        print('success')
    else:
        print('failed')


