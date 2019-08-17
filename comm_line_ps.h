/* 
 * File:   comm_line_ps.h
 * Communication detection using producer-consumer pattern
 * Author: agung
 *
 * Created on July 2, 2019, 9:53 PM
 */

#ifndef COMMLPS_H
#define COMMLPS_H

#include <set>
#include <vector>
//#include <atomic>
#include <unordered_set>
#include <sys/types.h>

class ThreadPayload {
private:
    PIN_MUTEX payLock;
    //PIN_RWMUTEX payLock;

public:
    THREADID m_Tid;
    //mutable std::atomic<int> m_Count;
    //mutable std::atomic<int> m_Size; 
    mutable UINT32 m_Count;
    mutable UINT64 m_Size; 
    
    
    ThreadPayload(THREADID tid) : 
    m_Tid(tid), m_Count(0), m_Size(0) {
        PIN_MutexInit(&payLock);
        //PIN_RWMutexInit(&payLock);
    }

    ~ThreadPayload() {
        PIN_MutexFini(&payLock);
        //PIN_RWMutexFini(&payLock);
    }
    
    ThreadPayload(THREADID tid, UINT32 n, UINT sz) : 
    m_Tid(tid), m_Count(n), m_Size(sz) {
    }

    bool operator<(const ThreadPayload &clObj) const {
        return (m_Tid < clObj.m_Tid);
    }

    bool operator==(const ThreadPayload &clObj) const {
        return (m_Tid == clObj.m_Tid);
    }
    
    bool isEmpty() {
        return m_Tid == 0;
    }

    void update(UINT32 n, UINT32 sz) {
        //PIN_MutexLock(&payLock);
        //PIN_RWMutexWriteLock(&payLock);
        m_Count += n;
        //PIN_RWMutexWriteLock(&payLock);
        PIN_MutexLock(&payLock);
        m_Size += sz;
        PIN_MutexUnlock(&payLock);
        //PIN_RWMutexUnlock(&payLock);
    }

    void resetLoad() {
        m_Count = m_Size = 0;
    }
};

class CommLineProdCons {
//private:
    //mutable std::set<THREADID> earls;
//    mutable PIN_MUTEX earlsLock;
//    mutable std::set<ThreadPayload> earls;

public:
    UINT64 m_Line;
    //size_t m_Ref; //Start from 1, 0 means dummy
    mutable THREADID m_First;
    mutable THREADID m_Second;
    
    mutable UINT64 m_First_addr;
    mutable UINT64 m_Second_addr;
    //mutable int n_earls;
    //mutable std::vector<UINT32> earls;
    //mutable UINT32* earls;
    
    CommLineProdCons(UINT64 line) : 
    m_Line(line), m_First(0), m_Second(0),
    m_First_addr(0), m_Second_addr(0) {
  //      PIN_MutexInit(&earlsLock);
    }

    CommLineProdCons() :
    m_Line(0), m_First(0), m_Second(0),
    m_First_addr(0), m_Second_addr(0) {
  //      PIN_MutexInit(&earlsLock);
    }
    
    //CommLineProdCons(UINT64 addr, int num_threads) :
    //m_Addr(addr), m_First(0), m_Second(0),
    //m_First_addr(0), m_Second_addr(0) {
  //      PIN_MutexInit(&earlsLock);
        //earls = new UINT32[n_earls];
        //earls = new std::vector<UINT32>(n_earls);
    //}
    
    ~CommLineProdCons() {
  //      PIN_MutexFini(&earlsLock);
        //printf("** [L-%ld] CommLine dtor: n_earls=%ld\n", m_Addr, earls.size());
    }

    bool operator<(const CommLineProdCons &clObj) const {
        return (this->m_Line < clObj.m_Line);
    }

    bool operator==(const CommLineProdCons &clObj) const {
        return (m_Line == clObj.m_Line);
    }

    // Update line assumes only for write op
    void update(THREADID tid, UINT64 addr) {
        // Consecutive writes reset the producer
        /*
        if (addr == m_First_addr) {
           m_Second = 0;
           m_Second_addr = 0; 
        }
        */
        //else {
        m_Second = m_First;
        m_First = tid;

        m_Second_addr = m_First_addr;
        //}
        m_First_addr = addr;
    }

    void updateFirst(THREADID tid) {
        m_First = tid;
    }

    THREADID getFirst() {
        return m_First;
    }

    THREADID getSecond() {
        return m_Second;
    }
    
    UINT64 getFirst_addr() {
        return m_First_addr;
    }

    UINT64 getSecond_addr() {
        return m_Second_addr;
    }

    void setEmpty() {
        setLine(0);
    }

    bool isEmpty() {
        return (m_Line == 0 ? true : false);
    }

    void setLine(UINT64 line) {
        m_Line = line;
    } 

/*
    void addEarl(THREADID tid, UINT32 n, UINT32 sz) {
        ThreadPayload cand = ThreadPayload(tid);
        auto it = earls.find(cand);
        const_cast<ThreadPayload&> (*it).update(n, sz);
    }

    int numEarls() {
        return earls.size();
        //return n_earls;
    }

    ThreadPayload getUpdateEarl(THREADID tid, UINT32 n, UINT64 sz) {
        ThreadPayload cand = ThreadPayload(tid);
        auto it = earls.find(cand);
        if (it != earls.end()) {
            const_cast<ThreadPayload&> (*it).update(n, sz);
            return (*it);
        }
        else {
            cand.update(n, sz);
            PIN_MutexLock(&earlsLock);
            auto result = earls.insert(cand);
            PIN_MutexUnlock(&earlsLock);
            //const_cast<ThreadPayload&> (*result.first).update(n, sz);
            return (*result.first);
        }
    }
    
    void updateCreateEarl(THREADID tid, UINT32 n, UINT64 sz) {
        ThreadPayload cand = ThreadPayload(tid);
        //PIN_MutexLock(&earlsLock);
        auto it = earls.find(cand);
        if (it != earls.end()) {
            const_cast<ThreadPayload&> (*it).update(n, sz);
        }
        else {
            cand.update(n, sz);
    //        PIN_MutexLock(&earlsLock);
            earls.insert(cand);
    //        PIN_MutexUnlock(&earlsLock);
            //const_cast<ThreadPayload&> (*result.first).update(n, sz);
        }
        //PIN_MutexUnlock(&earlsLock);
    }

    void deleteEarl(THREADID tid) {
        ThreadPayload cand = ThreadPayload(tid);
   //     PIN_MutexLock(&earlsLock);
        //earls.erase(tid);
        earls.erase(cand);
   //     PIN_MutexUnlock(&earlsLock);
    }
    
    void deleteEarlPayload(ThreadPayload pl) {
        //PIN_MutexLock(&earlsLock);
        //size_t n_erased = earls.erase(pl);
        earls.erase(pl);
        //PIN_MutexUnlock(&earlsLock);
        //printf(" [L-%ld] erased %ld payloads of tid=%d\n", m_Addr, n_erased, pl.m_Tid);
    }

    // CAUTION: clearing set is really slow
    void clearEarls() {
       //PIN_MutexLock(&vecLock);
       earls.clear();
       //PIN_MutexUnlock(&vecLock);
    }

    std::set<ThreadPayload> getEarls() {
        //PIN_MutexLock(&earlsLock);
        //std::set<ThreadPayload> c_earls = earls;
        //PIN_MutexUnlock(&earlsLock);
        //return c_earls;
        return earls;
    }
    
    void resetLoadEarl(ThreadPayload cand) {
        auto it = earls.find(cand);
        if (it != earls.end()) {
            const_cast<ThreadPayload&> (*it).resetLoad();
        }
    }
*/
};

class CommLineProdConsSet {
private:
    //PIN_RWMUTEX setLock;
    PIN_MUTEX setLock;
    //PIN_RWMUTEX vecLock;
    std::set<CommLineProdCons> setCommLPS;
    //std::unordered_set<CommL> setCommL;
    //std::vector<CommLWin *> lineWindows;

public:

    CommLineProdConsSet()
    : setCommLPS() {
    //, lineWindows() {
        PIN_MutexInit(&setLock);
        //PIN_RWMutexInit(&setLock);
        //PIN_RWMutexInit(&vecLock);
        
        /*
        lineWindows.reserve(1000);
        CommLWin * new_win = new CommLWin();
        lineWindows.push_back(new_win);
        */
    }

    ~CommLineProdConsSet() {
        PIN_MutexFini(&setLock);
        //PIN_RWMutexFini(&setLock);
    }

    bool exists(UINT64 line) {
        bool found = false;
        CommLineProdCons cand_cl = CommLineProdCons(line);
        //CommL *cand_cl = new CommL(line);
        //setLock.acquire_read();
        //PIN_RWMutexReadLock(&setLock);
        PIN_MutexLock(&setLock);
        //std::set<CommL>::iterator it = setCommL.find(cand_cl);
        auto it = setCommLPS.find(cand_cl);
        if (it != setCommLPS.end())
            found = true;
        //else
        //    found = false;
        //setLock.release();
        //PIN_RWMutexUnlock(&setLock);
        PIN_MutexUnlock(&setLock);
        return found;
    }

    CommLineProdCons getLine(UINT64 line) {
        CommLineProdCons cand_cl = CommLineProdCons(line);
        //CommL *cand_cl = new CommL(line);
        //setLock.acquire_read();
        auto it = setCommLPS.find(cand_cl);
        //setLock.release();
        if (it != setCommLPS.end()) {
            return (*it);
            //return it;
        } else {
            CommLineProdCons new_cl = CommLineProdCons(line);
            //CommL *new_cl = new CommL(line);
            //setLock.acquire();
            //PIN_RWMutexWriteLock(&setLock);
            PIN_MutexLock(&setLock);
            auto result = setCommLPS.insert(new_cl);
            PIN_MutexUnlock(&setLock);
            //PIN_RWMutexUnlock(&setLock);
            //setLock.release();
            return (*result.first);
        }
    }
    
    /*
    CommLineProdCons getLine(UINT64 line, int num_threads) {
        CommLineProdCons cand_cl = CommLineProdCons(line, num_threads);
        //CommLineProdCons cand_cl = CommLineProdCons(line);
        //CommL *cand_cl = new CommL(line);
        //setLock.acquire_read();
        auto it = setCommLPS.find(cand_cl);
        //setLock.release();
        if (it != setCommLPS.end()) {
            return (*it);
            //return it;
        } else {
            //CommLineProdCons new_cl = CommLineProdCons(line);
            //CommL *new_cl = new CommL(line);
            //setLock.acquire();
            //PIN_RWMutexWriteLock(&setLock);
            PIN_MutexLock(&setLock);
            auto result = setCommLPS.insert(cand_cl);
            PIN_MutexUnlock(&setLock);
            //PIN_RWMutexUnlock(&setLock);
            //setLock.release();
            return (*result.first);
        }
    }
    */

    CommLineProdCons getLineLazy(UINT64 line) {
        CommLineProdCons cand_cl = CommLineProdCons(line);
        auto it = setCommLPS.find(cand_cl);
        if (it != setCommLPS.end()) {
            return (*it);
            //return it;
        } else {
            cand_cl.setEmpty();
            return cand_cl;
        }
    }
    
    void updateCreateLine(UINT64 line, THREADID tid, UINT64 addr) {
        CommLineProdCons cand = CommLineProdCons(line);
        auto it = setCommLPS.find(cand);
        if (it != setCommLPS.end()) {
            const_cast<CommLineProdCons&> (*it).update(tid, addr);
        }
        else {
            cand.update(tid, addr);
            PIN_MutexLock(&setLock);
            setCommLPS.insert(cand);
            PIN_MutexUnlock(&setLock);
        }
    }
    
    /*
    void updateCreateLineBatch(UINT64 start, UINT64 addr, UINT32 len, THREADID tid) {
        // Tid must be > 0, e.g, tid should be in tid+1 outside the function
        //if (len > 1)
        //    printf(" [L-%ld] update m_First %ld..%ld\n", start, start, start+len-1);
        for (UINT64 line = start; line < start+len; ++line) {
            CommLineProdCons cand = CommLineProdCons(line);
            auto it = setCommLPS.find(cand);
            if (it == setCommLPS.end()) {
                // Not found, new insert
                //cand.updateFirst(tid);
                cand.update(tid, addr);
                PIN_MutexLock(&setLock);
                setCommLPS.insert(cand);
                PIN_MutexUnlock(&setLock);
            }
            else if (tid != (*it).m_First) {
                // Update only if current tid is different
                //printf(" [L-%ld] update m_First %d -> %d\n", line, (*it).m_First, tid);
                //const_cast<CommLineProdCons&> (*it).updateFirst(tid);
                const_cast<CommLineProdCons&> (*it).update(tid, addr);
            }
        } 
    }
    */
    
    void updateCreateLineBatch(UINT64 start, UINT64 addr, UINT32 len, THREADID tid) {
        // Tid must be > 0, e.g, tid should be in tid+1 outside the function
        CommLineProdCons cand = CommLineProdCons();
        cand.update(tid, addr);
        for (UINT64 line = start; line < start+len; ++line) {
            cand.setLine(line);
            auto it = setCommLPS.find(cand);
            if (it == setCommLPS.end()) {
                // Not found, new insert
                PIN_MutexLock(&setLock);
                setCommLPS.insert(cand);
                PIN_MutexUnlock(&setLock);
            }
            else if (tid != (*it).m_First) {
                // Update only if current tid is different
                const_cast<CommLineProdCons&> (*it).update(tid, addr);
            }
        } 
    }

    void updateLine(UINT64 line, THREADID tid, UINT64 addr) {
        CommLineProdCons cand_cl = CommLineProdCons(line);
        auto it = setCommLPS.find(cand_cl);
        if (it != setCommLPS.end()) {
            const_cast<CommLineProdCons&> (*it).update(tid, addr);
        }
        //else {
        //    printf(":: Cannot find line: %ld\n", line);
        //}
    }
/*    
    void addLineEarl(UINT64 line, THREADID tid, UINT32 n, UINT32 sz) {
        CommLineProdCons cand_cl = CommLineProdCons(line);
        auto it = setCommLPS.find(cand_cl);
        //const_cast<CommLineProdCons&> (*it).addEarl(tid, n, sz);
        const_cast<CommLineProdCons&> (*it).updateCreateEarl(tid, n, sz);
    }
    
    void removeLineEarl(UINT64 line, THREADID tid) {
        CommLineProdCons cand_cl = CommLineProdCons(line);
        auto it = setCommLPS.find(cand_cl);
        const_cast<CommLineProdCons&> (*it).deleteEarl(tid);
    }
    
    void removeLineEarlPayload(UINT64 line, ThreadPayload pl) {
        CommLineProdCons cand_cl = CommLineProdCons(line);
        auto it = setCommLPS.find(cand_cl);
        const_cast<CommLineProdCons&> (*it).deleteEarlPayload(pl);
    }

    void clearLineEarls(UINT64 line) {
        CommLineProdCons cand_cl = CommLineProdCons(line);
        auto it = setCommLPS.find(cand_cl);
        const_cast<CommLineProdCons&> (*it).clearEarls();
    }
    
    int numLineEarls(UINT64 line) {
        CommLineProdCons cand_cl = CommLineProdCons(line);
        auto it = setCommLPS.find(cand_cl);
        return const_cast<CommLineProdCons&> (*it).numEarls();
    }
    
    void resetLoadLineEarl(UINT64 line, ThreadPayload pl) {
        CommLineProdCons cand_cl = CommLineProdCons(line);
        auto it = setCommLPS.find(cand_cl);
        return const_cast<CommLineProdCons&> (*it).resetLoadEarl(pl);
    }

    std::set<ThreadPayload> getLineEarls(UINT64 line) {
        CommLineProdCons cand_cl = CommLineProdCons(line);
        auto it = setCommLPS.find(cand_cl);
        return const_cast<CommLineProdCons&> (*it).getEarls();
    }
*/
};

#endif /* COMMLPS_H */

