#include <pjsua2.hpp>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

using namespace pj;

static std::string env(const char* k){ const char* v=std::getenv(k); return v?v:""; }
static std::string jsonStr(const std::string&s){ std::string o; for(char c:s){ if(c=='"'||c=='\\')o+='\\'; if(c=='\n'){o+="\\n";continue;} o+=c;} return o; }
static std::string field(const std::string& body,const std::string& key){
  auto p=body.find("\""+key+"\""); if(p==std::string::npos)return "";
  p=body.find(':',p); if(p==std::string::npos)return ""; p=body.find('"',p); if(p==std::string::npos)return "";
  auto e=body.find('"',p+1); if(e==std::string::npos)return ""; return body.substr(p+1,e-p-1);
}
static void reply(int fd,int code,const std::string& body){
  std::string status=code==200?"OK":code==201?"Created":code==404?"Not Found":"Bad Request";
  std::string r="HTTP/1.1 "+std::to_string(code)+" "+status+"\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin: *\r\nContent-Length: "+std::to_string(body.size())+"\r\nConnection: close\r\n\r\n"+body;
  send(fd,r.data(),r.size(),0);
}

class AstelCall: public Call {
public:
  std::string state="CALLING";
  std::mutex mu;
  AstelCall(Account& a,int id=PJSUA_INVALID_ID):Call(a,id){}
  void onCallState(OnCallStateParam&) override {
    auto ci=getInfo(); std::lock_guard<std::mutex> l(mu); state=ci.stateText;
    std::cout<<"Call "<<ci.id<<" -> "<<state<<" ("<<ci.lastStatusCode<<")\n";
  }
  std::string getState(){ std::lock_guard<std::mutex> l(mu); return state; }
};

class AstelAccount: public Account {
public:
  void onRegState(OnRegStateParam&) override {
    auto ai=getInfo(); std::cout<<"SIP registration: "<<ai.regStatus<<" "<<ai.regStatusText<<"\n";
  }
};

static Endpoint ep;
static AstelAccount account;
static std::mutex callsMu;
static std::unordered_map<std::string,std::shared_ptr<AstelCall>> calls;
static std::atomic<unsigned long> seq{1};

static void handle(int fd){
  char buf[16384]; int n=recv(fd,buf,sizeof(buf)-1,0); if(n<=0){close(fd);return;} buf[n]=0;
  std::string req(buf,n), method, path; auto sp=req.find(' '), sp2=req.find(' ',sp+1);
  if(sp==std::string::npos||sp2==std::string::npos){reply(fd,400,"{\"error\":\"bad request\"}");close(fd);return;}
  method=req.substr(0,sp); path=req.substr(sp+1,sp2-sp-1);
  auto bp=req.find("\r\n\r\n"); std::string body=bp==std::string::npos?"":req.substr(bp+4);

  try {
    if(method=="GET" && path=="/health"){ reply(fd,200,"{\"ok\":true}"); }
    else if(method=="POST" && path=="/calls"){
      std::string number=field(body,"number"); if(number.empty()) number=field(body,"to");
      if(number.empty()){reply(fd,400,"{\"error\":\"number required\"}");}
      else {
        std::string id=std::to_string(seq++); auto c=std::make_shared<AstelCall>(account);
        CallOpParam prm(true); prm.opt.audioCount=1; prm.opt.videoCount=0;
        c->makeCall("sip:"+number+"@sip.callcentric.net",prm);
        {std::lock_guard<std::mutex> l(callsMu);calls[id]=c;}
        reply(fd,201,"{\"callId\":\""+id+"\",\"state\":\"CALLING\"}");
      }
    } else if(path.rfind("/calls/",0)==0) {
      std::string rest=path.substr(7), id=rest.substr(0,rest.find('/'));
      std::shared_ptr<AstelCall> c; {std::lock_guard<std::mutex> l(callsMu);auto it=calls.find(id);if(it!=calls.end())c=it->second;}
      if(!c){reply(fd,404,"{\"error\":\"call not found\"}");}
      else if(method=="GET" && rest==id){
        reply(fd,200,"{\"callId\":\""+id+"\",\"state\":\""+jsonStr(c->getState())+"\",\"transcript\":[]}");
      } else if(method=="DELETE" && rest==id){
        CallOpParam p; c->hangup(p); reply(fd,200,"{\"ok\":true}");
      } else if(method=="POST" && rest==id+"/dtmf"){
        std::string digits=field(body,"digits"); CallSendDtmfParam p; p.digits=digits; c->sendDtmf(p); reply(fd,200,"{\"ok\":true}");
      } else if(method=="POST" && rest==id+"/speak"){
        reply(fd,200,"{\"ok\":false,\"pending\":\"piper\"}");
      } else reply(fd,404,"{\"error\":\"not found\"}");
    } else reply(fd,404,"{\"error\":\"not found\"}");
  } catch(Error& e){ reply(fd,400,"{\"error\":\""+jsonStr(e.info())+"\"}"); }
  close(fd);
}

int main(){
  std::string user=env("CALLCENTRIC_USERNAME"), pass=env("CALLCENTRIC_PASSWORD");
  if(user.empty()||pass.empty()){std::cerr<<"CALLCENTRIC_USERNAME and CALLCENTRIC_PASSWORD are required\n";return 1;}
  ep.libCreate(); EpConfig ec; ep.libInit(ec);
  TransportConfig tc; tc.port=5060; ep.transportCreate(PJSIP_TRANSPORT_TCP,tc);
  ep.libStart();
  ep.audDevManager().setNullDev();

  AccountConfig ac; ac.idUri="sip:"+user+"@sip.callcentric.net"; ac.regConfig.registrarUri="sip:sip.callcentric.net";
  // Registration must use the TCP transport we created above. Without this,
  // PJSUA2 may try the default UDP transport, which this daemon does not create.
  ac.sipConfig.proxies.push_back("sip:sip.callcentric.net;transport=tcp");
  AuthCredInfo cred("digest","sip.callcentric.net",user,0,pass); ac.sipConfig.authCreds.push_back(cred);
  account.create(ac);

  int s=socket(AF_INET,SOCK_STREAM,0), yes=1; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
  sockaddr_in a{}; a.sin_family=AF_INET; a.sin_port=htons(8765); a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  if(bind(s,(sockaddr*)&a,sizeof(a))<0||listen(s,16)<0){perror("listen");return 2;}
  std::cout<<"Astel daemon listening on http://127.0.0.1:8765\n";
  while(true){int fd=accept(s,nullptr,nullptr); if(fd>=0) std::thread(handle,fd).detach();}
}
