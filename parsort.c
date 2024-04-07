#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int compare_i64(const void *left_, const void *right_) {
  int64_t left = *(int64_t *)left_;
  int64_t right = *(int64_t *)right_;
  if (left < right) return -1;
  if (left > right) return 1;
  return 0;
}

void seq_sort(int64_t *arr, size_t begin, size_t end) {
  size_t num_elements = end - begin;
  qsort(arr + begin, num_elements, sizeof(int64_t), compare_i64);
}

// Merge the elements in the sorted ranges [begin, mid) and [mid, end),
// copying the result into temparr.
void merge(int64_t *arr, size_t begin, size_t mid, size_t end, int64_t *temparr) {
  int64_t *endl = arr + mid;
  int64_t *endr = arr + end;
  int64_t *left = arr + begin, *right = arr + mid, *dst = temparr;

  for (;;) {
    int at_end_l = left >= endl;
    int at_end_r = right >= endr;

    if (at_end_l && at_end_r) break;

    if (at_end_l)
      *dst++ = *right++;
    else if (at_end_r)
      *dst++ = *left++;
    else {
      int cmp = compare_i64(left, right);
      if (cmp <= 0)
        *dst++ = *left++;
      else
        *dst++ = *right++;
    }
  }
}

void fatal(const char *msg) __attribute__ ((noreturn));

void fatal(const char *msg) {
  fprintf(stderr, "Error: %s\n", msg);
  exit(1);
}

void process_waitpid_output(int waitpid_out, int wstatus) {
  // If waitpid failed
  if (waitpid_out == -1) {
    fatal("waitpid failure.\n");
  }

  // If the subprocess did not exit normally
  if (!WIFEXITED(wstatus)) {
    fatal("subprocess did not exit normally.\n");
  }
  // If the subprocess exited with a non-zero exit code
  if (WEXITSTATUS(wstatus) != 0) {
    fatal("subprocess exited with a non-zero exit code.\n");
  }
}

void merge_sort(int64_t *arr, size_t begin, size_t end, size_t threshold) {
  assert(end >= begin);
  size_t size = end - begin;

  if (size <= threshold) {
    seq_sort(arr, begin, end);
    exit(0);
  }

  // recursively sort halves in parallel:

  size_t mid = begin + size/2;

  // Make a child to sort the left side of the array
  pid_t sort_left = fork();
  // If the fork attempt failed
  if (sort_left < 0) {
    fatal("failed to create child process.\n");
  }

  // If the left_sort child is running
  if (sort_left == 0) {
    // sort the left side of the array recursively, then have the child exit
    merge_sort(arr, begin, mid, threshold);
    exit(0);
  } 

  // Make a child to sort the right side of the array
  pid_t sort_right = fork();
  // If the fork attempt failed
  if (sort_right < 0) {
    fatal("failed to create child process.\n");
  }

  // If the right_sort child is running
  else if (sort_right == 0) {
    // sort the right side of the array recursively, then have the child exit
    merge_sort(arr, mid, end, threshold);
    exit(0);
  } 

  // If the parent is running
  else {
    int wstatus_1;
    int wstatus_2;

    // Wait for child 1
    int waitpid_out_1 = waitpid(sort_left, &wstatus_1, 0);
    process_waitpid_output(waitpid_out_1, wstatus_1);

    // Wait for child 2
    int waitpid_out_2 = waitpid(sort_right, &wstatus_2, 0);
    process_waitpid_output(waitpid_out_2, wstatus_2);
  }
  
  // allocate temp array now, so we can avoid unnecessary work
  // if the malloc fails
  int64_t *temp_arr = (int64_t *) malloc(size * sizeof(int64_t));
  if (temp_arr == NULL)
    fatal("malloc() failed");

  // child processes completed successfully, so in theory
  // we should be able to merge their results
  merge(arr, begin, mid, end, temp_arr);

  // copy data back to main array
  for (size_t i = 0; i < size; i++)
    arr[begin + i] = temp_arr[i];

  // now we can free the temp array
  free(temp_arr);

  // success!
}


int main(int argc, char **argv) {
  // check for correct number of command line arguments
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <filename> <sequential threshold>\n", argv[0]);
    return 1;
  }

  // process command line arguments
  const char *filename = argv[1];
  char *end;
  size_t threshold = (size_t) strtoul(argv[2], &end, 10);
  if (end != argv[2] + strlen(argv[2])) {
    fatal("invalid threshold value.\n");
  }

  // open the file
  int fd = open(filename, O_RDWR);
  if (fd < 0) {
    fatal("file did not open.\n");
  }

  // use fstat to determine the size of the file
  struct stat statbuf;
  int rc = fstat(fd, &statbuf);
  if (rc != 0) {
    fatal("failed to gain file's status information.\n");
  }
  size_t file_size_in_bytes = statbuf.st_size;

  // Map the file into memory using mmap
  int64_t *data = mmap(NULL, file_size_in_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  close(filename);

  if (data == MAP_FAILED) {
    fatal("memory mapping failed.\n");
  }

  // Sort the data
  merge_sort(data, 0, (file_size_in_bytes / sizeof(int64_t)), threshold);

  // Unmap and close the file
  munmap(data, file_size_in_bytes);
  close(data);

  return 0; 
}
