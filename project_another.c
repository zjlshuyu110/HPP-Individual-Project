#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <time.h>

/* Reference: QuicSort.pdf, lab8, lab9, lab11, Assignment4, https://iq.opengenus.org/parallel-quicksort/, chatgpt, deepseek */
typedef struct {
    int* data;
    int size;
} ThreadData;

// learn from lab10_task5
static inline double get_wall_seconds() {
    return omp_get_wtime();
}

// learn from lab9
static inline void swap(int* restrict a, int* restrict b) {
    int t = *a;
    *a = *b;
    *b = t;
}

// Loop unrooling, learn from lab7_task1
void sort_array(int* restrict arr, int size) {
    for (int i = 0; i < size - 1; i++) {
        for (int j = i + 1; j + 3 < size; j += 4) {  
            if (arr[i] > arr[j]) swap(&arr[i], &arr[j]);
            if (arr[i] > arr[j + 1]) swap(&arr[i], &arr[j + 1]);
            if (arr[i] > arr[j + 2]) swap(&arr[i], &arr[j + 2]);
            if (arr[i] > arr[j + 3]) swap(&arr[i], &arr[j + 3]);
        }
    }
}

// Reference: QuickSort.pdf
// Here I use strategy 3:Sort the medians and select the mean value of the two middlemost medians in each processor set and step.
int select_pivot(ThreadData* threadData, int group_size) {
    int myid = omp_get_thread_num();
    int num_groups = omp_get_num_threads() / group_size;

    // Local ID within the group and group ID
    int locid = myid % group_size;
    int group = myid / group_size;

    // 1. Sort the data in each thread (local array)
    int* data = threadData[myid].data;
    int data_size = threadData[myid].size;
    sort_array(data, data_size);

    // 2. Compute the median of each thread
    int median;
    if (data_size % 2 == 0) {
        median = (data[data_size / 2 - 1] + data[data_size / 2]) / 2;
    } else {
        median = data[data_size / 2];
    }

    static int** medians = NULL;
    static int* pivots = NULL;

    // Ensures only one thread executes this block, the use of single learn  from chatgpt
    #pragma omp single
    {
        medians = (int**) malloc(num_groups * sizeof(int*));
        for (int g = 0; g < num_groups; g++) {
            medians[g] = (int*) malloc(group_size * sizeof(int));
        }
        pivots = (int*) malloc(num_groups * sizeof(int));
    }
    medians[group][locid] = median;
    #pragma omp barrier

    // 3. Sorting the medians array and get the median of each group as pivot
    // Ensuring only one thread per group does sorting, avoding race conditions, I learn this method from chatgpt
    if (locid == 0) {
        sort_array(medians[group], group_size);
        if (group_size % 2 == 0) {
            pivots[group] = (medians[group][group_size / 2 - 1] + medians[group][group_size / 2]) / 2;
        } else {
            pivots[group] = medians[group][group_size / 2];
        }
    }
    #pragma omp barrier

    // free memory
    for (int g = 0; g < num_groups; g++) {
        free(medians[g]);
    }

    free(medians);
    return pivots[group];
}

// Here I split the data into different group based on the comparison with pivot, I made modification based on the advice of chatgpt.
int findsplit(int* restrict data, int data_size, int pivot) {
    int splitpoint = 0;

    // 1. Count how many elements belong to the left partition
    #pragma omp parallel for reduction(+:splitpoint)
    for (int i = 0; i < data_size; i++) {
        if (data[i] <= pivot) {
            splitpoint++;
        }
    }

    // 2. Using temp array, rearranging the data into left and right groups
    int* temp = (int*) malloc(data_size * sizeof(int));
    int left_index = 0, right_index = splitpoint;

    #pragma omp parallel for
    for (int i = 0; i < data_size; i++) {
        if (data[i] <= pivot) {
            temp[left_index++] = data[i];
        } else {
            temp[right_index++] = data[i];
        }
    }

    // 3. Copy back to the original array
    #pragma omp parallel for
    for (int i = 0; i < data_size; i++) {
        data[i] = temp[i];
    }

    free(temp);
    return splitpoint;
}

// Here is a basic merge function, I learn from https://www.geeksforgeeks.org/merge-sort/
void merge(int* restrict data, int size1, int size2) {
    int* temp = (int*) malloc((size1 + size2) * sizeof(int));
    int i = 0, j = size1, k = 0;

    while (i < size1 && j < size1 + size2) {
        if (data[i] <= data[j]) {
            temp[k++] = data[i++];
        } else {
            temp[k++] = data[j++];
        }
    }
    while (i < size1) {
        temp[k++] = data[i++];
    }
    while (j < size1 + size2) {
        temp[k++] = data[j++];
    }
    for (i = 0; i < size1 + size2; i++) {
        data[i] = temp[i];
    }

    free(temp);
}

void global_sort(ThreadData* threadData, int myid, int num_threads, int group_size) {
    int* data = threadData[myid].data;
    int size = threadData[myid].size;
    if (size <= 1) return;
    int locid = myid % group_size;  // Local ID within the group
    int pivot = select_pivot(threadData, group_size);
    int splitpoint = findsplit(data, size, pivot);

    #pragma omp barrier

    // Merge the data based on the split
    if (locid < group_size / 2) {
        merge(data, splitpoint, size - splitpoint);
    } else {
        merge(data, splitpoint, size - splitpoint);
    }

    
    #pragma omp barrier

    // Recursively sort
    if (group_size > 1) {
        global_sort(threadData, myid, num_threads, group_size / 2);
    }
}

void print_array(int arr[], int N) {
    for (int i = 0; i < N; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s <array_size> <num_threads>\n", argv[0]);
        return 1;
    }

    int size = atoi(argv[1]);
    int num_threads = atoi(argv[2]);
    omp_set_num_threads(num_threads);

    int* arr = (int*) malloc(size * sizeof(int));
    for (int i = 0; i < size; i++) {
        arr[i] = rand() % 1000;
    }

    printf("Unsorted array:\n");
    print_array(arr, size);

    ThreadData* threadData = (ThreadData*) malloc(num_threads * sizeof(ThreadData));
    // Dividing data into different threads
    int chunk_size = size / num_threads;
    for (int i = 0; i < num_threads; i++) {
        threadData[i].data = arr + i * chunk_size;
        if(i == num_threads - 1){
            threadData[i].size =size - i * chunk_size;

        }else{
            threadData[i].size =chunk_size;
        }
    }

    double startTime = get_wall_seconds();

    #pragma omp parallel num_threads(num_threads)
    {
        int myid = omp_get_thread_num();
        global_sort(threadData, myid, num_threads, num_threads);
    }

    double endTime = get_wall_seconds();

    printf("Sorted array:\n");
    print_array(arr, size);
    printf("Execution Time: %f seconds\n", endTime - startTime);

    free(threadData);
    free(arr);

    return 0;
}