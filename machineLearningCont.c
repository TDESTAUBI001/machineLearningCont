 
/*
Copyright (c) 2016-2020 Thomas de St. Aubin and Jeremy Iverson
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

/* assert */
#include <assert.h>

/* fabs */
#include <math.h>

/* MPI API */
#include <mpi.h>

/* printf, fopen, fclose, fscanf, scanf */
#include <stdio.h>

/* EXIT_SUCCESS, malloc, calloc, free, qsort */
#include <stdlib.h>

#include <omp.h>

#define MPI_SIZE_T MPI_UNSIGNED_LONG

struct distance_metric {
  size_t viewer_id;
  double distance;
};

static int
cmp(void const *ap, void const *bp)
{
  struct distance_metric const a = *(struct distance_metric*)ap;
  struct distance_metric const b = *(struct distance_metric*)bp;

  return a.distance < b.distance ? -1 : 1;
}

int
main(int argc, char * argv[])
{
  int ret, p, rank;
  size_t n, m, k;
  double * rating;

  /* Initialize MPI environment. */
  ret = MPI_Init(&argc, &argv);
  assert(MPI_SUCCESS == ret);

  /* Get size of world communicator. */
  ret = MPI_Comm_size(MPI_COMM_WORLD, &p);
  assert(ret == MPI_SUCCESS);

  /* Get my rank. */
  ret = MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  assert(ret == MPI_SUCCESS);

  /* Validate command line arguments. */
  assert(2 == argc);

  /* Read input --- only if your rank 0. */
  if (0 == rank) {
    /* ... */
    char const * const fn = argv[1];

    /* Validate input. */
    assert(fn);

    /* Open file. */
    FILE * const fp = fopen(fn, "r");
    assert(fp);

    /* Read file. */
    fscanf(fp, "%zu %zu", &n, &m);

    /* Allocate memory. */
    rating = malloc(n * m * sizeof(*rating));

    /* Check for success. */
    assert(rating);

    for (size_t i = 0; i < n; i++) {
      for (size_t j = 0; j < m; j++) {
        fscanf(fp, "%lf", &rating[i * m + j]);
      }
    }

    /* Close file. */
    ret = fclose(fp);
    assert(!ret);
  }
# if 0
  /* Send number of viewers and movies to rest of processes. */
  if (0 == rank) {
    for (int r = 1; r < p; r++) {
      ret = MPI_Send(&n, 1, MPI_SIZE_T, r, 0, MPI_COMM_WORLD);
      assert(MPI_SUCCESS == ret);
      ret = MPI_Send(&m, 1, MPI_SIZE_T, r, 0, MPI_COMM_WORLD);
      assert(MPI_SUCCESS == ret);
    }
  } else {
      ret = MPI_Recv(&n, 1, MPI_SIZE_T, 0, 0, MPI_COMM_WORLD,
        MPI_STATUS_IGNORE);
      assert(MPI_SUCCESS == ret);
      ret = MPI_Recv(&m, 1, MPI_SIZE_T, 0, 0, MPI_COMM_WORLD,
        MPI_STATUS_IGNORE);
      assert(MPI_SUCCESS == ret);
  }
# endif

  /* Send Viewer and Movie Information to Processors */
  MPI_Bcast(&n, 1, MPI_SIZE_T, 0, MPI_COMM_WORLD);
  MPI_Bcast(&m, 1, MPI_SIZE_T, 0, MPI_COMM_WORLD);
  
  /* Compute base number of viewers. */
  size_t const base = 1 + ((n - 1) / p); // ceil(n / p)

  /* Compute local number of viewers. */
  size_t const ln = (rank + 1) * base > n ? n - rank * base : base;
  
  if (rank != 0) {
    rating = malloc(ln * m * sizeof(*rating));
    assert(rating);
  }
  
# if 0
  /* Send viewer data to rest of processes. */
  if (0 == rank) {
    for (int r = 1; r < p; r++) {
      size_t const rn = (r + 1) * base > n ? n - r * base : base;
      ret = MPI_Send(rating + r * base * m, rn * m, MPI_DOUBLE, r, 0,
        MPI_COMM_WORLD);
      assert(MPI_SUCCESS == ret);
    }
  } else {
    /* Allocate memory. */
    rating = malloc(ln * m * sizeof(*rating));

    /* Check for success. */
    assert(rating);

    ret = MPI_Recv(rating, ln * m, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD,
      MPI_STATUS_IGNORE);
    assert(MPI_SUCCESS == ret);
  }
# endif

  int * sc;
  int * displs;
  
  //MPI_Scatterv(rating, sendCode, displs, MPI_DOUBLE, &rec_buf, base, MPI_DOUBLE, 0, MPI_COMM_WORLD);
  
  if(rank == 0) {
    sc = malloc(p * sizeof(*sc));
    assert(sc);
    displs = malloc(p * sizeof(*displs));
    assert(displs);
    for (int r = 0; r < p; r++) {
      size_t rn = (r + 1) * base > n ? n - r * base : base;
      sc[r] = m * rn;
      displs[r] = m * base * r;
    }
  }

  ret = MPI_Scatterv(rating, sc, displs, MPI_DOUBLE, rating, base * m, MPI_DOUBLE, 0, MPI_COMM_WORLD);
  assert(ret == MPI_SUCCESS);
  
  /* Allocate more memory. */
  double * const urating = malloc((m - 1) * sizeof(*urating));

  /* Check for success. */
  assert(urating);


  /* Get user input and send it to rest of processes. */
  if (0 == rank) {
    for (size_t j = 0; j < m - 1; j++) {
      printf("Enter your rating for movie %zu: ", j + 1);
      fflush(stdout);
      scanf("%lf", &urating[j]);
    }
  }

# if 0
  urating[0] = 4.0;
  urating[1] = 4.0;
  urating[2] = 4.0;
  urating[3] = 4.0;
# endif
  
  MPI_Bcast(urating, m - 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
  
# if 0
    for (int r = 1; r < p; r++) {
      ret = MPI_Send(urating, m - 1, MPI_DOUBLE, r, 0, MPI_COMM_WORLD);
      assert(MPI_SUCCESS == ret);
    }
  } else {
    ret = MPI_Recv(urating, m - 1, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD,
      MPI_STATUS_IGNORE);
    assert(MPI_SUCCESS == ret);
  }
# endif
  
  /* Allocate more memory. */
  double * distance = calloc(ln, sizeof(*distance));
 
  /* Check for success. */
  assert(distance);
  
  double ts = omp_get_wtime();
  
  /* Compute distances. */
  for (size_t i = 0; i < ln; i++) {
    for (size_t j = 0; j < m - 1; j++) {
      distance[i] += fabs(urating[j] - rating[i * m + j]);
    }
  }

  double te = omp_get_wtime();
  
  double time = te - ts;
  double totalTime;
  
  //MPI_Scatterv(distance, sendCode, displs, MPI_DOUBLE, &rec_buf, base, MPI_DOUBLE, 0, MPI_COMM_WORLD);

  ret = MPI_Reduce(&time, &totalTime, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
  assert(ret == MPI_SUCCESS);
  
  if (rank == 0) {
    printf("\n\nTime: %f\n\n\n", totalTime);
  }
  
# if 0
  if (0 == rank) {
    for (int r = 1; r < p; r++) {
      size_t const rn = (r + 1) * base > n ? n - r * base : base;
      ret = MPI_Recv(distance + r * base, rn, MPI_DOUBLE, r, 0,
        MPI_COMM_WORLD, MPI_STATUS_IGNORE);
      assert(MPI_SUCCESS == ret);
    }
  } else {
    ret = MPI_Send(distance, ln, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD);
    assert(MPI_SUCCESS == ret);
  }
# endif

  int * displs2;
  int * count;
  double * totDist;

  if(rank == 0) {
    displs2 = malloc(p * sizeof(*displs));
    count = malloc(p * sizeof(*count));
    totDist = malloc(n * sizeof(*totDist));
    for (size_t r = 0; r < p; r++) {
      size_t rn = (r + 1) * base > n ? n - r * base : base;
      count[r] = rn;
      displs2[r] = r * base;
    }
  }
  
  MPI_Gatherv(distance, ln, MPI_DOUBLE, totDist, count, displs2, MPI_DOUBLE, 0, MPI_COMM_WORLD);
  
  if (0 == rank) {
    struct distance_metric * distance2 = malloc(n * sizeof(*distance2));
    assert(distance2);

    for (size_t i = 0; i < n; i++) {
      distance2[i].viewer_id = i;
      distance2[i].distance = distance[i];
    }

    /* Sort distances. */
    qsort(distance2, n, sizeof(*distance2), cmp);


    /* Get user input. */
    printf("Enter the number of similar viewers to report: ");
    fflush(stdout);
    scanf("%zu", &k);

    
    //k = 10;
    
    /* Output k viewers who are least different from the user. */
    printf("Viewer ID   Movie five   Distance\n");
    printf("---------------------------------\n");

    for (size_t i = 0; i < k; i++) {
      printf("%9zu   %10.1lf   %8.1lf\n", distance2[i].viewer_id + 1,
        rating[distance2[i].viewer_id * m + 4], distance2[i].distance);
    }

    printf("---------------------------------\n");

    /* Compute the average to make the prediction. */
    double sum = 0.0;
    for (size_t i = 0; i < k; i++) {
      sum += rating[distance2[i].viewer_id * m + 4];
    }


    /* Output prediction. */
    printf("The predicted rating for movie five is %.1lf.\n", sum / k);

    free(distance2);
  }

  /* Deallocate memory. */
  free(rating);
  free(urating);
  free(distance);

  ret = MPI_Finalize();
  assert(MPI_SUCCESS == ret);

  return EXIT_SUCCESS;
}
