#include <pjsua2.hpp>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <cctype>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

using namespace pj;

static Endpoint ep;

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
static std::string shellQuote(const std::string& s){
  std::string out="'";
  for(char c:s){ if(c=='\'') out+="'\\''"; else out+=c; }
  out+="'";
  return out;
}

class AstelCall: public Call {
public:
  std::string state="CALLING";
  bool mediaReady=false;
  std::mutex mu;
  std::unique_ptr<AudioMediaPlayer> player;
  std::unique_ptr<AudioMediaRecorder> recorder;
  std::string recordPath;
  std::vector<std::string> transcript;
  std::atomic<bool> transcribing{false};
  std::atomic<bool> stopTranscription{false};
  AstelCall(Account& a,int id=PJSUA_INVALID_ID):Call(a,id){}
  void onCallState(OnCallStateParam&) override {
    auto ci=getInfo(); std::lock_guard<std::mutex> l(mu); state=ci.stateText;
    std::cout<<"Call "<<ci.id<<" -> "<<state<<" ("<<ci.lastStatusCode<<")\n";
  }
  void onCallMediaState(OnCallMediaStateParam&) override {
    try {
      auto ci=getInfo();
      bool ready=false;
      for(const auto& m:ci.media){
        if(m.type==PJMEDIA_TYPE_AUDIO && m.status==PJSUA_CALL_MEDIA_ACTIVE){ ready=true; break; }
      }
      { std::lock_guard<std::mutex> l(mu); mediaReady=ready; }
      std::cout<<"Call "<<ci.id<<" audio media -> "<<(ready?"ACTIVE":"not active")<<"\n";
      if(ready && !recorder){
        recordPath="/tmp/astel-in-"+std::to_string(ci.id)+".wav";
        recorder=std::make_unique<AudioMediaRecorder>();
        recorder->createRecorder(recordPath);
        getAudioMedia(-1).startTransmit(*recorder);
        startTranscriber();
      }
    } catch(Error& e){ std::cerr<<"Media state error: "<<e.info()<<"\n"; }
  }
  void startTranscriber(){
    if(transcribing.exchange(true)) return;
    auto self=this;
    std::thread([self]{
      try { ep.libRegisterThread("astel-stt"); } catch(...) {}
      unsigned long chunkNo=0;
      // Low-latency mode: shorter overlapping windows reduce the time between
      // the remote party speaking and text becoming available to the UI.
      while(!self->stopTranscription.load()){
        std::this_thread::sleep_for(std::chrono::milliseconds(1200));
        std::string src; { std::lock_guard<std::mutex> l(self->mu); src=self->recordPath; }
        if(src.empty()) continue;

        const std::string base="/tmp/astel-whisper-"+std::to_string(self->getId())+"-"+std::to_string(chunkNo++);
        const std::string chunk=base+".wav";

        // PJSIP's recorder WAV is intentionally unfinished while the call is
        // active. ffmpeg can recover the live PCM stream and write a normal,
        // finalized WAV for Whisper. Keep only the newest 2.5 seconds.
        std::string ff="ffmpeg -loglevel error -y -sseof -2.5 -i "+shellQuote(src)+
                       " -ar 16000 -ac 1 -c:a pcm_s16le "+shellQuote(chunk);
        if(std::system(ff.c_str())!=0){
          std::cerr<<"STT ffmpeg failed for call "<<self->getId()<<"\\n";
          std::remove(chunk.c_str());
          continue;
        }

        std::string wc="/home/ubuntu/whisper.cpp/build/bin/whisper-cli -m /home/ubuntu/whisper.cpp/models/ggml-tiny.en.bin -f "+shellQuote(chunk)+" -l en --no-timestamps --threads 2 -bs 1 -bo 1 -otxt -of "+shellQuote(base)+" >/dev/null 2>&1";
        int wr=std::system(wc.c_str());
        if(wr==0){
          std::ifstream in(base+".txt"); std::string line, all;
          while(std::getline(in,line)){ if(!all.empty()) all+=" "; all+=line; }
          while(!all.empty() && std::isspace((unsigned char)all.front())) all.erase(all.begin());
          while(!all.empty() && std::isspace((unsigned char)all.back())) all.pop_back();
          std::string lowered=all;
          for(char& ch:lowered) ch=(char)std::tolower((unsigned char)ch);
          bool blank=lowered.empty() || lowered=="[blank_audio]" || lowered=="[blank audio]" ||
                     lowered=="[silence]" || lowered=="(silence)" || lowered=="[music]";
          if(!blank){
            std::lock_guard<std::mutex> l(self->mu);
            if(self->transcript.empty() || self->transcript.back()!=all){
              self->transcript.push_back(all);
              std::cout<<"STT "<<self->getId()<<": "<<all<<"\\n";
            }
          }
        } else {
          std::cerr<<"STT whisper failed for call "<<self->getId()<<" (exit "<<wr<<")\\n";
        }

        // These are scratch files only. Delete each chunk immediately so STT
        // does not accumulate storage during a call.
        std::remove(chunk.c_str());
        std::remove((base+".txt").c_str());
      }
      self->transcribing=false;
    }).detach();
  }
  std::string getTranscriptJson(){
    std::lock_guard<std::mutex> l(mu); std::string j="[";
    for(size_t i=0;i<transcript.size();++i){ if(i) j+=","; j+="{\"speaker\":\"remote\",\"text\":\""+jsonStr(transcript[i])+"\"}"; }
    return j+"]";
  }
  std::string getState(){ std::lock_guard<std::mutex> l(mu); return state; }
  bool isMediaReady(){ std::lock_guard<std::mutex> l(mu); return mediaReady; }
  void speak(const std::string& text){
    auto ci=getInfo();
    // Some PSTN legs can provide usable audio while SIP is still in an
    // early-dialog state (for example 183 Session Progress). What matters
    // for TTS injection is that an audio media stream exists and is active.
    bool audioActive=false;
    for(const auto& m:ci.media){
      if(m.type==PJMEDIA_TYPE_AUDIO && m.status==PJSUA_CALL_MEDIA_ACTIVE){ audioActive=true; break; }
    }
    if(!audioActive) throw std::runtime_error("call audio is not ready yet");
    std::string wav="/tmp/astel-"+std::to_string(ci.id)+".wav";
    std::string cmd="/home/ubuntu/astel-piper/bin/python3 -m piper -m /home/ubuntu/astel-voices/en_US-lessac-medium.onnx -f "+shellQuote(wav)+" -- "+shellQuote(text);
    if(std::system(cmd.c_str())!=0) throw std::runtime_error("Piper synthesis failed");
    auto media=getAudioMedia(-1);
    std::lock_guard<std::mutex> l(mu);
    if(player){ try{player->stopTransmit(media);}catch(...){} player.reset(); }
    player=std::make_unique<AudioMediaPlayer>();
    player->createPlayer(wav, PJMEDIA_FILE_NO_LOOP);
    player->startTransmit(media);
  }
};

class AstelAccount: public Account {
public:
  void onRegState(OnRegStateParam&) override {
    auto ai=getInfo(); std::cout<<"SIP registration: "<<ai.regStatus<<" "<<ai.regStatusText<<"\n";
  }
};

static AstelAccount account;
static std::mutex callsMu;
static std::unordered_map<std::string,std::shared_ptr<AstelCall>> calls;
static std::atomic<unsigned long> seq{1};

static void handle(int fd){
  try { ep.libRegisterThread("astel-http"); }
  catch (Error& e) { std::cerr<<"PJLIB thread registration failed: "<<e.info()<<"\n"; close(fd); return; }
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
        reply(fd,200,"{\"callId\":\""+id+"\",\"state\":\""+jsonStr(c->getState())+"\",\"mediaReady\":"+(c->isMediaReady()?"true":"false")+",\"transcript\":"+c->getTranscriptJson()+"}");
      } else if(method=="DELETE" && rest==id){
        c->stopTranscription=true; CallOpParam p; c->hangup(p); reply(fd,200,"{\"ok\":true}");
      } else if(method=="POST" && rest==id+"/dtmf"){
        std::string digits=field(body,"digits"); CallSendDtmfParam p; p.digits=digits; c->sendDtmf(p); reply(fd,200,"{\"ok\":true}");
      } else if(method=="POST" && rest==id+"/speak"){
        std::string text=field(body,"text");
        if(text.empty()) reply(fd,400,"{\"error\":\"text required\"}");
        else { c->speak(text); reply(fd,200,"{\"ok\":true}"); }
      } else reply(fd,404,"{\"error\":\"not found\"}");
    } else reply(fd,404,"{\"error\":\"not found\"}");
  } catch(Error& e){ reply(fd,400,"{\"error\":\""+jsonStr(e.info())+"\"}"); }
    catch(std::exception& e){ reply(fd,400,"{\"error\":\""+jsonStr(e.what())+"\"}"); }
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
  ac.sipConfig.proxies.push_back("sip:sip.callcentric.net;transport=tcp");
  AuthCredInfo cred("digest","sip.callcentric.net",user,0,pass); ac.sipConfig.authCreds.push_back(cred);
  account.create(ac);

  int s=socket(AF_INET,SOCK_STREAM,0), yes=1; setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
  sockaddr_in a{}; a.sin_family=AF_INET; a.sin_port=htons(8765); a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  if(bind(s,(sockaddr*)&a,sizeof(a))<0||listen(s,16)<0){perror("listen");return 2;}
  std::cout<<"Astel daemon listening on http://127.0.0.1:8765\n";
  while(true){int fd=accept(s,nullptr,nullptr); if(fd>=0) std::thread(handle,fd).detach();}
}
