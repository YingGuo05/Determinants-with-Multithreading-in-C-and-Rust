#include<stdio.h>
#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#include<omp.h>
#include<pthread.h>

#define D_ARRAY_SIZE 32
#define BUFFER_LENGTH sizeof(int)*D_ARRAY_SIZE*D_ARRAY_SIZE  
#define N D_ARRAY_SIZE
#define STORE_SIZE 10
#define BUFFER_SIZE 56000
#define NUM_THREADS 70

double eps = 1e-6;
static int inMatrix[N*N];
static short ret = 1, ret0, ret1, ret2 = 1, fd;
static char d[STORE_SIZE];
static short num;
static int counter = 0;

volatile long count = 0;

static int matrix[BUFFER_SIZE][N*N];
static long long resultArr[BUFFER_SIZE];

struct timespec t1 = { 10/*seconds*/, 0/*nanoseconds*/};//time interval
struct timespec t2 = { 180/*seconds*/, 0/*nanoseconds*/};//total running time
pthread_mutex_t mtx;


short zero(double a) 
{  
    return a > -eps && a < eps;  
}  

void swap(int *a, int *b)
{
    int temp=*a;
    *a=*b;
    *b=temp;
} 
//calculate the matrix
double det(int C[N*N]) 
{ 
    double a[N][N];
    for (short i=0;i<N;i=i+1){
	
    	for(short j=0;j<N;j=j+1)
		a[i][j] = (double)(C[i*N+j]);
    }
    double mul,Result=1;  
    int i,j,k,b[N];    
    for(i=0;i<N;i++) 
	b[i]=i;  
    for(i=0;i<N;i++){  
	if(zero(a[b[i]][i])) 
	for(j=i+1;j<N;j++)  
		if(!zero(a[b[j]][i])) 
		{	
			swap(&b[i],&b[j]); 
			Result*=-1;
			break;  
		}  
		Result*=a[b[i]][i]; 
	for(j=i+1;j<N;j++)  
		if(!zero(a[b[j]][i])){  
		      mul=a[b[j]][i]/a[b[i]][i]; 
	
		      for(k=i;k<N;k++)  
		          a[b[j]][k]-=a[b[i]][k]*mul;  
		}  
    }  
    return Result;  
}  
// write calculation times per second into data.txt file
void *calRecord(void *d)
{
	FILE *fp = fopen("data.txt","w");
	while(1){   	
	   ret = nanosleep(&t1,NULL);//ret = 1,ini 
	   if(ret == 0){
			double result = count/10.0;
			fprintf(fp,"per second calculation: %lf\n",result);
			counter += count;
			printf("in this 10 secs %ld\n", count);	
		   	pthread_mutex_lock(&mtx);
	   		count=1;
	   		pthread_mutex_unlock(&mtx);
	   }
	   ret = 1;	
	   if (counter == BUFFER_SIZE) {
    	       counter = 0;
               return NULL;
	   }
	   if(ret2 == 0){
		return NULL;
	   }
	}
}
// when time is 180s, end this program
void *timeControl(void *e)
{
   ret2 = nanosleep(&t2,NULL);
   if(ret2 == 0)
   {
   	return NULL;
   }
}
//create threads for matrix calculation
void *computation(void *arg)
{
   int index; 
   int number = *(int*)arg;
   int res = BUFFER_SIZE/NUM_THREADS;
   for (index = (number)*res; index < (number+1)*res; index = index + 1){
	   resultArr[index] = det(matrix[index]); 
	   pthread_mutex_lock(&mtx);
	   count++;
	   pthread_mutex_unlock(&mtx);
	   if(ret2 == 0){
	       break;
	   }	
   }
   return NULL;		
}


int main()
{
  printf("Resetting the device \n");
	long double a[4], b[4], loadavg;
  FILE *fp;
  short fd2;
  char c = '0';
  int i,j;
  char stringToSend[BUFFER_LENGTH];
  int threadIndex[NUM_THREADS];
  

  fd2 = open("/sys/module/dummy/parameters/no_of_reads", O_WRONLY);
  write(fd2, &c, 1);
  close(fd2);
  
  printf("*************Testing... *************\n");
  fd = open("/dev/dummychar", O_RDWR); 
  if (fd < 0){
	perror("Failed to open...");
	return errno;
  }
//record the CPU info at the start time
	fp = fopen("/proc/stat","r");
        fscanf(fp,"%*s %Lf %Lf %Lf %Lf",&a[0],&a[1],&a[2],&a[3]);
        fclose(fp);
        sleep(1);

  for(i = 0; i < BUFFER_SIZE; i++)
  {
	ret0 = read(fd, inMatrix, BUFFER_LENGTH); 
	for(j = 0; j < N*N; j++){  		
	    if (ret0 < 0){
	        perror("Failed to read..."); 
	        return errno;
	    }
	matrix[i][j] = inMatrix[j];   
	}
  }

  pthread_t timeThreads[2];
  pthread_t calmatThreads[NUM_THREADS];
  pthread_create(&timeThreads[0], NULL, calRecord, NULL);

  while(1){
	  int temp;
      int correctNum = 0;
	  pthread_create(&timeThreads[1], NULL, timeControl, NULL);
	  for(int k = 0;k < NUM_THREADS; k++)
	  {
		threadIndex[k] = k;
		pthread_create(&calmatThreads[k], NULL, computation, &threadIndex[k]);       
	  }
	  for(temp = 0; temp < NUM_THREADS;temp++)
	  {
		if(pthread_join(calmatThreads[temp], NULL) == -1){
			puts("fail to recollect A1");
			exit(1);
	  	}
	  }	
	  for(temp = 0; temp < BUFFER_SIZE; temp++)
	  {
		ret1 = write(fd, &resultArr[temp], sizeof(long long));	
		if (ret1 == -1){
			printf("the result is not right\n");
		}		    
		else if (ret1 < 0)
			perror("Failed to write....");		
		else{
			correctNum++;
		}
	  }

	  if(ret2 == 0) 
		break;
	  for(i = 0; i < BUFFER_SIZE; i++)
	  {
		ret0 = read(fd, inMatrix, BUFFER_LENGTH); 
		for(j = 0; j < N*N; j++){  		
		    if (ret0 < 0){
			    perror("Failed to read the message from the device."); 
			    return errno;
		    }
		    matrix[i][j] = inMatrix[j];   
		}
	  }
	 
	}

	  if(pthread_join(timeThreads[0], NULL) == -1){
			puts("fail to recollect time thread 0");
			exit(1);
	  	if(pthread_join(timeThreads[1], NULL) == -1){
				puts("fail to recollect time thread 1");
				exit(1);
			}
  	}
	  //record the CPU info at the end time
	fp = fopen("/proc/stat","r");
        fscanf(fp,"%*s %Lf %Lf %Lf %Lf",&b[0],&b[1],&b[2],&b[3]);
        fclose(fp);
	// calculate the CPU utility
        loadavg = ((b[0]+b[1]+b[2]) - (a[0]+a[1]+a[2])) / ((b[0]+b[1]+b[2]+b[3]) - (a[0]+a[1]+a[2]+a[3]));
        printf("The CPU utilization is : %Lf\n",loadavg);
  
  close(fd);
  printf("*************Testing finish!*************");
  return 0;
}


