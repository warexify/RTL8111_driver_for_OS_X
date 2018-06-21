//
//  RealtekRTL8111_kdp.cpp
//  RealtekRTL8111
//
//  Created by Roman Peshkov on 21/06/2018.
//  Copyright © 2018 Laura Müller. All rights reserved.
//


#include "RealtekRTL8111.h"

IOReturn RTL8111::enable(IOKernelDebugger *netif) {
    return kIOReturnSuccess;
}

IOReturn RTL8111::disable(IOKernelDebugger *netif) {
    return kIOReturnSuccess;
}

void RTL8111::sendPacket(void *pkt, UInt32 pktLen) {
    UInt8 *pDest;
    
    if (pktLen > MAXPACK - 4)    // account for 4 FCS bytes
        return;

    // TODO: Implement
    // Poll until a transmit buffer becomes available.
//    while ( kOwnedByChip == fTxBufOwnership[ fTxSendIndex ] )
//        reclaimTransmitBuffer();
    
    txDescArray
    pDest = txDescArray + txNextDescIndex * TX_BUF_SIZE;
    
    
    
    
    bcopy(pkt, pDest, pktLen);    // Copy debugger packet to the Tx buffer
    
    // Pad small frames:
    
    if ( pktLen < MINPACK - 4 )        // account for 4 FCS bytes
    {
        memset(pDest + pktLen, 0x55, MINPACK - 4 - pktLen);
        pktLen = MINPACK - 4;
    }
    // Start the Tx by setting the frame length and clearing ownership:
    
//    csrWrite32( RTL_TSD0 + fTxSendIndex * sizeof( UInt32 ), pktLen | fTSD_ERTXTH );
//
//    fTxSendCount++;
//    fTxBufOwnership[ fTxSendIndex ] = kOwnedByChip;
//
//    if ( ++fTxSendIndex >= kTxBufferCount )
//        fTxSendIndex = 0;
    
    return;
}

void RTL8111::receivePacket( void * pkt, UInt32 * pktLen, UInt32 timeout ) {
    UInt16 status, count;    // from buffer header
    UInt16 rxLen;
    
    *pktLen = 0;
    timeout *= 1000;  // convert ms to us
    
    while (timeout && *pktLen == 0)
    {
        if ((ReadReg8(ChipCmd) & RxBufEmpty) == RxBufEmpty)
        {        // Receive buffer empty, wait and retry.
            IODelay(50);
            timeout -= 50;
            continue;
        }

        status    = OSReadLittleInt16( rxDescArray->addr, fRxOffset );
        count    = OSReadLittleInt16( fpRxBuffer, fRxOffset + sizeof( UInt16 ) );
//        rxLen    = count;        // includes 4 bytes of FCS
//        fRxOffset += 2 * sizeof( UInt16 );    // move past buffer header
//
//        //    kprintf( "RTL8139::receivePacket status %x len %d\n", status, rxLen );
//
//        if ( status & (R_RSR_FAE | R_RSR_CRC | R_RSR_LONG | R_RSR_RUNT | R_RSR_ISE) )
//            status &= ~R_RSR_ROK;
//
//        if ( (status & R_RSR_ROK) == 0 )
//        {
//            restartReceiver();
//            continue;
//        }
//
//        if ( rxLen >= MINPACK && rxLen <= MAXPACK )
//        {
//            bcopy( fpRxBuffer + fRxOffset, pkt, rxLen );
//            *pktLen = rxLen;
//        }
//
//        // Advance the Rx ring buffer to the start of the next packet:
//
//        fRxOffset += IORound( rxLen, 4 );
//        csrWrite16( RTL_CAPR, fRxOffset - 0x10 );    // leave a small gap
    }/* end WHILE */
    return;
}
