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

#include "swarm.h"
#include "db.h"
#include "search8.h"
#include "search16.h"
#include "utils/nt_codec.h"
#include "utils/search_data.h"
#include "utils/score_matrix.h"
#include "utils/threads.h"
#include <cassert>  // assert()
#include <cstdint>  // int64_t, uint64_t
#include <iterator>
#include <limits>
#include <memory>  // unique pointer
#include <pthread.h>  // pthread_mutex_init
#include <vector>


constexpr auto ullong_max = std::numeric_limits<unsigned long long int>::max();
static pthread_mutex_t scan_mutex;

static struct Search_data * search_data;
static uint64_t master_next;
static uint64_t master_length;
static uint64_t remainingchunks;
static uint64_t * master_targets;
static uint64_t * master_scores;
static uint64_t * master_diffs;
static uint64_t * master_alignlengths;
static int master_bits;
static uint64_t dirbuffersize;

queryinfo_t query;
uint64_t longestdbsequence;


auto allocate_per_thread_search_data(std::vector<struct Search_data>& search_data_v) -> void
{
  static constexpr auto one_kilobyte = 1024UL;
  static constexpr auto nt_per_uint64 = 32U;
  dirbuffersize = longestdbsequence * ((longestdbsequence + 3) / 4) * 4;

  for(auto& thread_data: search_data_v) {
    thread_data.qtable_v.resize(longestdbsequence);
    thread_data.qtable_w_v.resize(longestdbsequence);
    thread_data.dprofile_v.resize(2 * one_kilobyte);  // 4 * 16 * 32
    thread_data.dprofile_w_v.resize(1 * one_kilobyte);  // 4 * 2 * 8 * 32
    thread_data.hearray_v.resize(longestdbsequence * nt_per_uint64);
    thread_data.dir_array_v.resize(dirbuffersize);
  }
}


auto search_init(struct Search_data & thread_data) -> void
{
  static constexpr auto byte_multiplier = 64U;
  static constexpr auto word_multiplier = 32U;

  for(auto i = 0U; i < query.len; i++)
  {
    const auto nt_value {nt_extract(query.seq, i) + 1U};  // 1,  2,   3, or   4
    const auto byte_offset {byte_multiplier * nt_value};  // 1, 64, 128, or 192
    const auto word_offset {word_multiplier * nt_value};  // 1, 32,  64, or 128

    // refactoring: difficult to work directly on vectors (thread barrier)
    thread_data.qtable_v[i]   = &thread_data.dprofile_v[byte_offset];
    thread_data.qtable_w_v[i] = &thread_data.dprofile_w_v[word_offset];
  }
}


auto search_chunk(struct Search_data & thread_data, const int64_t bits) -> void
{
  static auto score_matrix_8 = create_score_matrix<unsigned char>(penalty_mismatch);
  static auto score_matrix_16 = create_score_matrix<unsigned short>(penalty_mismatch);
  static constexpr auto bit_mode_16 = 16U;
  auto const target_index = static_cast<long int>(thread_data.target_index);

  assert(thread_data.target_count != 0);
  assert((bits == bit_mode_16) or (bits == bit_mode_16 / 2));

  if (bits == bit_mode_16) {
    search16(thread_data.qtable_w_v.data(),
             static_cast<WORD>(penalty_gapopen),
             static_cast<WORD>(penalty_gapextend),
             score_matrix_16.data(),
             thread_data.dprofile_w_v.data(),
             reinterpret_cast<WORD *>(thread_data.hearray_v.data()),
             thread_data.target_count,
             std::next(master_targets, target_index),
             std::next(master_scores, target_index),
             std::next(master_diffs, target_index),
             std::next(master_alignlengths, target_index),
             static_cast<uint64_t>(query.len),
             thread_data.dir_array_v,
             longestdbsequence);
  } else {
    search8(thread_data.qtable_v.data(),
            static_cast<BYTE>(penalty_gapopen),
            static_cast<BYTE>(penalty_gapextend),
            score_matrix_8.data(),
            thread_data.dprofile_v.data(),
            thread_data.hearray_v.data(),
            thread_data.target_count,
            std::next(master_targets, target_index),
            std::next(master_scores, target_index),
            std::next(master_diffs, target_index),
            std::next(master_alignlengths, target_index),
            static_cast<uint64_t>(query.len),
            dirbuffersize,
            thread_data.dir_array_v.data(),
            longestdbsequence);
  }
}


auto search_getwork(uint64_t & countref, uint64_t & firstref) -> bool
{
  // countref = how many sequences to search
  // firstref = index into master_targets/scores/diffs where thread should start

  bool status {false};

  pthread_mutex_lock(&scan_mutex);

  if (master_next < master_length)
    {
      const uint64_t chunksize =
        ((master_length - master_next + remainingchunks - 1) / remainingchunks);

      countref = chunksize;
      firstref = master_next;

      master_next += chunksize;
      --remainingchunks;
      status = true;
    }

  pthread_mutex_unlock(&scan_mutex);

  return status;
}


auto search_worker_core(const int64_t thread_id) -> void {
  auto & thread_data = *std::next(search_data, thread_id);
  search_init(thread_data);
  while(search_getwork(thread_data.target_count, thread_data.target_index)) {
    search_chunk(thread_data, master_bits);
  }
}


auto adjust_thread_number(const int n_bits,
                          const uint64_t remaining_sequences,
                          uint64_t n_threads) -> uint64_t {
  static constexpr unsigned int channels_8 {8};
  static constexpr unsigned int channels_16 {16};
  static constexpr unsigned int bit_mode_16 {16};  // refactoring: should be an enum class
  const auto channels = (n_bits == bit_mode_16) ? channels_8 : channels_16;

  assert(remaining_sequences != 0);
  assert(n_threads != 0);
  assert((n_threads - 1) <= (ullong_max / channels_8));
  assert((n_bits == bit_mode_16) or (n_bits == bit_mode_16 / 2));

  while (remaining_sequences <= (n_threads - 1) * channels) {
    --n_threads;
  }

  return n_threads;
}

// arguments: bits, master_length, thr
// static_assert(adjust_thread_number( 8, 32, 10) == 2);
// static_assert(adjust_thread_number( 8, 32,  3) == 2);
// static_assert(adjust_thread_number( 8, 31,  2) == 2);
// static_assert(adjust_thread_number( 8, 17,  2) == 2);
// static_assert(adjust_thread_number( 8, 16,  2) == 1);
// static_assert(adjust_thread_number( 8,  1,  2) == 1);
// static_assert(adjust_thread_number( 8, 32,  1) == 1);
// static_assert(adjust_thread_number(16, 17, 10) == 3);
// static_assert(adjust_thread_number(16, 17,  3) == 3);
// static_assert(adjust_thread_number(16, 16,  3) == 2);
// static_assert(adjust_thread_number(16, 15,  2) == 2);
// static_assert(adjust_thread_number(16,  1,  3) == 1);
// static_assert(adjust_thread_number(16, 17,  1) == 1);


auto search_do(uint64_t query_no,
               uint64_t listlength,
               uint64_t * targets,
               uint64_t * scores,
               uint64_t * diffs,
               uint64_t * alignlengths,
               const int bits,
               std::unique_ptr<ThreadRunner>& search_threads) -> void
{
  unsigned int query_len = 0;
  query.qno = query_no;
  db_getsequenceandlength(query_no, query.seq, query_len);
  query.len = query_len;

  master_next = 0;
  master_length = listlength;
  master_targets = targets;
  master_scores = scores;
  master_diffs = diffs;
  master_alignlengths = alignlengths;
  master_bits = bits;

  const auto thr =
    adjust_thread_number(bits,
                         master_length,
                         static_cast<uint64_t>(opt_threads));

  remainingchunks = thr;

  if (thr == 1) {
    search_worker_core(0);
  }
  else {
    search_threads->run();
  }
}


auto search_begin(std::vector<struct Search_data>& search_data_v) -> void
{
  longestdbsequence = db_getlongestsequence();

  search_data = search_data_v.data();

  allocate_per_thread_search_data(search_data_v);

  pthread_mutex_init(& scan_mutex, nullptr);
}


auto search_end() -> void
{
  /* finish and clean up worker threads */

  pthread_mutex_destroy(& scan_mutex);
  search_data = nullptr;
}
