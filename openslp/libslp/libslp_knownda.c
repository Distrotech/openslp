
/***************************************************************************/
/*                                                                         */
/* Project:     OpenSLP - OpenSource implementation of Service Location    */
/*              Protocol                                                   */
/*                                                                         */
/* File:        slplib_knownda.c                                           */
/*                                                                         */
/* Abstract:    Internal implementation for generating unique XIDs.        */
/*              Provides functions that are supposed to generate 16-bit    */
/*              values that won't be generated for a long time in this     */
/*              process and hopefully won't be generated by other process  */ 
/*              for a long time.                                           */
/*                                                                         */
/*-------------------------------------------------------------------------*/
/*                                                                         */
/* Copyright (c) 1995, 1999  Caldera Systems, Inc.                         */
/*                                                                         */
/* This program is free software; you can redistribute it and/or modify it */
/* under the terms of the GNU Lesser General Public License as published   */
/* by the Free Software Foundation; either version 2.1 of the License, or  */
/* (at your option) any later version.                                     */
/*                                                                         */
/*     This program is distributed in the hope that it will be useful,     */
/*     but WITHOUT ANY WARRANTY; without even the implied warranty of      */
/*     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the       */
/*     GNU Lesser General Public License for more details.                 */
/*                                                                         */
/*     You should have received a copy of the GNU Lesser General Public    */
/*     License along with this program; see the file COPYING.  If not,     */
/*     please obtain a copy from http://www.gnu.org/copyleft/lesser.html   */
/*                                                                         */
/*-------------------------------------------------------------------------*/
/*                                                                         */
/*     Please submit patches to http://www.openslp.org                     */
/*                                                                         */
/***************************************************************************/


#include "slp.h"
#include "libslp.h"

/*=========================================================================*/
SLPList G_KnownDACache = {0,0,0};
/* The cache list of known DAs.                                            */
/*=========================================================================*/

/*=========================================================================*/
int   G_KnownDAScopesLen = 0;
char* G_KnownDAScopes   = 0;
/* Cached known scope list                                                 */
/*=========================================================================*/


/*=========================================================================*/
time_t G_KnownDALastCacheRefresh = 0;
/* The time of the last Multicast for known DAs                            */
/*=========================================================================*/


/*-------------------------------------------------------------------------*/
SLPDAEntry* KnownDAListFindByScope(SLPList* dalist,
                                   int scopelistlen,
                                   const char* scopelist)
/* Returns: pointer to found DA.                                           */
/*-------------------------------------------------------------------------*/
{
    SLPDAEntry*     entry;
    
    entry = (SLPDAEntry*)dalist->head;
    while(entry)
    {
        if(SLPCompareString(scopelistlen,
                            scopelist,
                            entry->scopelistlen,
                            entry->scopelist) == 0)
        {
            break;
        }   
        
        entry = (SLPDAEntry*) entry->listitem.next;
    }

    return entry;
}


/*-------------------------------------------------------------------------*/
int KnownDAListAdd(SLPList* dalist, SLPDAEntry* addition)
/* Add an entry to the KnownDA cache                                       */
/*                                                                         */
/* Returns: zero if DA was already in the list. >0 if DA was new           */
/*-------------------------------------------------------------------------*/
{
    SLPDAEntry*     entry;
    
    entry = (SLPDAEntry*)dalist->head;
    while(entry)
    {
        if(SLPCompareString(addition->urllen,
                            addition->url,
                            entry->urllen,
                            entry->url) == 0)
        {
            /* entry already in the list */
            break;
        }   

        entry = (SLPDAEntry*) entry->listitem.next;
    }

    if(entry == 0)
    {
        /* Add the entry if it does not exist */
        SLPListLinkHead(dalist,(SLPListItem*)addition);
        return 1;
    }

    /* entry already in the list */
    return 0;
}


/*-------------------------------------------------------------------------*/
int KnownDAListRemove(SLPList* dalist, SLPDAEntry* remove)
/* Remove an entry to the KnownDA cach                                     */
/*-------------------------------------------------------------------------*/
{
    SLPDAEntry*     entry;
    
    entry = (SLPDAEntry*)dalist->head;
    while(entry)
    {
        if(SLPCompareString(remove->urllen,
                            remove->url,
                            entry->urllen,
                            entry->url) == 0)
        {
            /* Remove entry from list and free it */
            SLPDAEntryFree( (SLPDAEntry*)SLPListUnlink(dalist, (SLPListItem*)entry) );
            break;
        }

        entry = (SLPDAEntry*) entry->listitem.next;
    }

    return 0;
}

/*-------------------------------------------------------------------------*/
SLPBoolean KnownDADiscoveryCallback(SLPError errorcode, 
                                    SLPMessage msg, 
                                    void* cookie)
/*-------------------------------------------------------------------------*/
{
    SLPSrvURL*      srvurl;
    SLPDAEntry      daentry;
    SLPDAEntry*     entry;
    int*            result;
    struct in_addr  addr;
    struct hostent* he;
    
    result = (int*)cookie;
    
    if (errorcode == 0)
    {
        if (msg && msg->header.functionid == SLP_FUNCT_DAADVERT)
        {
            if (msg->body.srvrply.errorcode == 0)
            { 
                /* NULL terminate scopelist */
                *((char*)msg->body.daadvert.scopelist + msg->body.daadvert.scopelistlen) = 0;
                if (SLPParseSrvURL(msg->body.daadvert.url, &srvurl) == 0)
                {
                    he = gethostbyname(srvurl->s_pcHost);
                    if (he)
                    {
                        /* create a daentry and add it to the knownda list */
                        daentry.langtaglen = msg->header.langtaglen;
                        daentry.langtag = msg->header.langtag;
                        daentry.bootstamp = msg->body.daadvert.bootstamp;
                        daentry.urllen = msg->body.daadvert.urllen;
                        daentry.url = msg->body.daadvert.url;
                        daentry.scopelistlen = msg->body.daadvert.scopelistlen;
                        daentry.scopelist = msg->body.daadvert.scopelist;
                        daentry.attrlistlen = msg->body.daadvert.attrlistlen;
                        daentry.attrlist = msg->body.daadvert.attrlist;
                        daentry.spilistlen = msg->body.daadvert.spilistlen;
                        daentry.spilist = msg->body.daadvert.spilist;
                        addr.s_addr = *((unsigned long*)(he->h_addr_list[0]));
                        entry = SLPDAEntryCreate(&addr, &daentry);
                        if(entry)
                        {
                            KnownDAListAdd(&G_KnownDACache,
                                       entry);
                            (*result) = (*result) + 1;
                        }
                    }

                    SLPFree(srvurl);
                }
            }
        }
    }

    return 1;
}


/*-------------------------------------------------------------------------*/
int KnownDADiscoveryRqstRply(int sock, 
                             struct sockaddr_in* peeraddr,
                             int scopelistlen,
                             const char* scopelist)
/* Returns: number of *new* DAEntries found                                */
/*-------------------------------------------------------------------------*/
{
    char*   buf;
    char*   curpos;
    int     bufsize;
    int     result = 0;
           
    /*-------------------------------------------------------------------*/
    /* determine the size of the fixed portion of the SRVRQST            */
    /*-------------------------------------------------------------------*/
    bufsize  = 31; /*  2 bytes for the srvtype length */
                   /* 23 bytes for "service:directory-agent" srvtype */
                   /*  2 bytes for scopelistlen */
                   /*  2 bytes for predicatelen */
                   /*  2 bytes for sprstrlen */
    bufsize += scopelistlen;

    /* TODO: make sure that we don't exceed the MTU */
    buf = curpos = (char*)malloc(bufsize);
    if (buf == 0)
    {
        return 0;
    }
    memset(buf,0,bufsize);

    /*------------------------------------------------------------*/
    /* Build a buffer containing the fixed portion of the SRVRQST */
    /*------------------------------------------------------------*/
    /* service type */
    ToUINT16(curpos,23);
    curpos = curpos + 2;
    memcpy(curpos,"service:directory-agent",23);
    curpos += 23;
    /* scope list */
    ToUINT16(curpos,scopelistlen);
    curpos = curpos + 2;
    memcpy(curpos,scopelist,scopelistlen);
    /* predicate zero length */
    /* spi list zero length */

    NetworkRqstRply(sock,
                    peeraddr,
                    "en",
                    buf,
                    SLP_FUNCT_DASRVRQST,
                    bufsize,
                    KnownDADiscoveryCallback,
                    &result);
    
    free(buf);

    return result;
}


/*-------------------------------------------------------------------------*/
int KnownDADiscoverFromMulticast()
/* Locates  DAs via multicast convergence                                  */
/*                                                                         */
/* Returns: number of *new* DAs found                                      */
/*-------------------------------------------------------------------------*/
{
    struct sockaddr_in peeraddr;
    time_t curtime;
    int sockfd; 
    int result = 0;

    if (SLPPropertyAsBoolean(SLPGetProperty("net.slp.activeDADetection")) &&
        SLPPropertyAsInteger(SLPGetProperty("net.slp.DAActiveDiscoveryInterval")))
    {
        curtime = time(&curtime);
        if(G_KnownDALastCacheRefresh == 0 ||
           curtime - G_KnownDALastCacheRefresh > MINIMUM_DISCOVERY_INTERVAL )
        {
            G_KnownDALastCacheRefresh = curtime;
            sockfd = NetworkConnectToMulticast(&peeraddr);
            if (sockfd >= 0)
            {
                result = KnownDADiscoveryRqstRply(sockfd,&peeraddr,0,"");
                close(sockfd);
            }               
        }
    }

    return result;
}


/*-------------------------------------------------------------------------*/
int KnownDADiscoverFromDHCP()
/* Locates  DAs via DHCP                                                   */
/*                                                                         */
/* Returns: number of *new* DAs found                                      */
/*-------------------------------------------------------------------------*/
{
    return 0;
}
 

/*-------------------------------------------------------------------------*/
int KnownDADiscoverFromProperties()
/* Locates DAs from a list of DA hostnames                                 */
/*                                                                         */
/* Returns: number of *new* DAs found                                      */
/*-------------------------------------------------------------------------*/
{
    char*               temp;
    char*               tempend;
    char*               slider1;
    char*               slider2;
    int                 sockfd;
    struct hostent*     he;
    struct sockaddr_in  peeraddr;
    struct timeval      timeout;
    int                 result      = 0;
    
    memset(&peeraddr,0,sizeof(peeraddr));
    peeraddr.sin_family = AF_INET;
    peeraddr.sin_port = htons(SLP_RESERVED_PORT);

    slider1 = slider2 = temp = strdup(SLPGetProperty("net.slp.DAAddresses"));
    if (temp)
    {
        tempend = temp + strlen(temp);
        while (slider1 != tempend)
        {
            timeout.tv_sec = SLPPropertyAsInteger(SLPGetProperty("net.slp.DADiscoveryMaximumWait"));
            timeout.tv_usec = (timeout.tv_sec % 1000) * 1000;
            timeout.tv_sec = timeout.tv_sec / 1000;

            while (*slider2 && *slider2 != ',') slider2++;
            *slider2 = 0;

            he = gethostbyname(slider1);
            if (he)
            {
                peeraddr.sin_addr.s_addr = *((unsigned long*)(he->h_addr_list[0]));
                sockfd = SLPNetworkConnectStream(&peeraddr,&timeout);
                if (sockfd >= 0)
                {
                    result = KnownDADiscoveryRqstRply(sockfd,&peeraddr,0,"");
                    close(sockfd);
                    if(result)
                    {
                        break;
                    }
                }
            } 

            slider1 = slider2;
            slider2++;
        }

        free(temp);
    }

    return result;
}


/*-------------------------------------------------------------------------*/
int KnownDADiscoverFromIPC()
/* Ask Slpd if it knows about a DA                                         */ 
/*                                                                         */
/* Returns: number of *new* DAs found                                      */
/*-------------------------------------------------------------------------*/
{
    struct sockaddr_in peeraddr;
    int sockfd; 
    int result = 0;

    sockfd = NetworkConnectToSlpd(&peeraddr);
    if (sockfd >= 0)
    {
        result = KnownDADiscoveryRqstRply(sockfd, &peeraddr, 0, "");
        close(sockfd);
    }               

    return result;
}

/*-------------------------------------------------------------------------*/
SLPDAEntry* KnownDAFromCache(int scopelistlen,
                             const char* scopelist)
/* Ask Slpd if it knows about a DA                                         */ 
/*                                                                         */
/* Returns: pointer to a new DA entry. Caller must NOT free()              */
/*-------------------------------------------------------------------------*/
{
    time_t      curtime;
    SLPDAEntry* entry;
    
    entry = KnownDAListFindByScope(&G_KnownDACache,scopelistlen,scopelist);
    if(entry == 0)
    {
        curtime = time(&curtime);
        if(G_KnownDALastCacheRefresh == 0 ||
           curtime - G_KnownDALastCacheRefresh > MINIMUM_DISCOVERY_INTERVAL )
        {
            /* discover DAs */
            if(KnownDADiscoverFromIPC() == 0)
            {
                KnownDADiscoverFromProperties();
                KnownDADiscoverFromDHCP();
                KnownDADiscoverFromMulticast();
            }
        }

        entry = KnownDAListFindByScope(&G_KnownDACache,scopelistlen,scopelist);
    }

    return entry; 
}


/*=========================================================================*/
int KnownDAConnect(int scopelistlen,
                   const char* scopelist,
                   struct sockaddr_in* peeraddr)
/* Get a connected socket to a DA that supports the specified scope        */
/*                                                                         */
/* scopelistlen (IN) stringlen of the scopelist                            */
/*                                                                         */
/* scopelist (IN) DA must support this scope                               */
/*                                                                         */
/* peeraddr (OUT) the peer that was connected to                           */
/*                                                                         */
/*                                                                         */
/* returns: valid socket file descriptor or -1 if no DA is found           */
/*=========================================================================*/
{
    struct timeval  timeout;
    SLPDAEntry*     daentry;
    int             sock = -1;

    /* Set up connect timeout */
    timeout.tv_sec = SLPPropertyAsInteger(SLPGetProperty("net.slp.DADiscoveryMaximumWait"));
    timeout.tv_usec = (timeout.tv_sec % 1000) * 1000;
    timeout.tv_sec = timeout.tv_sec / 1000;
    
    while(1)
    {
        daentry = KnownDAFromCache(scopelistlen,scopelist);
        if(daentry == 0)
        {
            break;
        }
        
        memset(peeraddr,0,sizeof(peeraddr));
        peeraddr->sin_family = AF_INET;
        peeraddr->sin_port = htons(SLP_RESERVED_PORT);
        peeraddr->sin_addr   = daentry->daaddr;
        
        sock = SLPNetworkConnectStream(peeraddr,&timeout);
        if (sock >= 0)
        {
            break;
        }
        
        KnownDAListRemove(&G_KnownDACache,daentry);
    }

    return sock;
}
    

/*=========================================================================*/
int KnownDAGetScopes(int* scopelistlen,
                     char** scopelist)
/* Gets a list of scopes from the known DA list                            */
/*                                                                         */
/* scopelistlen (OUT) stringlen of the scopelist                           */
/*                                                                         */
/* scopelist (OUT) NULL terminated list of scopes                          */
/*                                                                         */
/* returns: zero on success, non-zero on failure                           */
/*=========================================================================*/
{
    time_t  curtime;

    /* Refresh the cache if necessary */
    curtime = time(&curtime);
    if(G_KnownDALastCacheRefresh == 0 ||
       curtime - G_KnownDALastCacheRefresh > MINIMUM_DISCOVERY_INTERVAL )
    {
        /* discover DAs */
        if(KnownDADiscoverFromIPC() == 0)
        {
            KnownDADiscoverFromProperties();
            KnownDADiscoverFromDHCP();
            KnownDADiscoverFromMulticast();
        }

        /* TODO: */
        /* enumerate through all the knownda entries and generate a scopelist */

        /* TODO: */
        /* Explicitly add in the useScopes property */
    }

    
    if(G_KnownDAScopesLen)
    {
        *scopelist = malloc(G_KnownDAScopesLen + 1);
        if(*scopelist == 0)
        {
            return -1;
        }
        memcpy(*scopelist,G_KnownDAScopes, G_KnownDAScopesLen);
        *scopelistlen = G_KnownDAScopesLen;
    }
    else
    {
        *scopelist = strdup("");
        if(*scopelist == 0)
        {
            return -1;
        }
        *scopelistlen = 0; 
    }

    return 0;
}

