/* 
 * File:   comm_window.h
 * Author: agung
 *
 * Created on February 18, 2018, 5:04 PM
 */

#ifndef COMM_SEQ_H
#define COMM_SEQ_H

#include <sys/types.h>
//#include <unordered_map>
//#include <vector>

//define MAXTHREADS 512
//static const int CMAXTHREADS = MAXTHREADS;
#define MAX_SEQ 1000000000

class CommSeq {
private:
    PIN_MUTEX mtx;
    
public:
    UINT32 first;
    UINT32 second;
    
    CommSeq() {
        first = 0;
        second = 0;
        PIN_MutexInit(&mtx);
    }
    ~CommSeq() {
        PIN_MutexFini(&mtx);
    }
    
    /**
     * Update method is synchronized,
     * because the update() and get() can be called from many threads simultaneously.
     * @param tid
     */
    void update(THREADID tid) {
        PIN_MutexLock(&mtx);
        THREADID tmp = first;
        first = tid;
        second = tmp;
        PIN_MutexUnlock(&mtx);
    }
};

class CommSequences {
private:
    //PIN_MUTEX mapLock;
    //CommSeq sequences[MAX_SEQ];
    CommSeq* sequences;
    //vector<CommSeq> sequences;
    //unordered_map<UINT64, size_t> regionMap;
    
public:

    CommSequences() {
     
    }

    ~CommSequences() {
        //PIN_MutexFini(&mapLock);
        if (sequences != NULL)
            delete[] sequences;
    }
    
    void init(UINT64 size) {
        sequences = new CommSeq[size];
    }

    void insert(UINT64 line, THREADID tid) {
        sequences[line].update(tid);
    }
    
    CommSeq getSeq(UINT64 line) {
        return sequences[line];
    }
};

#endif /* COMM_SEQ_H */

