/**
Sequencer @ 50Hz 
        Period: 2ms
    CPU CORE: 0
    Priority: RT_MAX

Service 1 - Frame acq @25Hz
        Period: 40ms
        every 2nd sequencer loop
    CPU CORE: 1
    Priority: RT_MAX - 1

Service 2 - Frame diff @25Hz
        Period: 40ms
        every 2nd sequencer loop
    CPU CORE: 2
    Priority: RT_MAX - 2

Service 3 - Frame select @10Hz
        Period: 100ms
        every 4th sequencer loop
    CPU CORE: 2
    Priority: RT_MAX - 3

Service 4 - Frame Writeback @10Hz
        Period: 100ms
        every 4th sequencer loop
    CPU CORE: 3
    Priority: RT_MAX - 4

Service 5 - Additional Image processing feature
    Frequency: 
        Period: 
        every xth sequencer loop
    CPU CORE:
    Priority:
*/


// This is necessary for CPU affinity macros in Linux
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <semaphore.h>
#include <syslog.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include <errno.h>
#include <signal.h>

#define USEC_PER_MSEC (1000)
#define NANOSEC_PER_MSEC (1000000)
#define NANOSEC_PER_SEC (1000000000)
#define NUM_CPU_CORES (4)
#define TRUE (1)
#define FALSE (0)
#define NUM_THREADS (5)
#define MY_CLOCK_TYPE CLOCK_MONOTONIC_RAW

int abortTest=FALSE;
int abortS1=FALSE, abortS2=FALSE, abortS3=FALSE, abortS4=FALSE, abortS5=FALSE;
sem_t semS1, semS2, semS3, semS4, semS5;
struct timespec start_time_val;
double start_realtime;
unsigned long long sequencePeriods;

static timer_t timer_1;
static struct itimerspec itime = {{1,0}, {1,0}};
static struct itimerspec last_itime;

static unsigned long long seqCnt=0;

typedef struct
{
    int threadIdx;
} threadParams_t;


void Sequencer(int id);
void *Service_1(void *threadp);
void *Service_2(void *threadp);
void *Service_3(void *threadp);
void *Service_4(void *threadp);
void *Service_5(void *threadp);

double getTimeMsec(void);
double realtime(struct timespec *tsptr);
void print_scheduler(void);
void initialize_services(void);


void main(void)
{
    struct timespec current_time_val, current_time_res;
    double current_realtime, current_realtime_res;

    int i, rc, scope, flags=0;

    cpu_set_t threadcpu;
    cpu_set_t allcpuset;

    pthread_t threads[NUM_THREADS];
    threadParams_t threadParams[NUM_THREADS];
    pthread_attr_t rt_sched_attr[NUM_THREADS];
    int rt_max_prio, rt_min_prio, cpuidx;

    struct sched_param rt_param[NUM_THREADS];
    struct sched_param main_param;

    pthread_attr_t main_attr;
    pid_t mainpid;



    // TODO what CPU core do i want service 5(extra processing capability) to run on?
    int thread_core_aff[NUM_THREADS] = {1, 2, 2, 3, 3}; // array for setting core affinity of the services

    // initialize the sequencer semaphores
    if (sem_init (&semS1, 0, 0)) { printf ("Failed to initialize S1 semaphore\n"); exit (-1); }
    if (sem_init (&semS2, 0, 0)) { printf ("Failed to initialize S2 semaphore\n"); exit (-1); }
    if (sem_init (&semS3, 0, 0)) { printf ("Failed to initialize S3 semaphore\n"); exit (-1); }
    if (sem_init (&semS4, 0, 0)) { printf ("Failed to initialize S4 semaphore\n"); exit (-1); }
    if (sem_init (&semS5, 0, 0)) { printf ("Failed to initialize S5 semaphore\n"); exit (-1); }

    
    // init priority variables
    rt_max_prio = sched_get_priority_max(SCHED_FIFO);
    rt_min_prio = sched_get_priority_min(SCHED_FIFO);
    // TODO delete
    // printf("rt_max_prio=%d\n", rt_max_prio);
    // printf("rt_min_prio=%d\n", rt_min_prio);

    // init mainpid and main_param
    mainpid=getpid();
    rc=sched_getparam(mainpid, &main_param);

    // set main to max prio and SCHED_FIFO
    main_param.sched_priority=rt_max_prio;
    rc=sched_setscheduler(getpid(), SCHED_FIFO, &main_param);
    if(rc < 0) perror("main_param");

    //TODO set the core affinity of the main thread to core 0 since the sequencer runs out of the main thread?

    // init service attributes
    for(i=0; i < NUM_THREADS; i++)
    {
        rc=pthread_attr_init(&rt_sched_attr[i]);
        rc=pthread_attr_setinheritsched(&rt_sched_attr[i], PTHREAD_EXPLICIT_SCHED);
        rc=pthread_attr_setschedpolicy(&rt_sched_attr[i], SCHED_FIFO);
        threadParams[i].threadIdx=i;

        CPU_ZERO(&threadcpu);
        cpuidx=(thread_core_aff[i]);
        CPU_SET(cpuidx, &threadcpu);
        rc=pthread_attr_setaffinity_np(&rt_sched_attr[i], sizeof(cpu_set_t), &threadcpu);
    
        rt_param[i].sched_priority = rt_max_prio - i;
        pthread_attr_setschedparam(&rt_sched_attr[i], &rt_param[i]);
    }
    
    // Create Service threads which will block awaiting release for:

    // Servcie_1 = RT_MAX-1	@ 50 Hz
    rc=pthread_create(&threads[0], &rt_sched_attr[0], Service_1, (void *)&(threadParams[0]));
    if(rc < 0)
        perror("pthread_create for service 1");
    else
        printf("pthread_create successful for service 1\n");

    // Service_2 = RT_MAX-2	@ 20 Hz
    rc=pthread_create(&threads[1], &rt_sched_attr[1], Service_2, (void *)&(threadParams[1]));
    if(rc < 0)
        perror("pthread_create for service 2");
    else
        printf("pthread_create successful for service 2\n");

    // Service_3 = RT_MAX-3	@ 10 Hz
    rc=pthread_create(&threads[2], &rt_sched_attr[2], Service_3, (void *)&(threadParams[2]));
    if(rc < 0)
        perror("pthread_create for service 3");
    else
        printf("pthread_create successful for service 3\n");

    // Service_4 = RT_MAX-4	@ 5 Hz
    rc=pthread_create(&threads[3], &rt_sched_attr[3], Service_4, (void *)&(threadParams[3]));
    if(rc < 0)
        perror("pthread_create for service 4");
    else
        printf("pthread_create successful for service 4\n");

    // Service_5 = RT_MAX-4	@ 5 Hz
    rc=pthread_create(&threads[4], &rt_sched_attr[4], Service_5, (void *)&(threadParams[4]));
    if(rc < 0)
        perror("pthread_create for service 5");
    else
        printf("pthread_create successful for service 5\n");

    // Create Sequencer thread, which like a cyclic executive, is highest prio
    printf("Start sequencer\n");
    sequencePeriods=10000; // TODO temporary 20sec demo
    // sequencePeriods=900000; // (30mins * 60000 ms/min) / 2ms/interations = 900000 iterations 

    // Sequencer = RT_MAX	@ 200 Hz (5ms)
    /* set up to signal SIGALRM if timer expires */
    timer_create(CLOCK_REALTIME, NULL, &timer_1);

    signal(SIGALRM, (void(*)()) Sequencer);

    /* arm the interval timer */
    itime.it_interval.tv_sec = 0;
    itime.it_interval.tv_nsec = 10000000;
    itime.it_value.tv_sec = 0;
    itime.it_value.tv_nsec = 10000000;

    timer_settime(timer_1, flags, &itime, &last_itime);

    for(i=0;i<NUM_THREADS;i++)
    {
        if(rc=pthread_join(threads[i], NULL) < 0)
		perror("main pthread_join");
	// else
		// printf("joined thread %d\n", i);
    }

//    printf("\nTEST COMPLETE\n");
}

void Sequencer(int id)
{
    struct timespec current_time_val;
    double current_realtime;
    int rc, flags=0;

    // received interval timer signal
           
    seqCnt++;

    //clock_gettime(MY_CLOCK_TYPE, &current_time_val); current_realtime=realtime(&current_time_val);
    //printf("Sequencer on core %d for cycle %llu @ sec=%6.9lf\n", sched_getcpu(), seqCnt, current_realtime-start_realtime);
    //syslog(LOG_CRIT, "Sequencer on core %d for cycle %llu @ sec=%6.9lf\n", sched_getcpu(), seqCnt, current_realtime-start_realtime);


    // Release each service at a sub-rate of the generic sequencer rate

    // Servcie_1 = RT_MAX-1	@ 50 Hz
    if((seqCnt % 2) == 0) sem_post(&semS1);

    // Service_2 = RT_MAX-2	@ 20 Hz
    if((seqCnt % 2) == 0) sem_post(&semS2);

    // Service_3 = RT_MAX-3	@ 10 Hz
    if((seqCnt % 4) == 0) sem_post(&semS3);

    // Service_4 = RT_MAX-4	@ 5 Hz
    if((seqCnt % 4) == 0) sem_post(&semS4);

    // TODO TBD for extra image processing thread frequency 
    // Service_5 = RT_MAX-5	@ 5 Hz
    // if((seqCnt % 20) == 0) sem_post(&semS4);

    if(abortTest || (seqCnt >= sequencePeriods))
    {
        // disable interval timer
        itime.it_interval.tv_sec = 0;
        itime.it_interval.tv_nsec = 0;
        itime.it_value.tv_sec = 0;
        itime.it_value.tv_nsec = 0;
        timer_settime(timer_1, flags, &itime, &last_itime);
	// printf("Disabling sequencer interval timer with abort=%d and %llu of %lld\n", abortTest, seqCnt, sequencePeriods);

	// shutdown all services
        sem_post(&semS1); sem_post(&semS2); sem_post(&semS3);
        sem_post(&semS4); sem_post(&semS5);

        abortS1=TRUE; abortS2=TRUE; abortS3=TRUE;
        abortS4=TRUE; abortS5=TRUE;
    }

}



void *Service_1(void *threadp)
{
    struct timespec current_time_val;
    double current_realtime;
    unsigned long long S1Cnt=0;
    threadParams_t *threadParams = (threadParams_t *)threadp;

    // Start up processing and resource initialization
    clock_gettime(MY_CLOCK_TYPE, &current_time_val); current_realtime=realtime(&current_time_val);
    syslog(LOG_CRIT, "S1 thread @ sec=%6.9lf\n", current_realtime-start_realtime);
    printf("S1 thread @ sec=%6.9lf\n", current_realtime-start_realtime);

    while(!abortS1) // check for synchronous abort request
    {
	// wait for service request from the sequencer, a signal handler or ISR in kernel
        sem_wait(&semS1);

        S1Cnt++;

	// DO WORK

	// on order of up to milliseconds of latency to get time
        clock_gettime(MY_CLOCK_TYPE, &current_time_val); current_realtime=realtime(&current_time_val);
        syslog(LOG_CRIT, "S1 50 Hz on core %d for release %llu @ sec=%6.9lf\n", sched_getcpu(), S1Cnt, current_realtime-start_realtime);
    }

    // Resource shutdown here
    //
    pthread_exit((void *)0);
}


void *Service_2(void *threadp)
{
    struct timespec current_time_val;
    double current_realtime;
    unsigned long long S2Cnt=0;
    threadParams_t *threadParams = (threadParams_t *)threadp;

    clock_gettime(MY_CLOCK_TYPE, &current_time_val); current_realtime=realtime(&current_time_val);
    syslog(LOG_CRIT, "S2 thread @ sec=%6.9lf\n", current_realtime-start_realtime);
    printf("S2 thread @ sec=%6.9lf\n", current_realtime-start_realtime);

    while(!abortS2)
    {
        sem_wait(&semS2);
        S2Cnt++;

        clock_gettime(MY_CLOCK_TYPE, &current_time_val); current_realtime=realtime(&current_time_val);
        syslog(LOG_CRIT, "S2 20 Hz on core %d for release %llu @ sec=%6.9lf\n", sched_getcpu(), S2Cnt, current_realtime-start_realtime);
    }

    pthread_exit((void *)0);
}


void *Service_3(void *threadp)
{
    struct timespec current_time_val;
    double current_realtime;
    unsigned long long S3Cnt=0;
    threadParams_t *threadParams = (threadParams_t *)threadp;

    clock_gettime(MY_CLOCK_TYPE, &current_time_val); current_realtime=realtime(&current_time_val);
    syslog(LOG_CRIT, "S3 thread @ sec=%6.9lf\n", current_realtime-start_realtime);
    printf("S3 thread @ sec=%6.9lf\n", current_realtime-start_realtime);

    while(!abortS3)
    {
        sem_wait(&semS3);
        S3Cnt++;

        clock_gettime(MY_CLOCK_TYPE, &current_time_val); current_realtime=realtime(&current_time_val);
        syslog(LOG_CRIT, "S3 10 Hz on core %d for release %llu @ sec=%6.9lf\n", sched_getcpu(), S3Cnt, current_realtime-start_realtime);
    }

    pthread_exit((void *)0);
}


void *Service_4(void *threadp)
{
    struct timespec current_time_val;
    double current_realtime;
    unsigned long long S4Cnt=0;
    threadParams_t *threadParams = (threadParams_t *)threadp;

    clock_gettime(MY_CLOCK_TYPE, &current_time_val); current_realtime=realtime(&current_time_val);
    syslog(LOG_CRIT, "S4 thread @ sec=%6.9lf\n", current_realtime-start_realtime);
    printf("S4 thread @ sec=%6.9lf\n", current_realtime-start_realtime);

    while(!abortS4)
    {
        sem_wait(&semS4);
        S4Cnt++;

        clock_gettime(MY_CLOCK_TYPE, &current_time_val); current_realtime=realtime(&current_time_val);
        syslog(LOG_CRIT, "S4 5 Hz on core %d for release %llu @ sec=%6.9lf\n", sched_getcpu(), S4Cnt, current_realtime-start_realtime);
    }

    pthread_exit((void *)0);
}


void *Service_5(void *threadp)
{
    struct timespec current_time_val;
    double current_realtime;
    unsigned long long S5Cnt=0;
    threadParams_t *threadParams = (threadParams_t *)threadp;

    clock_gettime(MY_CLOCK_TYPE, &current_time_val); current_realtime=realtime(&current_time_val);
    syslog(LOG_CRIT, "S5 thread @ sec=%6.9lf\n", current_realtime-start_realtime);
    printf("S5 thread @ sec=%6.9lf\n", current_realtime-start_realtime);

    while(!abortS5)
    {
        sem_wait(&semS5);
        S5Cnt++;

        clock_gettime(MY_CLOCK_TYPE, &current_time_val); current_realtime=realtime(&current_time_val);
        syslog(LOG_CRIT, "S5 2 Hz on core %d for release %llu @ sec=%6.9lf\n", sched_getcpu(), S5Cnt, current_realtime-start_realtime);
    }

    pthread_exit((void *)0);
}


// double getTimeMsec(void)
// {
//   struct timespec event_ts = {0, 0};

//   clock_gettime(MY_CLOCK_TYPE, &event_ts);
//   return ((event_ts.tv_sec)*1000.0) + ((event_ts.tv_nsec)/1000000.0);
// }


double realtime(struct timespec *tsptr)
{
    return ((double)(tsptr->tv_sec) + (((double)tsptr->tv_nsec)/1000000000.0));
}

void initialize_services(void)
{
    
}


// void print_scheduler(void)
// {
//    int schedType;

//    schedType = sched_getscheduler(getpid());

//    switch(schedType)
//    {
//        case SCHED_FIFO:
//            printf("Pthread Policy is SCHED_FIFO\n");
//            break;
//        case SCHED_OTHER:
//            printf("Pthread Policy is SCHED_OTHER\n"); exit(-1);
//          break;
//        case SCHED_RR:
//            printf("Pthread Policy is SCHED_RR\n"); exit(-1);
//            break;
//        //case SCHED_DEADLINE:
//        //    printf("Pthread Policy is SCHED_DEADLINE\n"); exit(-1);
//        //    break;
//        default:
//            printf("Pthread Policy is UNKNOWN\n"); exit(-1);
//    }
// }
