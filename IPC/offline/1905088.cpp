#include<iostream>
#include<pthread.h>
#include<semaphore.h>
#include<vector>
#include<math.h>
#include<random>
#include<unistd.h> 
#include<chrono> 
#include<fstream>

using namespace std;

int N,M,w,x,y;

const int NP = 4;
const int NB = 2;

ifstream in;
ofstream out;

int entry = 0;
int readersCount = 0;
int nGroups;
int* GroupOfStudents;
chrono::high_resolution_clock::time_point startTime ;

#define IDLE 0
#define WAITING 1
#define PRINTING 2

int* PSTATE;
int STATION_STATE[4];
bool* NeedPrinting;

sem_t readersSem;
sem_t writersSem;
sem_t printing_mutex;
sem_t binding_mutex;
sem_t* printingS;
sem_t* group_print_done;
sem_t* group_bind_done;
sem_t* for_submission;
sem_t bindingStationSem[2];
pthread_mutex_t print_mutex[NP];
pthread_mutex_t bind_mutex[NB];
pthread_mutex_t entryMutex;

void init_semaphore()
{
    sem_init(&printing_mutex,0,1);
    sem_init(&binding_mutex, 0, 2);
    sem_init(&readersSem, 0, 1);
    sem_init(&writersSem, 0, 1);

    printingS = new sem_t[N];
    for(int i=0; i<N; i++){
        sem_init(&printingS[i],0,0);
    }

    group_print_done = new sem_t[nGroups];
    for (int i = 0; i < nGroups; ++i) {
        sem_init(&group_print_done[i], 0, M);
    }

    group_bind_done = new sem_t[nGroups];
    for (int i = 0; i < nGroups; ++i) {
        sem_init(&group_bind_done[i], 0, 0);
    }

    for_submission = new sem_t[nGroups];
    for (int i = 0; i < nGroups; ++i) {
        sem_init(&for_submission[i], 0, 0);
    }

    for(int i=0; i<2; i++){
        sem_init(&bindingStationSem[i],0,1);
    }

}

int poisonDistribution(){
    int lambda = 5;
    poisson_distribution<int> poisson(lambda);
    random_device rd;
    mt19937 gen(rd());
    int poissonValue = poisson(gen);
    int minDelay = 1;
    int maxDelay = 15;
    int randomDelay = min(maxDelay, max(minDelay, poissonValue));
    //out<<randomDelay<<"s"<<endl;
    return randomDelay;
}

void test(int i) {
    if(PSTATE[i]==WAITING && STATION_STATE[(i+1)%NP] != PRINTING){
        STATION_STATE[(i+1)%NP] == PRINTING;
        PSTATE[i] = PRINTING;
        sem_post(&printingS[i]);
    }
}

void take_printing_station(int i){
    int printingStation = (i+1)% NP;
    pthread_mutex_lock(&print_mutex[printingStation]);
    sem_wait(&printing_mutex);
    auto currentTime = chrono::high_resolution_clock::now();
    int elapsedTime = chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
    out << "Student " << i +1<< " has arrived at the print station at time " << elapsedTime<<"s\n";
    PSTATE[i] = WAITING;
    test(i);
    sem_post(&printing_mutex);
    sem_wait(&printingS[i]);
    pthread_mutex_unlock(&print_mutex[printingStation]);
}

void return_printing_station(int i){
    int printingStation = (i + 1) % NP;
    pthread_mutex_lock(&print_mutex[printingStation]);
    sem_wait(&printing_mutex);
    auto currentTime = chrono::high_resolution_clock::now();
    int elapsedTime = chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
    out << "Student " << i+1<< " has finished printing at time " << elapsedTime<<"s\n";
    PSTATE[i] = IDLE;
    STATION_STATE[printingStation] = IDLE;
    for (int j = 0; j < N; ++j) {
        //sem_wait(&printingS[j]);
        if (j != i && ((j+1)%NP) == printingStation &&  GroupOfStudents[j] == GroupOfStudents[i] && PSTATE[j] == WAITING) {
            test(j);
            //sem_post(&printingS[j]);
            break; // Only give to one groupmate first
        }
    }
    for (int j = 0; j < N; ++j) {
        //sem_wait(&printingS[j]);
        if (j != i && ((j+1)%NP) == printingStation && PSTATE[j] == WAITING) {
            test(j);
            //sem_post(&printingS[j]);
            break; 
        }
    }
    NeedPrinting[i] = false; 
    int group_id = i / M;
    sem_post(&printing_mutex);
    pthread_mutex_unlock(&print_mutex[printingStation]);
    sem_wait(&group_print_done[group_id]);
    sem_post(&group_bind_done[group_id]);  
}

void printing_student(int i){
    while(NeedPrinting[i]){
        //PSTATE[i] = WAITING;
        take_printing_station(i);
        sleep(w);
        return_printing_station(i);
    }
}

void* Printing(void* arg){
    int student_id = *(int*) arg;
    sleep(poisonDistribution());
    printing_student(student_id);
}

void* Binding(void* arg){
    int group_id = *(int*) arg;

    for (int i = group_id * M; i < (group_id + 1) * M; ++i) {
        sem_wait(&group_bind_done[group_id]);
    }
    int bindingStation = -1;
    for(int i=0; i<NB; ++i){
        if(sem_trywait(&bindingStationSem[i])==0){
            bindingStation = i;
            break;
        }
    }

    if(bindingStation == -1){
        for (int i=0; i<NB; ++i){
            sem_wait(&bindingStationSem[i]);
            bindingStation = i;
            break;
        }
    }

    pthread_mutex_lock(&bind_mutex[bindingStation]);  // Lock the binding station
    auto currentTime = chrono::high_resolution_clock::now();
    int elapsedTime = chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
    out << "Group " << group_id + 1 << " has finished printing at time " << elapsedTime << "s\n";
    pthread_mutex_unlock(&bind_mutex[bindingStation]);  // Unlock the binding station
    sleep(poisonDistribution());

    pthread_mutex_lock(&bind_mutex[bindingStation]); 
    currentTime = chrono::high_resolution_clock::now();
    elapsedTime = chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
    out << "Group " << group_id + 1 << " has started binding at time " << elapsedTime << "s\n";
    pthread_mutex_unlock(&bind_mutex[bindingStation]);
    sleep(x);

    pthread_mutex_lock(&bind_mutex[bindingStation]); 
    currentTime = chrono::high_resolution_clock::now();
    elapsedTime = chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
    out << "Group " << group_id + 1 << " has finished binding at time " << elapsedTime << "s\n";
    pthread_mutex_unlock(&bind_mutex[bindingStation]); 
    sem_post(&bindingStationSem[bindingStation]);
    sem_post(&group_print_done[group_id]);
    sem_post(&for_submission[group_id]);

}

void writeEntry(int i){
    sem_wait(&writersSem);
    pthread_mutex_lock(&entryMutex);
    auto currentTime = chrono::high_resolution_clock::now();
    int elapsedTime = chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
    out << "Group " << i + 1 << " has submitted the report at time " << elapsedTime << "s\n";
    pthread_mutex_unlock(&entryMutex);
    sleep(y);
    pthread_mutex_lock(&entryMutex);
    entry++;
    pthread_mutex_unlock(&entryMutex);
    sem_post(&writersSem);
}

void readEntry(int i){
    sem_wait(&readersSem);
    auto currentTime = chrono::high_resolution_clock::now();
    auto elapsedTime = chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
    out << "Staff " << i<< " has started reading the entry book " << elapsedTime << "s. Number of submissions: " << entry << endl;
    sleep(y);
    readersCount++;

    if (readersCount == 1) {
        sem_wait(&writersSem);
    }

    sem_post(&readersSem);

    sem_wait(&readersSem);
    readersCount--;

    if (readersCount == 0) {
        sem_post(&writersSem);
    }

    sem_post(&readersSem);
}

void* inform(void* arg){
    int staff_id = *(int*) arg;
    while(true){
        sleep(poisonDistribution());
        readEntry(staff_id);
    }
}

void* display(void* arg){
    int staff_id = *(int*) arg;
    while(true){
        sleep(poisonDistribution());
        readEntry(staff_id);
    }
}

void* Submission(void* arg){
    int group_id = *(int*) arg;
    sem_wait(&for_submission[group_id]);
    sleep(poisonDistribution());
    writeEntry(group_id);
}

int main(void){
    startTime = chrono::high_resolution_clock::now();

    in.open("input.txt");
    out.open("output.txt");
    in>>N>>M>>w>>x>>y;
    nGroups = N/M;

    PSTATE = new int[N];
    for(int i=0; i<N; i++){
        PSTATE[i] = IDLE;
    }

    for(int i=0; i<N; i++){
        STATION_STATE[i] = IDLE;
    }

    GroupOfStudents = new int[N];
    for(int i=0; i<N; i++){
        GroupOfStudents[i] = i/M +1;
    }

    NeedPrinting = new bool[N];
    for (int i = 0; i < N; ++i) {
        NeedPrinting[i] = true;
}

    init_semaphore();

    vector<pthread_t> student_threads;
    for(int i=0; i<N; ++i){
        int *temp = new int;
        *temp = i;
        //out<<*temp<<endl;
        pthread_t thread;
        pthread_create(&thread,NULL,Printing,(void*) temp);
        student_threads.push_back(thread);
    }

    vector<pthread_t> leader_threads;
    for(int i=0; i<nGroups; ++i){
        int *temp = new int;
        *temp = i;
        pthread_t thread;
        pthread_create(&thread,NULL,Binding,(void*) temp);
        leader_threads.push_back(thread);
    }

    vector<pthread_t> submission_threads;
    for(int i=0; i<nGroups; ++i){
        int *temp = new int;
        *temp = i;
        pthread_t thread;
        pthread_create(&thread,NULL,Submission,(void*) temp);
        submission_threads.push_back(thread);
    }

    pthread_t staff1;
    int *temp = new int;
    *temp = 1;
    pthread_create(&staff1,NULL,inform,(void*)temp );

    pthread_t staff2;
    int *temp2 = new int;
    *temp2 = 2;
	pthread_create(&staff2,NULL,display,(void*)temp2 );

    for( int i=0; i<N; ++i){
        pthread_join(student_threads[i],NULL);
    }

    for( int i=0; i<nGroups; ++i){
        pthread_join(leader_threads[i],NULL);
    }

    for( int i=0; i<nGroups; ++i){
        pthread_join(submission_threads[i],NULL);
    }

    pthread_join(staff1,NULL);
    pthread_join(staff2,NULL);

    delete[] NeedPrinting;
    delete[] PSTATE;
    delete[] GroupOfStudents;
    delete[] printingS;
    delete[] group_print_done;
    delete[] group_bind_done;
    delete[] for_submission;

    in.close();
    out.close();

    return 0;
}