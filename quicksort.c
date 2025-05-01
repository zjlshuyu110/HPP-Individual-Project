/*
The mission separates into the following small tasks:
1. Write the serial code and optimize it.
2. Handle the parallelization challenge: Load balancing and synchronization.
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>  // For time measurement


#define MAX_SIZE 1000       // or whatever array size you want
#define NUM_THREADS 8       // number of chunks/groups

// Swap helper ideal from : https://www.tutorialspoint.com/learn_c_by_examples/median_program_in_c.htm  which to help find the medial value, and use this for help sort
void swap(double *p, double *q) {
    double t = *p;
    *p = *q;
    *q = t;
}

// Bubble sort (can be replaced with qsort or quicksort later) 
void sort(double a[], int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (a[j] > a[j + 1]) {
                swap(&a[j], &a[j + 1]);
            }
        }
    }
}

// Generate random array , this just as test later remove this and use large size and try 3 differt distrubution data, tips from Jarmo.
void generate_random_array(double arr[], int size, int range) {
    for (int i = 0; i < size; i++) {
        arr[i] = rand() % range;
    }
}


// Print array
void print_array(const double A[], int size) {
    for (int i = 0; i < size; i++)
        printf("%.0f ", A[i]);
    printf("\n");
}


// Partition data into chunks for processors (mocked for serial)
void partition_data(double* array, int total_size, int group_size, double** groups) {
    int chunk_size = total_size / group_size;
    for (int i = 0; i < group_size; i++) {
        groups[i] = &array[i * chunk_size];
    }
}

// Find median from a sorted local array 
double find_median(double* array, int size) { // since we alredy do the local sort- so now we need find mieal -for each the processor data
    sort(array, size);
    if (size % 2 == 0) {
        return (array[size / 2 - 1] + array[size / 2]) / 2.0; // -1 as array indexing in C starts from 0
    } else {
        return array[size / 2]; // for the parallerl we need soted in the empty array ! and then select it ! this is my ideal ! 
    }
}




// Strategy 3: sort medians and select mean of two middlemost
double pivot_select(double medians[], int size) {
    sort(medians, size);
    if (size % 2 == 0) {
        return (medians[size / 2 - 1] + medians[size / 2]) / 2.0;
    } else {
        return medians[size / 2];
    }
}


void partition_local(double *local_data, int local_size, double pivot,
                     double **left_part, int *left_count,
                     double **right_part, int *right_count)
{
    // We'll allocate new arrays as big as local_size (worst case).
    // Then we'll copy them back or keep them for exchange.
    double *leftArr = (double *)malloc(local_size * sizeof(double));
    double *rightArr = (double *)malloc(local_size * sizeof(double));
    if (!leftArr || !rightArr) {
        printf("Allocation error in partition_local!\n");
        exit(1);
    }

    *left_count = 0;
    *right_count = 0;

    // Partition loop
    for(int i = 0; i < local_size; i++){
        if(local_data[i] <= pivot){
            leftArr[(*left_count)++] = local_data[i];
        } else {
            rightArr[(*right_count)++] = local_data[i];
        }
    }

    // We hand back these allocated arrays
    *left_part = leftArr;
    *right_part = rightArr;
}

// Merge two arrays and keep lower or upper part
void merge(double* data1, int size1, double* data2, int size2, int keep_lower) {
    int total_size = size1 + size2;
    double* temp = (double*)malloc(total_size * sizeof(double));
    if (!temp) {
        printf("Memory allocation failed in merge!\n");
        return;
    }

    for (int i = 0; i < size1; i++) temp[i] = data1[i];
    for (int i = 0; i < size2; i++) temp[size1 + i] = data2[i];

    sort(temp, total_size);

    if (keep_lower) {
        for (int i = 0; i < size1; i++) data1[i] = temp[i];
    } else {
        for (int i = 0; i < size1; i++) data1[i] = temp[total_size - size1 + i];
    }

    free(temp);
}



// Placeholder global sort function (non-parallel for now) 
// based onthe serious version add the OpenMP paraller 
void global_sort(double* data[], int size, int group_size) {
    if (group_size <= 1) return;   // revusize to the "base", no need to revusisev anymore

    double medians[NUM_THREADS];
    for (int i = 0; i < group_size; i++) {
        medians[i] = find_median(data[i], size);
    }

    double pivot = pivot_select(medians, group_size);

    // 3) Partition each chunk around pivot
    //    We'll store the "smaller" and "bigger" arrays in temporary arrays of pointers
    double *smallers[NUM_THREADS], *biggers[NUM_THREADS];
    int count_smallers[NUM_THREADS], count_biggers[NUM_THREADS];

    for (int i = 0; i < group_size; i++) {
        partition_local(
            data[i], sizes[i], pivot,
            &smallers[i], &count_smallers[i],
            &biggers[i], &count_biggers[i]
        );
    }


    for (int i = 0; i < group_size / 2; i++) {
        merge(data[i], size, data[i + group_size / 2], size, 1);
    }
    for (int i = group_size / 2; i < group_size; i++) {
        merge(data[i], size, data[i - group_size / 2], size, 0);
    }

    global_sort(data, size, group_size / 2);
}

int main() {
    srand(time(NULL));
    int size = MAX_SIZE;
    double arr[MAX_SIZE];

    generate_random_array(arr, size, 1000);
    printf("Unsorted array:\n");
    print_array(arr, size);

    // Partition for mock 8-thread sort
    double* groups[NUM_THREADS];
    partition_data(arr, size, NUM_THREADS, groups);

    clock_t start = clock();
    global_sort(groups, size / NUM_THREADS, NUM_THREADS);
    
    clock_t end = clock();
    printf("\nSorted array (partial group chunks):\n");
    for (int i = 0; i < NUM_THREADS; i++) {
        print_array(groups[i], size / NUM_THREADS);
    }
    double time_taken = (double)(end - start) / CLOCKS_PER_SEC;
    printf("\nExecution Time: %f seconds\n", time_taken);

    return 0;
}
