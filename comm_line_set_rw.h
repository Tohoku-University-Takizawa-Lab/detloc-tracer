/* 
 * File:   comm_l_set.h
 * Author: agung
 *
 * Created on July 2, 2018, 9:53 PM
 */

#ifndef COMMLRW_H
#define COMMLRW_H

#include <set>
#include <vector>
#include <unordered_set>
#include <sys/types.h>

class CommLRW {
public:
    UINT64 m_Addr;
    size_t m_Ref; //Start from 1, 0 means dummy
    mutable THREADID m_First;
    mutable THREADID m_Second;
    mutable THREADID m_Third;
    mutable THREADID m_Fourth;
    
    mutable bool m_First_w;
    mutable bool m_Second_w;
    mutable bool m_Third_w;
    mutable bool m_Fourth_w;
    
    mutable UINT64 m_First_addr;
    mutable UINT64 m_Second_addr;
    mutable UINT64 m_Third_addr;
    mutable UINT64 m_Fourth_addr;

    CommLRW(UINT64 addr) :
    m_Addr(addr), m_Ref(0), m_First(0), m_Second(0), m_Third(0), m_Fourth(0),
    m_First_w(false), m_Second_w(false), m_Third_w(false), m_Fourth_w(false),
    m_First_addr(0), m_Second_addr(0), m_Third_addr(0), m_Fourth_addr(0) {
    }

    CommLRW(UINT64 addr, size_t ref) :
    m_Addr(addr), m_Ref(ref), m_First(0), m_Second(0), m_Third(0), m_Fourth(0),
    m_First_w(false), m_Second_w(false), m_Third_w(false), m_Fourth_w(false),
    m_First_addr(0), m_Second_addr(0), m_Third_addr(0), m_Fourth_addr(0) {
    }

    bool operator<(const CommLRW &clObj) const {
        return (this->m_Addr < clObj.m_Addr);
    }

    bool operator==(const CommLRW &clObj) const {
        return (m_Addr == clObj.m_Addr);
    }

    void update(THREADID tid, bool write, UINT64 addr) {
        //THREADID tmp = m_First;
        //m_First = tid;
        //m_Second = tmp;
        m_Fourth = m_Third;
        m_Third = m_Second;
        m_Second = m_First;
        m_First = tid;
    
        m_Fourth_w = m_Third_w;
        m_Third_w = m_Second_w;
        m_Second_w = m_First_w;
        m_First_w = write;
        
        m_Fourth_addr = m_Third_addr;
        m_Third_addr = m_Second_addr;
        m_Second_addr = m_First_addr;
        m_First_addr = addr;

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
    
    bool getFirst_w() {
        return m_First_w;
    }

    bool getSecond_w() {
        return m_Second_w;
    }
    
    bool getThird_w() {
        return m_Third_w;
    }
    
    bool getFourth_w() {
        return m_Fourth_w;
    }
    
    UINT64 getFirst_addr() {
        return m_First_addr;
    }

    UINT64 getSecond_addr() {
        return m_Second_addr;
    }
    
    UINT64 getThird_addr() {
        return m_Third_addr;
    }
    
    UINT64 getFourth_addr() {
        return m_Fourth_addr;
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

class CommLRWSet {
private:
    //PIN_RWMUTEX setLock;
    PIN_MUTEX setLock;
    //PIN_RWMUTEX vecLock;
    std::set<CommLRW> setCommLRW;
    //std::unordered_set<CommL> setCommL;
    //std::vector<CommLWin *> lineWindows;

public:

    CommLRWSet()
    : setCommLRW() {
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

    ~CommLRWSet() {
        PIN_MutexFini(&setLock);
        //PIN_RWMutexFini(&setLock);
        //PIN_RWMutexFini(&vecLock);
    }

    bool exists(UINT64 line) {
        bool found = false;
        CommLRW cand_cl = CommLRW(line);
        //CommL *cand_cl = new CommL(line);
        //setLock.acquire_read();
        //PIN_RWMutexReadLock(&setLock);
        PIN_MutexLock(&setLock);
        //std::set<CommL>::iterator it = setCommL.find(cand_cl);
        auto it = setCommLRW.find(cand_cl);
        if (it != setCommLRW.end())
            found = true;
        else
            found = false;
        //setLock.release();
        //PIN_RWMutexUnlock(&setLock);
        PIN_MutexUnlock(&setLock);
        return found;
    }

    CommLRW getLine(UINT64 line) {
        CommLRW cand_cl = CommLRW(line);
        //CommL *cand_cl = new CommL(line);
        //setLock.acquire_read();
        auto it = setCommLRW.find(cand_cl);
        //setLock.release();
        if (it != setCommLRW.end()) {
            return (*it);
            //return it;
        } else {
            CommLRW new_cl = CommLRW(line);
            //CommL *new_cl = new CommL(line);
            //setLock.acquire();
            //PIN_RWMutexWriteLock(&setLock);
            PIN_MutexLock(&setLock);
            auto result = setCommLRW.insert(new_cl);
            PIN_MutexUnlock(&setLock);
            //PIN_RWMutexUnlock(&setLock);
            //setLock.release();
            return (*result.first);
        }
    }

    void updateLine(UINT64 line, THREADID tid, bool isWrite, UINT64 addr) {
        CommLRW cand_cl = CommLRW(line);
        auto it = setCommLRW.find(cand_cl);
        const_cast<CommLRW&> (*it).update(tid, isWrite, addr);
    }

};

#endif /* COMMLRW_H */

