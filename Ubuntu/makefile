
#
# Author: Lorenz Pullwitt <memorysurfer@lorenz-pullwitt.de>
# Copyright 2019-2020
#
# This file is part of MemorySurfer.
#
# MemorySurfer is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, you can find it here:
# https://www.gnu.org/licenses/old-licenses/gpl-2.0.html
#

memorysurfer.cgi : memorysurfer.o indexedmemoryfile.o sha1.o
	gcc -Wall -g -O0 -fsanitize=address -o memorysurfer.cgi memorysurfer.o indexedmemoryfile.o sha1.o -lm

memorysurfer.o : ../memorysurfer.c ../imf/indexedmemoryfile.h
	gcc -Wall -g -O0 -fsanitize=address -c ../memorysurfer.c

indexedmemoryfile.o : ../imf/indexedmemoryfile.c ../imf/indexedmemoryfile.h ../imf/sha1.h
	gcc -Wall -g -O0 -fsanitize=address -c ../imf/indexedmemoryfile.c

sha1.o : ../imf/sha1.c ../imf/sha1.h
	gcc -Wall -g -O0 -fsanitize=address -c ../imf/sha1.c

clean :
	rm sha1.o indexedmemoryfile.o memorysurfer.o memorysurfer.cgi
