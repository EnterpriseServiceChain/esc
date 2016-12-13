#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#pragma pack(1)
typedef struct handshake_s { //maybe this should be just header_t + peer_msid
	//uint16_t type; // version of server, not needed (is in servers.hpp)
	//uint16_t srvn; // number of legal servers
	//uint32_t ipv4; // ip of connecting server
	//uint32_t port; // port of connecting server
	//uint32_t path; // last block
	//uint8_t hash[SHA256_DIGEST_LENGTH]; // hash of last block
	header_t head; // last header
	uint32_t msid; // peer msid
	uint8_t msha[SHA256_DIGEST_LENGTH]; // hash of last peer message
} handshake_t;
#pragma pack()

typedef struct svidmsid_s {
	uint16_t svid;
	uint32_t msid;
} svidmsid_t;
typedef struct msidhash_s {
	uint32_t msid;
	hash_t sigh;
} msidhash_t;

class message :
  public boost::enable_shared_from_this<message>
{
public:
  enum { header_length = 8 }; // should be minimum message size, probably 8bytes
  enum { max_msid = 0xffffffff }; // ignore this later
  enum { max_length = 0xffffff }; // ignore this later
  enum { data_offset = 4+64+10 }; // replace this later

  uint32_t len;		// length of data
  uint8_t* data;	// data pointer
  uint32_t msid;	// msid from the server
  uint32_t now;		// time message created, updated with every download request (busy_insert)
  uint32_t got;		// time message received
  uint16_t svid;	// server id of message author
  uint16_t peer;	// server id of peer sending message
  union {uint64_t num; uint8_t dat[8];} hash; // header hash, TODO change this name to 'head'
  uint8_t status; // 0:info 1:data 2:valid 3:invalid |0x4:saved
  std::set<uint16_t> know; // peers that know about this item
  std::set<uint16_t> busy; // peers downloading this item
  std::set<uint16_t> sent; // peers that downloaded this item
  uint32_t path;	// path == block_id
  uint8_t sigh[SHA256_DIGEST_LENGTH]; // hash of signature
  uint8_t preh[SHA256_DIGEST_LENGTH]; // hash of previous signature
  boost::mutex mtx_;	// to update the sets and data

  // can be a request for data , an info , data ...
  // assume head size 8b
  message() :
	len(header_length),
	msid(0),
	now(0),
	svid(0),
        peer(0),
	status(MSGSTAT_INF),
	path(0)
  { data=(uint8_t*)std::malloc(len);
    hash.num=0;
  }

  message(uint32_t l) :
	len(l),
	msid(0),
	now(0),
	svid(0),
        peer(0),
	status(MSGSTAT_DAT), // not VAL because not yet saved
	path(0)
  { data=(uint8_t*)std::malloc(len);
    hash.num=0;
  }

  message(uint8_t text_type,const uint8_t* text,int text_len,uint16_t mysvid,uint32_t mymsid,ed25519_secret_key mysk,ed25519_public_key mypk) : // create from terminal/rpc
	len(data_offset+text_len),
	msid(mymsid),
	svid(mysvid),
	peer(mysvid),
	status(MSGSTAT_DAT), // not VAL because not yet saved
	path(0)
  { data=(uint8_t*)std::malloc(len);
    now=time(NULL);
    got=now;
    assert(mymsid<=max_msid); // this server can not send any more messages
    assert(len<=max_length);
    assert(data!=NULL);
    data[0]=text_type;
    memcpy(data+1,&len,3); //assume bigendian :-)
    //this will be the signature
    memcpy(data+4+64+0,&mysvid,2);
    memcpy(data+4+64+2,&mymsid,4); //FIXME, consider removing for _BLK and _CND
    memcpy(data+4+64+6,&now,4);
    memcpy(data+4+64+10,text,text_len);
    if(text_type==MSGTYPE_BLK){
      ed25519_sign(data+4+64+10,sizeof(header_t)-4,mysk,mypk,data+4); // consider signing also svid,msid,0
      char hash[4*SHA256_DIGEST_LENGTH];
      ed25519_key2text(hash,data+4,2*SHA256_DIGEST_LENGTH);
      fprintf(stderr,"BLOCK SIGNATURE created %.*s (%d)\n",4*SHA256_DIGEST_LENGTH,hash,mysvid);}
    else if(text_type==MSGTYPE_CND){
      ed25519_sign(data+4+64,10+sizeof(hash_t),mysk,mypk,data+4);}
    else{
      ed25519_sign(data+4+64,10+text_len,mysk,mypk,data+4);}
    hash.num=dohash(mysvid);
    hash_signature(data+4);
  }

  ~message()
  { if(data!=NULL){
      free(data);
      data=NULL;}
  }

  uint8_t hashtype(void)
  { return hash.dat[1];
  }

  uint64_t dohash(void) // default double_spend type
  { union {uint64_t num; uint8_t dat[8];} h;
    h.dat[0]=0;
    h.dat[1]=MSGTYPE_DBL;
    memcpy(h.dat+2,&msid,4);
    memcpy(h.dat+6,&svid,2);
    return(h.num);
  }

  uint64_t dohash(uint16_t mysvid)
  { union {uint64_t num; uint8_t dat[8];} h;
    h.dat[0]=data[4+(mysvid%64)];
    h.dat[1]=data[0];
    memcpy(h.dat+2,&msid,4);
    memcpy(h.dat+6,&svid,2);
    return(h.num);
  }

  uint64_t dohash(uint8_t* d)
  { union {uint64_t num; uint8_t dat[8];} h;
    h.dat[0]=d[1];
    h.dat[1]=*d;
    if(*d==MSGTYPE_PUT||*d==MSGTYPE_GET){
      h.dat[1]=MSGTYPE_TXS;}
    if(*d==MSGTYPE_CNP||*d==MSGTYPE_CNG){
      h.dat[1]=MSGTYPE_CND;}
    if(*d==MSGTYPE_BLP||*d==MSGTYPE_BLG){
      h.dat[1]=MSGTYPE_BLK;}
    if(*d==MSGTYPE_DBP||*d==MSGTYPE_DBG){
      h.dat[1]=MSGTYPE_DBL;}
    memcpy(h.dat+2,&msid,4);
    memcpy(h.dat+6,&svid,2);
    return(h.num);
  }

  // parse header, this should go to peer so that peer can react faster
  int header(uint32_t peer_svid)
  { 
    assert(know.empty());
    assert(busy.empty());
    assert(sent.empty());
    got=time(NULL);
    if(data[0]==MSGTYPE_INI){
      memcpy(&len,data+1,3);
      if(len!=4+64+10+sizeof(handshake_t)){
        std::cerr << "ERROR: no handshake \n"; // TODO, ban ip
        return 0;}
      data=(uint8_t*)std::realloc(data,len);
      if(data==NULL){
        std::cerr << "ERROR: realloc failed \n";
        return 0;}
      return 2;}
    if(!peer_svid){ // peer not authenticated yet
      std::cerr << "ERROR: peer not authenticated\n"; // TODO, ban ip
      return 0;}
    peer=peer_svid; // set source of message
    if( data[0]==MSGTYPE_PUT||
        data[0]==MSGTYPE_GET||
        data[0]==MSGTYPE_DBP||
        data[0]==MSGTYPE_DBG||
        data[0]==MSGTYPE_BLP||
        data[0]==MSGTYPE_BLG||
        data[0]==MSGTYPE_CNP||
        data[0]==MSGTYPE_CNG){
      memcpy(&msid,data+2,4);
      memcpy(&svid,data+6,2);
      hash.num=dohash(data);
      len=header_length;
      return 1;} // short message
    if(data[0]==MSGTYPE_TXS||data[0]==MSGTYPE_DBL||data[0]==MSGTYPE_CND||data[0]==MSGTYPE_BLK){
      memcpy(&len,data+1,3);
      if((data[0]==MSGTYPE_TXS && len>max_length) || (data[0]==MSGTYPE_DBL && len>4+2*max_length) || len<=4+64+10){ // bad format
        std::cerr<<"ERROR in message format\n";
        return 0;}
      data=(uint8_t*)std::realloc(data,len);
      if(data==NULL){
        std::cerr << "ERROR: failed to allocate memory\n"; // TODO, ban ip
        return 0;}
      return 2;}
    if(data[0]==MSGTYPE_STP){
      std::cerr << "STOP header received\n";
      len=SHA256_DIGEST_LENGTH+1;
      data=(uint8_t*)std::realloc(data,len);
      return 2;}
    if(data[0]==MSGTYPE_SER){
      std::cerr << "SERVERS request header received\n";
      svid=peer_svid;
      len=header_length;
      return 1;}
    if(data[0]==MSGTYPE_HEA){
      std::cerr << "HEADERS request header received\n";
      svid=peer_svid;
      len=header_length;
      return 1;}
    if(data[0]==MSGTYPE_TXL){
      std::cerr << "TXSLIST request header received\n";
      svid=peer_svid;
      len=header_length;
      return 1;}
    if(data[0]==MSGTYPE_TXP){
      memcpy(&len,data+1,3);
      std::cerr << "TXSLIST data header received\n";
      svid=peer_svid;
      return 1;}
    if(data[0]==MSGTYPE_SOK){
      std::cerr << "SYNC OK received\n";
      svid=peer_svid;
      len=header_length;
      return 1;} // short message
    std::cerr << "ERROR: unknown message from peer: " << (int)((int)(data[0])+0) << "\n"; // TODO, ban ip
    return 0;
  }

  void hash_signature(uint8_t* sig)
  { if(sig==NULL){
      bzero(sigh,SHA256_DIGEST_LENGTH); 
      return;}
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    uint64_t h[4];
    uint64_t g[8];
    memcpy(g,sig,8*sizeof(uint64_t));
    h[0]=g[0]^g[4];
    h[1]=g[1]^g[5];
    h[2]=g[2]^g[6];
    h[3]=g[3]^g[7];
    SHA256_Update(&sha256,h,4*sizeof(uint64_t));
    SHA256_Final(sigh,&sha256);
  }

  int check_signature(servers& srvs,uint16_t mysvid)
  {
    //FIXME, should include previous hash in signed message
    if(data[0]==MSGTYPE_TXS || data[0]==MSGTYPE_INI || data[0]==MSGTYPE_CND || data[0]==MSGTYPE_DBL || data[0]==MSGTYPE_BLK){
      memcpy(&svid,data+4+64+0,2);
      memcpy(&msid,data+4+64+2,4);
      memcpy( &now,data+4+64+6,4);
      if(srvs.nodes.size()<=svid){ //unknown svid
std::cerr << "ERROR: unknown svid\n";
        return(-1);}
std::cerr << "MSG LEN: " << len << " SVID: " << svid << " MSID: " << msid << "\n";
      hash.num=dohash(mysvid);
      status=MSGSTAT_DAT; // have data
      hash_signature(data+4);
      if(data[0]==MSGTYPE_BLK){ //this signature format is different because these signatures are stored later without the header
	if(memcmp(data+4+64+2,data+4+64+10,4)){ //WARNING, 'now' must be first element of header_t
	  std::cerr<<"ERROR, BLK message msid error\n";
	  return(1);}
        return(ed25519_sign_open(data+4+64+10,sizeof(header_t)-4,srvs.nodes[svid].pk,data+4));}
      if(data[0]==MSGTYPE_CND){ //FIXME, consider changing the signature format
        return(ed25519_sign_open(data+4+64,10+sizeof(hash_t),srvs.nodes[svid].pk,data+4));}
      return(ed25519_sign_open(data+4+64,len-4-64,srvs.nodes[svid].pk,data+4));}
    if(data[0]==MSGTYPE_DBL){ // double message //TODO untested !!!
//FIXME, check time, if any of the 2 messages is too old ignore dbl spend and maybe react
//FIXME, compare only same type of message, detect type based on length
      uint32_t len1,len2,msid2,now2;
      uint16_t svid2;
      uint8_t *data1,*data2; 
      data1=data+4;
      memcpy(&len1,data1+1,3);
      if(len<4+len1+4+64+10){ // bad format
        return(-1);}
      memcpy(&svid,data1+4+64+0,2);
      memcpy(&msid,data1+4+64+2,4);
      memcpy( &now,data1+4+64+6,4);
      memcpy(&len2,data1+len1+1,3);
      data2=data1+len1;
      if(4+len1+len2!=len){ // bad format
        return(-1);}
      memcpy(&svid2,data2+4+64+0,2);
      memcpy(&msid2,data2+4+64+2,4);
      memcpy( &now2,data2+4+64+6,4);
      if((svid!=svid2)||(msid!=msid2)){ // bad format
        return(-1);}
      if(len1==len2 && !memcmp(data1+4+64,data2+4+64,len1-4-64)){ // equal messages
        return(-1);}
      if(srvs.nodes.size()<=svid){ //unknown svid
        return(-1);}
      const uint8_t* m[2]={data1+4+64,data2+4+64};
      size_t mlen[2]={len1,len2};
      const uint8_t* pk[2]={srvs.nodes[svid].pk,srvs.nodes[svid].pk};
      const uint8_t* rs[2]={data1+4,data2+4};
      int valid[2];
      hash.num=dohash();
      status=MSGSTAT_DAT; // have data
      hash_signature(NULL);
      return(ed25519_sign_open_batch(m,mlen,pk,rs,2,valid));}
  }

  void print_text(const char* suffix) const
  { std::printf("%04X[%04X-%02X]%08X:[%d]",peer,svid,msid&0xff,time,len-4-64-10);
    if(len>4+64+10){
      std::cout.write((char*)data+4+64+10,len-4-64-10);}
    std::cout << suffix << "/" << msid << "\n";
  }

  void print(const char* suffix) const
  { std::printf("%04X[%04X-%02X]%08X:[%d]",peer,svid,msid&0xff,time,len-4-64-10);
    //std::cout << suffix << "\n";
    std::cout << suffix << "/" << msid << "\n";
  }

  void print_header()
  { char hash[2*SHA256_DIGEST_LENGTH];
    header_t* h=(header_t*)(data+4+64+10);
    fprintf(stderr,"HEADER: now:%08x txs:%08x nod:%d\n",h->now,h->txs,h->nod);
    ed25519_key2text(hash,h->oldhash,32);
    fprintf(stderr,"OLDHASH: %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
    ed25519_key2text(hash,h->txshash,32);
    fprintf(stderr,"TXSHASH: %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
    ed25519_key2text(hash,h->nodhash,32);
    fprintf(stderr,"NODHASH: %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
    ed25519_key2text(hash,h->nowhash,32);
    fprintf(stderr,"NOWHASH: %.*s\n",2*SHA256_DIGEST_LENGTH,hash);
  }

  int load() //TODO, consider locking 
  { uint32_t head;
    char filename[64];
    if(data!=NULL){
      std::cerr << "ERROR: loading message while data not empty\n";
      return(0);}
    sprintf(filename,"%08X/%02x_%04x_%08x.txt",path,(uint32_t)hashtype(),svid,msid); // size depends on the time_ shift and maximum number of banks (0xffff expected) !!
    std::ifstream myfile(filename,std::ifstream::binary);
    if(!myfile){
      std::cerr << "ERROR: opening message failed\n";
      return(0);}
    myfile.read((char*)&head,4);
    if(!myfile){
      std::cerr << "ERROR: reading length\n";
      return(0);}
    if(len==header_length){
      len=(head)>>8;}
    else if(len!=((head)>>8)){
      std::cerr << "ERROR: length mismatch\n";
      return(0);}
    assert(len>4+64 && len<=4+2*max_length); // accept DBL messages length
    data=(uint8_t*)std::malloc(len);
    memcpy(data,&head,4);
    myfile.read((char*)data+4,len-4); // TODO, consider loading more saved data (status?)
    if(!myfile){
      std::cerr << "ERROR: reading data\n";
      free(data);
      data=NULL;
      return(0);}
    myfile.close();
    //status=MSGSTAT_VAL;
    return(1);
  }

  int save(uint32_t nowpath,uint32_t newpath) //TODO, consider locking
  { char filename[64];
    if(hashtype()==MSGTYPE_BLK || hashtype()==MSGTYPE_CND){
      path=msid;
      if(path!=nowpath && path!=newpath){
        return(0);}}
    else{
      path=now-now%BLOCKSEC;
      if(path<nowpath){
        path=nowpath;}
      if(path>newpath){
        return(0);}}
    sprintf(filename,"%08X/%02x_%04x_%08x.txt",path,(uint32_t)hashtype(),svid,msid); // size depends on the time_ shift and maximum number of banks (0xffff expected) !!
    std::ofstream myfile(filename,std::ifstream::binary);
    if(!myfile){
      std::cerr << "ERROR: failed to open " << filename << "\n";
      return(0);}
    myfile.write((char*)data,len); // TODO, consider saving more data ... maybe the status too !!!
    if(!myfile){
      std::cerr << "ERROR: failed to write to " << filename << "\n";
      return(0);}
    myfile.close();
    // should set file attributes (time)
    //TODO, maybe change status to VAL here
    //status=MSGSTAT_VAL;
    return(1);
  }

  int move(uint32_t newpath) //TODO, consider locking
  { char filename[64];
    sprintf(filename,"%08X/%02x_%04x_%08x.txt",path,(uint32_t)hashtype(),svid,msid); // size depends on the time_ shift and maximum number of banks (0xffff expected) !!
    std::ofstream myfile(filename,std::ifstream::binary);
    if(!myfile){
      std::cerr << "ERROR: failed to open " << filename << "\n";
      return(0);}
    myfile.write((char*)data,len); // TODO, consider saving more data ... maybe the status too !!!
    if(!myfile){
      std::cerr << "ERROR: failed to write to " << filename << "\n";
      return(0);}
    myfile.close();
    sprintf(filename,"%08X/%02x_%04x_%08x.txt",path,(uint32_t)hashtype(),svid,msid); // size depends on the time_ shift and maximum number of banks (0xffff expected) !!
    unlink(filename);
    path=newpath;
    return(1);
  }

  void remove() //TODO, consider locking
  { char filename[64];
    sprintf(filename,"%08X/%02x_%04x_%08x.txt",path,(uint32_t)hashtype(),svid,msid); // size depends on the time_ shift and maximum number of banks (0xffff expected) !!
    unlink(filename);
    return;
  }

  void update(boost::shared_ptr<message>& msg)
  { uint32_t l=msg->len;
    uint8_t *d=msg->data;
    mtx_.lock();
    msg->len=len;
    msg->data=data;
    len=l;
    data=d;
    know.insert(msg->peer);
    memcpy(sigh,msg->sigh,SHA256_DIGEST_LENGTH);
    peer=msg->peer;
    now=msg->now;
    got=msg->got;
    if(len>header_length){
      status=MSGSTAT_DAT;}
    mtx_.unlock();
  }

  void busy_insert(uint16_t p)
  { mtx_.lock();
    got=time(NULL);
    busy.insert(p);
    mtx_.unlock();
  }

  void know_insert(uint16_t p)
  { mtx_.lock();
    know.insert(p);
    mtx_.unlock();
  }

  void sent_insert(uint16_t p)
  { mtx_.lock();
    sent.insert(p);
    mtx_.unlock();
  }

  uint16_t request() //find a peer from which we will request the message
  { if(status>=MSGSTAT_DAT || len!=header_length){
      std::cerr<<"IGNORING REQUEST for "<<svid<<":"<<msid<<"\n";
      return(0);}
    uint32_t mynow=time(NULL);
    mtx_.lock();
    if(mynow<=got+MAX_MSGWAIT){
      mtx_.unlock();
      return(0);}
    for(auto k=know.begin();k!=know.end();k++){
      auto s=busy.find(*k);
      if(s==busy.end()){
        got=mynow;
        busy.insert(*k);
        mtx_.unlock();
        return(*k);}}
    mtx_.unlock();
    return(0);
  }
     

private:
};

typedef boost::shared_ptr<message> message_ptr;
typedef std::deque<message_ptr> message_queue;

#endif // MESSAGE_HPP
