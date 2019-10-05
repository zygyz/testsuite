/*
 * See the dyninst/COPYRIGHT file for copyright information.
 *
 * We provide the Paradyn Tools (below described as "Paradyn")
 * on an AS IS basis, and do not warrant its validity or performance.
 * We reserve the right to update, modify, or discontinue this
 * software at any time.  We shall have no obligation to supply such
 * updates or modifications or any other form of support to you.
 *
 * By your use of Paradyn, you understand and agree that we (or any
 * other person or entity with proprietary rights in Paradyn) are
 * under no obligation to provide either maintenance services,
 * update services, notices of latent defects, or correction of
 * defects for Paradyn.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include <omp.h>
#include <stdlib.h>
#include <string.h>

#include "mutatee_util.h"
#include "solo_mutatee_boilerplate.h"

#define NUM_OMP_THREADS 8
#define K 256

int test_parallel_region(int* array);
int test_parallel_for_loops(int* array);

static int global_sum = 0;

int* initialize_array() {
  int array_size = NUM_OMP_THREADS * K;
  int* array = (int*)malloc(sizeof(int) * array_size);
  int i = 0;
  for (; i < array_size; ++i) {
    array[i] = i;
    global_sum += i;
  }
  return array;
}

int test_parallel_region(int* array) {
  omp_set_num_threads(NUM_OMP_THREADS);
  int sum = 0;
  #pragma omp parallel shared(sum)
  {
    int partial_sum = 0;
    int thread_id = omp_get_thread_num();
    int start = thread_id * K;
    int end = start + K - 1;
    int i = start;
    for (; i <= end; ++i) {
      partial_sum += array[i]; 
    }
    #pragma omp critical
    {
      sum += partial_sum;
    }
  }
  return sum;
}

int test_parallel_for_loops(int* array) {
  omp_set_num_threads(NUM_OMP_THREADS);
  int sum = 0;
  int array_size = NUM_OMP_THREADS * K;
  int i = 0;
  #pragma omp parallel for schedule(dynamic) reduction(+:sum)
  for (i = 0; i < array_size; ++i) {
    sum += array[i]; 
  }
  return sum;
}

/*
 * After instrumentation, the computation result should still be correct.
 */
int test_mem_trace_openmp_mutatee() {
  int* array = initialize_array();
  int sum = test_parallel_region(array);
  if (sum != global_sum) {
    free(array);
    return -1;
  }
  sum = test_parallel_for_loops(array);
  if (sum != global_sum) {
    free(array);
    return -1;
  }
  test_passes(testname);
  free(array);
  return 0;
}
