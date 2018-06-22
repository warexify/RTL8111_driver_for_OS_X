//
//  RealtekRTL8111_kdp.cpp
//  RealtekRTL8111
//
//  Created by Roman Peshkov on 21/06/2018.
//  Copyright © 2018 Laura Müller. All rights reserved.
//


#include "RealtekRTL8111.h"

enum { kDebuggerPollDelayUS = 10 };

IOReturn RTL8111::enable(IOKernelDebugger *netif) {
    return kIOReturnSuccess;
}

IOReturn RTL8111::disable(IOKernelDebugger *netif) {
    return kIOReturnSuccess;
}

void RTL8111::sendPacket(void *pkt, UInt32 pktLen) {
    
    IOPhysicalSegment txSegments[kMaxSegs];
    RtlDmaDesc *desc, *firstDesc;
    UInt32 result = kIOReturnOutputDropped;
    UInt32 cmd = 0;
    UInt32 opts2 = 0;
    mbuf_tso_request_flags_t tsoFlags;
    mbuf_csum_request_flags_t checksums;
    UInt32 mssValue;
    UInt32 opts1;
    UInt32 vlanTag;
    UInt32 numSegs;
    UInt32 lastSeg;
    UInt32 index;
    UInt32 i;
    
    if (!pkt || pktLen > kIOEthernetMaxPacketSize) {
        return;
    }
    
    memcpy(mbuf_data(fKDPMbuf), pkt, pktLen);
    
    numSegs = txMbufCursor->getPhysicalSegmentsWithCoalesce(fKDPMbuf, &txSegments[0], kMaxSegs);
    
    if (!numSegs) {
        DebugLog("Ethernet [RealtekRTL8111]: getPhysicalSegmentsWithCoalesce() failed. Dropping packet.\n");
        etherStats->dot3TxExtraEntry.resourceErrors++;
        goto error;
    }
    if (mbuf_get_tso_requested(fKDPMbuf, &tsoFlags, &mssValue)) {
        DebugLog("Ethernet [RealtekRTL8111]: mbuf_get_tso_requested() failed. Dropping packet.\n");
        goto error;
    }
    if (tsoFlags & (MBUF_TSO_IPV4 | MBUF_TSO_IPV6)) {
        if (tsoFlags & MBUF_TSO_IPV4) {
            getTso4Command(&cmd, &opts2, mssValue, tsoFlags);
        } else {
            /* The pseudoheader checksum has to be adjusted first. */
            //adjustIPv6Header(fKDPMbuf);
            getTso6Command(&cmd, &opts2, mssValue, tsoFlags);
        }
    } else {
        /* We use mssValue as a dummy here because it isn't needed anymore. */
        mbuf_get_csum_requested(fKDPMbuf, &checksums, &mssValue);
        getChecksumCommand(&cmd, &opts2, checksums);
    }
    /* Alloc required number of descriptors. As the descriptor which has been freed last must be
     * considered to be still in use we never fill the ring completely but leave at least one
     * unused.
     */
    if ((txNumFreeDesc <= numSegs)) {
        DebugLog("Ethernet [RealtekRTL8111]: Not enough descriptors. Stalling.\n");
        result = kIOReturnOutputStall;
        stalled = true;
        goto done;
    }
    OSAddAtomic(-numSegs, &txNumFreeDesc);
    index = txNextDescIndex;
    txNextDescIndex = (txNextDescIndex + numSegs) & kTxDescMask;
    firstDesc = &txDescArray[index];
    lastSeg = numSegs - 1;
    
    /* Next fill in the VLAN tag. */
    opts2 |= (getVlanTagDemand(fKDPMbuf, &vlanTag)) ? (OSSwapInt16(vlanTag) | TxVlanTag) : 0;
    
    /* And finally fill in the descriptors. */
    for (i = 0; i < numSegs; i++) {
        desc = &txDescArray[index];
        opts1 = (((UInt32)txSegments[i].length) | cmd);
        opts1 |= (i == 0) ? FirstFrag : DescOwn;
        
        if (i == lastSeg) {
            opts1 |= LastFrag;
            txMbufArray[index] = fKDPMbuf;
        } else {
            txMbufArray[index] = NULL;
        }
        if (index == kTxLastDesc)
            opts1 |= RingEnd;
        
        desc->addr = OSSwapHostToLittleInt64(txSegments[i].location);
        desc->opts2 = OSSwapHostToLittleInt32(opts2);
        desc->opts1 = OSSwapHostToLittleInt32(opts1);
        
        //DebugLog("opts1=0x%x, opts2=0x%x, addr=0x%llx, len=0x%llx\n", opts1, opts2, txSegments[i].location, txSegments[i].length);
        ++index &= kTxDescMask;
    }
    firstDesc->opts1 |= DescOwn;
    
    /* Set the polling bit. */
    WriteReg8(TxPoll, NPQ);
    
    result = kIOReturnOutputSuccess;
    
done:
    //DebugLog("outputPacket() <===\n");
    
    return;
    
error:

    goto done;
}

void RTL8111::receivePacket(void * pkt, UInt32 * pktLen, UInt32 timeout) {
    int timeoutUS = timeout * 1000;
    
    IOPhysicalSegment rxSegment;
    RtlDmaDesc *desc = &rxDescArray[rxNextDescIndex];
    mbuf_t bufPkt, newPkt;
    UInt64 addr;
    UInt32 opts1, opts2;
    UInt32 descStatus1, descStatus2;
    UInt32 pktSize;
    UInt32 goodPkts = 0;
    UInt16 vlanTag;
    
    UInt16 rxMask;
    
    *pktLen = 0;
    
    while (1) {
        UInt16 status;
        
        WriteReg16(IntrMask, 0x0000);
        status = ReadReg16(IntrStatus);
        
        if (status & (RxOK | RxDescUnavail | RxFIFOOver)) {
            break;
        }
        
        if (timeoutUS <= 0) {
            return; // timed out. Return to KDP
        }
        
        IODelay(kDebuggerPollDelayUS);
        timeoutUS -= kDebuggerPollDelayUS;
    }
    descStatus1 = OSSwapLittleToHostInt32(desc->opts1);
    opts1 = (rxNextDescIndex == kRxLastDesc) ? (RingEnd | DescOwn) : DescOwn;
    opts2 = 0;
    addr = 0;
    pktSize = (descStatus1 & 0x1fff) - kIOEthernetCRCSize;
    bufPkt = rxMbufArray[rxNextDescIndex];
    
    const UInt8 * frameData = (const UInt8 *) mbuf_data(rxMbufArray[rxNextDescIndex]);
    memcpy(pkt, frameData, pktSize);
    *pktLen = pktSize;
    
nextDesc:
    if (addr)
        desc->addr = OSSwapHostToLittleInt64(addr);
    
    desc->opts2 = OSSwapHostToLittleInt32(opts2);
    desc->opts1 = OSSwapHostToLittleInt32(opts1);
    
    ++rxNextDescIndex &= kRxDescMask;
    desc = &rxDescArray[rxNextDescIndex];

    return;
}
