#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_SIZE 10000
#define NUM_THREADS 1

// Simple swap
void swap(double *p, double *q) {
    double t = *p;
    *p = *q;
    *q = t;
}

// Bubble sort (for demonstration; you may replace with QuickSort or qsort)
void sort(double a[], int n) {
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (a[j] > a[j + 1]) {
                swap(&a[j], &a[j + 1]);
            }
        }
    }
}

// Generate random array
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

// Partition data into chunk pointers, each chunk having size total_size / group_size
void partition_data(double* array, int total_size, int group_size, double** groups) {
    int chunk_size = total_size / group_size;
    for (int i = 0; i < group_size; i++) {
        groups[i] = &array[i * chunk_size];
    }
}

// Find median from an array (sort first, then pick middle)
double find_median(double* array, int size) {
    sort(array, size);
    if (size % 2 == 0) {
        return (array[size / 2 - 1] + array[size / 2]) / 2.0;
    } else {
        return array[size / 2];
    }
}

// Select pivot from array of medians by sorting them and taking average of middle two
double pivot_select(double medians[], int size) {
    sort(medians, size);
    if (size % 2 == 0) {
        return (medians[size/2 - 1] + medians[size/2]) / 2.0;
    } else {
        return medians[size/2];
    }
}

// ----------------- NEW: PARTITION LOCAL DATA AROUND PIVOT -----------------
// This function splits the array `data[i]` (size=local_size) into
// two arrays: "left" for <= pivot, "right" for > pivot.
// We return them, along with how many elements ended up in each.
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

// ----------------- NEW: EXCHANGE DATA BETWEEN PARTNERS -----------------
// chunk i gives its "bigger" part to chunk j, and receives j's "smaller" part, etc.
// In a real parallel code, this would be an MPI (or similar) exchange, but here
// we do it in a "serial mock" way: by concatenating arrays.
void exchange_data(double **data_i, int *size_i,
                   double *small_i, int count_small_i,
                   double *big_i,   int count_big_i,
                   double **data_j, int *size_j,
                   double *small_j, int count_small_j,
                   double *big_j,   int count_big_j,
                   int keep_lower)
{
    // keep_lower==1 means chunk i should keep the smaller portion
    // and chunk j should keep the bigger portion.
    // So chunk i will get: "small_i + small_j" if it's the left chunk,
    // chunk j will get: "big_i + big_j" if it's the right chunk.

    // We free the old data_i/data_j and build new arrays for each chunk.
    free(*data_i);
    free(*data_j);

    if (keep_lower) {
        // i gets small_i + small_j
        int newSizeI = count_small_i + count_small_j;
        double *newDataI = (double *)malloc(newSizeI * sizeof(double));
        for(int k=0; k < count_small_i; k++){
            newDataI[k] = small_i[k];
        }
        for(int k=0; k < count_small_j; k++){
            newDataI[count_small_i + k] = small_j[k];
        }
        sort(newDataI, newSizeI); // sort after combining
        *data_i = newDataI;
        *size_i = newSizeI;

        // j gets big_i + big_j
        int newSizeJ = count_big_i + count_big_j;
        double *newDataJ = (double *)malloc(newSizeJ * sizeof(double));
        for(int k=0; k < count_big_i; k++){
            newDataJ[k] = big_i[k];
        }
        for(int k=0; k < count_big_j; k++){
            newDataJ[count_big_i + k] = big_j[k];
        }
        sort(newDataJ, newSizeJ);
        *data_j = newDataJ;
        *size_j = newSizeJ;
    } else {
        // i gets big_i + big_j
        int newSizeI = count_big_i + count_big_j;
        double *newDataI = (double *)malloc(newSizeI * sizeof(double));
        for(int k=0; k < count_big_i; k++){
            newDataI[k] = big_i[k];
        }
        for(int k=0; k < count_big_j; k++){
            newDataI[count_big_i + k] = big_j[k];
        }
        sort(newDataI, newSizeI);
        *data_i = newDataI;
        *size_i = newSizeI;

        // j gets small_i + small_j
        int newSizeJ = count_small_i + count_small_j;
        double *newDataJ = (double *)malloc(newSizeJ * sizeof(double));
        for(int k=0; k < count_small_i; k++){
            newDataJ[k] = small_i[k];
        }
        for(int k=0; k < count_small_j; k++){
            newDataJ[count_small_i + k] = small_j[k];
        }
        sort(newDataJ, newSizeJ);
        *data_j = newDataJ;
        *size_j = newSizeJ;
    }
}

// ----------------- CHANGED: GLOBAL SORT -----------------
// Now uses the partition & exchange approach instead of
// just merging everything and splitting in half by size.
void global_sort(double** data, int* sizes, int group_size)
{
    // If there's only one chunk in this group, it's already "sorted" in local sense
    if (group_size <= 1) return;

    // 1) Gather local medians
    double medians[NUM_THREADS]; // or dynamic if group_size > NUM_THREADS
    for (int i = 0; i < group_size; i++) {
        medians[i] = find_median(data[i], sizes[i]);
    }

    // 2) Select pivot
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

    // 4) Exchange data among pairs: i <-> i + group_size/2
    //    We'll do: For i in [0..group_size/2-1], exchange with partner (i+group_size/2).
    //    The left half keep the "lower" portion, right half keep the "higher" portion.
    int half = group_size / 2;
    for (int i = 0; i < half; i++) {
        int partner = i + half;
        exchange_data(
            &data[i], &sizes[i],
            smallers[i], count_smallers[i], biggers[i], count_biggers[i],
            &data[partner], &sizes[partner],
            smallers[partner], count_smallers[partner], biggers[partner], count_biggers[partner],
            1 // keep_lower=1 for the left chunk
        );
    }

    // 5) Free the temporary smaller/bigger arrays (already copied in exchange_data).
    for (int i = 0; i < group_size; i++) {
        // We do not free data[i] here, because it was replaced in exchange_data()
        // But we do free smallers[i] and biggers[i].
        if (smallers[i]) free(smallers[i]);
        if (biggers[i]) free(biggers[i]);
    }

    // 6) Recurse on the left half and right half
    //    We'll call global_sort on the left half: [0..half-1], and the right half: [half..group_size-1].
    //    But we need to pass them in sub-arrays (or treat them as separate groups).
    global_sort(data, sizes, half); // sort the left side
    global_sort(&data[half], &sizes[half], group_size - half); // sort the right side
}

// ------------------- MAIN PROGRAM -----------------------
int main() {
    srand((unsigned)time(NULL));

    // Generate random array
    int size = MAX_SIZE;
    double arr[MAX_SIZE];
    generate_random_array(arr, size, 1000);

    printf("Unsorted array:\n");
    print_array(arr, size);

    // Partition for 8 "threads" (chunks).
    // NOTE: We'll store the pointers to each chunk in `groups`,
    // and keep track of chunk sizes in `sizes`.
    double* groups[NUM_THREADS];
    int sizes[NUM_THREADS];

    partition_data(arr, size, NUM_THREADS, groups);

    // Each chunk has size = size / NUM_THREADS (assuming it divides evenly for simplicity)
    int chunk_size = size / NUM_THREADS;
    for (int i = 0; i < NUM_THREADS; i++) {
        sizes[i] = chunk_size;
    }

    // (Optional) We might do an initial local sort for each chunk, but it's not strictly necessary
    // since the pivot selection only needs the median, and we do a final local sort inside global_sort anyway.
    for (int i = 0; i < NUM_THREADS; i++) {
        sort(groups[i], sizes[i]);
    }

    clock_t start = clock();
    global_sort(groups, sizes, NUM_THREADS);
    clock_t end = clock();

    // Print final data chunk by chunk
    printf("\nChunks after global sort (each chunk is sorted, and collectively they should be globally sorted if no overlap):\n");
    for (int i = 0; i < NUM_THREADS; i++) {
        print_array(groups[i], sizes[i]);
    }

    double time_taken = (double)(end - start) / CLOCKS_PER_SEC;
    printf("\nExecution Time: %f seconds\n", time_taken);

    return 0;
}
