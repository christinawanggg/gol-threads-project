/*
 Authors: Christina Wang and Ilana Epstein
 Date: May 1, 2019

 In this lab, we used threads to implement the game of life and used
 row and column partitioning.

*Swarthmore College, CS 31
* Copyright (c) 2019 Swarthmore College Computer Science Department,
* Swarthmore PA, Professors Tia Newhall and Kevin Webb and Vasanta Chaganti
*/

#include <pthread.h>
#include <pthreadGridVisi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "colors.h"

#define OUTPUT_NONE (0)
#define OUTPUT_TEXT (1)
#define OUTPUT_VISI (2)

/* For counting the number of live cells in each round. */
static int live = 0;

//synchronization variables
static pthread_mutex_t lock;
static pthread_barrier_t barrier;

/* These values need to be shared by all the threads for visualization.
* You do NOT need to worry about synchronized access to these, the graphics
* library will take care of that for you. */
static visi_handle handle;
static color3 *image_buf;
static char *visi_name = "Parallel GOL";

// This struct represents all the data you need to keep track of your GOL

struct gol_data {
  /* The number of rows on your GOL game board. */
  int rows;

  /* The number of columns on your GOL game board. */
  int cols;

  /* The number of iterations to run your GOL simulation. */
  int iters;

  /* Which form of output we're generating:
  * 0: only print the first and last board (good for timing).
  * 1: print the board to the terminal at each round.
  * 2: draw the board using the visualization library. */
  int output_mode;

  //ID to give each thread a unique number.
  int id;

  //other GOL data that you need here
  int *board_mem; //current board

  int *boardChange; //next board

  int num_alive; //num of cells alive

  //  partitioning information here
  // the starting index for the thread
  int start;

  // the ending index for the thread
  int end;

  // add any other variables you need each thread to have
  int row_or_column; //indicates either row or column partitioning

  int argv; //command line option #5 to print partitioning

  int num_threads; //number of threads

};



/*
Params: data struct, number of iterations
Returns: none
Purpose: print the game of life board display
*/
void print_board(struct gol_data *data, int round) {

  /* Print the round number. */
  fprintf(stderr, "Round: %d\n", round);

  int result;

  for (int i = 0; i < data->rows; i++) {
    for (int j = 0; j < data->cols; j++) {
      int index = (i*data->cols) + j;

      if (data->board_mem[index] == 1) {

        //mutex to protect the global variable live
        result = pthread_mutex_lock(&lock);
        if (result){
          perror("pthread_mutex_lock failed.");
        }
        live += 1;
        result = pthread_mutex_unlock(&lock);
        if (result){
          perror("pthread_mutex_unlock failed.");
        }
        fprintf(stderr, " @");

      }
      else {
        fprintf(stderr, " _");
      }
    }
    fprintf(stderr, "\n");
  }


  /* Print the total number of live cells. */
  fprintf(stderr, "Live cells: %d\n", live);

  /* Add some blank space between rounds. */
  fprintf(stderr, "\n\n");
}




/*
Params: data struct, i, j
Returns: number of alive neighbors
Purpose: count the number of alive neighbor cells
*/
int numAliveNeighbors(struct gol_data* data, int i, int j){
  int aliveneigh = 0; //counter for total alive neighbors

  int pos,pcols;
  //below and above neighbors
  pos = (i+1)%data->rows;
  pcols = pos*data->cols;
  //up and down
  if (data->board_mem[pcols + j%data->cols] == 1){
    aliveneigh +=1;
  }
  pos = (i - 1 + data->rows) % data->rows;
  pcols = pos*data->cols;
  if (data->board_mem[pcols + j%data->cols] == 1){
    aliveneigh +=1;
  }
  //right and left
  if (data->board_mem[i* data->cols + (j + 1) % data->cols] == 1){
    aliveneigh +=1;
  }
  if (data->board_mem[i*data->cols + (data->cols + j-1) % data->cols] == 1){
    aliveneigh +=1;
  }
  //diagonal neighbors
  pos = (i+1)%data->rows;
  pcols = pos*data->cols;
  if (data->board_mem[pcols + (j + 1)%data->cols  ] == 1){
    aliveneigh +=1;
  }
  if (data->board_mem[pcols + (j + data->cols-1)%data->cols] == 1){
    aliveneigh +=1;
    }
  pos = (i - 1 + data->rows) % data->rows;
  pcols = pos*data->cols;
  if (data->board_mem[pcols + (j + 1)%data->cols ] == 1){
    aliveneigh +=1;
  }
  if (data->board_mem[pcols + (j - 1 + data->cols)%data->cols ] == 1){
    aliveneigh +=1;
  }

  return aliveneigh;
}

/*
Params: buff, data struct
Returns: none
Purpose: perform a round of GOL simulation
*/
void gol_step(color3 *buff, struct gol_data* data) {
  int i, j;
  int cstart, cend, rstart, rend;

  if(data->row_or_column == 0){ //if doing row partitioning
    rstart = data->start;
    rend = data->end;
    cstart = 0;
    cend = data->cols-1;
  }
  else{ //column partitioning
    rstart = 0;
    rend = data->rows-1;
    cstart = data->start;
    cend = data->end;
  }

    for (i = rstart; i <= rend; ++i) {
      for (j = cstart; j <= cend; ++j) {

      //Determine each cell's fate in this simulation round.
      //position of current cell in board array

        // call function to find num of alive neighbors
        int neighalive = numAliveNeighbors(data, i, j);

        //conditions for a dead cell
        if ((neighalive == 0) | (neighalive == 1) | (neighalive >= 4)){
          data->boardChange[i*data->cols+j] = 0;
        }

        //conditions for alive cells
        else if (neighalive == 3){
          data->boardChange[i*data->cols+j] = 1;
        }
        else{ //stays the same
          data->boardChange[i*data->cols+j] = data->board_mem[i*data->cols+j];
        }

        /* When using visualization, also update the graphical board. */
        if (buff != NULL) {
          /* Convert row/column number to an index into the
          * visualization grid.  The graphics library uses coordinates
          * that place the origin at a different location from your
          * GOL board. You shouldn't need to change this. */
          int buff_index = (data->rows - (i + 1)) * data->cols + j;


          if(data->boardChange[i*data->cols+j] == 1){//becomes alive
            buff[buff_index].r = 0;
            buff[buff_index].g = 255;
            buff[buff_index].b = 0;
          }
          else {
            buff[buff_index].r = 0;
            buff[buff_index].g = 0;
            buff[buff_index].b = 0;
          }
          //int buff_index = (data->rows - (i + 1)) * data->cols + j;
          if (data->boardChange[i * data->cols + j]) {
          /* Live cells get the color using this thread's ID as the index */
            buff[buff_index] = colors[data->id];
          } else {
          /* Dead cells are blank. */
            buff[buff_index] = c3_black;
            }
        }

      }

}
  //do a swap with current and next board
  int *temp = data->board_mem;
  data->board_mem = data->boardChange;
  data->boardChange = temp;
}

/*
Params: input file, struct data
Returns: none
Purpose: takes in input file and reads the data, sets it to struct variables
*/
void read_file(FILE *infile, struct gol_data *data, int num_threads){

  int rows;
  int cols;
  int iters;
  int num_alive;

  int ret = fscanf(infile, "%d", &rows); // Read one integer (%d) from the file
  if(ret != 1) {
    printf("%s\n", "Error reading file");
    exit(1);

  }
  ret = fscanf(infile, "%d", &cols);// Read one integer (%d) from the file
  if(ret != 1) {
    printf("%s\n", "Error reading file");
    exit(1);

  }
  ret = fscanf(infile, "%d", &iters);// Read one integer (%d) from the file
  if(ret != 1) {
    printf("%s\n", "Error reading file");
    exit(1);

  }
  ret = fscanf(infile, "%d", &num_alive);// Read one integer (%d) from the file
  if(ret != 1) {
    printf("%s\n", "Error reading file");
    exit(1);
  }

  int sizeA = rows * cols;
  int* current = malloc(sizeA*sizeof(int)); //pointer to current board
  int* next = malloc(sizeA*sizeof(int)); //pointer to next board
  for(int i = 0; i < num_threads; i++){  //struct initializations
    data[i].rows = rows;
    data[i].cols = cols;
    data[i].iters = iters;
    data[i].num_alive = num_alive;
    data[i].board_mem = current; //set board_mem to current board
    data[i].boardChange = next; //set boardChange to next board
  }
}

/*
Params: data struct, number of threads
Returns: none
Purpose: initialize the array boards
*/
void arrayBoard(struct gol_data *data, int num_threads){
  // size of array is total rows times columns
  int sizeA = data->rows * data->cols;

  // initialize the boards to start off as an array of 0s
  for (int j = 0;j<num_threads;j++){
    for (int i=0;i<sizeA;i++){
      data[j].board_mem[i] = 0;
      data[j].boardChange[i] = 0;
    }
  }
}

/*
Params: input file, data struct
Returns: none
Purpose: read in the num live cells and location from the text file
*/
void readLivecells(FILE *inputfile, struct gol_data *data){
  // initialize aspects of the input file
  int row;
  int column;
  int index;
  int cells = data->num_alive;

  // when the cells are not ==0, they are alive, so set those to alive on
  // the game boards
  while (cells != 0){
    fscanf(inputfile, "%d  %d", &row,&column);
    index = row *data->cols + column;
    data->board_mem[index] = 1;
    data->boardChange[index] = 1;
    cells--;
  }
}

/*
Params: void data struct
Returns: none
Purpose: used when threads are created
*/
void *worker(void *datastruct) {
  // use type casting to get a struct gol_data * from the void * input.
  struct gol_data *data = (struct gol_data *)datastruct;
  int ret;

  printf("\nThread %d, starting up from [%d:%d].\n", data->id, data->start, data->end);



  // steps for each round:
  //
  // 1) if the output mode is OUTPUT_TEXT, only one thread should print the
  // board.
  // threads than can call pthread_barrier_wait to synchronize:
  ret = pthread_barrier_wait(&barrier);
  if(ret != 0 && ret != PTHREAD_BARRIER_SERIAL_THREAD) {
    perror("pthread_barrier_wait");
    exit(1);
  }

  for(int i=0;i<data->iters;i++) {

    if(data->output_mode == OUTPUT_TEXT && data->id == 0){
      print_board(data,i);
      live = 0;
    }
    // threads than can call pthread_barrier_wait to synchronize:
    ret = pthread_barrier_wait(&barrier);
    if(ret != 0 && ret != PTHREAD_BARRIER_SERIAL_THREAD) {
      perror("pthread_barrier_wait");
      exit(1);
    }
    // 2) with appropriate synchronization, execute the core logic of the round
    gol_step(image_buf, data);

    // 3) if the output mode is > 0, usleep() for a short time to slow down the
    // speed of the animations.
    if(data->output_mode > 0){
      usleep(100000);
      system("clear");
    }
    // 4) if output mode is OUTPUT_VISI, call draw_ready(handle)
    if(data->output_mode == OUTPUT_VISI){
      draw_ready(handle);
    }
    // threads than can call pthread_barrier_wait to synchronize:
    ret = pthread_barrier_wait(&barrier);
    if(ret != 0 && ret != PTHREAD_BARRIER_SERIAL_THREAD) {
      perror("pthread_barrier_wait");
      exit(1);
    }
  }



  // After the final round, one thread should print the final board state
  if(data->output_mode == OUTPUT_TEXT && data->id == 0){
    print_board(data,data->iters);

  }
  // threads than can call pthread_barrier_wait to synchronize:
  ret = pthread_barrier_wait(&barrier);
  if(ret != 0 && ret != PTHREAD_BARRIER_SERIAL_THREAD) {
    perror("pthread_barrier_wait");
    exit(1);
  }
  if (data->argv != 0){ // if print partitioning is called

    if(data->row_or_column == 0){ //print row partioning
      printf("tid %d: rows: %d:%d (%d) cols: 0:%d (%d)\n", data->id, data->start, data->end, data->end-data->start +1, (data->cols-1), data->cols);
    }
    else{ //call column partitioning
      printf("tid %d: rows: 0:%d (%d) cols: %d:%d (%d)\n", data->id, data->rows-1, data->rows, data->start, data->end, data->end-data->start+1);
    }

  }
  /* You're not expecting the workers to return anything important. */
  return NULL;
}


int main(int argc, char *argv[]) {
  //declare main's local variables
  int i;
  int result;
  int num_threads;
  int partition;
  pthread_t *threads;
  struct gol_data *data;
  int output_mode = 0;
  struct timeval start_time;
  double secs = 0.0;

  // sees if you can get the time of day or not
  int getTime = gettimeofday(&start_time, NULL);
  if (getTime == -1) {
    printf("Failed to get time of day.");
    return -1;
  }
  // casting, change the seconds to microseconds first, then change it back
  double seconds = start_time.tv_sec *1000000;
  double microsecs = start_time.tv_usec;
  double startseconds  = seconds + microsecs; //start time


  // make sure 6 args are passed in
  if (argc != 6) {
    printf("Wrong number of arguments.\n");
    printf("Usage: %s <input file> <0|1|2> <num threads> <partition> <print_partition>\n", argv[0]);
    return 1;
  }

  // make sure output mode is properly specified
  if(atoi(argv[2])<0 || atoi(argv[2])>2){
    printf("Must enter a 0, 1, or 2 for output mode\n");
    return 1;
  }

  // make sure partitioning is properly specified
  if (atoi(argv[4]) != 0 && atoi(argv[4]) != 1){
    printf("Must enter a 0 or 1 for partition value\n");
    printf("To partition by rows, enter 0. For columns, enter 1.\n");
    return 1;
  }

  // make sure print-partitioning is properly specified
  if (atoi(argv[5]) != 0 && atoi(argv[5]) != 1){
    printf("Must enter a 0 or 1 for print-partition value\n");
    printf("To print allocation info, enter 1. Otherwise, enter 0.\n");
    return 1;
  }
  output_mode = atoi(argv[2]);


  // read in the command line arguments, initialize all your variables (board state, synchronization), etc.
  num_threads = atoi(argv[3]);

  partition = atoi(argv[4]);

  //initialize mutex and barrier
  result = pthread_mutex_init(&lock,NULL);
  if (result){
    perror("pthread_barrier_init failed");
  }
  result = pthread_barrier_init(&barrier, NULL, num_threads);
  if (result){
    perror("pthread_barrier_init failed");
  }

  //malloc memory for threads
  threads = malloc(num_threads * sizeof(pthread_t));
  if (threads == NULL) {
    perror("malloc()");
    exit(1);
  }
  //malloc mem for data struct
  data = malloc(num_threads * sizeof(struct gol_data));
  if (data == NULL) {
    perror("malloc()");
    free(threads);
    exit(1);
  }

  // open file
  FILE *infile;
  infile = fopen(argv[1], "r");  //open file
  if (infile == NULL) {
    perror("fopen");
    exit(1);
  }

  // read file and create board, read live cells data
  read_file(infile, data, num_threads);
  arrayBoard(data,num_threads);
  readLivecells(infile, data);

  // read in file info to the struct
  for(int i = 0;i<num_threads;i++){
    data[i].output_mode = output_mode; //set output mode to struct
    data[i].num_threads = num_threads;  //set number threads to struct
  }

  if (data->output_mode == OUTPUT_VISI) {
    //pass the init_pthread_animation function, in order:
    //# of threads, # of rows, # of cols, visi_name (defined above).
    handle = init_pthread_animation(data->num_threads, data->rows, data->cols, visi_name);
    if (handle == NULL) {
      printf("visi init error\n");
      return 1;
    }

    image_buf = get_animation_buffer(handle);
    if (image_buf == NULL) {
      printf("visi buffer error\n");
      return 1;
    }
  } else {
    handle = NULL;
    image_buf = NULL;
  }



// set up a gol_data struct for each thread (e.g., with the thread's
// partition assignment) and create the threads.
data->argv = atoi(argv[5]);


for (i = 0; i < num_threads; i++) {

  data[i].id = i;
  data[i].argv = atoi(argv[5]);
  //  Set other thread arguments (i.e., partitioning info,
  // synchronization, etc.)

  //partitioning
  if(partition == 0){
    data[i].row_or_column = 0;

    if(data->rows % num_threads == 0){ //even rows
      data[i].start = (i * (data->rows/num_threads));
      data[i].end = (data[i].start + (data->rows/num_threads) - 1);

    }
    else{ //if uneven rows

      int rem = data->rows % num_threads;
      if (i < rem){
        for (int j = 1;j<=rem;j++){
          if(i == 0){
            data[0].start = 0;
            data[0].end = data->rows/num_threads;

          }
          else{
            data[i].start = (data[i-1].end + 1);
            data[i].end = (data[i].start + data->rows/num_threads);

          }
        }
      }
      else{
        data[i].start = (data[i-1].end + 1);
        data[i].end = (data[i].start + data->rows/num_threads-1);

      }
    }
  }
  else{ //  partition to deal with columns instead of rows
    data[i].row_or_column = 1;

    if(data->cols % num_threads == 0){
      data[i].start = (i * (data->rows/num_threads));
      data[i].end = (data[i].start + (data->rows/num_threads) - 1);

    }
    else{ //for uneven columns
      int rem = data->cols % num_threads;
      if (i < rem){
        for (int j = 1;j<=rem;j++){
          if(i == 0){
            data[0].start = 0;
            data[0].end = data->cols/num_threads;

          }
          else{
            data[i].start = (data[i-1].end + 1);
            data[i].end = (data[i].start + data->cols/num_threads);

          }
        }
      }
      else{
        data[i].start = (data[i-1].end + 1);
        data[i].end = (data[i].start + data->cols/num_threads-1);

      }
    }
  }
  pthread_create(&threads[i], NULL, worker, &data[i]); //create the threads
}
/* If we're doing graphics, call run_animation to tell it how many
* iterations there will be. */
  if (output_mode == OUTPUT_VISI) {
  //pass in the number of iterations as the second parameter
  run_animation(handle, data->iters);
  }


// join all the threads (that is, wait in this main thread until all
// the workers are finished.
  for (i = 0; i < num_threads; i++) {
    pthread_join(threads[i], NULL);
  }

  //clean up and exit
  free(data[0].board_mem);
  free(data[0].boardChange);

  free(threads);
  free(data);
  fclose(infile);
  //destroy mutex and barrier
  result  = pthread_mutex_destroy(&lock);
  if (result){
    perror("pthread_mutex_destroy failed");
  }
  result = pthread_barrier_destroy(&barrier);
  if (result) {
    perror("pthread_barrier_destroy failed");
  }




  //  stop the clock, print timing results, and clean up memory.
  //Compute the total runtime in seconds, including fractional
  // seconds (e.g., 10.5 seconds should NOT be truncated to 10).
  int finalTime = gettimeofday(&start_time, NULL);
  if (finalTime == -1) {
    printf("Failed to get time of day.");
    return -1;
  }
  // get final time
  double finalseconds = start_time.tv_sec *1000000 + start_time.tv_usec;
  secs = finalseconds/1000000 - startseconds/1000000 ;

  /* Print the total runtime, in seconds. */
  printf("\nTotal time: %0.3f seconds.\n", secs);



  return 0;
}
