/*
    SWARM

    Copyright (C) 2012-2024 Torbjorn Rognes and Frederic Mahe

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Contact: Torbjorn Rognes <torognes@ifi.uio.no>,
    Department of Informatics, University of Oslo,
    PO Box 1080 Blindern, NO-0316 Oslo, Norway
*/

#include <cstdint>  // int64_t, uint64_t
#include <vector>


using BYTE = unsigned char;
using WORD = unsigned short;

struct Search_data
{
  std::vector<BYTE *> qtable_v;
  BYTE ** qtable = nullptr;
  std::vector<WORD *> qtable_w_v;
  WORD ** qtable_w = nullptr;

  std::vector<BYTE> dprofile_v;
  BYTE * dprofile = nullptr;
  std::vector<WORD> dprofile_w_v;
  WORD * dprofile_w = nullptr;

  std::vector<BYTE> hearray_v;
  BYTE * hearray = nullptr;

  std::vector<uint64_t> dir_array_v;
  uint64_t * dir_array = nullptr;

  uint64_t target_count = 0;
  uint64_t target_index = 0;
};
