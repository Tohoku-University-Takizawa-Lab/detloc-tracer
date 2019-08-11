/* 
 * File:   comm_l_set.h
 * Author: agung
 *
 * Created on July 2, 2018, 9:53 PM
 */

#ifndef COMML_H
#define COMML_H

#include <set>
#include <vector>
#include <unordered_set>
#include <sys/types.h>

struct CommLWin {
    THREADID first;
    THREADID second;
    THREADID third;
    THREADID fourth;

    CommLWin() : first(0), second(0), third(0), fourth(0) {
    }

    void update(THREADID new_tid) {
        //THREADID tmp = third;
        fourth = third;
        third = second;
        second = first;
        first = new_tid;
    }
};

class CommL {
public:
    UINT64 m_Addr;
    size_t m_Ref; //Start from 1, 0 means dummy
    mutable THREADID m_First;
    mutable THREADID m_Second;
    mutable THREADID m_Third;
    mutable THREADID m_Fourth;

    CommL(UINT64 addr) :
    m_Addr(addr), m_Ref(0), m_First(0), m_Second(0), m_Third(0), m_Fourth(0) {
    }

    CommL(UINT64 addr, size_t ref) :
    m_Addr(addr), m_Ref(ref), m_First(0), m_Second(0), m_Third(0), m_Fourth(0) {
    }

    bool operator<(const CommL &clObj) const {
        return (this->m_Addr < clObj.m_Addr);
    }

    bool operator==(const CommL &clObj) const {
        return (m_Addr == clObj.m_Addr);
    }

    void update(THREADID tid) {
        //THREADID tmp = m_First;
        //m_First = tid;
        //m_Second = tmp;
        m_Fourth = m_Third;
        m_Third = m_Second;
        m_Second = m_First;
        m_First = tid;
    }

    THREADID getFirst() {
        return m_First;
    }

    THREADID getSecond() {
        return m_Second;
    }
    
    THREADID getThird() {
        return m_Third;
    }
    
    THREADID getFourth() {
        return m_Fourth;
    }
};

/*
namespace std
{
  template<>
    struct hash<CommL>
    {
      size_t operator()(const CommL &obj) const {
        return hash<UInt64>()(obj.m_Addr);
      }
    };
}
 */

class CommLSet {
private:
    //PIN_RWMUTEX setLock;
    PIN_MUTEX setLock;
    //PIN_RWMUTEX vecLock;
    std::set<CommL> setCommL;
    //std::unordered_set<CommL> setCommL;
    //std::vector<CommLWin *> lineWindows;

public:

    CommLSet()
    : setCommL() {
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

    ~CommLSet() {
        PIN_MutexFini(&setLock);
        //PIN_RWMutexFini(&setLock);
        //PIN_RWMutexFini(&vecLock);
    }

    bool exists(UINT64 line) {
        bool found = false;
        CommL cand_cl = CommL(line);
        //CommL *cand_cl = new CommL(line);
        //setLock.acquire_read();
        //PIN_RWMutexReadLock(&setLock);
        PIN_MutexLock(&setLock);
        //std::set<CommL>::iterator it = setCommL.find(cand_cl);
        auto it = setCommL.find(cand_cl);
        if (it != setCommL.end())
            found = true;
        else
            found = false;
        //setLock.release();
        //PIN_RWMutexUnlock(&setLock);
        PIN_MutexUnlock(&setLock);
        return found;
    }

    CommL getLine(UINT64 line) {
        CommL cand_cl = CommL(line);
        //CommL *cand_cl = new CommL(line);
        //setLock.acquire_read();
        auto it = setCommL.find(cand_cl);
        //setLock.release();
        if (it != setCommL.end()) {
            return (*it);
            //return it;
        } else {
            CommL new_cl = CommL(line);
            //CommL *new_cl = new CommL(line);
            //setLock.acquire();
            //PIN_RWMutexWriteLock(&setLock);
            PIN_MutexLock(&setLock);
            auto result = setCommL.insert(new_cl);
            PIN_MutexUnlock(&setLock);
            //PIN_RWMutexUnlock(&setLock);
            //setLock.release();
            return (*result.first);
        }
    }

    void updateLine(UINT64 line, THREADID tid) {
        CommL cand_cl = CommL(line);
        auto it = setCommL.find(cand_cl);
        const_cast<CommL&> (*it).update(tid);
    }

};

#endif /* COMML_H */

