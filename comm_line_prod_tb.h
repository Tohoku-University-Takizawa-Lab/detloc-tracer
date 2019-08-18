/* 
 * File:   comm_line_ps.h
 * Communication detection using producer-consumer pattern
 * Author: agung
 *
 * Created on July 2, 2019, 9:53 PM
 */

#ifndef COMMLPSTB_H
#define COMMLPSTB_H

#include <set>
#include <vector>
//#include <atomic>
//#include <unordered_set>
#include <sys/types.h>
#include "comm_line.h"
#include <sys/ipc.h>
#include <sys/msg.h>

class CommLineProdTB {
//private:

public:
    UINT64 m_Addr;
    size_t m_Ref; //Start from 1, 0 means dummy
    mutable THREADID m_First;
    mutable THREADID m_Second;
    
    mutable UINT64 m_First_addr;
    mutable UINT64 m_Second_addr;
    
    CommLineProdTB(UINT64 addr) : 
    m_Addr(addr), m_Ref(0), m_First(0), m_Second(0),
    m_First_addr(0), m_Second_addr(0) {
    }

    CommLineProdTB(UINT64 addr, size_t ref) :
    m_Addr(addr), m_Ref(ref), m_First(0), m_Second(0),
    m_First_addr(0), m_Second_addr(0) {
    }
    
    CommLineProdTB(UINT64 addr, int num_threads) :
    m_Addr(addr), m_Ref(0), m_First(0), m_Second(0),
    m_First_addr(0), m_Second_addr(0) {
    }
    
    ~CommLineProdTB() {
    }

    bool operator<(const CommLineProdTB &clObj) const {
        return (this->m_Addr < clObj.m_Addr);
    }

    bool operator==(const CommLineProdTB &clObj) const {
        return (m_Addr == clObj.m_Addr);
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
        m_Addr = 0;
    }

    bool isEmpty() {
        return (m_Addr == 0 ? true : false);
    }

};

class CommLineProdSetLF {
private:
    std::set<CommLineProdTB> setCommLPS;
    //std::unordered_set<CommL> setCommL;
    //std::vector<CommLWin *> lineWindows;
    THREADID lastPeer;

public:

    CommLineProdSetLF()
    : setCommLPS(), lastPeer(1) {
    }

    ~CommLineProdSetLF() {
    }

    CommLineProdTB getLine(UINT64 line) {
        CommLineProdTB cand_cl = CommLineProdTB(line);
        //CommL *cand_cl = new CommL(line);
        //setLock.acquire_read();
        auto it = setCommLPS.find(cand_cl);
        //setLock.release();
        if (it != setCommLPS.end()) {
            return (*it);
            //return it;
        } else {
            CommLineProdTB new_cl = CommLineProdTB(line);
            //CommL *new_cl = new CommL(line);
            //setLock.acquire();
            //PIN_RWMutexWriteLock(&setLock);
            //PIN_MutexLock(&setLock);
            auto result = setCommLPS.insert(new_cl);
            //PIN_MutexUnlock(&setLock);
            //PIN_RWMutexUnlock(&setLock);
            //setLock.release();
            return (*result.first);
        }
    }
    
    CommLineProdTB getLineLazy(UINT64 line) {
        CommLineProdTB cand_cl = CommLineProdTB(line);
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
        CommLineProdTB cand = CommLineProdTB(line);
        auto it = setCommLPS.find(cand);
        if (it != setCommLPS.end()) {
            const_cast<CommLineProdTB&> (*it).update(tid, addr);
        }
        else {
            cand.update(tid, addr);
            //PIN_MutexLock(&setLock);
            setCommLPS.insert(cand);
            //PIN_MutexUnlock(&setLock);
        }
    }
    
    void updateCreateLineBatch(UINT64 start, UINT64 addr, UINT32 len, THREADID tid) {
        // Tid must be > 0, e.g, tid should be in tid+1 outside the function
        //if (len > 1)
        //    printf(" [L-%ld] update m_First %ld..%ld\n", start, start, start+len-1);
        for (UINT64 line = start; line < start+len; ++line) {
            CommLineProdTB cand = CommLineProdTB(line);
            auto it = setCommLPS.find(cand);
            if (it == setCommLPS.end()) {
                // Not found, new insert
                //cand.updateFirst(tid);
                cand.update(tid, addr);
                //PIN_MutexLock(&setLock);
                setCommLPS.insert(cand);
                //PIN_MutexUnlock(&setLock);
            }
            else if (tid != (*it).m_First) {
                // Update only if current tid is different
                //printf(" [L-%ld] update m_First %d -> %d\n", line, (*it).m_First, tid);
                //const_cast<CommLineProdTB&> (*it).updateFirst(tid);
                const_cast<CommLineProdTB&> (*it).update(tid, addr);
            }
        } 
    }

    void updateLine(UINT64 line, THREADID tid, UINT64 addr) {
        CommLineProdTB cand_cl = CommLineProdTB(line);
        auto it = setCommLPS.find(cand_cl);
        const_cast<CommLineProdTB&> (*it).update(tid, addr);
    }

    std::set<CommLineProdTB> * getCommLPS() {
        return &setCommLPS;
    }
    
};

struct payload_buff {
    THREADID tid;
    UINT64 line;
    bool w_op;
} msg_payload;

class CommLineProdTBArr {
private:
    CommLineProdSetLF tbLines[MAXTHREADS+1];
    
public:
    CommLineProdTBArr() {
        /*
        struct payload_buff message;
        int msqid;
        key_t key;
        key = ftok("detloc", 65);
        msqid = msgget(key, 0666 | IPC_CREAT);
        message.tid = 0;
        message.line = 0;
        
        msgsnd(msgid, &message, sizeof(message), 0);
        */
    }

    CommLineProdTB getTBLineLazy(THREADID my_tid, UINT64 line, UINT32 num_threads) {
        THREADID t;
        CommLineProdTB cand = CommLineProdTB(line);
        //THREADID peer = tbLines[my_tid].lastPeer;
        for (t = 1; t < my_tid; ++t) {
            auto it = tbLines[t].getCommLPS()->find(cand);
            if (it != tbLines[t].getCommLPS()->end()) 
                return (*it);
        }
        for (t = my_tid; t < num_threads+1; ++t) {
            auto it = tbLines[t].getCommLPS()->find(cand);
            if (it != tbLines[t].getCommLPS()->end()) 
                return (*it);
        }
        //printf("** Cannot find line %ld\n", line);
        cand.setEmpty();
        return cand;
    } 

    void updateCreateTBLineBatch(UINT64 start, UINT64 addr, UINT32 len, THREADID my_tid) {
        tbLines[my_tid].updateCreateLineBatch(start, addr, len, my_tid);
    }
    
};

#endif /* COMMLPS_H */

