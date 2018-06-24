//
//  RealtekRTL8111_kdp.cpp
//  RealtekRTL8111
//
//  Created by Roman Peshkov on 21/06/2018.
//  Copyright © 2018 Laura Müller. All rights reserved.
//


#include "RealtekRTL8111.h"

enum { kDebuggerPollDelayUS = 50 };

IOReturn RTL8111::enable(IOKernelDebugger *netif) {
    if (enabledByKDP || enabledByBSD) {
        enabledByKDP = true;
        return kIOReturnSuccess;
    }
    
    enabledByKDP = setActivationLevel(kActivationLevelKDP);
    return enabledByKDP ? kIOReturnSuccess : kIOReturnIOError;
}

IOReturn RTL8111::disable(IOKernelDebugger *netif) {
    enabledByKDP = false;
    
    if (enabledByBSD == false) {
        setActivationLevel(kActivationLevelDisable);
    }
    
    return kIOReturnSuccess;
}

void RTL8111::sendPacket(void *pkt, UInt32 pktLen) {
    
    IOLog("Debugger. Send packet");
    
    if (!pkt || pktLen > kIOEthernetMaxPacketSize - 4) {
        return;
    }
    
    txInterrupt();
    
    RtlDmaDesc *desc = &txDescArray[txDirtyDescIndex];
    bcopy(pkt, (void *)desc->addr, pktLen);    // Copy debugger packet to the Tx buffer
    
    if ( pktLen < MINPACK - 4 )        // account for 4 FCS bytes
    {
        memset( (void *) (desc->addr + pktLen), 0x55, MINPACK - 4 - pktLen );
        pktLen = MINPACK - 4;
    }
    desc->opts1 = pktLen;
    
    WriteReg8(TxPoll, NPQ);
    
    ++txDirtyDescIndex &= kTxDescMask;
    return;
    
}

void RTL8111::receivePacket(void * pkt, UInt32 * pktLen, UInt32 timeout) {
    IOLog("Debugger. Receive packet");
    timeout *= 1000;
    
    *pktLen = 0;
    
    while (timeout && *pktLen == 0) {
        if ((ReadReg8(ChipCmd) & RxBufEmpty) == RxBufEmpty) {
            // Receive buffer empty, wait and retry.
            IODelay(kDebuggerPollDelayUS);
            timeout -= kDebuggerPollDelayUS;
            continue;
        }
        
        RtlDmaDesc *desc = &rxDescArray[rxNextDescIndex];
        UInt32 status = desc->opts1;
        UInt32 rxLen = (status & 0x1fff) - kIOEthernetCRCSize;
        
        if (rxLen >= MINPACK && rxLen <= MAXPACK )
        {
            memcpy(pkt, (void *)desc->addr, rxLen);
            
            *pktLen = rxLen;
        }
        ++rxNextDescIndex &= kRxDescMask;
    }
    return;
}
