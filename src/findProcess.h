/*
	unlockupd
	Copyright (c) 2004-2009 John Bafford
	http://bafford.com/software/unlockupd/
	
	unlockupd is a simple threads-based daemon that checks to see if
	lookupd has died, and if so, kills it so it can respawn.

    This file is part of Unlockupd.

    Unlockupd is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Unlockupd is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Unlockupd.  If not, see <http://www.gnu.org/licenses/>.
*/

pid_t FindProcess(const char* name);
