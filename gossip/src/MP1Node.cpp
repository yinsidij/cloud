/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address) {
	for( int i = 0; i < 6; i++ ) {
		NULLADDR[i] = 0;
	}
	this->memberNode = member;
	this->emulNet = emul;
	this->log = log;
	this->par = params;
	this->memberNode->addr = *address;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * 				This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop() {
    if ( memberNode->bFailed ) {
    	return false;
    }
    else {
    	return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if( initThisNode(&joinaddr) == -1 ) {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if( !introduceSelfToGroup(&joinaddr) ) {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {
	/*
	 * This function is partially implemented and may require changes
	 */
	int id = *(int*)(&memberNode->addr.addr);
	int port = *(short*)(&memberNode->addr.addr[4]);

	memberNode->bFailed = false;
	memberNode->inited = true;
	memberNode->inGroup = false;
    // node is up!
	memberNode->nnb = 0;
	memberNode->heartbeat = 0;
	memberNode->pingCounter = TFAIL;
	memberNode->timeOutCounter = -1;
    initMemberListTable(memberNode);

    return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
	MessageHdr *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if ( 0 == memcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr), sizeof(memberNode->addr.addr))) {
        // I am the group booter (first process to join the group). Boot up the group
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
	int id;
	int port;
	memcpy(&id, &memberNode->addr.addr[0], sizeof(int));
	memcpy(&port, &memberNode->addr.addr[4], sizeof(short));
	MemberListEntry entry(id,port,1/*heartbeat*/,par->getcurrtime());
	memberNode->memberList.push_back(entry);
	log->logNodeAdd(&memberNode->addr, &memberNode->addr);
    }
    else {
        size_t msgsize = sizeof(MessageHdr) + sizeof(joinaddr->addr) + sizeof(long) + 1;
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        // create JOINREQ message: format of data is {struct Address myaddr}
        msg->msgType = JOINREQ;
        memcpy((char *)(msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
        memcpy((char *)(msg+1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        log->LOG(&memberNode->addr, s);
#endif

        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, msgsize);

        free(msg);
    }

    return 1;

}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode(){
   /*
    * Your code goes here
    */
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop() {
    if (memberNode->bFailed) {
    	return;
    }

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if( !memberNode->inGroup ) {
    	return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages() {
    void *ptr;
    int size;

    // Pop waiting messages from memberNode's mp1q
    while ( !memberNode->mp1q.empty() ) {
    	ptr = memberNode->mp1q.front().elt;
    	size = memberNode->mp1q.front().size;
    	memberNode->mp1q.pop();
    	recvCallBack((void *)memberNode, (char *)ptr, size);
    }
    return;
}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size ) {
	/*
	 * Your code goes here
	 */
    MessageHdr* msg = (MessageHdr*) data;
    Address addr; //send's address
    memcpy(&addr, data+sizeof(MessageHdr), sizeof(Address));
    if (msg->msgType == JOINREQ) {

        addToMembershipList(addr);
        send(addr, JOINREP);

    } else if (msg->msgType == JOINREP) {
        memberNode->inGroup = true;
	vector<MemberListEntry> membershipList;
	deserializeMembership(data,size,membershipList);
	mergeMembership(addr,membershipList);

    } else if (msg->msgType == PING) {
	vector<MemberListEntry> membershipList;
	deserializeMembership(data,size,membershipList);
	mergeMembership(addr,membershipList);
    }

}

void MP1Node::addToMembershipList(Address& addr) {
    int id;
    short port;
    memcpy(&id,   &addr.addr[0], sizeof(int));
    memcpy(&port, &addr.addr[4], sizeof(short));

    for (auto& myEntry : memberNode->memberList) {
        if (id == myEntry.getid() && port == myEntry.getport()) {
            //exist in the membershiplist
            myEntry.timestamp = par->getcurrtime();
            return;
        }
    }

    MemberListEntry e(id,port,0/*heartbeat*/, par->getcurrtime());
    memberNode->memberList.push_back(e);

    log->logNodeAdd(&memberNode->addr, &addr);
}

void MP1Node::send(Address& addr, MsgTypes type) {

    int size = memberNode->memberList.size();
    size_t msgsize = sizeof(MessageHdr) + sizeof(Address) + sizeof(MemberListEntry)*size + 1;
    MessageHdr* msg;
    msg = (MessageHdr*) malloc(msgsize * sizeof(char));
    msg->msgType = type;
    memcpy((char*)(msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
    char* ptr = serializeMembership(memberNode->memberList);
    memcpy((char*)(msg+1) + sizeof(memberNode->addr.addr) + 1, ptr, sizeof(MemberListEntry)*size);


    emulNet->ENsend(&memberNode->addr, &addr, (char*) msg, msgsize);
    //string logging = "sending to " + addr.getAddress();
    //if (type == PING) {
    //	logging += " PING";	 
    //} else if (type == JOINREQ) {
    //logging += " JOINREQ"; 
    //} else if (type == JOINREP) {
    //logging += " JOINREP"; 
    //}
    //log->LOG(&memberNode->addr, logging.c_str());
    delete msg;
    delete ptr;
}

void MP1Node::mergeMembership(Address& addr, vector<MemberListEntry>& membershipList) {

    int size = membershipList.size();
    for (int i=0;i<size;i++) {
	MemberListEntry entry = membershipList[i];
        bool found=false;
        for (auto& myEntry : memberNode->memberList) {
	    // myEntry is self --- TODO
            if (myEntry.getid() == entry.getid() && myEntry.getport() == entry.getport()) {
                found=true;
		if (entry.heartbeat > myEntry.heartbeat) {
                    myEntry.heartbeat = entry.heartbeat;
		    myEntry.timestamp = par->getcurrtime();
		}
            }
        }
        if (!found) {
            // new entry
            MemberListEntry e(entry);
            e.timestamp = par->getcurrtime();
            memberNode->memberList.push_back(e);
            Address addr = getAddress(e.getid(), e.getport());
            log->logNodeAdd(&memberNode->addr, &addr);
        }
    }
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period and then delete
 * 				the nodes
 * 				Propagate your membership list
 */
void MP1Node::nodeLoopOps() {

	/*
	 * Your code goes here
	 */
    memberNode->heartbeat++;

    int id;
    int port;
    memcpy(&id, &memberNode->addr.addr[0], sizeof(int));
    memcpy(&port, &memberNode->addr.addr[4], sizeof(short));
    vector<MemberListEntry> toRemoveList;
    vector<MemberListEntry> newMemberList;

    for (auto& myEntry : memberNode->memberList) {
        //string logging0 = "par->getcurrtime()  = " + to_string(par->getcurrtime()) + "; " + to_string(myEntry.getid()) + " timestamp = " + to_string(myEntry.timestamp);
        //log->LOG(&memberNode->addr, logging0.c_str());
	if (id == myEntry.getid() && port == myEntry.getport()) {
	// self. don't remove, but update the entry
	    myEntry.setheartbeat(memberNode->heartbeat);
	    myEntry.settimestamp(par->getcurrtime());
	    continue;
	}
        if (par->getcurrtime() - myEntry.timestamp >= TREMOVE) {
            toRemoveList.push_back(myEntry);
        } 
    }

    int size = memberNode->memberList.size();
    string logging1 = "before removeList, size = " + to_string(size);
    log->LOG(&memberNode->addr, logging1.c_str());

    for (int i=0;i<size;i++) {
	bool found = false;
	auto toRemoveEntry = MemberListEntry(0,0);
    	for (auto& removeEntry : toRemoveList) {
            if (memberNode->memberList[i].getid() == removeEntry.getid() && memberNode->memberList[i].getport() == removeEntry.getport()) {
	    	found=true;
		toRemoveEntry = removeEntry;
	    }
	}
	if (found) {
            Address addr = getAddress(toRemoveEntry.getid(), toRemoveEntry.getport());
            log->logNodeRemove(&memberNode->addr, &addr);
	} else {
	    newMemberList.push_back(memberNode->memberList[i]);
	}
    
    }

    memberNode->memberList = newMemberList;

    size = memberNode->memberList.size();
    string logging2 = "after removeList, size = " + to_string(size);
    log->LOG(&memberNode->addr, logging2.c_str());

    for (int i=0;i<size;i++) {
	auto& myEntry = memberNode->memberList[i];
        Address addr = getAddress(myEntry.getid(), myEntry.getport());
        if (addr == memberNode->addr) {
            continue;
        }
        send(addr,PING);
    }
}

Address MP1Node::getAddress(int id, short port) {

    Address addr;
    memcpy(addr.addr, &id, sizeof(int));
    memcpy(addr.addr + sizeof(int), &port, sizeof(short));

    return addr;
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
	return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode) {
	memberNode->memberList.clear();
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
                                                       addr->addr[3], *(short*)&addr->addr[4]) ;    
}
	
char* MP1Node::serializeMembership(vector<MemberListEntry>& membershipList) {
	int size = membershipList.size();
	size_t totalSize = sizeof(MemberListEntry)*size;
	MemberListEntry* ptr = (MemberListEntry*) malloc(totalSize);

	for (int i=0;i<size;i++) {
		memcpy((char*)(ptr+i), &memberNode->memberList[i], sizeof(MemberListEntry));
	}
	return (char*) ptr;
}


void MP1Node::deserializeMembership(char* data, int size, vector<MemberListEntry>& membershipList) {

	//memcpy((char *)(msg+1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));
	//
	int prefixSize = sizeof(MessageHdr)+sizeof(memberNode->addr.addr)+1;
	int allEntrySize = size - prefixSize;
	char* ptr = (char*)(data + prefixSize);

	int numOfEntries = allEntrySize/sizeof(MemberListEntry);
	MemberListEntry* pEntry = (MemberListEntry*) ptr;
	for (int i=0;i<numOfEntries;i++) {
		membershipList.push_back(*pEntry);
		pEntry++;
	}
}
