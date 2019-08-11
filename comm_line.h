/* 
 * File:   comm_line.hh
 * Author: agung
 *
 * Created on February 18, 2018, 5:04 PM
 */

#ifndef COMM_LINE_H
#define COMM_LINE_H

#include <sys/types.h>
#include <unordered_map>
#include <fstream>
//#include <vector>
#include "thread_line.h"
#include "comm_seq.h"

//#define MAXTHREADS 1024   #parsec needs more threads
#define MAXTHREADS 128
//static const int CMAXTHREADS = MAXTHREADS;

class CommWindow {
private:
    PIN_MUTEX _rwftx;
    //PIN_RWMUTEX _rwstx;
    THREADID first;
    THREADID second;

public:

    CommWindow() {
        first = 0;
        second = 0;
        PIN_MutexInit(&_rwftx);
    }

    ~CommWindow() {
        PIN_MutexFini(&_rwftx);
    }

    void insert(THREADID tid) {
        //_rwftx->WriteLock();
        //PIN_RWMutexWriteLock(&_rwftx);
        PIN_MutexLock(&_rwftx);
        THREADID a = first;
        first = tid;
        second = a;
        //_rwftx->Unlock();
        //PIN_RWMutexUnlock(&_rwftx);
        PIN_MutexUnlock(&_rwftx);

        //_rwstx.WriteLock();
        //second = a;
        //_rwstx.Unlock();
    }

    THREADID getFirst() {
        return first;
    }

    THREADID getSecond() {
        return second;
    }
};

struct LineTuple {
    UINT64 first;
    UINT64 second;

    LineTuple() {
        first = 0;
        second = 0;
    }
};

class CommLine {
private:
    //mutable PIN_MUTEX _mtx;
    //mutable PIN_MUTEX _smtx;
    //PIN_RWMUTEX _rwmtx;
    //PIN_RWMUTEX _rwsmtx;
    PIN_MUTEX _mtx;
    //PIN_RWMUTEX _rwsmtx;
    //unordered_map<UINT64, THREADID> _firstMap;
    //unordered_map<UINT64, THREADID> _secondMap;
    //unordered_map<UINT64, CommWindow> lines;
    unordered_map<UINT64, UINT64> firsts;
    unordered_map<UINT64, UINT64> seconds;
    unordered_map<UINT64, LineTuple> lineTuples;

    //unordered_map<UINT64, PIN_MUTEX> lineLocks;
    //PIN_MUTEX insMtx;

public:

    CommLine() {
        //PIN_RWMutexInit(&_rwmtx);
        PIN_MutexInit(&_mtx);
        //PIN_MutexInit(&insMtx);
        //PIN_RWMutexInit(&_lmtx);
    }

    ~CommLine() {
        //PIN_RWMutexFini(&_rwmtx);
        PIN_MutexFini(&_mtx);
    }

    void insert(UINT64 line, THREADID tid) {
        //_rwmtx.TryWriteLock();
        //_mtx.Lock();
        //THREADID a = _firstMap[line];
        //_firstMap[line] = tid;
        //_mtx.Unlock();
        //_rwmtx.Unlock();

        //_rwsmtx.TryWriteLock();
        //_smtx.Lock();
        // _secondMap[line] = a;
        //_smtx.Unlock();
        //_rwsmtx.Unlock();
        //lines[line].insert(tid);
        //_mtx.Unlock();
        //PIN_RWMutexWriteLock(&_rwmtx);
        PIN_MutexLock(&_mtx);
        //PIN_MutexLock(&insMtx);
        //PIN_MutexLock(&lineLocks[line]);
        //PIN_RWMutexWriteLock(&_rwmtx);
        //if (tid ==0) 
        //   cout << "Err tid is zero: " << line << endl;
        THREADID tmp = firsts[line];
        firsts[line] = tid;
        seconds[line] = tmp;
        //THREADID tmp = lineTuples[line].first;
        //lineTuples[line].first = tid;
        //lineTuples[line].second = tmp;
        //cout << "First: " << firsts[line] << endl;
        //if (firsts[line] == 0 ) {
        //     cout << "[InsertLine] "<< line << ", first: " << firsts[line] << ", second: " 
        //         << tmp << ", old_seconds: " << seconds[line] << endl;
        // }

        //PIN_RWMutexUnlock(&_rwmtx);
        PIN_MutexUnlock(&_mtx);
        //PIN_MutexUnlock(&insMtx);
        //PIN_MutexUnlock(&lineLocks[line]);
        //PIN_MutexUnlock(line_lock);
        //printf("\t[commline] ins line=%lu,tid=%d,b=%lu\n",line,tid,seconds[line]);
    }

    LineTuple firstSecond(UINT64 line) {
        //static THREADID tids[2];
        //PIN_MutexLock(&_mtx);
        LineTuple lt;
        //PIN_RWMutexWriteLock(&_rwmtx);
        //PIN_MutexLock(&_mtx);
        //if (firsts.count(line) == 0) {
        //    firsts[line] = 0;
        //    seconds[line] = 0;
        //}
        //tids[0] = firsts[line];
        //tids[1] = seconds[line];

        bool create = false;
        if (lineTuples.count(line) == 0)
            create = true;

        if (create == true)
            PIN_MutexLock(&_mtx);

        lt = lineTuples[line];

        if (create == true) {
            PIN_MutexUnlock(&_mtx);
            //create = false;
        }

        //PIN_RWMutexUnlock(&_mtx);
        //PIN_MutexUnlock(&_mtx);
        //return tids;
        return lt;
    }

    THREADID first(UINT64 line) {
        //CommWindow cw;
        //THREADID f;
        /*
        if (_firstMap.count(line) == 0) {
            _rwmtx.WriteLock();
            f = _firstMap[line];
            _rwmtx.Unlock();
        }
        else {
            _rwmtx.TryReadLock();
            f = _firstMap[line];
            _rwmtx.Unlock();
        }*/
        //_rwmtx.WriteLock();
        //f = _firstMap[line];
        //_rwmtx.Unlock();
        //_mtx.Lock();
        //THREADID f = _firstMap[line];
        //_mtx.Unlock();
        //PIN_RWMutexWriteLock(&_rwmtx);
        THREADID f;
        //THREADID f = firsts[line];
        PIN_MutexLock(&_mtx);
        //f = firsts[line];
        //THREADID s = seconds[line];
        //PIN_MutexUnlock(&_mtx);
        //return f;
        
        if (firsts.count(line) == 0) {
            //PIN_MutexLock(&_mtx);
            //_rwmtx->WriteLock();
            //PIN_MutexUnlock(&_mtx);f = lines[line].getFirst();
            //PIN_RWMutexWriteLock(&_rwmtx);
            //f = firsts[line];
            //cw = lines[line];
            // lines[line] = CommWindow();
            //lines.insert({line, CommWindow()});
            //firsts[line] = 0;
            //seconds[line] = 0;
            // Even with the same statements, this will creates new line
            f = firsts[line];
            if (seconds.count(line) == 0)
                seconds[line] = 0;
            //THREADID s = seconds[line];
            //cout << "[InitLine] "<< line << ", first: " << f << ", second: " << seconds[line] << endl;
            //_rwmtx->Unlock();
            //PIN_RWMutexUnlock(&_rwmtx);
            PIN_MutexUnlock(&_mtx);
            //f = 0;
            //return 0;
        } else {
            //PIN_RWMutexReadLock(&_rwmtx);
            PIN_MutexUnlock(&_mtx);
            f = firsts[line];
            //return firsts[line];
            //PIN_RWMutexUnlock(&_rwmtx);
        }
        return f;
        //return lines[line].getFirst();
        //return 0;
    }

    THREADID second(UINT64 line) {
        //THREADID s;
        /*
        if (_secondMap.count(line) == 0) {
            _rwsmtx.TryWriteLock();
            s = _secondMap[line];
            _rwsmtx.Unlock();
        }
        else {
            _rwsmtx.TryReadLock();
            s = _secondMap[line];
            _rwsmtx.Unlock();
        }
         */
        //_rwsmtx.WriteLock();
        //s = _secondMap[line];
        //_rwsmtx.Unlock();
        /*
        _smtx.Lock();
        THREADID s = _secondMap[line];
        //_smtx.Unlock();
         */
        //s = lines[line].getSecond();
        //return s;
        //return lines[line].getSecond();
        //return 0;
        //PIN_RWMutexReadLock(&_rwmtx);
        //s = seconds[line];
        //PIN_RWMutexUnlock(&_rwmtx);
        //return s;
        //PIN_MutexLock(&_mtx);
        return seconds[line];
        //PIN_MutexUnlock(&_mtx);
    }

    void initZeros(UINT64 max) {
        UINT64 i;
        for (i = 0; i < max; i++) {
            //_firstMap[i] = 0;
            //_secondMap[i] = 0;
        }
        printf("Comm line initialization finished (%lu) \n", max);
    }
};

class Payload {
private:
    //PIN_RWMUTEX _pmtx;

public:
    UINT64 count;
    UINT64 size;

    Payload() {
        count = 0;
        size = 0;
        //PIN_RWMutexInit(&_pmtx);
    }

    Payload(UINT64 init_count, UINT64 init_size) {
        count = init_count;
        size = init_size;
    }

    ~Payload() {
        //PIN_RWMutexFini(&_pmtx);
    }

    void incr(UINT64 dsize) {
        //PIN_RWMutexWriteLock(&_pmtx);
        //_pmtx->WriteLock();
        count++;
        size += dsize;
        //_pmtx->Unlock();
        //PIN_RWMutexUnlock(&_pmtx);
    }
};

struct CommLoad {
    UINT64 count;
    UINT64 size;

    CommLoad(UINT64 C = 0, UINT64 S = 0) : count(C), size(S) {
    }
} CommLoad;

class TimePayload {
private:
    PIN_MUTEX _tpmtx;
    THREADID t;
public:
    unordered_map<UINT64, Payload> events;
    unordered_map<UINT64, struct CommLoad> eventLoads;

    TimePayload() {
        PIN_MutexInit(&_tpmtx);
    }

    ~TimePayload() {
        PIN_MutexFini(&_tpmtx);
    }

    Payload getPayload(UINT64 ts) {
        return events[ts];
        /*
        if (events.count(ts) > 0)
            return events[ts];
        else {
            Payload p;
            PIN_RWMutexWriteLock(&_tpmtx);
            events[ts] = p;
            PIN_RWMutexUnlock(&_tpmtx);
            return p;
        }
         */
    }

    void setThread(THREADID tid) {
        t = tid;
    }

    THREADID getThread() {
        return t;
    }

    void incr(UINT64 ts, UINT64 dsize) {
        //PIN_MutexLock(&_tpmtx);
        //_tpmtx.WriteLock();
        //if (events.count(ts) == 0)
        //    events[ts] = Payload();
        //printf("\t[incr] (tid=%d,t=%d,ts=%lu,new=%d)\n", PIN_ThreadId(), t, ts, events.count(ts)==0);
        //if (events.insert({ts, Payload(1, dsize)}).second == false)
        //    events[ts].incr(dsize);
        //eventLoads[ts].count += 1;
        //eventLoads[ts].size += dsize;
        //_tpmtx.Unlock();
        //PIN_MutexUnlock(&_tpmtx);
    }

};

class CommTrace {
private:
    //mutable PIN_MUTEX _cmtx;
    //mutable PIN_MUTEX _smtx;
    //PIN_RWMUTEX threadLocks[MAXTHREADS +1];
    //PIN_RWMUTEX _rwmtx;

    // Temporal communication events
    unordered_map<UINT64, UINT64> countMap[MAXTHREADS + 1][MAXTHREADS + 1];
    unordered_map<UINT64, UINT64> sizeMap[MAXTHREADS + 1][MAXTHREADS + 1];

    //unordered_map<UINT64, Payload> payloads [MAXTHREADS + 1][MAXTHREADS + 1];
    //TimePayload payloads [MAXTHREADS + 1][MAXTHREADS + 1];
    //TimePayload payloads [MAXTHREADS + 1][MAXTHREADS + 1];
    //vector<vector <TimePayload>> payloads{MAXTHREADS + 1, vector<TimePayload>(MAXTHREADS + 1)};
    //vector<TimePayload[MAXTHREADS +1]> payloads[MAXTHREADS + 1];

    // Spatial communication matrix
    UINT64 countMatrix[MAXTHREADS + 1][MAXTHREADS + 1];
    UINT64 sizeMatrix[MAXTHREADS + 1][MAXTHREADS + 1];
    // Temporal matrix for accumulate left and right threads
    unordered_map<UINT64, Payload> accumMap [MAXTHREADS + 1][MAXTHREADS + 1];
    int real_tid[MAXTHREADS + 1];

    // Mutexes for the map modification
    PIN_MUTEX mapLocks[MAXTHREADS + 1][MAXTHREADS +1];

public:

    CommTrace() {
        /*
        for (int i = 0 ; i < MAXTHREADS+1; i++) {
            for (int j = 0; j < MAXTHREADS +1; j++) {
                //payloads[i][j] = TimePayload(i);
                //TimePayload tp = payloads[i][j];
                //payloads[i][j].setThread(i);
                //countMap[i][j] = {};
                //sizeMap[i][j] = {};
                //printf("%d\n", countMap[i][j].empty());
            }
            //assert(PIN_RWMutexInit(threadLocks));
            //assert(PIN_RWMutexInit(&threadLocks[i]));
        }
        //PIN_RWMutexInit(&_rwmtx);
        //PIN_MutexInit(&_cmtx);
        //printf("CommTrace initialized finished\n");
         */
        for (int i = 0; i < MAXTHREADS + 1; i++) {
            for (int j = 0; j < MAXTHREADS +1; j++)
                PIN_MutexInit(&mapLocks[i][j]);
        }
    }

    ~CommTrace() {
        //`PIN_RWMutexFini(&_rwmtx);
        for (int i = 0; i < MAXTHREADS + 1; i++) {
            for (int j = 0; j < MAXTHREADS +1; j++)
                PIN_MutexFini(&mapLocks[i][j]);
        }
    }
    
    void clear_events_map(unordered_map<UINT64, UINT64> * p_map) {
        if (!p_map->empty())
            p_map->clear();
    }
    
    void print_tempo_clear(map<UINT32, UINT32> pidmap, string name, int num_threads,
            int commsize) {
        static long n = 0;

        ofstream f_accu;
        string fname_accu;

        int i = 0, j, a, b, l, r;

        for (auto it : pidmap) {
            real_tid[it.second] = i++;
        }

        for (i = num_threads - 1; i >= 0; i--) {
            a = real_tid[i];
            for (j = 0; j < num_threads; j++) {
                b = real_tid[j];
                
                PIN_MutexLock(&mapLocks[a][b]);
                for (auto it : countMap[a][b]) {
                    // Accumulated version
                    // Pair of (T1,T2) is considered same with (T2,T1)
                    if (a < b) {
                        l = a;
                        r = b;
                    } else {
                        l = b;
                        r = a;
                    }
                    accumMap[l][r][it.first].count = accumMap[l][r][it.first].count + it.second;
                    accumMap[l][r][it.first].size = accumMap[l][r][it.first].size + sizeMap[a][b][it.first];
                }
                //Delete the events
                clear_events_map(&countMap[a][b]);
                clear_events_map(&sizeMap[a][b]);
                //unordered_map<UINT64, UINT64> *cm = &countMap[a][b];
                //unordered_map<UINT64, UINT64> *sm = &sizeMap[a][b];
                //sizeMap[a][b]->clear();
                //cm->clear();
                //sm->clear();
                PIN_MutexUnlock(&mapLocks[a][b]);
            }
        }

        fname_accu = name + "." + decstr(n++) + '.' + decstr(commsize) 
                + ".comm_events.csv";
        cout << fname_accu << endl;
        
        f_accu.open(fname_accu.c_str());
        if (f_accu.is_open()) {
            for (i = num_threads - 1; i >= 0; i--) {
                a = real_tid[i];
                for (j = 0; j < num_threads; j++) {
                    b = real_tid[j];
                    for (auto it : accumMap[a][b]) {
                        //if (it.first >= 0 && it.second.count > 0) {
                        f_accu << a << ',' << b << ',' << it.first << ','
                                << it.second.count << "," << it.second.size << '\n';
                        //}
                    }
                    //Reset the buffers
                    unordered_map<UINT64, Payload> *a_m = &accumMap[a][b];
                    if (!a_m->empty())
                        a_m->clear();
                }
            }
            f_accu << endl;
            f_accu.close();
        }
    }

    //inline
    void updateTempo(THREADID a, THREADID b, UINT64 tsc, UINT32 dsize) {
        /*
         * Lock to prevent SEGV error in the case of many threads
         */

        //_cmtx.Lock();
        //PIN_RWMutexWriteLock(&threadLocks[a]);
        //PIN_MutexLock(&_cmtx);
        //if (countMap[a][b].count(tsc) == 0)
        //countMap[a][b][tsc] = 0;
        //printf("%d\n", payloads[a][b].getThread());
        //     if (b > 0)
        //         printf("[updateTempo] (tid=%d,a=%d,b=%d)\n", PIN_ThreadId(), a, b);
        //TimePayload p = payloads[a][b];

        PIN_MutexLock(&mapLocks[a][b]);
        countMap[a][b][tsc] = countMap[a][b][tsc] + 1;
        sizeMap[a][b][tsc] = sizeMap[a][b][tsc] + dsize;

        PIN_MutexUnlock(&mapLocks[a][b]);
                //PIN_RWMutexUnlock(&threadLocks[a]);
                //PIN_MutexUnlock(&_cmtx);
                //_cmtx.Unlock();

                //_smtx.Lock();
                //sizeMap[a][b][tsc] = sizeMap[a][b][tsc] + dsize;
                //_smtx.Unlock();

                //PIN_RWMutexWriteLock(&_rwmtx);
                //Payload p = payloads[a][b][tsc];
                //PIN_RWMutexUnlock(&_rwmtx);
                //TimePayload tp = payloads[a][b];
                //Payload p = payloads[a][b].getPayload(tsc);
                //p.incr(dsize);
                //tp.getPayload(tsc).incr(dsize);
                //payloads[a][b].incr(tsc, dsize);

                //PIN_RWMutexWriteLock(&_rwmtx);
                //payloads[a][b][tsc].incr(dsize);
                //if (PINThreadId() < 2)
                //printf("[updateTempo] (tid=%d,a=%d,b=%d)\n", PIN_ThreadId(), a, b);
                //PIN_RWMutexWriteLock(&threadLocks[a]);
                //payloads[a][b].incr(tsc, dsize);
                //PIN_RWMutexUnlock(&threadLocks[a]);
                //PIN_RWMutexUnlock(&_rwmtx);

    }

    inline
    void updateSpat(THREADID a, THREADID b, UINT32 dsize) {
        countMatrix[a][b] += 1;
        sizeMatrix[a][b] += dsize;
    }
    
    inline
    void updateSpat(THREADID a, THREADID b, UINT32 count, UINT32 dsize) {
        countMatrix[a][b] += count;
        sizeMatrix[a][b] += dsize;
    }

    inline
    void updateAll(THREADID a, THREADID b, UINT64 tsc, UINT32 dsize) {
        updateSpat(a, b, dsize);
        updateTempo(a, b, tsc, dsize);
    }

    void print_spat(map<UINT32, UINT32> pidmap, string name, int num_threads,
            int commsize) {
        //long n = 0;
        ofstream f, fl;
        string fname, fname_l;
        /*
        char fname[255];
        char fname_l[255];

        sprintf(fname, "%s.full.%d.comm.csv", name.c_str(), commsize);
        sprintf(fname_l, "%s.full.%d.comm_size.csv", name.c_str(), commsize);
         */
        fname = name + "." + decstr(num_threads) + "." + decstr(commsize) + ".comm.csv";
        fname_l = name + "." + decstr(num_threads) + "." +  decstr(commsize) + ".comm_size.csv";

        int real_tid[MAXTHREADS + 1];
        int i = 0, a, b;

        for (auto it : pidmap) {
            real_tid[it.second] = i++;
            //cout << "real_tid " << it.second << ": " << real_tid[it.second] << endl;
        }

        cout << fname << endl;
        cout << fname_l << endl;

        f.open(fname.c_str(), ios::out | ios::trunc);
        fl.open(fname_l.c_str(), ios::out | ios::trunc);

        for (int i = num_threads - 1; i >= 0; i--) {
            a = real_tid[i];
            for (int j = 0; j < num_threads; j++) {
                b = real_tid[j];
                f << countMatrix[a][b] + countMatrix[b][a];
                fl << sizeMatrix[a][b] + sizeMatrix[b][a];
                if (j != num_threads - 1) {
                    f << ",";
                    fl << ",";
                }
            }
            f << endl;
            fl << endl;
        }
        f << endl;
        fl << endl;
        f.close();
        fl.close();
    }

    void print_tempo_interval(map<UINT32, UINT32> pidmap, string name, int num_threads,
            int commsize) {
        static long n = 0;
        name = name + "." + decstr(n++);
        print_tempo(pidmap, name, num_threads, commsize);
    }

    void print_tempo(map<UINT32, UINT32> pidmap, string name, int num_threads,
            int commsize) {
        //unordered_map<UINT64, Payload> accums [MAXTHREADS + 1][MAXTHREADS + 1];
        //ofstream f
        ofstream f_accu;
        string fname_accu;
        //char fname[255];
        //char fname_accu[255];

        int real_tid[MAXTHREADS + 1];
        int i = 0, j, a, b, l, r;

        //sprintf(fname_accu, "%s.full.%d.accu_comm_events.csv", name.c_str(), commsize);
        fname_accu = name + "." + decstr(commsize) + ".comm_events.csv";
        cout << fname_accu << endl;

        for (auto it : pidmap) {
            real_tid[it.second] = i++;
            //cout << "real_tid " << it.second << ": " << real_tid[it.second] << endl;
        }

        for (i = num_threads - 1; i >= 0; i--) {
            a = real_tid[i];
            for (j = 0; j < num_threads; j++) {
                b = real_tid[j];
                for (auto it : countMap[a][b]) {
                    // Accumulated version
                    // Pair of (T1,T2) is considered same with (T2,T1)
                    if (a < b) {
                        l = a;
                        r = b;
                    } else {
                        l = b;
                        r = a;
                    }
                    //accums[l][r][it.first].size = accums[l][r][it.first].size +
                    //        it.second.size;
                    //accums[l][r][it.first].count = accums[l][r][it.first].count +
                    //        it.second.count;
                    accumMap[l][r][it.first].count = accumMap[l][r][it.first].count + it.second;
                    accumMap[l][r][it.first].size = accumMap[l][r][it.first].size + sizeMap[a][b][it.first];
                    //accumMap[l][r][it.first].size = accumMap[l][r][it.first].size + it.second.size;
                }
            }
        }

        f_accu.open(fname_accu.c_str());
        if (f_accu.is_open()) {
            for (i = num_threads - 1; i >= 0; i--) {
                a = real_tid[i];
                for (j = 0; j < num_threads; j++) {
                    b = real_tid[j];
                    for (auto it : accumMap[a][b]) {
                        //if (it.first >= 0 && it.second.count > 0) {
                        f_accu << a << ',' << b << ',' << it.first << ','
                                << it.second.count << "," << it.second.size << '\n';
                        //}
                    }
                }
            }
            f_accu << endl;
            f_accu.close();
        }
    }

    void print_all(map<UINT32, UINT32> pidmap, string name, int num_threads,
            int commsize) {
        print_spat(pidmap, name, num_threads, commsize);
        print_tempo(pidmap, name, num_threads, commsize);
    }

};

#endif /* COMM_LINE_H */

