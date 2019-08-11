/* 
 * File:   comm_line.hh
 * Author: agung
 *
 * Created on February 18, 2018, 5:04 PM
 */

#ifndef THREAD_LINE_H
#define THREAD_LINE_H

#include <sys/types.h>
#include <unordered_map>
#include <vector>

//#define MAXTHREADS 512
//static const int CMAXTHREADS = MAXTHREADS;

class CommRegion {
private:
    // Ceveats! Lock is not required generally,
    // because a line is sequentially accessed
    //PIN_MUTEX mtx;
    
public:
    UINT64 first;
    UINT64 second;
    
    
    CommRegion() {
        first = 0;
        second = 0;
        //PIN_MutexInit(&mtx);
    }
    ~CommRegion() {
        //PIN_MutexFini(&mtx);
    }
    
    /**
     * Update method is synchronized,
     * because the update() and get() can be called from many threads simultaneously.
     * @param tid
     */
    void update(THREADID tid) {
        //PIN_MutexLock(&mtx);
        THREADID tmp = first;
        first = tid;
        second = tmp;
        //PIN_MutexUnlock(&mtx);
    }
};

class ThreadLine {
private:
    PIN_MUTEX mapLock;
    vector<CommRegion *> regions;
    unordered_map<UINT64, size_t> regionMap;    // Global region map
    //unordered_map<UINT64, size_t> *tRegions[MAXTHREADS +1];    // Local region
    //unordered_map<UINT64, size_t> tRegions[MAXTHREADS +1];    // Local region
    //vector<unordered_map<UINT64, size_t>> tRegions;
    unordered_map<UINT64, size_t> *tRegions;
    /**
     * Ceveats!
     * Unordered map can raise a segv error when it is initialized without pointer
     * (see heap vs stack usage for initialization)
     */
    //PIN_MUTEX *tLocks;
    //size_t num_threads = 0;
    int numRegPrecreate = 0;
    bool firstPrecreated = false;
    
public:

    ThreadLine() {
        PIN_MutexInit(&mapLock);
        CommRegion *cr = new CommRegion();      // Dummy region, pos=0 means unallocated
        regions.push_back(cr);
        
        //tRegions.reserve(MAXTHREADS + 1); // Reserve memory not to allocate it 10 times...
        //for (int i = 0; i < MAXTHREADS + 1; ++i) {
        //    tRegions.push_back (unordered_map<UINT64, size_t>());
       // }
        //init();
    }
    
    void init(const size_t nthreads, const int numRegPre) {
        //num_threads = nthreads;
        numRegPrecreate = numRegPre;
        tRegions = new unordered_map<UINT64, size_t>[nthreads];
        //tLocks = new PIN_MUTEX[nthreads];
        //size_t i = 0;
        //for (i = 0; i < nthreads; i++) {
        //    PIN_MutexInit(&tLocks[i]);
        //}
    }
    
    ~ThreadLine() {
        PIN_MutexFini(&mapLock);
        // Cleanup
        delete[] tRegions;
        
        size_t i;
        for (i = 0; i < regions.size(); i++) {
            delete[] regions[i];
        }
        //for (i = 0; i < num_threads; i++) {
        //    PIN_MutexFini(&tLocks[i]);
        //}
    }

    void insert(UINT64 line, THREADID tid) {
        /*
        size_t pos = regionMap[line];
        PIN_MutexLock(&regionMutexes[pos]);
        THREADID tmp = regions[pos].first;
        regions[pos].first = tid;
        regions[pos].second = tmp;
        PIN_MutexUnlock(&regionMutexes[pos]);
        */
        size_t pos = regionMap[line];
        regions[pos]->update(tid);
    }
    
    /**
     * Get region is synchronized if creating new region,
     * because this method can be called from many threads simultaneously.
     * @param line
     * @return 
     */
    CommRegion* getRegion(UINT64 line) {
        //size_t region_exist = 0;
        PIN_MutexLock(&mapLock);
        size_t pos = regionMap[line];
        //region_exist = regionMap.count(line); 
        //PIN_MutexUnlock(&mapLock);
        //cout << "Line: " << line << endl;

        if (pos == 0) {
            // New line will be inserted
            //CommRegion cr;
            CommRegion *cr = new CommRegion();
            pos = regions.size();
            regions.push_back(cr);
            regionMap[line] = pos;
            //PIN_MutexUnlock(&mapLock);
            //return cr;
        }
        //lineAddr.insert(line);
        //else {
            // Existing line just to be read
        //    PIN_MutexUnlock(&mapLock);
        //    return regions[regionMap[line]];
        //}
        PIN_MutexUnlock(&mapLock);
        return regions[pos];
    }

    void preCreateRegion(UINT64 startLine, int n) {
        size_t new_pos;
        for (int i = 1; i < n+1; i++){
            //if (regionMap[startLine + i] == 0) {
            CommRegion *cr = new CommRegion();
            new_pos = regions.size();
            regions.push_back(cr);
            regionMap[startLine + i] = new_pos;
            //}
        }
    }

    /**
     * A method to sync the thread (local) region to global (mutexed)
     * @param line
     * @return 
     */
    size_t emplaceRegion(UINT64 line) {
        PIN_MutexLock(&mapLock);
        size_t globalPos = regionMap[line];     // Check if any threads have it
        if (globalPos == 0) {
            // New line will be inserted
            //CommRegion cr;
            
            CommRegion *cr = new CommRegion();
            globalPos = regions.size();
            regions.push_back(cr);
            regionMap[line] = globalPos;
            
            if (!firstPrecreated) {
                preCreateRegion(line, numRegPrecreate);
                firstPrecreated = true;
            }
        }
        //tRegions[tid][line] = globalPos;
        PIN_MutexUnlock(&mapLock);
        return globalPos;
        //*pos = globalPos;
    }
    
    /*
     * A method to make a local access become lock-free to increase performance
     */
    CommRegion* getThreadRegion(THREADID tid, UINT64 line) {
        //THREADID curr_t = PIN_GetTid();
        //unordered_map<UINT64, size_t> tR = tRegions[tid];
        //size_t pos = tRegions[tid][line];
        //size_t pos = tR[line];
        //printf("Tid access %d,%lu\n", tid, line);
        //size_t pos = tR[line];
        // Check from global map
        
        //PIN_MutexLock(&mapLock);
        size_t pos = tRegions[tid][line];
        //printf("Pos: %lu\n", *pos);
        //if (tRegions[curr_t].count(line) == 0)
        if (pos == 0) {
            pos = emplaceRegion(line);
            //emplaceRegion(line, pos);
            tRegions[tid][line] = pos;
            //printf("Thread-%d create line=%lu, pos=%lu\n", tid, line, pos);
        }
        //PIN_MutexUnlock(&mapLock);
        return regions[pos];
    }
};

#endif /* THREAD_LINE_H */

